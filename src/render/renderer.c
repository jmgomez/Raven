/**
 * HUI - render/renderer.c
 * Render thread and its command queue.
 */

#include "renderer.h"
#include "../core/context.h"

typedef enum hui_rcmd_type_t {
	HUI_RCMD_INIT,
	HUI_RCMD_SHUTDOWN,
	HUI_RCMD_CREATE_SURFACE,
	HUI_RCMD_DESTROY_SURFACE,
	HUI_RCMD_CREATE_TEXTURE,
	HUI_RCMD_UPDATE_TEXTURE,
	HUI_RCMD_SET_TEXTURE_FILTER,
	HUI_RCMD_DESTROY_TEXTURE,
	HUI_RCMD_FRAME,
	HUI_RCMD_CALL,
	HUI_RCMD_STOP
} hui_rcmd_type_t;

/** Filled by the render thread for commands the UI thread waits for. */
typedef struct hui_rcmd_wait_t {
	int done;
	int result;
} hui_rcmd_wait_t;

typedef struct hui_rcmd_t {
	hui_rcmd_type_t type;
	HUI_id id;
	HUI_NativeWindow native;
	HUI_TextureDesc desc;
	HUI_BackendInitInfo init;
	HUI_EFilterType filter;
	uint32_t x, y, width, height;
	void* pixels;
	hui_frame_t* frame;
	hui_render_fn function;
	void* arg;
	hui_rcmd_wait_t* wait;
	struct hui_rcmd_t* next;
} hui_rcmd_t;

#define HUI_FRAME_POOL_MAX 6

static struct {
	HUI_BackendVTable backend;
	int threaded;
	int running;
	hui_thread_t thread;
	hui_mutex_t mutex;
	/** Signalled when a command is queued. */
	hui_cond_t queue_cv;
	/** Broadcast when a waited command is done. */
	hui_cond_t done_cv;
	/** Broadcast when a frame has been rendered. */
	hui_cond_t frame_cv;
	hui_rcmd_t* head;
	hui_rcmd_t* tail;
	hui_frame_t* free_frames;
	int free_frame_count;
	/** Called while the UI thread waits for the render thread, or NULL. */
	void (*wait_service)(void);
} g_renderer;

/* ---------------------------------------------------------------------------
 *  Execution (render thread, or the UI thread when not threaded)
 * ------------------------------------------------------------------------- */

static void hui_frame_release(hui_frame_t* frame) {
	hui_mutex_lock(&g_renderer.mutex);
	if (g_renderer.free_frame_count < HUI_FRAME_POOL_MAX) {
		frame->next = g_renderer.free_frames;
		g_renderer.free_frames = frame;
		g_renderer.free_frame_count++;
		frame = NULL;
	}
	hui_mutex_unlock(&g_renderer.mutex);
	if (frame) {
		hui_drawlist_free(&frame->list);
		hui_free(frame);
	}
}

static int hui_rcmd_execute(hui_rcmd_t* cmd) {
	const HUI_BackendVTable* backend = &g_renderer.backend;
	int result = HUI_TRUE;

	switch (cmd->type) {
		case HUI_RCMD_INIT:
			result = backend->Init(&cmd->init);
			break;
		case HUI_RCMD_SHUTDOWN:
			backend->WaitIdle();
			backend->Shutdown();
			break;
		case HUI_RCMD_CREATE_SURFACE:
			result = backend->CreateSurface(cmd->id, &cmd->native, cmd->width, cmd->height);
			break;
		case HUI_RCMD_DESTROY_SURFACE:
			backend->DestroySurface(cmd->id);
			break;
		case HUI_RCMD_CREATE_TEXTURE:
			result = backend->CreateTexture(cmd->id, &cmd->desc, cmd->pixels);
			hui_free(cmd->pixels);
			break;
		case HUI_RCMD_UPDATE_TEXTURE:
			backend->UpdateTexture(cmd->id, cmd->x, cmd->y, cmd->width, cmd->height, cmd->pixels);
			hui_free(cmd->pixels);
			break;
		case HUI_RCMD_SET_TEXTURE_FILTER:
			backend->SetTextureFilter(cmd->id, cmd->filter);
			break;
		case HUI_RCMD_DESTROY_TEXTURE:
			backend->DestroyTexture(cmd->id);
			break;
		case HUI_RCMD_FRAME: {
			hui_frame_t* frame = cmd->frame;
			double start = hui_clock_seconds();
			backend->RenderFrame(&frame->data, frame->capture);
			if (frame->render_us) {
				hui_atomic_store(frame->render_us, (int)((hui_clock_seconds() - start) * 1e6));
			}
			if (frame->frames_in_flight) {
				hui_atomic_add(frame->frames_in_flight, -1);
				hui_mutex_lock(&g_renderer.mutex);
				hui_cond_broadcast(&g_renderer.frame_cv);
				hui_mutex_unlock(&g_renderer.mutex);
			}
			hui_frame_release(frame);
			/* a continuously redrawing window is paced by this wake-up */
			hui_wake();
			break;
		}
		case HUI_RCMD_CALL:
			if (cmd->function) cmd->function(cmd->arg);
			break;
		case HUI_RCMD_STOP:
			break;
	}
	return result;
}

static void hui_render_thread(void* arg) {
	HUI_UNUSED(arg);
	for (;;) {
		hui_rcmd_t* cmd;
		int stop, result;

		hui_mutex_lock(&g_renderer.mutex);
		while (!g_renderer.head) hui_cond_wait(&g_renderer.queue_cv, &g_renderer.mutex);
		cmd = g_renderer.head;
		g_renderer.head = cmd->next;
		if (!g_renderer.head) g_renderer.tail = NULL;
		hui_mutex_unlock(&g_renderer.mutex);

		stop = cmd->type == HUI_RCMD_STOP;
		result = hui_rcmd_execute(cmd);
		if (cmd->wait) {
			hui_mutex_lock(&g_renderer.mutex);
			cmd->wait->result = result;
			cmd->wait->done = HUI_TRUE;
			hui_cond_broadcast(&g_renderer.done_cv);
			hui_mutex_unlock(&g_renderer.mutex);
		}
		hui_free(cmd);
		if (stop) break;
	}
}

/*
 * Waits on a condition of the renderer (mutex held). On some window systems
 * the UI thread must keep answering the window system while it is blocked on
 * the render thread (see hui_renderer_set_wait_service): the wait is then cut
 * into short slices with a call to the service in between. The caller loops
 * on its predicate, so returning early is harmless.
 */
static void hui_renderer_wait(hui_cond_t* cond) {
	if (!g_renderer.wait_service) {
		hui_cond_wait(cond, &g_renderer.mutex);
		return;
	}
	if (hui_cond_wait_timeout(cond, &g_renderer.mutex, 0.002)) return;
	hui_mutex_unlock(&g_renderer.mutex);
	g_renderer.wait_service();
	hui_mutex_lock(&g_renderer.mutex);
}

void hui_renderer_set_wait_service(void (*service)(void)) {
	g_renderer.wait_service = service;
}

/**
 * Queues a command. With `wait`, blocks until it was executed and returns
 * its result. Takes ownership of `cmd`.
 */
static int hui_rcmd_submit(hui_rcmd_t* cmd, int wait) {
	hui_rcmd_wait_t waiter;
	int result;

	if (!g_renderer.running) {
		/* renderer not started or already stopped: drop the command */
		if (cmd->type == HUI_RCMD_CREATE_TEXTURE || cmd->type == HUI_RCMD_UPDATE_TEXTURE) hui_free(cmd->pixels);
		if (cmd->type == HUI_RCMD_FRAME) {
			if (cmd->frame->frames_in_flight) hui_atomic_add(cmd->frame->frames_in_flight, -1);
			hui_drawlist_free(&cmd->frame->list);
			hui_free(cmd->frame);
		}
		hui_free(cmd);
		return HUI_FALSE;
	}

	if (!g_renderer.threaded) {
		result = hui_rcmd_execute(cmd);
		hui_free(cmd);
		return result;
	}

	waiter.done = HUI_FALSE;
	waiter.result = HUI_TRUE;
	cmd->wait = wait ? &waiter : NULL;
	cmd->next = NULL;

	hui_mutex_lock(&g_renderer.mutex);
	if (g_renderer.tail) g_renderer.tail->next = cmd;
	else g_renderer.head = cmd;
	g_renderer.tail = cmd;
	hui_cond_signal(&g_renderer.queue_cv);
	if (wait) {
		while (!waiter.done) hui_renderer_wait(&g_renderer.done_cv);
	}
	hui_mutex_unlock(&g_renderer.mutex);
	return waiter.result;
}

static hui_rcmd_t* hui_rcmd_new(hui_rcmd_type_t type, HUI_id id) {
	hui_rcmd_t* cmd = (hui_rcmd_t*)hui_calloc(sizeof(hui_rcmd_t));
	cmd->type = type;
	cmd->id = id;
	return cmd;
}

/* ---------------------------------------------------------------------------
 *  Lifecycle
 * ------------------------------------------------------------------------- */

int hui_renderer_init(const HUI_BackendVTable* backend, const HUI_BackendInitInfo* info, int threaded) {
	hui_rcmd_t* cmd;
	int ok;

	memset(&g_renderer, 0, sizeof(g_renderer));
	g_renderer.backend = *backend;
	g_renderer.threaded = threaded;
	hui_mutex_init(&g_renderer.mutex);
	hui_cond_init(&g_renderer.queue_cv);
	hui_cond_init(&g_renderer.done_cv);
	hui_cond_init(&g_renderer.frame_cv);

	if (threaded && !hui_thread_create(&g_renderer.thread, hui_render_thread, NULL, "hui-render")) {
		hui_error(HUI_ERROR_PLATFORM, HUI_SEVERITY_WARNING, "cannot start the render thread, rendering on the UI thread");
		g_renderer.threaded = HUI_FALSE;
	}
	g_renderer.running = HUI_TRUE;

	/* the backend is initialized on the thread that will use it */
	cmd = hui_rcmd_new(HUI_RCMD_INIT, HUI_INVALID_ID);
	cmd->init = *info;
	ok = hui_rcmd_submit(cmd, HUI_TRUE);
	if (!ok) {
		if (g_renderer.threaded) {
			hui_rcmd_submit(hui_rcmd_new(HUI_RCMD_STOP, HUI_INVALID_ID), HUI_FALSE);
			hui_thread_join(&g_renderer.thread);
		}
		g_renderer.running = HUI_FALSE;
		hui_cond_destroy(&g_renderer.frame_cv);
		hui_cond_destroy(&g_renderer.done_cv);
		hui_cond_destroy(&g_renderer.queue_cv);
		hui_mutex_destroy(&g_renderer.mutex);
	}
	return ok;
}

void hui_renderer_shutdown(void) {
	hui_frame_t* frame;
	if (!g_renderer.running) return;
	hui_rcmd_submit(hui_rcmd_new(HUI_RCMD_SHUTDOWN, HUI_INVALID_ID), HUI_TRUE);
	if (g_renderer.threaded) {
		hui_rcmd_submit(hui_rcmd_new(HUI_RCMD_STOP, HUI_INVALID_ID), HUI_FALSE);
		hui_thread_join(&g_renderer.thread);
	}
	g_renderer.running = HUI_FALSE;
	frame = g_renderer.free_frames;
	while (frame) {
		hui_frame_t* next = frame->next;
		hui_drawlist_free(&frame->list);
		hui_free(frame);
		frame = next;
	}
	hui_cond_destroy(&g_renderer.frame_cv);
	hui_cond_destroy(&g_renderer.done_cv);
	hui_cond_destroy(&g_renderer.queue_cv);
	hui_mutex_destroy(&g_renderer.mutex);
	memset(&g_renderer, 0, sizeof(g_renderer));
}

int hui_renderer_is_threaded(void) {
	return g_renderer.threaded;
}

/* ---------------------------------------------------------------------------
 *  Commands
 * ------------------------------------------------------------------------- */

int hui_renderer_create_surface(HUI_id window, const HUI_NativeWindow* native, uint32_t width, uint32_t height) {
	hui_rcmd_t* cmd = hui_rcmd_new(HUI_RCMD_CREATE_SURFACE, window);
	cmd->native = *native;
	cmd->width = width;
	cmd->height = height;
	return hui_rcmd_submit(cmd, HUI_TRUE);
}

void hui_renderer_destroy_surface(HUI_id window) {
	hui_rcmd_submit(hui_rcmd_new(HUI_RCMD_DESTROY_SURFACE, window), HUI_TRUE);
}

void hui_renderer_create_texture(HUI_id texture, const HUI_TextureDesc* desc, void* pixels) {
	hui_rcmd_t* cmd = hui_rcmd_new(HUI_RCMD_CREATE_TEXTURE, texture);
	cmd->desc = *desc;
	cmd->pixels = pixels;
	hui_rcmd_submit(cmd, HUI_FALSE);
}

void hui_renderer_update_texture(HUI_id texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* pixels) {
	hui_rcmd_t* cmd = hui_rcmd_new(HUI_RCMD_UPDATE_TEXTURE, texture);
	cmd->x = x;
	cmd->y = y;
	cmd->width = width;
	cmd->height = height;
	cmd->pixels = pixels;
	hui_rcmd_submit(cmd, HUI_FALSE);
}

void hui_renderer_set_texture_filter(HUI_id texture, HUI_EFilterType filter) {
	hui_rcmd_t* cmd = hui_rcmd_new(HUI_RCMD_SET_TEXTURE_FILTER, texture);
	cmd->filter = filter;
	hui_rcmd_submit(cmd, HUI_FALSE);
}

void hui_renderer_destroy_texture(HUI_id texture) {
	hui_rcmd_submit(hui_rcmd_new(HUI_RCMD_DESTROY_TEXTURE, texture), HUI_FALSE);
}

void hui_renderer_call(hui_render_fn function, void* arg, int wait) {
	hui_rcmd_t* cmd = hui_rcmd_new(HUI_RCMD_CALL, HUI_INVALID_ID);
	cmd->function = function;
	cmd->arg = arg;
	hui_rcmd_submit(cmd, wait);
}

void hui_renderer_sync(void) {
	if (g_renderer.running && g_renderer.threaded) hui_renderer_call(NULL, NULL, HUI_TRUE);
}

/* ---------------------------------------------------------------------------
 *  Frames
 * ------------------------------------------------------------------------- */

hui_frame_t* hui_renderer_acquire_frame(void) {
	hui_frame_t* frame = NULL;
	if (g_renderer.running) {
		hui_mutex_lock(&g_renderer.mutex);
		frame = g_renderer.free_frames;
		if (frame) {
			g_renderer.free_frames = frame->next;
			g_renderer.free_frame_count--;
		}
		hui_mutex_unlock(&g_renderer.mutex);
	}
	if (!frame) {
		frame = (hui_frame_t*)hui_calloc(sizeof(hui_frame_t));
		hui_drawlist_init(&frame->list);
	}
	hui_drawlist_reset(&frame->list);
	memset(&frame->data, 0, sizeof(frame->data));
	frame->capture = NULL;
	frame->frames_in_flight = NULL;
	frame->render_us = NULL;
	frame->next = NULL;
	return frame;
}

void hui_renderer_throttle(hui_atomic_t* frames_in_flight, int max_frames) {
	if (!g_renderer.running || !g_renderer.threaded) return;
	hui_mutex_lock(&g_renderer.mutex);
	while (hui_atomic_load(frames_in_flight) >= max_frames) hui_renderer_wait(&g_renderer.frame_cv);
	hui_mutex_unlock(&g_renderer.mutex);
}

void hui_renderer_submit_frame(hui_frame_t* frame) {
	hui_rcmd_t* cmd = hui_rcmd_new(HUI_RCMD_FRAME, frame->data.window);
	frame->data.instances = frame->list.instances;
	frame->data.instance_count = frame->list.instance_count;
	frame->data.commands = frame->list.commands;
	frame->data.command_count = frame->list.command_count;
	if (frame->frames_in_flight) hui_atomic_add(frame->frames_in_flight, 1);
	cmd->frame = frame;
	hui_rcmd_submit(cmd, HUI_FALSE);
}
