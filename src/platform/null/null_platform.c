/**
 * HUI - platform/null/null_platform.c
 * Window system that opens no window. Windows only exist as a size, which is
 * all the layout needs: used for headless runs and unit tests, together with
 * HUI_Inject... for input.
 */

#include "../platform.h"
#include "../../core/thread.h"

struct hui_pwindow_t {
	int width, height;
	int x, y;
	void* user;
	char* title;
};

static struct {
	hui_pevent_handler_t handler;
	hui_mutex_t mutex;
	hui_cond_t cond;
	int woken;
	char* clipboard;
} g_null;

static int hui_null_init(hui_pevent_handler_t handler) {
	memset(&g_null, 0, sizeof(g_null));
	g_null.handler = handler;
	hui_mutex_init(&g_null.mutex);
	hui_cond_init(&g_null.cond);
	return HUI_TRUE;
}

static void hui_null_shutdown(void) {
	hui_free(g_null.clipboard);
	hui_cond_destroy(&g_null.cond);
	hui_mutex_destroy(&g_null.mutex);
	memset(&g_null, 0, sizeof(g_null));
}

static void* hui_null_native_display(void) {
	return NULL;
}

static hui_pwindow_t* hui_null_window_create(const hui_pwindow_desc_t* desc) {
	hui_pwindow_t* window = (hui_pwindow_t*)hui_calloc(sizeof(hui_pwindow_t));
	window->width = desc->width;
	window->height = desc->height;
	window->user = desc->user;
	window->title = hui_strdup(desc->title);
	return window;
}

static void hui_null_window_destroy(hui_pwindow_t* window) {
	hui_free(window->title);
	hui_free(window);
}

static void hui_null_window_set_title(hui_pwindow_t* window, const char* title) {
	hui_str_set(&window->title, title);
}

static void hui_null_window_set_size(hui_pwindow_t* window, int width, int height) {
	hui_pevent_t event;
	if (window->width == width && window->height == height) return;
	window->width = width;
	window->height = height;
	/* behave like a real window system: the core learns the size from an event */
	memset(&event, 0, sizeof(event));
	event.type = HUI_PEVENT_RESIZE;
	event.user = window->user;
	event.width = width;
	event.height = height;
	event.scale = 1.0f;
	g_null.handler(&event);
}

static void hui_null_window_get_size(hui_pwindow_t* window, int* width, int* height) {
	*width = window->width;
	*height = window->height;
}

static void hui_null_window_set_min_size(hui_pwindow_t* window, int width, int height) {
	HUI_UNUSED(window);
	HUI_UNUSED(width);
	HUI_UNUSED(height);
}

static void hui_null_window_set_position(hui_pwindow_t* window, int x, int y) {
	window->x = x;
	window->y = y;
}

static void hui_null_window_get_position(hui_pwindow_t* window, int* x, int* y) {
	*x = window->x;
	*y = window->y;
}

static float hui_null_window_get_scale(hui_pwindow_t* window) {
	HUI_UNUSED(window);
	return 1.0f;
}

static void hui_null_window_show(hui_pwindow_t* window, int visible) {
	HUI_UNUSED(window);
	HUI_UNUSED(visible);
}

static void hui_null_window_set_state(hui_pwindow_t* window, hui_pwindow_state_t state) {
	HUI_UNUSED(window);
	HUI_UNUSED(state);
}

static void hui_null_window_set_cursor(hui_pwindow_t* window, HUI_ECursor cursor) {
	HUI_UNUSED(window);
	HUI_UNUSED(cursor);
}

static void hui_null_window_get_native(hui_pwindow_t* window, HUI_NativeWindow* native) {
	HUI_UNUSED(window);
	memset(native, 0, sizeof(*native));
	native->system = HUI_WINDOW_SYSTEM_NONE;
}

static hui_pwindow_t* hui_null_child_create(hui_pwindow_t* parent, int x, int y, int width, int height, void* user) {
	HUI_UNUSED(parent);
	HUI_UNUSED(x);
	HUI_UNUSED(y);
	HUI_UNUSED(width);
	HUI_UNUSED(height);
	HUI_UNUSED(user);
	return NULL;
}

static void hui_null_child_set_rect(hui_pwindow_t* child, int x, int y, int width, int height, int visible) {
	HUI_UNUSED(child);
	HUI_UNUSED(x);
	HUI_UNUSED(y);
	HUI_UNUSED(width);
	HUI_UNUSED(height);
	HUI_UNUSED(visible);
}

static void hui_null_pump(double timeout_seconds) {
	hui_mutex_lock(&g_null.mutex);
	if (!g_null.woken && timeout_seconds != 0.0) {
		if (timeout_seconds < 0.0) {
			while (!g_null.woken) hui_cond_wait(&g_null.cond, &g_null.mutex);
		} else {
			hui_cond_wait_timeout(&g_null.cond, &g_null.mutex, timeout_seconds);
		}
	}
	g_null.woken = HUI_FALSE;
	hui_mutex_unlock(&g_null.mutex);
}

static void hui_null_wake(void) {
	hui_mutex_lock(&g_null.mutex);
	g_null.woken = HUI_TRUE;
	hui_cond_signal(&g_null.cond);
	hui_mutex_unlock(&g_null.mutex);
}

static void hui_null_clipboard_set(const char* text) {
	hui_str_set(&g_null.clipboard, text);
}

static char* hui_null_clipboard_get(void) {
	return hui_strdup(g_null.clipboard);
}

const hui_platform_t* hui_platform_null(void) {
	static const hui_platform_t platform = {
		"null",
		HUI_WINDOW_SYSTEM_NONE,
		hui_null_init,
		hui_null_shutdown,
		hui_null_native_display,
		hui_null_window_create,
		hui_null_window_destroy,
		hui_null_window_set_title,
		hui_null_window_set_size,
		hui_null_window_get_size,
		hui_null_window_set_min_size,
		hui_null_window_set_position,
		hui_null_window_get_position,
		hui_null_window_get_scale,
		hui_null_window_show,
		hui_null_window_set_state,
		hui_null_window_set_cursor,
		hui_null_window_get_native,
		hui_null_child_create,
		hui_null_child_set_rect,
		hui_null_pump,
		hui_null_wake,
		hui_null_clipboard_set,
		hui_null_clipboard_get,
		NULL
	};
	return &platform;
}
