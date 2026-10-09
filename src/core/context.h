/**
 * Raven - core/context.h
 * Global state of the library. Like HRL, Raven has one implicit context.
 */

#ifndef RVN_CONTEXT_H
#define RVN_CONTEXT_H

#include "base.h"
#include "thread.h"
#include "handle.h"
#include "jobs.h"
#include "../platform/platform.h"

struct rvn_window_t;
struct rvn_widget_t;

typedef struct rvn_theme_t {
	rvn_color_t colors[R_COLOR_COUNT];
	float metrics[R_METRIC_COUNT];
	Rid font;
	/** Glass theme: controls are drawn as glass and animate their state. */
	int glass;
	/** Dark theme: glass is tinted accordingly. */
	int dark;
	/** Background drawn behind the content of windows, or R_INVALID_ID (glass themes). */
	Rid wallpaper;
	/** The built-in wallpapers, created on first use: dark, light. */
	Rid wallpapers[2];
} rvn_theme_t;

typedef struct rvn_task_t {
	RTaskCallback function;
	void* user_data;
} rvn_task_t;

typedef struct rvn_timer_t {
	Rid id;
	double interval;
	double deadline;
	int repeat;
	RTimerCallback callback;
	void* user_data;
} rvn_timer_t;

typedef struct rvn_action_t {
	char* name;
	RActionCallback callback;
	void* user_data;
} rvn_action_t;

typedef struct rvn_context_t {
	int initialized;
	uint64_t ui_thread;
	double start_time;
	float ui_scale;
	int vsync;
	RBackend backend;
	const rvn_platform_t* platform;

	rvn_pool_t windows;
	rvn_pool_t widgets;
	rvn_pool_t fonts;
	rvn_pool_t textures;
	rvn_pool_t timers;
	rvn_pool_t tree_items;

	/* main loop */
	rvn_atomic_t quit_requested;
	rvn_atomic_t exit_code;
	/** Set by Rwindow_request_redraw when called from another thread. */
	rvn_atomic_t redraw_all;
	int had_window;
	int in_update;
	double last_update_time;
	RFrameCallback frame_callback;
	void* frame_user;

	/* tasks posted from other threads */
	rvn_mutex_t task_mutex;
	rvn_task_t* tasks;
	uint32_t task_count;
	uint32_t task_cap;

	/* actions */
	rvn_action_t* actions;
	uint32_t action_count;
	uint32_t action_cap;
	RActionCallback default_action;
	void* default_action_user;

	/*
	 * Deferred destruction. While events are being dispatched, deleted
	 * widgets and windows lose their handle at once but their memory is
	 * released only when the dispatch is over, so the code that called the
	 * application back can finish safely.
	 */
	int dispatch_depth;
	struct rvn_widget_t** dead_widgets;
	uint32_t dead_widget_count;
	uint32_t dead_widget_cap;
	struct rvn_window_t** dead_windows;
	uint32_t dead_window_count;
	uint32_t dead_window_cap;

	rvn_theme_t theme;
	Rid default_font;
	char* clipboard;
	char* xml_base_path;
} rvn_context_t;

extern rvn_context_t g_rvn;

#define RVN_THEME_COLOR(role) (g_rvn.theme.colors[(int)(role) - (int)R_COLOR_WINDOW])
#define RVN_THEME_METRIC(metric) (g_rvn.theme.metrics[(int)(metric) - (int)R_METRIC_FONT_SIZE])

/**
 * Checks that the library is initialized and that the caller is the UI
 * thread; reports an error naming `function` otherwise.
 */
int rvn_api_check(const char* function);

#define RVN_API_GUARD(result) do { if (!rvn_api_check(__func__)) return result; } while (0)
#define RVN_API_GUARD_VOID() do { if (!rvn_api_check(__func__)) return; } while (0)

/** Calls the function bound to an action name (or the default handler). */
void rvn_fire_action(Rid sender, const char* action);

/** Seconds until the next timer fires, or a negative value when there is none. */
double rvn_timers_next_delay(void);

/** Brackets code that calls application callbacks (see dispatch_depth). */
void rvn_dispatch_begin(void);
void rvn_dispatch_end(void);

/** Wakes the event loop so that Rupdate runs soon. Thread-safe. */
void rvn_wake(void);

/**
 * Marks every widget for a new layout and every window for a new paint
 * (theme, font or texture size change). Implemented in widget/window.c.
 */
void rvn_invalidate_all_windows(void);
/** Marks every window for a new paint, keeping the layout. Implemented in widget/window.c. */
void rvn_repaint_all_windows(void);

#endif /* RVN_CONTEXT_H */
