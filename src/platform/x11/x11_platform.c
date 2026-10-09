/**
 * Raven - platform/x11/x11_platform.c
 * Xlib implementation of the platform interface (Linux, BSD; also works
 * through XWayland). Only libX11 is needed.
 *
 *   - UTF-8 text input through an X input context,
 *   - CLIPBOARD selection (UTF-8),
 *   - scale factor from the Xft.dpi resource,
 *   - child windows for native viewports,
 *   - a self-pipe so that other threads can wake the event loop.
 */

#include "x11_internal.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/XKBlib.h>
#include <X11/cursorfont.h>

#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <poll.h>
#include <unistd.h>

#define RVN_X11_CURSOR_COUNT 10
#define RVN_X11_CLIPBOARD_TIMEOUT_MS 500

struct rvn_pwindow_t {
	Window handle;
	void* user;
	XIC input_context;
	int width, height;
	int is_child;
	uint32_t flags;
	int min_width, min_height;
};

static struct {
	Display* display;
	int screen;
	Window root;
	XContext context;
	XIM input_method;
	rvn_pevent_handler_t handler;
	float scale;
	int wake_pipe[2];
	Cursor cursors[RVN_X11_CURSOR_COUNT];
	/* never mapped: owns the clipboard selection and receives pasted data */
	Window helper;
	char* clipboard;
	uint8_t keys_down[(R_KEY_COUNT + 7) / 8];

	Atom wm_protocols;
	Atom wm_delete_window;
	Atom net_wm_name;
	Atom net_wm_state;
	Atom net_wm_state_maximized_vert;
	Atom net_wm_state_maximized_horz;
	Atom motif_wm_hints;
	Atom utf8_string;
	Atom clipboard_atom;
	Atom targets;
	Atom incr;
	Atom paste_property;
} g_x11;

/* ---------------------------------------------------------------------------
 *  Helpers
 * ------------------------------------------------------------------------- */

static rvn_pwindow_t* rvn_x11_find(Window handle) {
	XPointer data = NULL;
	if (XFindContext(g_x11.display, handle, g_x11.context, &data) != 0) return NULL;
	return (rvn_pwindow_t*)data;
}

static void rvn_x11_emit(rvn_pwindow_t* window, rvn_pevent_t* event) {
	event->user = window->user;
	g_x11.handler(event);
}

/* The desktop scale is published by the settings daemon as the Xft.dpi resource. */
static float rvn_x11_read_scale(void) {
	char* resources = XResourceManagerString(g_x11.display);
	float scale = 1.0f;
	if (resources) {
		XrmDatabase database = XrmGetStringDatabase(resources);
		if (database) {
			XrmValue value;
			char* type = NULL;
			if (XrmGetResource(database, "Xft.dpi", "Xft.Dpi", &type, &value) && type && strcmp(type, "String") == 0 && value.addr) {
				double dpi = atof(value.addr);
				if (dpi >= 48.0 && dpi <= 960.0) scale = (float)(dpi / 96.0);
			}
			XrmDestroyDatabase(database);
		}
	}
	return scale;
}

static void rvn_x11_set_size_hints(rvn_pwindow_t* window, int width, int height) {
	XSizeHints* hints = XAllocSizeHints();
	if (!hints) return;
	if (window->flags & R_WINDOW_NO_RESIZE) {
		hints->flags |= PMinSize | PMaxSize;
		hints->min_width = hints->max_width = width;
		hints->min_height = hints->max_height = height;
	} else if (window->min_width > 0 || window->min_height > 0) {
		hints->flags |= PMinSize;
		hints->min_width = window->min_width;
		hints->min_height = window->min_height;
	}
	XSetWMNormalHints(g_x11.display, window->handle, hints);
	XFree(hints);
}

/* Asks the window manager to add or remove the maximized state. */
static void rvn_x11_send_maximize(rvn_pwindow_t* window, int maximize) {
	XEvent event;
	memset(&event, 0, sizeof(event));
	event.xclient.type = ClientMessage;
	event.xclient.window = window->handle;
	event.xclient.message_type = g_x11.net_wm_state;
	event.xclient.format = 32;
	event.xclient.data.l[0] = maximize ? 1 : 0; /* _NET_WM_STATE_ADD / _REMOVE */
	event.xclient.data.l[1] = (long)g_x11.net_wm_state_maximized_vert;
	event.xclient.data.l[2] = (long)g_x11.net_wm_state_maximized_horz;
	event.xclient.data.l[3] = 1;
	XSendEvent(g_x11.display, g_x11.root, False, SubstructureNotifyMask | SubstructureRedirectMask, &event);
}

/* ---------------------------------------------------------------------------
 *  Lifecycle
 * ------------------------------------------------------------------------- */

static int rvn_x11_init(rvn_pevent_handler_t handler) {
	static const unsigned int shapes[RVN_X11_CURSOR_COUNT] = {
		XC_left_ptr, XC_xterm, XC_hand2, XC_crosshair, XC_sb_h_double_arrow, XC_sb_v_double_arrow,
		XC_bottom_right_corner, XC_bottom_left_corner, XC_fleur, XC_X_cursor
	};
	XSetWindowAttributes attributes;
	Bool supported = False;
	int i;

	memset(&g_x11, 0, sizeof(g_x11));
	g_x11.handler = handler;
	g_x11.wake_pipe[0] = g_x11.wake_pipe[1] = -1;

	/* the render thread talks to the X server too (Vulkan presentation) */
	XInitThreads();
	/* text input needs the character encoding of the user; do not touch the other categories */
	if (strcmp(setlocale(LC_CTYPE, NULL), "C") == 0) setlocale(LC_CTYPE, "");
	XrmInitialize();

	g_x11.display = XOpenDisplay(NULL);
	if (!g_x11.display) {
		rvn_error(R_ERROR_PLATFORM, R_SEVERITY_ERROR, "X11: cannot open the display (is DISPLAY set?)");
		return R_FALSE;
	}
	g_x11.screen = DefaultScreen(g_x11.display);
	g_x11.root = RootWindow(g_x11.display, g_x11.screen);
	g_x11.context = XUniqueContext();
	g_x11.scale = rvn_x11_read_scale();

	g_x11.wm_protocols = XInternAtom(g_x11.display, "WM_PROTOCOLS", False);
	g_x11.wm_delete_window = XInternAtom(g_x11.display, "WM_DELETE_WINDOW", False);
	g_x11.net_wm_name = XInternAtom(g_x11.display, "_NET_WM_NAME", False);
	g_x11.net_wm_state = XInternAtom(g_x11.display, "_NET_WM_STATE", False);
	g_x11.net_wm_state_maximized_vert = XInternAtom(g_x11.display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
	g_x11.net_wm_state_maximized_horz = XInternAtom(g_x11.display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	g_x11.motif_wm_hints = XInternAtom(g_x11.display, "_MOTIF_WM_HINTS", False);
	g_x11.utf8_string = XInternAtom(g_x11.display, "UTF8_STRING", False);
	g_x11.clipboard_atom = XInternAtom(g_x11.display, "CLIPBOARD", False);
	g_x11.targets = XInternAtom(g_x11.display, "TARGETS", False);
	g_x11.incr = XInternAtom(g_x11.display, "INCR", False);
	g_x11.paste_property = XInternAtom(g_x11.display, "RVN_PASTE", False);

	/* a held key sends presses only: releases then always mean a real release */
	XkbSetDetectableAutoRepeat(g_x11.display, True, &supported);

	if (XSupportsLocale()) {
		XSetLocaleModifiers("");
		g_x11.input_method = XOpenIM(g_x11.display, NULL, NULL, NULL);
	}

	for (i = 0; i < RVN_X11_CURSOR_COUNT; i++) g_x11.cursors[i] = XCreateFontCursor(g_x11.display, shapes[i]);

	memset(&attributes, 0, sizeof(attributes));
	attributes.event_mask = PropertyChangeMask;
	g_x11.helper = XCreateWindow(g_x11.display, g_x11.root, -10, -10, 1, 1, 0, 0, InputOnly, CopyFromParent,
		CWEventMask, &attributes);

	if (pipe(g_x11.wake_pipe) == 0) {
		for (i = 0; i < 2; i++) {
			int flags = fcntl(g_x11.wake_pipe[i], F_GETFL, 0);
			fcntl(g_x11.wake_pipe[i], F_SETFL, flags | O_NONBLOCK);
			fcntl(g_x11.wake_pipe[i], F_SETFD, FD_CLOEXEC);
		}
	} else {
		g_x11.wake_pipe[0] = g_x11.wake_pipe[1] = -1;
	}
	return R_TRUE;
}

static void rvn_x11_shutdown(void) {
	int i;
	if (!g_x11.display) return;
	for (i = 0; i < RVN_X11_CURSOR_COUNT; i++) {
		if (g_x11.cursors[i]) XFreeCursor(g_x11.display, g_x11.cursors[i]);
	}
	if (g_x11.helper) XDestroyWindow(g_x11.display, g_x11.helper);
	if (g_x11.input_method) XCloseIM(g_x11.input_method);
	XCloseDisplay(g_x11.display);
	if (g_x11.wake_pipe[0] >= 0) close(g_x11.wake_pipe[0]);
	if (g_x11.wake_pipe[1] >= 0) close(g_x11.wake_pipe[1]);
	rvn_free(g_x11.clipboard);
	memset(&g_x11, 0, sizeof(g_x11));
}

static void* rvn_x11_native_display(void) {
	return g_x11.display;
}

/* ---------------------------------------------------------------------------
 *  Windows
 * ------------------------------------------------------------------------- */

static void rvn_x11_window_set_title(rvn_pwindow_t* window, const char* title) {
	XStoreName(g_x11.display, window->handle, title);
	XChangeProperty(g_x11.display, window->handle, g_x11.net_wm_name, g_x11.utf8_string, 8, PropModeReplace,
		(const unsigned char*)title, (int)strlen(title));
}

static rvn_pwindow_t* rvn_x11_window_create(const rvn_pwindow_desc_t* desc) {
	rvn_pwindow_t* window = (rvn_pwindow_t*)rvn_calloc(sizeof(rvn_pwindow_t));
	XSetWindowAttributes attributes;
	XClassHint* class_hint;
	Atom protocols[1];

	window->user = desc->user;
	window->flags = desc->flags;
	window->width = rvn_maxi(1, (int)((float)desc->width * g_x11.scale + 0.5f));
	window->height = rvn_maxi(1, (int)((float)desc->height * g_x11.scale + 0.5f));

	memset(&attributes, 0, sizeof(attributes));
	/* no background: the server never paints over our frame, which avoids flicker when resizing */
	attributes.background_pixmap = None;
	attributes.bit_gravity = NorthWestGravity;
	attributes.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
		ButtonReleaseMask | PointerMotionMask | EnterWindowMask | LeaveWindowMask | FocusChangeMask;
	window->handle = XCreateWindow(g_x11.display, g_x11.root, 0, 0, (unsigned int)window->width, (unsigned int)window->height,
		0, CopyFromParent, InputOutput, CopyFromParent, CWBackPixmap | CWBitGravity | CWEventMask, &attributes);
	if (!window->handle) {
		rvn_free(window);
		return NULL;
	}
	XSaveContext(g_x11.display, window->handle, g_x11.context, (XPointer)window);

	protocols[0] = g_x11.wm_delete_window;
	XSetWMProtocols(g_x11.display, window->handle, protocols, 1);
	rvn_x11_window_set_title(window, desc->title ? desc->title : "");
	rvn_x11_set_size_hints(window, window->width, window->height);

	class_hint = XAllocClassHint();
	if (class_hint) {
		char name[] = "raven";
		class_hint->res_name = name;
		class_hint->res_class = name;
		XSetClassHint(g_x11.display, window->handle, class_hint);
		XFree(class_hint);
	}
	if (desc->flags & R_WINDOW_BORDERLESS) {
		/* Motif hints: flags = decorations, decorations = none */
		long hints[5] = { 2, 0, 0, 0, 0 };
		XChangeProperty(g_x11.display, window->handle, g_x11.motif_wm_hints, g_x11.motif_wm_hints, 32, PropModeReplace,
			(const unsigned char*)hints, 5);
	}
	if (desc->flags & R_WINDOW_MAXIMIZED) {
		/* before the window is mapped the state is set directly on the window */
		Atom states[2];
		states[0] = g_x11.net_wm_state_maximized_vert;
		states[1] = g_x11.net_wm_state_maximized_horz;
		XChangeProperty(g_x11.display, window->handle, g_x11.net_wm_state, XA_ATOM, 32, PropModeReplace,
			(const unsigned char*)states, 2);
	}
	if (g_x11.input_method) {
		window->input_context = XCreateIC(g_x11.input_method, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
			XNClientWindow, window->handle, XNFocusWindow, window->handle, NULL);
	}
	XFlush(g_x11.display);
	return window;
}

static void rvn_x11_window_destroy(rvn_pwindow_t* window) {
	if (window->input_context) XDestroyIC(window->input_context);
	if (!window->is_child) XDeleteContext(g_x11.display, window->handle, g_x11.context);
	XDestroyWindow(g_x11.display, window->handle);
	XFlush(g_x11.display);
	rvn_free(window);
}

static void rvn_x11_window_set_size(rvn_pwindow_t* window, int width, int height) {
	width = rvn_maxi(width, 1);
	height = rvn_maxi(height, 1);
	if (window->flags & R_WINDOW_NO_RESIZE) rvn_x11_set_size_hints(window, width, height);
	XResizeWindow(g_x11.display, window->handle, (unsigned int)width, (unsigned int)height);
	XFlush(g_x11.display);
}

static void rvn_x11_window_get_size(rvn_pwindow_t* window, int* width, int* height) {
	*width = window->width;
	*height = window->height;
}

static void rvn_x11_window_set_min_size(rvn_pwindow_t* window, int width, int height) {
	window->min_width = width;
	window->min_height = height;
	rvn_x11_set_size_hints(window, window->width, window->height);
}

static void rvn_x11_window_set_position(rvn_pwindow_t* window, int x, int y) {
	XMoveWindow(g_x11.display, window->handle, x, y);
	XFlush(g_x11.display);
}

static void rvn_x11_window_get_position(rvn_pwindow_t* window, int* x, int* y) {
	Window child;
	/* position of the client area on the screen, whatever the window manager added around it */
	XTranslateCoordinates(g_x11.display, window->handle, g_x11.root, 0, 0, x, y, &child);
}

static float rvn_x11_window_get_scale(rvn_pwindow_t* window) {
	RVN_UNUSED(window);
	return g_x11.scale;
}

static void rvn_x11_window_show(rvn_pwindow_t* window, int visible) {
	if (visible) XMapRaised(g_x11.display, window->handle);
	else XWithdrawWindow(g_x11.display, window->handle, g_x11.screen);
	XFlush(g_x11.display);
}

static void rvn_x11_window_set_state(rvn_pwindow_t* window, rvn_pwindow_state_t state) {
	switch (state) {
		case RVN_PWINDOW_MINIMIZED:
			XIconifyWindow(g_x11.display, window->handle, g_x11.screen);
			break;
		case RVN_PWINDOW_MAXIMIZED:
			XMapRaised(g_x11.display, window->handle);
			rvn_x11_send_maximize(window, R_TRUE);
			break;
		case RVN_PWINDOW_NORMAL:
			XMapRaised(g_x11.display, window->handle);
			rvn_x11_send_maximize(window, R_FALSE);
			break;
	}
	XFlush(g_x11.display);
}

static void rvn_x11_window_set_cursor(rvn_pwindow_t* window, RCursor cursor) {
	int index = (int)cursor - (int)R_CURSOR_ARROW;
	if (index < 0 || index >= RVN_X11_CURSOR_COUNT) index = 0;
	XDefineCursor(g_x11.display, window->handle, g_x11.cursors[index]);
	XFlush(g_x11.display);
}

static void rvn_x11_window_get_native(rvn_pwindow_t* window, RNativeWindow* native) {
	native->system = R_WINDOW_SYSTEM_X11;
	native->display = g_x11.display;
	native->window = (uintptr_t)window->handle;
	native->visual = (uintptr_t)XVisualIDFromVisual(DefaultVisual(g_x11.display, g_x11.screen));
}

/* ---------------------------------------------------------------------------
 *  Child windows (native viewports)
 * ------------------------------------------------------------------------- */

static rvn_pwindow_t* rvn_x11_child_create(rvn_pwindow_t* parent, int x, int y, int width, int height, void* user) {
	rvn_pwindow_t* child = (rvn_pwindow_t*)rvn_calloc(sizeof(rvn_pwindow_t));
	XSetWindowAttributes attributes;

	memset(&attributes, 0, sizeof(attributes));
	attributes.background_pixel = BlackPixel(g_x11.display, g_x11.screen);
	/* No input is selected on the child: pointer events propagate to the
	 * parent with coordinates relative to it, so the viewport widget receives
	 * them like any other widget. */
	attributes.event_mask = 0;
	child->user = user;
	child->is_child = R_TRUE;
	child->width = rvn_maxi(width, 1);
	child->height = rvn_maxi(height, 1);
	child->handle = XCreateWindow(g_x11.display, parent->handle, x, y, (unsigned int)child->width, (unsigned int)child->height,
		0, CopyFromParent, InputOutput, CopyFromParent, CWBackPixel | CWEventMask, &attributes);
	if (!child->handle) {
		rvn_free(child);
		return NULL;
	}
	XMapWindow(g_x11.display, child->handle);
	XFlush(g_x11.display);
	return child;
}

static void rvn_x11_child_set_rect(rvn_pwindow_t* child, int x, int y, int width, int height, int visible) {
	if (visible) {
		child->width = rvn_maxi(width, 1);
		child->height = rvn_maxi(height, 1);
		XMoveResizeWindow(g_x11.display, child->handle, x, y, (unsigned int)child->width, (unsigned int)child->height);
		XMapWindow(g_x11.display, child->handle);
	} else {
		XUnmapWindow(g_x11.display, child->handle);
	}
	XFlush(g_x11.display);
}

/* ---------------------------------------------------------------------------
 *  Clipboard
 * ------------------------------------------------------------------------- */

static void rvn_x11_clipboard_set(const char* text) {
	rvn_str_set(&g_x11.clipboard, text);
	XSetSelectionOwner(g_x11.display, g_x11.clipboard_atom, g_x11.helper, CurrentTime);
	XFlush(g_x11.display);
}

/* Another application asks for the text we own. */
static void rvn_x11_selection_request(const XSelectionRequestEvent* request) {
	XEvent reply;
	const char* text = g_x11.clipboard ? g_x11.clipboard : "";

	memset(&reply, 0, sizeof(reply));
	reply.xselection.type = SelectionNotify;
	reply.xselection.requestor = request->requestor;
	reply.xselection.selection = request->selection;
	reply.xselection.target = request->target;
	reply.xselection.time = request->time;
	reply.xselection.property = request->property != None ? request->property : request->target;

	if (request->target == g_x11.targets) {
		Atom targets[3];
		targets[0] = g_x11.targets;
		targets[1] = g_x11.utf8_string;
		targets[2] = XA_STRING;
		XChangeProperty(g_x11.display, request->requestor, reply.xselection.property, XA_ATOM, 32, PropModeReplace,
			(const unsigned char*)targets, 3);
	} else if (request->target == g_x11.utf8_string || request->target == XA_STRING) {
		XChangeProperty(g_x11.display, request->requestor, reply.xselection.property, request->target, 8, PropModeReplace,
			(const unsigned char*)text, (int)strlen(text));
	} else {
		reply.xselection.property = None;
	}
	XSendEvent(g_x11.display, request->requestor, False, 0, &reply);
	XFlush(g_x11.display);
}

static char* rvn_x11_clipboard_get(void) {
	XEvent event;
	int waited = 0;

	if (XGetSelectionOwner(g_x11.display, g_x11.clipboard_atom) == g_x11.helper) {
		return rvn_strdup(g_x11.clipboard ? g_x11.clipboard : "");
	}
	XConvertSelection(g_x11.display, g_x11.clipboard_atom, g_x11.utf8_string, g_x11.paste_property, g_x11.helper, CurrentTime);
	XFlush(g_x11.display);

	/* the owner answers with a SelectionNotify; give it a moment, not forever */
	while (!XCheckTypedWindowEvent(g_x11.display, g_x11.helper, SelectionNotify, &event)) {
		struct pollfd fd;
		if (waited >= RVN_X11_CLIPBOARD_TIMEOUT_MS) return NULL;
		fd.fd = ConnectionNumber(g_x11.display);
		fd.events = POLLIN;
		fd.revents = 0;
		poll(&fd, 1, 10);
		waited += 10;
	}
	if (event.xselection.property == None) return NULL;
	{
		Atom type = None;
		int format = 0;
		unsigned long count = 0, remaining = 0;
		unsigned char* data = NULL;
		char* result = NULL;
		XGetWindowProperty(g_x11.display, g_x11.helper, g_x11.paste_property, 0, 0x1FFFFFFF, True, AnyPropertyType,
			&type, &format, &count, &remaining, &data);
		/* very large texts use the incremental protocol, which is not supported */
		if (data && type != g_x11.incr && format == 8) result = rvn_strndup((const char*)data, (size_t)count);
		if (data) XFree(data);
		return result;
	}
}

/* ---------------------------------------------------------------------------
 *  Events
 * ------------------------------------------------------------------------- */

static int rvn_x11_key_test_and_set(RKey key, int down) {
	int index = (int)key, was;
	if (index <= 0 || index >= R_KEY_COUNT) return R_FALSE;
	was = (g_x11.keys_down[index / 8] >> (index % 8)) & 1;
	if (down) g_x11.keys_down[index / 8] |= (uint8_t)(1u << (index % 8));
	else g_x11.keys_down[index / 8] &= (uint8_t)~(1u << (index % 8));
	return was;
}

/* The key as labelled in the active layout: what a shortcut like Ctrl+Z means. */
static RKey rvn_x11_event_key(XKeyEvent* event) {
	unsigned int group = XkbGroupForCoreState(event->state);
	RKey key = rvn_x11_translate_keysym(XkbKeycodeToKeysym(g_x11.display, (KeyCode)event->keycode, (int)group, 0));
	/* layouts with digits on the shifted level (AZERTY) */
	if (key == R_KEY_UNKNOWN) {
		key = rvn_x11_translate_keysym(XkbKeycodeToKeysym(g_x11.display, (KeyCode)event->keycode, (int)group, 1));
	}
	return key;
}

static void rvn_x11_key_press(rvn_pwindow_t* window, XKeyEvent* key_event, int filtered) {
	rvn_pevent_t event;
	char buffer[64];
	int length = 0;
	KeySym symbol = NoSymbol;
	Status status = 0;

	/* the key itself; an event eaten by the input method (dead key, compose) is text only */
	if (!filtered) {
		memset(&event, 0, sizeof(event));
		event.type = RVN_PEVENT_KEY;
		event.key = rvn_x11_event_key(key_event);
		event.pressed = R_TRUE;
		event.modifiers = rvn_x11_translate_modifiers(key_event->state);
		event.repeat = rvn_x11_key_test_and_set(event.key, R_TRUE);
		rvn_x11_emit(window, &event);
	}

	/* then the text it produces, if any */
	if (window->input_context) {
		if (filtered) return;
		length = Xutf8LookupString(window->input_context, key_event, buffer, (int)sizeof(buffer) - 1, &symbol, &status);
		if (status != XLookupChars && status != XLookupBoth) length = 0;
	} else {
		/* no input method: Latin-1 only */
		char latin[16];
		int count = XLookupString(key_event, latin, (int)sizeof(latin), &symbol, NULL), i;
		for (i = 0; i < count; i++) length += rvn_utf8_encode((unsigned char)latin[i], buffer + length);
	}
	if (length > 0 && !(key_event->state & (ControlMask | Mod1Mask))) {
		const char* cursor = buffer;
		const char* end = buffer + length;
		while (cursor < end) {
			uint32_t codepoint = rvn_utf8_decode(&cursor, end);
			if (codepoint < 32 || codepoint == 127) continue;
			memset(&event, 0, sizeof(event));
			event.type = RVN_PEVENT_TEXT;
			event.codepoint = codepoint;
			rvn_x11_emit(window, &event);
		}
	}
}

static void rvn_x11_button(rvn_pwindow_t* window, const XButtonEvent* button, int pressed) {
	rvn_pevent_t event;
	memset(&event, 0, sizeof(event));
	event.x = button->x;
	event.y = button->y;
	event.modifiers = rvn_x11_translate_modifiers(button->state);

	/* buttons 4 to 7 are the scroll wheel */
	if (button->button >= 4 && button->button <= 7) {
		if (!pressed) return;
		event.type = RVN_PEVENT_MOUSE_WHEEL;
		if (button->button == 4) event.wheel_y = 1.0f;
		else if (button->button == 5) event.wheel_y = -1.0f;
		else if (button->button == 6) event.wheel_x = 1.0f;
		else event.wheel_x = -1.0f;
		rvn_x11_emit(window, &event);
		return;
	}
	event.type = RVN_PEVENT_MOUSE_BUTTON;
	event.pressed = pressed;
	switch (button->button) {
		case Button1: event.button = R_MOUSE_LEFT; break;
		case Button2: event.button = R_MOUSE_MIDDLE; break;
		case Button3: event.button = R_MOUSE_RIGHT; break;
		case 8: event.button = R_MOUSE_X1; break;
		case 9: event.button = R_MOUSE_X2; break;
		default: return;
	}
	rvn_x11_emit(window, &event);
}

static void rvn_x11_process(XEvent* xevent) {
	rvn_pwindow_t* window;
	rvn_pevent_t event;
	int filtered;

	/* the input method sees every event first (dead keys, compose sequences) */
	filtered = XFilterEvent(xevent, None);

	if (xevent->type == SelectionRequest) {
		rvn_x11_selection_request(&xevent->xselectionrequest);
		return;
	}
	if (xevent->type == MappingNotify) {
		XRefreshKeyboardMapping(&xevent->xmapping);
		return;
	}

	window = rvn_x11_find(xevent->xany.window);
	if (!window) return;
	memset(&event, 0, sizeof(event));

	switch (xevent->type) {
		case ClientMessage:
			if (xevent->xclient.message_type == g_x11.wm_protocols &&
				(Atom)xevent->xclient.data.l[0] == g_x11.wm_delete_window) {
				event.type = RVN_PEVENT_CLOSE;
				rvn_x11_emit(window, &event);
			}
			break;
		case ConfigureNotify:
			if (xevent->xconfigure.width != window->width || xevent->xconfigure.height != window->height) {
				window->width = xevent->xconfigure.width;
				window->height = xevent->xconfigure.height;
				event.type = RVN_PEVENT_RESIZE;
				event.width = window->width;
				event.height = window->height;
				event.scale = g_x11.scale;
				rvn_x11_emit(window, &event);
			}
			break;
		case Expose:
			if (xevent->xexpose.count == 0) {
				event.type = RVN_PEVENT_EXPOSE;
				rvn_x11_emit(window, &event);
			}
			break;
		case MotionNotify:
			event.type = RVN_PEVENT_MOUSE_MOVE;
			event.x = xevent->xmotion.x;
			event.y = xevent->xmotion.y;
			event.modifiers = rvn_x11_translate_modifiers(xevent->xmotion.state);
			rvn_x11_emit(window, &event);
			break;
		case EnterNotify:
			event.type = RVN_PEVENT_MOUSE_MOVE;
			event.x = xevent->xcrossing.x;
			event.y = xevent->xcrossing.y;
			event.modifiers = rvn_x11_translate_modifiers(xevent->xcrossing.state);
			rvn_x11_emit(window, &event);
			break;
		case LeaveNotify:
			/* a pointer grab (button held, menu of the window manager) is not a real exit */
			if (xevent->xcrossing.mode != NotifyNormal) break;
			event.type = RVN_PEVENT_MOUSE_LEAVE;
			rvn_x11_emit(window, &event);
			break;
		case ButtonPress:
			rvn_x11_button(window, &xevent->xbutton, R_TRUE);
			break;
		case ButtonRelease:
			rvn_x11_button(window, &xevent->xbutton, R_FALSE);
			break;
		case KeyPress:
			rvn_x11_key_press(window, &xevent->xkey, filtered);
			break;
		case KeyRelease:
			event.type = RVN_PEVENT_KEY;
			event.key = rvn_x11_event_key(&xevent->xkey);
			event.pressed = R_FALSE;
			event.modifiers = rvn_x11_translate_modifiers(xevent->xkey.state);
			rvn_x11_key_test_and_set(event.key, R_FALSE);
			rvn_x11_emit(window, &event);
			break;
		case FocusIn:
			if (window->input_context) XSetICFocus(window->input_context);
			event.type = RVN_PEVENT_FOCUS;
			event.focused = R_TRUE;
			rvn_x11_emit(window, &event);
			break;
		case FocusOut:
			if (window->input_context) XUnsetICFocus(window->input_context);
			/* key releases will go to another window */
			memset(g_x11.keys_down, 0, sizeof(g_x11.keys_down));
			event.type = RVN_PEVENT_FOCUS;
			event.focused = R_FALSE;
			rvn_x11_emit(window, &event);
			break;
		default:
			break;
	}
}

static void rvn_x11_pump(double timeout_seconds) {
	XFlush(g_x11.display);

	if (!XPending(g_x11.display) && timeout_seconds != 0.0) {
		struct pollfd fds[2];
		int timeout_ms = timeout_seconds < 0.0 ? -1 : (int)(timeout_seconds * 1000.0 + 0.999);
		fds[0].fd = ConnectionNumber(g_x11.display);
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		fds[1].fd = g_x11.wake_pipe[0];
		fds[1].events = POLLIN;
		fds[1].revents = 0;
		while (poll(fds, g_x11.wake_pipe[0] >= 0 ? 2 : 1, timeout_ms) < 0 && errno == EINTR) {}
	}
	/* empty the wake-up pipe, whether we slept or not */
	if (g_x11.wake_pipe[0] >= 0) {
		char buffer[64];
		while (read(g_x11.wake_pipe[0], buffer, sizeof(buffer)) > 0) {}
	}

	while (g_x11.display && XPending(g_x11.display)) {
		XEvent xevent;
		XNextEvent(g_x11.display, &xevent);
		/* only the last position of a burst of mouse moves matters */
		if (xevent.type == MotionNotify) {
			while (XEventsQueued(g_x11.display, QueuedAlready) > 0) {
				XEvent next;
				XPeekEvent(g_x11.display, &next);
				if (next.type != MotionNotify || next.xmotion.window != xevent.xmotion.window) break;
				XNextEvent(g_x11.display, &xevent);
			}
		}
		rvn_x11_process(&xevent);
	}
}

static void rvn_x11_wake(void) {
	if (g_x11.wake_pipe[1] >= 0) {
		char byte = 1;
		/* a full pipe already guarantees a wake-up */
		if (write(g_x11.wake_pipe[1], &byte, 1) < 0) {}
	}
}

const rvn_platform_t* rvn_platform_x11(void) {
	static const rvn_platform_t platform = {
		"X11",
		R_WINDOW_SYSTEM_X11,
		rvn_x11_init,
		rvn_x11_shutdown,
		rvn_x11_native_display,
		rvn_x11_window_create,
		rvn_x11_window_destroy,
		rvn_x11_window_set_title,
		rvn_x11_window_set_size,
		rvn_x11_window_get_size,
		rvn_x11_window_set_min_size,
		rvn_x11_window_set_position,
		rvn_x11_window_get_position,
		rvn_x11_window_get_scale,
		rvn_x11_window_show,
		rvn_x11_window_set_state,
		rvn_x11_window_set_cursor,
		rvn_x11_window_get_native,
		rvn_x11_child_create,
		rvn_x11_child_set_rect,
		rvn_x11_pump,
		rvn_x11_wake,
		rvn_x11_clipboard_set,
		rvn_x11_clipboard_get,
		NULL
	};
	return &platform;
}
