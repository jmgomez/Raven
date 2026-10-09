/**
 * HUI - platform/platform.h
 * Windowing interface implemented once per window system.
 *
 *   platform/x11/     Xlib               (Linux, BSD)
 *   platform/win32/   Win32              (Windows)
 *   platform/null/    no window at all   (headless runs, tests)
 *
 * To port HUI to a new window system (Wayland, Cocoa, ...), implement this
 * table and return it from hui_platform_select.
 *
 * Unless noted, sizes and positions are in pixels of the client area and all
 * functions are called from the UI thread.
 */

#ifndef HUI_PLATFORM_H
#define HUI_PLATFORM_H

#include "../core/base.h"

/** A native window. Each platform defines its own structure. */
typedef struct hui_pwindow_t hui_pwindow_t;

typedef enum hui_pevent_type_t {
	/** The user asked to close the window. */
	HUI_PEVENT_CLOSE,
	/** The client area changed: width, height, scale. */
	HUI_PEVENT_RESIZE,
	/** The content of the window must be redrawn. */
	HUI_PEVENT_EXPOSE,
	HUI_PEVENT_MOUSE_MOVE,
	HUI_PEVENT_MOUSE_BUTTON,
	HUI_PEVENT_MOUSE_WHEEL,
	HUI_PEVENT_MOUSE_LEAVE,
	HUI_PEVENT_KEY,
	/** A character was typed: codepoint. */
	HUI_PEVENT_TEXT,
	HUI_PEVENT_FOCUS,
	/**
	 * The platform is inside a modal loop (Win32 live resize): the core
	 * should produce a frame now because HUI_Update is not being called.
	 */
	HUI_PEVENT_MODAL_TICK
} hui_pevent_type_t;

typedef struct hui_pevent_t {
	hui_pevent_type_t type;
	/** User pointer given when the window was created. */
	void* user;
	int x, y;
	int width, height;
	float scale;
	float wheel_x, wheel_y;
	int button;
	int pressed;
	HUI_EKey key;
	uint32_t modifiers;
	int repeat;
	uint32_t codepoint;
	int focused;
} hui_pevent_t;

typedef void (*hui_pevent_handler_t)(const hui_pevent_t* event);

typedef struct hui_pwindow_desc_t {
	const char* title;
	/** Client size in logical units: the platform multiplies by its scale. */
	int width, height;
	/** Bitmask of HUI_EWindowFlags. */
	uint32_t flags;
	void* user;
} hui_pwindow_desc_t;

typedef enum hui_pwindow_state_t {
	HUI_PWINDOW_NORMAL,
	HUI_PWINDOW_MINIMIZED,
	HUI_PWINDOW_MAXIMIZED
} hui_pwindow_state_t;

typedef struct hui_platform_t {
	const char* name;
	HUI_EWindowSystem system;

	int   (*init)(hui_pevent_handler_t handler);
	void  (*shutdown)(void);
	/** Display connection handed to the rendering backend (X11 Display*), or NULL. */
	void* (*native_display)(void);

	hui_pwindow_t* (*window_create)(const hui_pwindow_desc_t* desc);
	void  (*window_destroy)(hui_pwindow_t* window);
	void  (*window_set_title)(hui_pwindow_t* window, const char* title);
	void  (*window_set_size)(hui_pwindow_t* window, int width, int height);
	void  (*window_get_size)(hui_pwindow_t* window, int* width, int* height);
	void  (*window_set_min_size)(hui_pwindow_t* window, int width, int height);
	void  (*window_set_position)(hui_pwindow_t* window, int x, int y);
	void  (*window_get_position)(hui_pwindow_t* window, int* x, int* y);
	/** Pixels per logical unit on the monitor showing the window. */
	float (*window_get_scale)(hui_pwindow_t* window);
	void  (*window_show)(hui_pwindow_t* window, int visible);
	void  (*window_set_state)(hui_pwindow_t* window, hui_pwindow_state_t state);
	void  (*window_set_cursor)(hui_pwindow_t* window, HUI_ECursor cursor);
	void  (*window_get_native)(hui_pwindow_t* window, HUI_NativeWindow* native);

	/** Child window used by native viewports. Destroyed with window_destroy. */
	hui_pwindow_t* (*child_create)(hui_pwindow_t* parent, int x, int y, int width, int height, void* user);
	void  (*child_set_rect)(hui_pwindow_t* child, int x, int y, int width, int height, int visible);

	/**
	 * Dispatches pending events to the handler.
	 * @param timeout_seconds 0 returns at once, negative waits without limit.
	 */
	void  (*pump)(double timeout_seconds);
	/** Interrupts a waiting pump. Thread-safe. */
	void  (*wake)(void);

	void  (*clipboard_set)(const char* text);
	/** Returns a hui_alloc'ed string, or NULL. */
	char* (*clipboard_get)(void);

	/**
	 * Optional (may be NULL). Called repeatedly while the UI thread is blocked
	 * waiting for the render thread. A window system whose windows must keep
	 * answering requests coming from other threads (Win32: messages sent by
	 * the graphics driver while it creates or resizes a swapchain) answers
	 * them here, without dispatching anything to the core.
	 */
	void  (*wait_service)(void);
} hui_platform_t;

/** Picks the platform compiled in for this OS, or the null platform. */
const hui_platform_t* hui_platform_select(int headless);

const hui_platform_t* hui_platform_null(void);
#if defined(HUI_PLATFORM_X11)
const hui_platform_t* hui_platform_x11(void);
#endif
#if defined(HUI_PLATFORM_WIN32)
const hui_platform_t* hui_platform_win32(void);
#endif

#endif /* HUI_PLATFORM_H */
