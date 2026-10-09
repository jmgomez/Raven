/**
 * Raven - platform/platform.h
 * Windowing interface implemented once per window system.
 *
 *   platform/x11/     Xlib               (Linux, BSD)
 *   platform/win32/   Win32              (Windows)
 *   platform/null/    no window at all   (headless runs, tests)
 *
 * To port Raven to a new window system (Wayland, Cocoa, ...), implement this
 * table and return it from rvn_platform_select.
 *
 * Unless noted, sizes and positions are in pixels of the client area and all
 * functions are called from the UI thread.
 */

#ifndef RVN_PLATFORM_H
#define RVN_PLATFORM_H

#include "../core/base.h"

/** A native window. Each platform defines its own structure. */
typedef struct rvn_pwindow_t rvn_pwindow_t;

typedef enum rvn_pevent_type_t {
	/** The user asked to close the window. */
	RVN_PEVENT_CLOSE,
	/** The client area changed: width, height, scale. */
	RVN_PEVENT_RESIZE,
	/** The content of the window must be redrawn. */
	RVN_PEVENT_EXPOSE,
	RVN_PEVENT_MOUSE_MOVE,
	RVN_PEVENT_MOUSE_BUTTON,
	RVN_PEVENT_MOUSE_WHEEL,
	RVN_PEVENT_MOUSE_LEAVE,
	RVN_PEVENT_KEY,
	/** A character was typed: codepoint. */
	RVN_PEVENT_TEXT,
	RVN_PEVENT_FOCUS,
	/**
	 * The platform is inside a modal loop (Win32 live resize): the core
	 * should produce a frame now because Rupdate is not being called.
	 */
	RVN_PEVENT_MODAL_TICK
} rvn_pevent_type_t;

typedef struct rvn_pevent_t {
	rvn_pevent_type_t type;
	/** User pointer given when the window was created. */
	void* user;
	int x, y;
	int width, height;
	float scale;
	float wheel_x, wheel_y;
	int button;
	int pressed;
	RKey key;
	uint32_t modifiers;
	int repeat;
	uint32_t codepoint;
	int focused;
} rvn_pevent_t;

typedef void (*rvn_pevent_handler_t)(const rvn_pevent_t* event);

typedef struct rvn_pwindow_desc_t {
	const char* title;
	/** Client size in logical units: the platform multiplies by its scale. */
	int width, height;
	/** Bitmask of RWindowFlags. */
	uint32_t flags;
	void* user;
} rvn_pwindow_desc_t;

typedef enum rvn_pwindow_state_t {
	RVN_PWINDOW_NORMAL,
	RVN_PWINDOW_MINIMIZED,
	RVN_PWINDOW_MAXIMIZED
} rvn_pwindow_state_t;

typedef struct rvn_platform_t {
	const char* name;
	RWindowSystem system;

	int   (*init)(rvn_pevent_handler_t handler);
	void  (*shutdown)(void);
	/** Display connection handed to the rendering backend (X11 Display*), or NULL. */
	void* (*native_display)(void);

	rvn_pwindow_t* (*window_create)(const rvn_pwindow_desc_t* desc);
	void  (*window_destroy)(rvn_pwindow_t* window);
	void  (*window_set_title)(rvn_pwindow_t* window, const char* title);
	void  (*window_set_size)(rvn_pwindow_t* window, int width, int height);
	void  (*window_get_size)(rvn_pwindow_t* window, int* width, int* height);
	void  (*window_set_min_size)(rvn_pwindow_t* window, int width, int height);
	void  (*window_set_position)(rvn_pwindow_t* window, int x, int y);
	void  (*window_get_position)(rvn_pwindow_t* window, int* x, int* y);
	/** Pixels per logical unit on the monitor showing the window. */
	float (*window_get_scale)(rvn_pwindow_t* window);
	void  (*window_show)(rvn_pwindow_t* window, int visible);
	void  (*window_set_state)(rvn_pwindow_t* window, rvn_pwindow_state_t state);
	void  (*window_set_cursor)(rvn_pwindow_t* window, RCursor cursor);
	void  (*window_get_native)(rvn_pwindow_t* window, RNativeWindow* native);

	/** Child window used by native viewports. Destroyed with window_destroy. */
	rvn_pwindow_t* (*child_create)(rvn_pwindow_t* parent, int x, int y, int width, int height, void* user);
	void  (*child_set_rect)(rvn_pwindow_t* child, int x, int y, int width, int height, int visible);

	/**
	 * Dispatches pending events to the handler.
	 * @param timeout_seconds 0 returns at once, negative waits without limit.
	 */
	void  (*pump)(double timeout_seconds);
	/** Interrupts a waiting pump. Thread-safe. */
	void  (*wake)(void);

	void  (*clipboard_set)(const char* text);
	/** Returns a rvn_alloc'ed string, or NULL. */
	char* (*clipboard_get)(void);

	/**
	 * Optional (may be NULL). Called repeatedly while the UI thread is blocked
	 * waiting for the render thread. A window system whose windows must keep
	 * answering requests coming from other threads (Win32: messages sent by
	 * the graphics driver while it creates or resizes a swapchain) answers
	 * them here, without dispatching anything to the core.
	 */
	void  (*wait_service)(void);
} rvn_platform_t;

/** Picks the platform compiled in for this OS, or the null platform. */
const rvn_platform_t* rvn_platform_select(int headless);

const rvn_platform_t* rvn_platform_null(void);
#if defined(RAVEN_PLATFORM_X11)
const rvn_platform_t* rvn_platform_x11(void);
#endif
#if defined(RAVEN_PLATFORM_WIN32)
const rvn_platform_t* rvn_platform_win32(void);
#endif

#endif /* RVN_PLATFORM_H */
