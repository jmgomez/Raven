/**
 * HUI - core/context.h
 * Global state of the library. Like HRL, HUI has one implicit context.
 */

#ifndef HUI_CONTEXT_H
#define HUI_CONTEXT_H

#include "base.h"
#include "thread.h"
#include "handle.h"
#include "jobs.h"
#include "../platform/platform.h"

struct hui_window_t;
struct hui_widget_t;

typedef struct hui_theme_t {
	hui_color_t colors[HUI_COLOR_COUNT];
	float metrics[HUI_METRIC_COUNT];
	HUI_id font;
} hui_theme_t;

typedef struct hui_task_t {
	HUI_CTask function;
	void* user_data;
} hui_task_t;

typedef struct hui_timer_t {
	HUI_id id;
	double interval;
	double deadline;
	int repeat;
	HUI_CTimer callback;
	void* user_data;
} hui_timer_t;

typedef struct hui_action_t {
	char* name;
	HUI_CAction callback;
	void* user_data;
} hui_action_t;

typedef struct hui_context_t {
	int initialized;
	uint64_t ui_thread;
	double start_time;
	float ui_scale;
	int vsync;
	HUI_EBackend backend;
	const hui_platform_t* platform;

	hui_pool_t windows;
	hui_pool_t widgets;
	hui_pool_t fonts;
	hui_pool_t textures;
	hui_pool_t timers;
	hui_pool_t tree_items;

	/* main loop */
	hui_atomic_t quit_requested;
	hui_atomic_t exit_code;
	/** Set by HUI_RequestWindowRedraw when called from another thread. */
	hui_atomic_t redraw_all;
	int had_window;
	int in_update;
	double last_update_time;
	HUI_CFrame frame_callback;
	void* frame_user;

	/* tasks posted from other threads */
	hui_mutex_t task_mutex;
	hui_task_t* tasks;
	uint32_t task_count;
	uint32_t task_cap;

	/* actions */
	hui_action_t* actions;
	uint32_t action_count;
	uint32_t action_cap;
	HUI_CAction default_action;
	void* default_action_user;

	/*
	 * Deferred destruction. While events are being dispatched, deleted
	 * widgets and windows lose their handle at once but their memory is
	 * released only when the dispatch is over, so the code that called the
	 * application back can finish safely.
	 */
	int dispatch_depth;
	struct hui_widget_t** dead_widgets;
	uint32_t dead_widget_count;
	uint32_t dead_widget_cap;
	struct hui_window_t** dead_windows;
	uint32_t dead_window_count;
	uint32_t dead_window_cap;

	hui_theme_t theme;
	HUI_id default_font;
	char* clipboard;
	char* xml_base_path;
} hui_context_t;

extern hui_context_t g_hui;

#define HUI_THEME_COLOR(role) (g_hui.theme.colors[(int)(role) - (int)HUI_COLOR_WINDOW])
#define HUI_THEME_METRIC(metric) (g_hui.theme.metrics[(int)(metric) - (int)HUI_METRIC_FONT_SIZE])

/**
 * Checks that the library is initialized and that the caller is the UI
 * thread; reports an error naming `function` otherwise.
 */
int hui_api_check(const char* function);

#define HUI_API_GUARD(result) do { if (!hui_api_check(__func__)) return result; } while (0)
#define HUI_API_GUARD_VOID() do { if (!hui_api_check(__func__)) return; } while (0)

/** Calls the function bound to an action name (or the default handler). */
void hui_fire_action(HUI_id sender, const char* action);

/** Seconds until the next timer fires, or a negative value when there is none. */
double hui_timers_next_delay(void);

/** Brackets code that calls application callbacks (see dispatch_depth). */
void hui_dispatch_begin(void);
void hui_dispatch_end(void);

/** Wakes the event loop so that HUI_Update runs soon. Thread-safe. */
void hui_wake(void);

/**
 * Marks every widget for a new layout and every window for a new paint
 * (theme, font or texture size change). Implemented in widget/window.c.
 */
void hui_invalidate_all_windows(void);
/** Marks every window for a new paint, keeping the layout. Implemented in widget/window.c. */
void hui_repaint_all_windows(void);

#endif /* HUI_CONTEXT_H */
