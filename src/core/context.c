/**
 * HUI - core/context.c
 * Library lifecycle, main loop, posted tasks, timers, actions, clipboard.
 */

#include "context.h"
#include "../widgets/common.h"
#include "../render/renderer.h"

hui_context_t g_hui;

static HUI_BackendVTable g_custom_backend;
static int g_custom_backend_set;

void hui_tasks_run(void);

/* ---------------------------------------------------------------------------
 *  Guards
 * ------------------------------------------------------------------------- */

int hui_api_check(const char* function) {
	if (!g_hui.initialized) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "%s: HUI is not initialized", function);
		return HUI_FALSE;
	}
	if (hui_thread_id() != g_hui.ui_thread) {
		hui_error(HUI_ERROR_WRONG_THREAD, HUI_SEVERITY_ERROR,
			"%s must be called from the UI thread (use HUI_PostTask from other threads)", function);
		return HUI_FALSE;
	}
	return HUI_TRUE;
}

void hui_dispatch_begin(void) {
	g_hui.dispatch_depth++;
}

void hui_dispatch_end(void) {
	if (--g_hui.dispatch_depth > 0) return;
	g_hui.dispatch_depth = 0;
	/* nobody up the stack holds pointers any more: release what was deleted */
	if (g_hui.dead_widget_count) hui_widgets_collect();
	if (g_hui.dead_window_count) hui_windows_collect();
}

void hui_wake(void) {
	if (g_hui.initialized && g_hui.platform) g_hui.platform->wake();
}

/* ---------------------------------------------------------------------------
 *  Initialization
 * ------------------------------------------------------------------------- */

void HUI_RegisterBackend(const HUI_BackendVTable* vtable) {
	if (g_hui.initialized) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_RegisterBackend must be called before HUI_Init");
		return;
	}
	if (!vtable || !vtable->Init || !vtable->Shutdown || !vtable->CreateSurface || !vtable->DestroySurface ||
		!vtable->CreateTexture || !vtable->UpdateTexture || !vtable->SetTextureFilter || !vtable->DestroyTexture ||
		!vtable->RenderFrame || !vtable->WaitIdle) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_RegisterBackend: every function of the table is required");
		return;
	}
	g_custom_backend = *vtable;
	g_custom_backend_set = HUI_TRUE;
}

static const HUI_BackendVTable* hui_select_backend(HUI_EBackend backend) {
	switch (backend) {
		case HUI_BACKEND_VULKAN:
#if defined(HUI_BACKEND_VULKAN_ENABLED)
			return hui_backend_vulkan();
#else
			hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_FATAL, "HUI was built without the Vulkan backend");
			return NULL;
#endif
		case HUI_BACKEND_NULL:
			return hui_backend_null();
		case HUI_BACKEND_CUSTOM:
			if (!g_custom_backend_set) {
				hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_FATAL, "HUI_BACKEND_CUSTOM needs HUI_RegisterBackend first");
				return NULL;
			}
			return &g_custom_backend;
	}
	hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_FATAL, "HUI_Init: unknown backend");
	return NULL;
}

int HUI_Init(HUI_EBackend backend) {
	HUI_InitDesc desc;
	memset(&desc, 0, sizeof(desc));
	desc.backend = backend;
	return HUI_InitEx(&desc);
}

int HUI_InitEx(const HUI_InitDesc* user_desc) {
	HUI_InitDesc desc;
	HUI_BackendInitInfo info;
	const HUI_BackendVTable* backend;
	int workers, threaded;

	if (g_hui.initialized) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_WARNING, "HUI_Init: already initialized");
		return HUI_FALSE;
	}
	memset(&desc, 0, sizeof(desc));
	if (user_desc) desc = *user_desc;
	if (desc.backend == 0) desc.backend = HUI_BACKEND_VULKAN;

	backend = hui_select_backend(desc.backend);
	if (!backend) return HUI_FALSE;

	memset(&g_hui, 0, sizeof(g_hui));
	g_hui.ui_thread = hui_thread_id();
	g_hui.start_time = hui_clock_seconds();
	g_hui.ui_scale = desc.ui_scale > 0.0f ? desc.ui_scale : 1.0f;
	g_hui.vsync = !desc.disable_vsync;
	g_hui.backend = desc.backend;
	g_hui.default_font = HUI_INVALID_ID;
	hui_pool_init(&g_hui.windows, HUI_HANDLE_WINDOW);
	hui_pool_init(&g_hui.widgets, HUI_HANDLE_WIDGET);
	hui_pool_init(&g_hui.fonts, HUI_HANDLE_FONT);
	hui_pool_init(&g_hui.textures, HUI_HANDLE_TEXTURE);
	hui_pool_init(&g_hui.timers, HUI_HANDLE_TIMER);
	hui_pool_init(&g_hui.tree_items, HUI_HANDLE_TREE_ITEM);
	hui_mutex_init(&g_hui.task_mutex);

	/* windowing */
	g_hui.platform = hui_platform_select(desc.headless);
	if (!g_hui.platform || !g_hui.platform->init(hui_window_on_platform_event)) {
		hui_error(HUI_ERROR_PLATFORM, HUI_SEVERITY_FATAL, "HUI_Init: cannot initialize the %s window system",
			g_hui.platform ? g_hui.platform->name : "native");
		goto fail_platform;
	}

	/* job system: leave one core to the UI thread and one to the render thread */
	threaded = !desc.disable_render_thread;
	if (desc.worker_threads < 0) {
		workers = 0;
	} else if (desc.worker_threads > 0) {
		workers = desc.worker_threads;
	} else {
		workers = hui_cpu_count() - 1 - (threaded ? 1 : 0);
		workers = hui_clampi(workers, 1, 16);
	}
	hui_jobs_init(workers);

	/* rendering */
	memset(&info, 0, sizeof(info));
	info.app_name = desc.app_name ? desc.app_name : "HUI application";
	info.enable_validation = desc.enable_validation;
	info.vsync = g_hui.vsync;
	info.window_system = g_hui.platform->system;
	info.native_display = g_hui.platform->native_display();
	if (!hui_renderer_init(backend, &info, threaded)) {
		hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_FATAL, "HUI_Init: cannot initialize the %s backend", backend->name);
		goto fail_renderer;
	}
	hui_renderer_set_wait_service(g_hui.platform->wait_service);

	g_hui.initialized = HUI_TRUE;
	hui_paint_init();
	hui_theme_apply(HUI_THEME_DARK);
	hui_theme_reset_metrics();
	hui_fonts_init();
	g_hui.theme.font = g_hui.default_font;
	g_hui.last_update_time = hui_clock_seconds();
	return HUI_TRUE;

fail_renderer:
	hui_jobs_shutdown();
	g_hui.platform->shutdown();
fail_platform:
	hui_mutex_destroy(&g_hui.task_mutex);
	hui_pool_free(&g_hui.windows);
	hui_pool_free(&g_hui.widgets);
	hui_pool_free(&g_hui.fonts);
	hui_pool_free(&g_hui.textures);
	hui_pool_free(&g_hui.timers);
	hui_pool_free(&g_hui.tree_items);
	memset(&g_hui, 0, sizeof(g_hui));
	return HUI_FALSE;
}

void HUI_Shutdown(void) {
	uint32_t i;
	if (!hui_api_check(__func__)) return;

	/* windows first: this deletes every widget that is shown */
	hui_windows_destroy_all();
	/* then the widgets the application created but never attached */
	for (i = 0; i < g_hui.widgets.used; i++) {
		hui_widget_t* widget = (hui_widget_t*)hui_pool_at(&g_hui.widgets, i);
		/* internal widgets are destroyed by the widget that owns them */
		if (widget && !widget->parent && !(widget->flags & HUI_WF_INTERNAL)) hui_widget_destroy(widget);
	}
	hui_widgets_collect();

	/* let the running jobs finish and deliver their results */
	hui_jobs_shutdown();
	hui_tasks_run();

	for (i = 0; i < g_hui.timers.used; i++) {
		hui_timer_t* timer = (hui_timer_t*)hui_pool_at(&g_hui.timers, i);
		if (timer) {
			hui_pool_remove(&g_hui.timers, timer->id);
			hui_free(timer);
		}
	}

	hui_xml_shutdown();
	hui_fonts_shutdown();
	hui_textures_shutdown();
	hui_paint_shutdown();
	hui_renderer_shutdown();
	g_hui.platform->shutdown();

	for (i = 0; i < g_hui.action_count; i++) hui_free(g_hui.actions[i].name);
	hui_free(g_hui.actions);
	hui_free(g_hui.tasks);
	hui_free(g_hui.dead_widgets);
	hui_free(g_hui.dead_windows);
	hui_free(g_hui.clipboard);
	hui_free(g_hui.xml_base_path);
	hui_mutex_destroy(&g_hui.task_mutex);
	hui_pool_free(&g_hui.windows);
	hui_pool_free(&g_hui.widgets);
	hui_pool_free(&g_hui.fonts);
	hui_pool_free(&g_hui.textures);
	hui_pool_free(&g_hui.timers);
	hui_pool_free(&g_hui.tree_items);
	memset(&g_hui, 0, sizeof(g_hui));
}

int HUI_IsInitialized(void) {
	return g_hui.initialized;
}

const char* HUI_GetVersion(void) {
	return HUI_VERSION_STRING;
}

/* ---------------------------------------------------------------------------
 *  Tasks
 * ------------------------------------------------------------------------- */

void HUI_PostTask(HUI_CTask task, void* user_data) {
	if (!task) return;
	if (!g_hui.initialized) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_PostTask: HUI is not initialized");
		return;
	}
	hui_mutex_lock(&g_hui.task_mutex);
	HUI_ARRAY_RESERVE(g_hui.tasks, g_hui.task_cap, g_hui.task_count + 1);
	g_hui.tasks[g_hui.task_count].function = task;
	g_hui.tasks[g_hui.task_count].user_data = user_data;
	g_hui.task_count++;
	hui_mutex_unlock(&g_hui.task_mutex);
	hui_wake();
}

void hui_tasks_run(void) {
	hui_task_t* tasks;
	uint32_t count, i;

	/* take the whole batch: tasks posted while running wait for the next update */
	hui_mutex_lock(&g_hui.task_mutex);
	count = g_hui.task_count;
	tasks = NULL;
	if (count) {
		tasks = (hui_task_t*)hui_alloc(sizeof(hui_task_t) * count);
		memcpy(tasks, g_hui.tasks, sizeof(hui_task_t) * count);
		g_hui.task_count = 0;
	}
	hui_mutex_unlock(&g_hui.task_mutex);

	if (!count) return;
	hui_dispatch_begin();
	for (i = 0; i < count; i++) tasks[i].function(tasks[i].user_data);
	hui_dispatch_end();
	hui_free(tasks);
}

static int hui_tasks_pending(void) {
	int pending;
	hui_mutex_lock(&g_hui.task_mutex);
	pending = g_hui.task_count > 0;
	hui_mutex_unlock(&g_hui.task_mutex);
	return pending;
}

void HUI_RunAsync(HUI_CTask task, void* user_data) {
	if (!task) return;
	if (!g_hui.initialized) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_RunAsync: HUI is not initialized");
		return;
	}
	hui_jobs_submit(NULL, task, user_data);
}

void HUI_ParallelFor(HUI_uint count, HUI_uint grain, HUI_CParallelRange function, void* user_data) {
	if (!function) return;
	if (!g_hui.initialized) {
		function(0, count, user_data);
		return;
	}
	hui_jobs_parallel_for(count, grain, function, user_data);
}

void HUI_WakeUp(void) {
	hui_wake();
}

int HUI_GetWorkerThreadCount(void) {
	return g_hui.initialized ? hui_jobs_worker_count() : 0;
}

int HUI_IsUIThread(void) {
	return g_hui.initialized && hui_thread_id() == g_hui.ui_thread;
}

/* ---------------------------------------------------------------------------
 *  Timers
 * ------------------------------------------------------------------------- */

HUI_id HUI_CreateTimer(double interval_seconds, int repeat, HUI_CTimer callback, void* user_data) {
	hui_timer_t* timer;
	HUI_API_GUARD(HUI_INVALID_ID);
	if (!callback || interval_seconds < 0.0) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_CreateTimer: invalid arguments");
		return HUI_INVALID_ID;
	}
	timer = (hui_timer_t*)hui_calloc(sizeof(hui_timer_t));
	/* a repeating timer of zero seconds would spin the loop */
	timer->interval = repeat && interval_seconds < 0.001 ? 0.001 : interval_seconds;
	timer->deadline = hui_clock_seconds() + timer->interval;
	timer->repeat = repeat != 0;
	timer->callback = callback;
	timer->user_data = user_data;
	timer->id = hui_pool_add(&g_hui.timers, timer);
	if (timer->id == HUI_INVALID_ID) {
		hui_free(timer);
		return HUI_INVALID_ID;
	}
	return timer->id;
}

void HUI_DeleteTimer(HUI_id id) {
	hui_timer_t* timer;
	HUI_API_GUARD_VOID();
	timer = (hui_timer_t*)hui_pool_get(&g_hui.timers, id);
	if (!timer) {
		hui_error(HUI_ERROR_INVALID_ID, HUI_SEVERITY_ERROR, "HUI_DeleteTimer: invalid timer");
		return;
	}
	hui_pool_remove(&g_hui.timers, id);
	hui_free(timer);
}

int HUI_IsValidTimer(HUI_id timer) {
	return g_hui.initialized && hui_pool_get(&g_hui.timers, timer) != NULL;
}

double hui_timers_next_delay(void) {
	double now = hui_clock_seconds(), best = -1.0;
	uint32_t i;
	for (i = 0; i < g_hui.timers.used; i++) {
		hui_timer_t* timer = (hui_timer_t*)hui_pool_at(&g_hui.timers, i);
		double delay;
		if (!timer) continue;
		delay = timer->deadline - now;
		if (delay < 0.0) delay = 0.0;
		if (best < 0.0 || delay < best) best = delay;
	}
	return best;
}

static void hui_timers_run(void) {
	double now = hui_clock_seconds();
	uint32_t i, used = g_hui.timers.used;

	hui_dispatch_begin();
	for (i = 0; i < used; i++) {
		hui_timer_t* timer = (hui_timer_t*)hui_pool_at(&g_hui.timers, i);
		HUI_id id;
		HUI_CTimer callback;
		void* user_data;
		if (!timer || timer->deadline > now) continue;
		id = timer->id;
		callback = timer->callback;
		user_data = timer->user_data;
		if (timer->repeat) {
			timer->deadline += timer->interval;
			/* do not try to catch up after a long stall */
			if (timer->deadline < now) timer->deadline = now + timer->interval;
		} else {
			hui_pool_remove(&g_hui.timers, id);
			hui_free(timer);
		}
		callback(id, user_data);
	}
	hui_dispatch_end();
}

/* ---------------------------------------------------------------------------
 *  Actions
 * ------------------------------------------------------------------------- */

static hui_action_t* hui_action_find(const char* name) {
	uint32_t i;
	for (i = 0; i < g_hui.action_count; i++) {
		if (strcmp(g_hui.actions[i].name, name) == 0) return &g_hui.actions[i];
	}
	return NULL;
}

void HUI_BindAction(const char* name, HUI_CAction callback, void* user_data) {
	hui_action_t* action;
	HUI_API_GUARD_VOID();
	if (!name || !name[0]) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_BindAction: empty action name");
		return;
	}
	if (!callback) {
		HUI_UnbindAction(name);
		return;
	}
	action = hui_action_find(name);
	if (!action) {
		HUI_ARRAY_RESERVE(g_hui.actions, g_hui.action_cap, g_hui.action_count + 1);
		action = &g_hui.actions[g_hui.action_count++];
		action->name = hui_strdup(name);
	}
	action->callback = callback;
	action->user_data = user_data;
}

void HUI_UnbindAction(const char* name) {
	hui_action_t* action;
	HUI_API_GUARD_VOID();
	if (!name) return;
	action = hui_action_find(name);
	if (!action) return;
	hui_free(action->name);
	*action = g_hui.actions[--g_hui.action_count];
}

void HUI_SetDefaultActionHandler(HUI_CAction callback, void* user_data) {
	HUI_API_GUARD_VOID();
	g_hui.default_action = callback;
	g_hui.default_action_user = user_data;
}

void hui_fire_action(HUI_id sender, const char* name) {
	hui_action_t* action;
	if (!name || !name[0]) return;
	action = hui_action_find(name);
	if (action) {
		/* copy: the callback may bind or unbind actions */
		HUI_CAction callback = action->callback;
		void* user_data = action->user_data;
		callback(sender, name, user_data);
	} else if (g_hui.default_action) {
		g_hui.default_action(sender, name, g_hui.default_action_user);
	}
}

/* ---------------------------------------------------------------------------
 *  Clipboard
 * ------------------------------------------------------------------------- */

void HUI_SetClipboardText(const char* text) {
	HUI_API_GUARD_VOID();
	g_hui.platform->clipboard_set(text ? text : "");
}

const char* HUI_GetClipboardText(void) {
	HUI_API_GUARD("");
	hui_free(g_hui.clipboard);
	g_hui.clipboard = g_hui.platform->clipboard_get();
	return g_hui.clipboard ? g_hui.clipboard : "";
}

/* ---------------------------------------------------------------------------
 *  Main loop
 * ------------------------------------------------------------------------- */

void HUI_Quit(int exit_code) {
	if (!g_hui.initialized) return;
	hui_atomic_store(&g_hui.exit_code, exit_code);
	hui_atomic_store(&g_hui.quit_requested, 1);
	hui_wake();
}

int HUI_IsRunning(void) {
	uint32_t i;
	if (!g_hui.initialized || hui_atomic_load(&g_hui.quit_requested)) return HUI_FALSE;
	if (!g_hui.had_window) return HUI_TRUE;
	for (i = 0; i < g_hui.windows.used; i++) {
		hui_window_t* window = (hui_window_t*)hui_pool_at(&g_hui.windows, i);
		if (window && !window->dead) return HUI_TRUE;
	}
	return HUI_FALSE;
}

void HUI_SetFrameCallback(HUI_CFrame callback, void* user_data) {
	HUI_API_GUARD_VOID();
	g_hui.frame_callback = callback;
	g_hui.frame_user = user_data;
}

double HUI_GetTime(void) {
	return g_hui.initialized ? hui_clock_seconds() - g_hui.start_time : 0.0;
}

void HUI_PollEvents(void) {
	HUI_API_GUARD_VOID();
	g_hui.platform->pump(0.0);
}

void HUI_WaitEvents(double timeout_seconds) {
	uint32_t i;
	double timer_delay;
	HUI_API_GUARD_VOID();

	/* do not sleep when there is already something to do */
	if (hui_tasks_pending() || hui_atomic_load(&g_hui.redraw_all) || hui_atomic_load(&g_hui.quit_requested)) {
		timeout_seconds = 0.0;
	}
	for (i = 0; i < g_hui.windows.used && timeout_seconds != 0.0; i++) {
		hui_window_t* window = (hui_window_t*)hui_pool_at(&g_hui.windows, i);
		if (window && hui_window_wants_frame(window)) timeout_seconds = 0.0;
	}
	timer_delay = hui_timers_next_delay();
	if (timer_delay >= 0.0 && (timeout_seconds < 0.0 || timer_delay < timeout_seconds)) timeout_seconds = timer_delay;
	g_hui.platform->pump(timeout_seconds);
}

void HUI_Update(void) {
	double now;
	uint32_t i;
	HUI_API_GUARD_VOID();
	/* a platform modal loop may call back while an update is already running */
	if (g_hui.in_update) return;
	g_hui.in_update = HUI_TRUE;

	hui_tasks_run();
	hui_timers_run();

	now = hui_clock_seconds();
	if (g_hui.frame_callback) {
		hui_dispatch_begin();
		g_hui.frame_callback(now - g_hui.last_update_time, g_hui.frame_user);
		hui_dispatch_end();
	}
	g_hui.last_update_time = now;

	if (hui_atomic_exchange(&g_hui.redraw_all, 0)) hui_repaint_all_windows();
	for (i = 0; i < g_hui.windows.used; i++) {
		hui_window_t* window = (hui_window_t*)hui_pool_at(&g_hui.windows, i);
		if (window) hui_window_update(window);
	}
	g_hui.in_update = HUI_FALSE;
}

int HUI_Run(void) {
	HUI_API_GUARD(-1);
	while (HUI_IsRunning()) {
		HUI_WaitEvents(-1.0);
		HUI_Update();
	}
	return hui_atomic_load(&g_hui.exit_code);
}
