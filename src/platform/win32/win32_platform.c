/**
 * Raven - platform/win32/win32_platform.c
 * Win32 implementation of the platform interface. Only user32, gdi32 and
 * kernel32 are needed.
 *
 *   - Unicode everywhere: the API of Raven is UTF-8, converted to UTF-16 here,
 *   - per-monitor DPI awareness (the scale follows the window across monitors),
 *   - frames keep coming while the user drags or resizes the window,
 *   - child windows for native viewports (OpenGL, Direct3D, another swapchain),
 *   - an event object so that other threads can wake the event loop.
 */

#include "../platform.h"

#if defined(_WIN32)

/* the wide-character flavor of every Win32 name, whatever the build options */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif
#ifndef USER_DEFAULT_SCREEN_DPI
#define USER_DEFAULT_SCREEN_DPI 96
#endif
#ifndef USER_TIMER_MINIMUM
#define USER_TIMER_MINIMUM 0x0000000A
#endif
#ifndef PM_QS_SENDMESSAGE
#define PM_QS_SENDMESSAGE (QS_SENDMESSAGE << 16)
#endif

#define RVN_WIN32_CLASS L"Raven_Window"
#define RVN_WIN32_CHILD_CLASS L"Raven_Viewport"
#define RVN_WIN32_CURSOR_COUNT 10
#define RVN_WIN32_MODAL_TIMER 0x4855

struct rvn_pwindow_t {
	HWND handle;
	void* user;
	uint32_t flags;
	/** Client size in pixels. */
	int width, height;
	int min_width, min_height;
	float scale;
	HCURSOR cursor;
	int is_child;
	/** WM_MOUSELEAVE was requested. */
	int tracking;
	/** Mouse buttons held: the mouse is captured while it is not zero. */
	int buttons_down;
	WCHAR high_surrogate;
	int shown_once;
	/** The size changed while events could not be delivered. */
	int size_dirty;
	struct rvn_pwindow_t* next;
};

/* GetProcAddress returns a generic function pointer: convert it without a warning. */
#define RVN_WIN32_FUNCTION(type, pointer) ((type)(void (*)(void))(pointer))

typedef UINT (WINAPI *rvn_win32_get_dpi_for_window_fn)(HWND);
typedef BOOL (WINAPI *rvn_win32_adjust_rect_for_dpi_fn)(LPRECT, DWORD, BOOL, DWORD, UINT);
typedef BOOL (WINAPI *rvn_win32_set_awareness_context_fn)(HANDLE);
typedef BOOL (WINAPI *rvn_win32_set_dpi_aware_fn)(void);

static struct {
	HINSTANCE instance;
	rvn_pevent_handler_t handler;
	HANDLE wake_event;
	DWORD thread_id;
	HCURSOR cursors[RVN_WIN32_CURSOR_COUNT];
	rvn_pwindow_t* windows;
	/** Inside the modal loop Windows runs while a window is moved or resized. */
	int modal_loop;
	/** Inside rvn_win32_wait_service: nothing may be dispatched to the core. */
	int waiting;
	/* Windows 10 1607+; NULL on older systems */
	rvn_win32_get_dpi_for_window_fn get_dpi_for_window;
	rvn_win32_adjust_rect_for_dpi_fn adjust_rect_for_dpi;
} g_win32;

/* ---------------------------------------------------------------------------
 *  Helpers
 * ------------------------------------------------------------------------- */

/* UTF-8 -> UTF-16. Free with rvn_free. */
static WCHAR* rvn_win32_widen(const char* text) {
	int count;
	WCHAR* wide;
	if (!text) text = "";
	count = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
	wide = (WCHAR*)rvn_calloc(sizeof(WCHAR) * (size_t)(count > 0 ? count : 1));
	if (count > 0) MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, count);
	return wide;
}

/* UTF-16 -> UTF-8. Free with rvn_free. */
static char* rvn_win32_narrow(const WCHAR* wide) {
	int count = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
	char* text = (char*)rvn_calloc((size_t)(count > 0 ? count : 1));
	if (count > 0) WideCharToMultiByte(CP_UTF8, 0, wide, -1, text, count, NULL, NULL);
	return text;
}

/* Looks up a function that only recent versions of Windows have. */
static FARPROC rvn_win32_user32(const char* name) {
	HMODULE user32 = GetModuleHandleW(L"user32.dll");
	return user32 ? GetProcAddress(user32, name) : NULL;
}

static UINT rvn_win32_dpi(HWND handle) {
	UINT dpi = 0;
	if (g_win32.get_dpi_for_window && handle) dpi = g_win32.get_dpi_for_window(handle);
	if (dpi == 0) {
		HDC screen = GetDC(NULL);
		dpi = screen ? (UINT)GetDeviceCaps(screen, LOGPIXELSX) : USER_DEFAULT_SCREEN_DPI;
		if (screen) ReleaseDC(NULL, screen);
	}
	return dpi ? dpi : USER_DEFAULT_SCREEN_DPI;
}

/* Grows a client rectangle to the rectangle of the whole window (title bar, borders). */
static void rvn_win32_adjust(HWND handle, RECT* rect) {
	DWORD style = (DWORD)GetWindowLongW(handle, GWL_STYLE);
	DWORD ex_style = (DWORD)GetWindowLongW(handle, GWL_EXSTYLE);
	if (g_win32.adjust_rect_for_dpi) g_win32.adjust_rect_for_dpi(rect, style, FALSE, ex_style, rvn_win32_dpi(handle));
	else AdjustWindowRectEx(rect, style, FALSE, ex_style);
}

static void rvn_win32_emit(rvn_pwindow_t* window, rvn_pevent_t* event) {
	event->user = window->user;
	g_win32.handler(event);
}

static void rvn_win32_emit_resize(rvn_pwindow_t* window) {
	rvn_pevent_t event;
	memset(&event, 0, sizeof(event));
	event.type = RVN_PEVENT_RESIZE;
	event.width = window->width;
	event.height = window->height;
	event.scale = window->scale;
	rvn_win32_emit(window, &event);
}

static void rvn_win32_emit_modal_tick(void) {
	rvn_pevent_t event;
	memset(&event, 0, sizeof(event));
	event.type = RVN_PEVENT_MODAL_TICK;
	g_win32.handler(&event);
}

static uint32_t rvn_win32_modifiers(void) {
	uint32_t modifiers = 0;
	int ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
	int alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
	/* AltGr is reported as Ctrl + right Alt: it types characters, it is not a shortcut */
	if (ctrl && (GetKeyState(VK_RMENU) & 0x8000) && !(GetKeyState(VK_RCONTROL) & 0x8000)) {
		ctrl = R_FALSE;
		alt = R_FALSE;
	}
	if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= R_MOD_SHIFT;
	if (ctrl) modifiers |= R_MOD_CTRL;
	if (alt) modifiers |= R_MOD_ALT;
	if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) modifiers |= R_MOD_SUPER;
	return modifiers;
}

static RKey rvn_win32_translate_key(WPARAM virtual_key, LPARAM info) {
	int extended = (int)((info >> 24) & 1);
	UINT scancode = (UINT)((info >> 16) & 0xFF);

	if (virtual_key >= 'A' && virtual_key <= 'Z') return (RKey)(R_KEY_A + (int)(virtual_key - 'A'));
	if (virtual_key >= '0' && virtual_key <= '9') return (RKey)(R_KEY_0 + (int)(virtual_key - '0'));
	if (virtual_key >= VK_F1 && virtual_key <= VK_F12) return (RKey)(R_KEY_F1 + (int)(virtual_key - VK_F1));
	if (virtual_key >= VK_NUMPAD0 && virtual_key <= VK_NUMPAD9) return (RKey)(R_KEY_KP_0 + (int)(virtual_key - VK_NUMPAD0));

	switch (virtual_key) {
		case VK_SPACE: return R_KEY_SPACE;
		case VK_OEM_7: return R_KEY_APOSTROPHE;
		case VK_OEM_COMMA: return R_KEY_COMMA;
		case VK_OEM_MINUS: return R_KEY_MINUS;
		case VK_OEM_PERIOD: return R_KEY_PERIOD;
		case VK_OEM_2: return R_KEY_SLASH;
		case VK_OEM_1: return R_KEY_SEMICOLON;
		case VK_OEM_PLUS: return R_KEY_EQUAL;
		case VK_OEM_4: return R_KEY_LEFT_BRACKET;
		case VK_OEM_5: return R_KEY_BACKSLASH;
		case VK_OEM_6: return R_KEY_RIGHT_BRACKET;
		case VK_OEM_3: return R_KEY_GRAVE;

		case VK_ESCAPE: return R_KEY_ESCAPE;
		case VK_RETURN: return extended ? R_KEY_KP_ENTER : R_KEY_ENTER;
		case VK_TAB: return R_KEY_TAB;
		case VK_BACK: return R_KEY_BACKSPACE;
		case VK_INSERT: return R_KEY_INSERT;
		case VK_DELETE: return R_KEY_DELETE;
		case VK_RIGHT: return R_KEY_RIGHT;
		case VK_LEFT: return R_KEY_LEFT;
		case VK_DOWN: return R_KEY_DOWN;
		case VK_UP: return R_KEY_UP;
		case VK_PRIOR: return R_KEY_PAGE_UP;
		case VK_NEXT: return R_KEY_PAGE_DOWN;
		case VK_HOME: return R_KEY_HOME;
		case VK_END: return R_KEY_END;
		case VK_CAPITAL: return R_KEY_CAPS_LOCK;
		case VK_SNAPSHOT: return R_KEY_PRINT_SCREEN;
		case VK_PAUSE: return R_KEY_PAUSE;
		case VK_APPS: return R_KEY_MENU;

		case VK_DECIMAL: return R_KEY_KP_DECIMAL;
		case VK_DIVIDE: return R_KEY_KP_DIVIDE;
		case VK_MULTIPLY: return R_KEY_KP_MULTIPLY;
		case VK_SUBTRACT: return R_KEY_KP_SUBTRACT;
		case VK_ADD: return R_KEY_KP_ADD;

		/* the generic codes do not tell left from right */
		case VK_SHIFT:
			return MapVirtualKeyW(scancode, MAPVK_VSC_TO_VK_EX) == VK_RSHIFT ? R_KEY_RIGHT_SHIFT : R_KEY_LEFT_SHIFT;
		case VK_CONTROL: return extended ? R_KEY_RIGHT_CTRL : R_KEY_LEFT_CTRL;
		case VK_MENU: return extended ? R_KEY_RIGHT_ALT : R_KEY_LEFT_ALT;
		case VK_LWIN: return R_KEY_LEFT_SUPER;
		case VK_RWIN: return R_KEY_RIGHT_SUPER;
		default: return R_KEY_UNKNOWN;
	}
}

/* ---------------------------------------------------------------------------
 *  Window procedure
 * ------------------------------------------------------------------------- */

static void rvn_win32_mouse_button(rvn_pwindow_t* window, RMouseButton button, int pressed, LPARAM position) {
	rvn_pevent_t event;

	/* keep receiving the mouse while a button is held, also outside the window (drags) */
	if (pressed) {
		if (window->buttons_down++ == 0) SetCapture(window->handle);
	} else if (window->buttons_down > 0 && --window->buttons_down == 0) {
		ReleaseCapture();
	}

	memset(&event, 0, sizeof(event));
	event.type = RVN_PEVENT_MOUSE_BUTTON;
	event.button = (int)button;
	event.pressed = pressed;
	event.x = GET_X_LPARAM(position);
	event.y = GET_Y_LPARAM(position);
	event.modifiers = rvn_win32_modifiers();
	rvn_win32_emit(window, &event);
}

static void rvn_win32_key(rvn_pwindow_t* window, WPARAM virtual_key, LPARAM info, int pressed) {
	rvn_pevent_t event;
	memset(&event, 0, sizeof(event));
	event.type = RVN_PEVENT_KEY;
	event.key = rvn_win32_translate_key(virtual_key, info);
	event.pressed = pressed;
	event.modifiers = rvn_win32_modifiers();
	/* bit 30: the key was already down before this message */
	event.repeat = pressed && ((info >> 30) & 1);
	rvn_win32_emit(window, &event);
}

static void rvn_win32_char(rvn_pwindow_t* window, WCHAR unit) {
	rvn_pevent_t event;
	uint32_t codepoint = unit;

	/* characters outside the basic plane arrive as two messages */
	if (unit >= 0xD800 && unit <= 0xDBFF) {
		window->high_surrogate = unit;
		return;
	}
	if (unit >= 0xDC00 && unit <= 0xDFFF) {
		if (!window->high_surrogate) return;
		codepoint = 0x10000u + (((uint32_t)window->high_surrogate - 0xD800u) << 10) + ((uint32_t)unit - 0xDC00u);
	}
	window->high_surrogate = 0;
	if (codepoint < 32 || codepoint == 127) return;

	memset(&event, 0, sizeof(event));
	event.type = RVN_PEVENT_TEXT;
	event.codepoint = codepoint;
	rvn_win32_emit(window, &event);
}

static LRESULT CALLBACK rvn_win32_window_proc(HWND handle, UINT message, WPARAM wparam, LPARAM lparam) {
	rvn_pwindow_t* window = (rvn_pwindow_t*)GetWindowLongPtrW(handle, GWLP_USERDATA);
	rvn_pevent_t event;

	/* being created or destroyed: the core does not know or no longer knows this window */
	if (!window) return DefWindowProcW(handle, message, wparam, lparam);

	if (g_win32.waiting) {
		/*
		 * The UI thread is blocked on the render thread and only answers
		 * messages sent by other threads (see rvn_win32_wait_service). The
		 * core is in the middle of something: nothing is dispatched to it.
		 */
		switch (message) {
			case WM_SIZE:
				if (wparam != SIZE_MINIMIZED) window->size_dirty = R_TRUE;
				return 0;
			case WM_CLOSE:
				PostMessageW(handle, WM_CLOSE, 0, 0);
				return 0;
			case WM_ERASEBKGND:
				return 1;
			default:
				return DefWindowProcW(handle, message, wparam, lparam);
		}
	}

	memset(&event, 0, sizeof(event));
	switch (message) {
		case WM_CLOSE:
			/* the core decides: it destroys the window unless the application refuses */
			event.type = RVN_PEVENT_CLOSE;
			rvn_win32_emit(window, &event);
			return 0;

		case WM_SIZE: {
			int width = (int)LOWORD(lparam), height = (int)HIWORD(lparam);
			/* a minimized window keeps its size: there is nothing to lay out or render */
			if (wparam == SIZE_MINIMIZED) return 0;
			if (width != window->width || height != window->height) {
				window->width = width;
				window->height = height;
				rvn_win32_emit_resize(window);
				/* live resize: Rupdate is not running, draw the new size now */
				if (g_win32.modal_loop) rvn_win32_emit_modal_tick();
			}
			return 0;
		}

		case WM_DPICHANGED: {
			/* moved to a monitor with another scale: Windows suggests the new rectangle */
			const RECT* suggested = (const RECT*)lparam;
			window->scale = (float)HIWORD(wparam) / (float)USER_DEFAULT_SCREEN_DPI;
			SetWindowPos(handle, NULL, suggested->left, suggested->top, suggested->right - suggested->left,
				suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
			rvn_win32_emit_resize(window);
			return 0;
		}

		case WM_GETMINMAXINFO:
			if (window->min_width > 0 || window->min_height > 0) {
				MINMAXINFO* info = (MINMAXINFO*)lparam;
				RECT rect;
				rect.left = rect.top = 0;
				rect.right = window->min_width;
				rect.bottom = window->min_height;
				rvn_win32_adjust(handle, &rect);
				info->ptMinTrackSize.x = rect.right - rect.left;
				info->ptMinTrackSize.y = rect.bottom - rect.top;
				return 0;
			}
			break;

		case WM_ERASEBKGND:
			/* the renderer covers the whole client area: erasing would only flicker */
			return 1;

		case WM_PAINT:
			ValidateRect(handle, NULL);
			event.type = RVN_PEVENT_EXPOSE;
			rvn_win32_emit(window, &event);
			return 0;

		/*
		 * While the user moves or resizes the window, or uses the system
		 * menu, Windows runs its own message loop and Rupdate is not
		 * called. A timer keeps frames coming.
		 */
		case WM_ENTERSIZEMOVE:
		case WM_ENTERMENULOOP:
			g_win32.modal_loop++;
			SetTimer(handle, RVN_WIN32_MODAL_TIMER, USER_TIMER_MINIMUM, NULL);
			break;
		case WM_EXITSIZEMOVE:
		case WM_EXITMENULOOP:
			if (g_win32.modal_loop > 0) g_win32.modal_loop--;
			KillTimer(handle, RVN_WIN32_MODAL_TIMER);
			break;
		case WM_TIMER:
			if (wparam == RVN_WIN32_MODAL_TIMER) {
				rvn_win32_emit_modal_tick();
				return 0;
			}
			break;

		case WM_MOUSEMOVE:
			if (!window->tracking) {
				TRACKMOUSEEVENT track;
				memset(&track, 0, sizeof(track));
				track.cbSize = sizeof(track);
				track.dwFlags = TME_LEAVE;
				track.hwndTrack = handle;
				TrackMouseEvent(&track);
				window->tracking = R_TRUE;
			}
			event.type = RVN_PEVENT_MOUSE_MOVE;
			event.x = GET_X_LPARAM(lparam);
			event.y = GET_Y_LPARAM(lparam);
			event.modifiers = rvn_win32_modifiers();
			rvn_win32_emit(window, &event);
			return 0;

		case WM_MOUSELEAVE:
			window->tracking = R_FALSE;
			event.type = RVN_PEVENT_MOUSE_LEAVE;
			rvn_win32_emit(window, &event);
			return 0;

		case WM_LBUTTONDOWN: rvn_win32_mouse_button(window, R_MOUSE_LEFT, R_TRUE, lparam); return 0;
		case WM_LBUTTONUP: rvn_win32_mouse_button(window, R_MOUSE_LEFT, R_FALSE, lparam); return 0;
		case WM_RBUTTONDOWN: rvn_win32_mouse_button(window, R_MOUSE_RIGHT, R_TRUE, lparam); return 0;
		case WM_RBUTTONUP: rvn_win32_mouse_button(window, R_MOUSE_RIGHT, R_FALSE, lparam); return 0;
		case WM_MBUTTONDOWN: rvn_win32_mouse_button(window, R_MOUSE_MIDDLE, R_TRUE, lparam); return 0;
		case WM_MBUTTONUP: rvn_win32_mouse_button(window, R_MOUSE_MIDDLE, R_FALSE, lparam); return 0;
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
			rvn_win32_mouse_button(window, GET_XBUTTON_WPARAM(wparam) == XBUTTON1 ? R_MOUSE_X1 : R_MOUSE_X2,
				message == WM_XBUTTONDOWN, lparam);
			/* these two messages must answer TRUE */
			return TRUE;

		case WM_CAPTURECHANGED:
			window->buttons_down = 0;
			break;

		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL: {
			float notches = (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
			event.type = RVN_PEVENT_MOUSE_WHEEL;
			/* same convention as the other platforms: positive is up / left */
			if (message == WM_MOUSEWHEEL) event.wheel_y = notches;
			else event.wheel_x = -notches;
			event.modifiers = rvn_win32_modifiers();
			rvn_win32_emit(window, &event);
			return 0;
		}

		case WM_KEYDOWN:
			rvn_win32_key(window, wparam, lparam, R_TRUE);
			return 0;
		case WM_KEYUP:
			rvn_win32_key(window, wparam, lparam, R_FALSE);
			return 0;
		case WM_SYSKEYDOWN:
			/* keys pressed with Alt: report them, and let Windows keep Alt+F4 and Alt+Space */
			rvn_win32_key(window, wparam, lparam, R_TRUE);
			break;
		case WM_SYSKEYUP:
			rvn_win32_key(window, wparam, lparam, R_FALSE);
			break;

		case WM_CHAR:
			rvn_win32_char(window, (WCHAR)wparam);
			return 0;
		case WM_SYSCHAR:
			/* Alt+letter would beep; Alt+Space still opens the system menu */
			if (wparam != ' ') return 0;
			break;
		case WM_SYSCOMMAND:
			/* Alt or F10 alone would enter the (absent) menu bar and freeze the application */
			if ((wparam & 0xFFF0) == SC_KEYMENU && lparam == 0) return 0;
			break;

		case WM_SETFOCUS:
			event.type = RVN_PEVENT_FOCUS;
			event.focused = R_TRUE;
			rvn_win32_emit(window, &event);
			return 0;
		case WM_KILLFOCUS:
			if (window->buttons_down > 0) {
				window->buttons_down = 0;
				ReleaseCapture();
			}
			window->high_surrogate = 0;
			event.type = RVN_PEVENT_FOCUS;
			event.focused = R_FALSE;
			rvn_win32_emit(window, &event);
			return 0;

		case WM_SETCURSOR:
			/* over the client area the cursor is the one of the hovered widget */
			if (LOWORD(lparam) == HTCLIENT) {
				SetCursor(window->cursor ? window->cursor : g_win32.cursors[0]);
				return TRUE;
			}
			break;

		default:
			break;
	}
	return DefWindowProcW(handle, message, wparam, lparam);
}

/* Child windows are surfaces for an external renderer: they take no input. */
static LRESULT CALLBACK rvn_win32_child_proc(HWND handle, UINT message, WPARAM wparam, LPARAM lparam) {
	switch (message) {
		case WM_NCHITTEST:
			/* the mouse goes through to the parent, so the viewport widget gets the events */
			return HTTRANSPARENT;
		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT:
			ValidateRect(handle, NULL);
			return 0;
		default:
			return DefWindowProcW(handle, message, wparam, lparam);
	}
}

/* ---------------------------------------------------------------------------
 *  Lifecycle
 * ------------------------------------------------------------------------- */

static int rvn_win32_init(rvn_pevent_handler_t handler) {
	/* same order as RCursor */
	const LPCWSTR cursor_names[RVN_WIN32_CURSOR_COUNT] = {
		IDC_ARROW, IDC_IBEAM, IDC_HAND, IDC_CROSS, IDC_SIZEWE, IDC_SIZENS, IDC_SIZENWSE, IDC_SIZENESW, IDC_SIZEALL, IDC_NO
	};
	WNDCLASSEXW window_class;
	FARPROC function;
	int i;

	memset(&g_win32, 0, sizeof(g_win32));
	g_win32.handler = handler;
	g_win32.instance = GetModuleHandleW(NULL);
	g_win32.thread_id = GetCurrentThreadId();

	/* sharp rendering on every monitor; ignored when the manifest of the application already chose */
	function = rvn_win32_user32("SetProcessDpiAwarenessContext");
	if (function) {
		/* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 */
		RVN_WIN32_FUNCTION(rvn_win32_set_awareness_context_fn, function)((HANDLE)(intptr_t)-4);
	} else {
		function = rvn_win32_user32("SetProcessDPIAware");
		if (function) RVN_WIN32_FUNCTION(rvn_win32_set_dpi_aware_fn, function)();
	}
	g_win32.get_dpi_for_window = RVN_WIN32_FUNCTION(rvn_win32_get_dpi_for_window_fn, rvn_win32_user32("GetDpiForWindow"));
	g_win32.adjust_rect_for_dpi = RVN_WIN32_FUNCTION(rvn_win32_adjust_rect_for_dpi_fn, rvn_win32_user32("AdjustWindowRectExForDpi"));

	for (i = 0; i < RVN_WIN32_CURSOR_COUNT; i++) g_win32.cursors[i] = LoadCursorW(NULL, cursor_names[i]);

	memset(&window_class, 0, sizeof(window_class));
	window_class.cbSize = sizeof(window_class);
	window_class.lpfnWndProc = rvn_win32_window_proc;
	window_class.hInstance = g_win32.instance;
	window_class.hIcon = LoadIconW(NULL, IDI_APPLICATION);
	window_class.lpszClassName = RVN_WIN32_CLASS;
	if (!RegisterClassExW(&window_class)) {
		rvn_error(R_ERROR_PLATFORM, R_SEVERITY_ERROR, "Win32: cannot register the window class (error %lu)", (unsigned long)GetLastError());
		return R_FALSE;
	}

	memset(&window_class, 0, sizeof(window_class));
	window_class.cbSize = sizeof(window_class);
	/* a private device context, as OpenGL requires */
	window_class.style = CS_OWNDC;
	window_class.lpfnWndProc = rvn_win32_child_proc;
	window_class.hInstance = g_win32.instance;
	window_class.lpszClassName = RVN_WIN32_CHILD_CLASS;
	if (!RegisterClassExW(&window_class)) {
		UnregisterClassW(RVN_WIN32_CLASS, g_win32.instance);
		rvn_error(R_ERROR_PLATFORM, R_SEVERITY_ERROR, "Win32: cannot register the viewport class (error %lu)", (unsigned long)GetLastError());
		return R_FALSE;
	}

	/* auto-reset: one SetEvent wakes exactly one wait */
	g_win32.wake_event = CreateEventW(NULL, FALSE, FALSE, NULL);
	return R_TRUE;
}

static void rvn_win32_shutdown(void) {
	if (g_win32.wake_event) CloseHandle(g_win32.wake_event);
	UnregisterClassW(RVN_WIN32_CHILD_CLASS, g_win32.instance);
	UnregisterClassW(RVN_WIN32_CLASS, g_win32.instance);
	memset(&g_win32, 0, sizeof(g_win32));
}

static void* rvn_win32_native_display(void) {
	return (void*)g_win32.instance;
}

/* ---------------------------------------------------------------------------
 *  Windows
 * ------------------------------------------------------------------------- */

static void rvn_win32_read_client_size(rvn_pwindow_t* window) {
	RECT client;
	if (GetClientRect(window->handle, &client)) {
		window->width = (int)(client.right - client.left);
		window->height = (int)(client.bottom - client.top);
	}
}

static void rvn_win32_window_set_size(rvn_pwindow_t* window, int width, int height) {
	RECT rect;
	rect.left = rect.top = 0;
	rect.right = rvn_maxi(width, 1);
	rect.bottom = rvn_maxi(height, 1);
	rvn_win32_adjust(window->handle, &rect);
	/* answers with WM_SIZE, which reports the new size to the core */
	SetWindowPos(window->handle, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static rvn_pwindow_t* rvn_win32_window_create(const rvn_pwindow_desc_t* desc) {
	rvn_pwindow_t* window = (rvn_pwindow_t*)rvn_calloc(sizeof(rvn_pwindow_t));
	WCHAR* title = rvn_win32_widen(desc->title);
	DWORD style;

	if (desc->flags & R_WINDOW_BORDERLESS) style = WS_POPUP;
	else if (desc->flags & R_WINDOW_NO_RESIZE) style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	else style = WS_OVERLAPPEDWINDOW;
	/* native viewports are child windows: never draw over them */
	style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

	window->user = desc->user;
	window->flags = desc->flags;
	window->handle = CreateWindowExW(WS_EX_APPWINDOW, RVN_WIN32_CLASS, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, g_win32.instance, NULL);
	rvn_free(title);
	if (!window->handle) {
		rvn_error(R_ERROR_PLATFORM, R_SEVERITY_ERROR, "Win32: cannot create the window (error %lu)", (unsigned long)GetLastError());
		rvn_free(window);
		return NULL;
	}

	/* the scale is the one of the monitor Windows placed the window on */
	window->scale = (float)rvn_win32_dpi(window->handle) / (float)USER_DEFAULT_SCREEN_DPI;
	rvn_win32_window_set_size(window, (int)((float)desc->width * window->scale + 0.5f),
		(int)((float)desc->height * window->scale + 0.5f));
	rvn_win32_read_client_size(window);

	/* from now on the window procedure reports events to the core */
	SetWindowLongPtrW(window->handle, GWLP_USERDATA, (LONG_PTR)window);
	window->next = g_win32.windows;
	g_win32.windows = window;
	return window;
}

static void rvn_win32_window_destroy(rvn_pwindow_t* window) {
	rvn_pwindow_t** link;
	for (link = &g_win32.windows; *link; link = &(*link)->next) {
		if (*link == window) {
			*link = window->next;
			break;
		}
	}
	/* DestroyWindow sends messages: they must not reach the core any more */
	SetWindowLongPtrW(window->handle, GWLP_USERDATA, 0);
	DestroyWindow(window->handle);
	rvn_free(window);
}

static void rvn_win32_window_set_title(rvn_pwindow_t* window, const char* title) {
	WCHAR* wide = rvn_win32_widen(title);
	SetWindowTextW(window->handle, wide);
	rvn_free(wide);
}

static void rvn_win32_window_get_size(rvn_pwindow_t* window, int* width, int* height) {
	*width = window->width;
	*height = window->height;
}

static void rvn_win32_window_set_min_size(rvn_pwindow_t* window, int width, int height) {
	window->min_width = width;
	window->min_height = height;
}

static void rvn_win32_window_set_position(rvn_pwindow_t* window, int x, int y) {
	RECT rect;
	/* x, y is the client area: find where the frame around it starts */
	rect.left = rect.top = rect.right = rect.bottom = 0;
	rvn_win32_adjust(window->handle, &rect);
	SetWindowPos(window->handle, NULL, x + rect.left, y + rect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static void rvn_win32_window_get_position(rvn_pwindow_t* window, int* x, int* y) {
	POINT origin;
	origin.x = origin.y = 0;
	ClientToScreen(window->handle, &origin);
	*x = (int)origin.x;
	*y = (int)origin.y;
}

static float rvn_win32_window_get_scale(rvn_pwindow_t* window) {
	return window->scale > 0.0f ? window->scale : 1.0f;
}

static void rvn_win32_window_show(rvn_pwindow_t* window, int visible) {
	if (!visible) {
		ShowWindow(window->handle, SW_HIDE);
	} else if (!window->shown_once && (window->flags & R_WINDOW_MAXIMIZED)) {
		ShowWindow(window->handle, SW_SHOWMAXIMIZED);
	} else {
		ShowWindow(window->handle, SW_SHOW);
	}
	if (visible) window->shown_once = R_TRUE;
}

static void rvn_win32_window_set_state(rvn_pwindow_t* window, rvn_pwindow_state_t state) {
	switch (state) {
		case RVN_PWINDOW_MINIMIZED: ShowWindow(window->handle, SW_MINIMIZE); break;
		case RVN_PWINDOW_MAXIMIZED: ShowWindow(window->handle, SW_MAXIMIZE); break;
		case RVN_PWINDOW_NORMAL: ShowWindow(window->handle, SW_RESTORE); break;
	}
	window->shown_once = R_TRUE;
}

static void rvn_win32_window_set_cursor(rvn_pwindow_t* window, RCursor cursor) {
	int index = (int)cursor - (int)R_CURSOR_ARROW;
	POINT position;
	if (index < 0 || index >= RVN_WIN32_CURSOR_COUNT) index = 0;
	window->cursor = g_win32.cursors[index];
	/* WM_SETCURSOR only comes with the next mouse move: apply now when the mouse is over the window */
	if (GetCursorPos(&position) && WindowFromPoint(position) == window->handle) SetCursor(window->cursor);
}

static void rvn_win32_window_get_native(rvn_pwindow_t* window, RNativeWindow* native) {
	native->system = R_WINDOW_SYSTEM_WIN32;
	native->display = (void*)g_win32.instance;
	native->window = (uintptr_t)window->handle;
	native->visual = 0;
}

/* ---------------------------------------------------------------------------
 *  Child windows (native viewports)
 * ------------------------------------------------------------------------- */

static rvn_pwindow_t* rvn_win32_child_create(rvn_pwindow_t* parent, int x, int y, int width, int height, void* user) {
	rvn_pwindow_t* child = (rvn_pwindow_t*)rvn_calloc(sizeof(rvn_pwindow_t));
	child->user = user;
	child->is_child = R_TRUE;
	child->width = rvn_maxi(width, 1);
	child->height = rvn_maxi(height, 1);
	child->scale = parent->scale;
	child->handle = CreateWindowExW(0, RVN_WIN32_CHILD_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
		x, y, child->width, child->height, parent->handle, NULL, g_win32.instance, NULL);
	if (!child->handle) {
		rvn_free(child);
		return NULL;
	}
	return child;
}

static void rvn_win32_child_set_rect(rvn_pwindow_t* child, int x, int y, int width, int height, int visible) {
	if (visible) {
		child->width = rvn_maxi(width, 1);
		child->height = rvn_maxi(height, 1);
		SetWindowPos(child->handle, NULL, x, y, child->width, child->height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
	} else {
		ShowWindow(child->handle, SW_HIDE);
	}
}

/* Child windows are not in the list of windows and have no user data. */
static void rvn_win32_any_destroy(rvn_pwindow_t* window) {
	if (window->is_child) {
		DestroyWindow(window->handle);
		rvn_free(window);
	} else {
		rvn_win32_window_destroy(window);
	}
}

/* ---------------------------------------------------------------------------
 *  Clipboard
 * ------------------------------------------------------------------------- */

static void rvn_win32_clipboard_set(const char* text) {
	WCHAR* wide = rvn_win32_widen(text);
	size_t bytes = (wcslen(wide) + 1) * sizeof(WCHAR);
	HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);

	if (memory) {
		void* target = GlobalLock(memory);
		if (target) {
			memcpy(target, wide, bytes);
			GlobalUnlock(memory);
		}
		if (target && OpenClipboard(NULL)) {
			EmptyClipboard();
			/* on success the clipboard owns the memory */
			if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
			CloseClipboard();
		} else {
			GlobalFree(memory);
		}
	}
	rvn_free(wide);
}

static char* rvn_win32_clipboard_get(void) {
	char* text = NULL;
	HANDLE memory;
	if (!OpenClipboard(NULL)) return NULL;
	memory = GetClipboardData(CF_UNICODETEXT);
	if (memory) {
		const WCHAR* wide = (const WCHAR*)GlobalLock(memory);
		if (wide) {
			text = rvn_win32_narrow(wide);
			GlobalUnlock(memory);
		}
	}
	CloseClipboard();
	return text;
}

/* ---------------------------------------------------------------------------
 *  Events
 * ------------------------------------------------------------------------- */

static void rvn_win32_pump(double timeout_seconds) {
	rvn_pwindow_t* window;
	MSG message;

	if (timeout_seconds != 0.0) {
		DWORD timeout = timeout_seconds < 0.0 ? INFINITE : (DWORD)(timeout_seconds * 1000.0 + 0.999);
		/* returns when a message is available, when another thread calls wake, or after the timeout */
		MsgWaitForMultipleObjectsEx(g_win32.wake_event ? 1 : 0, &g_win32.wake_event, timeout, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
	}

	/* sizes that changed while the core could not be told */
	for (window = g_win32.windows; window; window = window->next) {
		if (window->size_dirty) {
			int width = window->width, height = window->height;
			window->size_dirty = R_FALSE;
			rvn_win32_read_client_size(window);
			if (window->width != width || window->height != height) {
				rvn_win32_emit_resize(window);
				/* the event may have destroyed windows: the list is no longer safe to walk */
				break;
			}
		}
	}

	while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
		/* WM_QUIT is not used: the application stops with Rquit */
		if (message.message == WM_QUIT) continue;
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
}

static void rvn_win32_wake(void) {
	if (g_win32.wake_event) SetEvent(g_win32.wake_event);
}

/*
 * Called while the UI thread waits for the render thread. Graphics drivers
 * send messages to the window from the render thread when they create or
 * resize a swapchain; if this thread did not answer them, both threads would
 * wait for each other forever. PeekMessage delivers the messages sent by
 * other threads; the posted ones (input, paint) stay in the queue.
 */
static void rvn_win32_wait_service(void) {
	MSG message;
	if (GetCurrentThreadId() != g_win32.thread_id) return;
	g_win32.waiting++;
	PeekMessageW(&message, NULL, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE);
	g_win32.waiting--;
}

const rvn_platform_t* rvn_platform_win32(void) {
	static const rvn_platform_t platform = {
		"Win32",
		R_WINDOW_SYSTEM_WIN32,
		rvn_win32_init,
		rvn_win32_shutdown,
		rvn_win32_native_display,
		rvn_win32_window_create,
		rvn_win32_any_destroy,
		rvn_win32_window_set_title,
		rvn_win32_window_set_size,
		rvn_win32_window_get_size,
		rvn_win32_window_set_min_size,
		rvn_win32_window_set_position,
		rvn_win32_window_get_position,
		rvn_win32_window_get_scale,
		rvn_win32_window_show,
		rvn_win32_window_set_state,
		rvn_win32_window_set_cursor,
		rvn_win32_window_get_native,
		rvn_win32_child_create,
		rvn_win32_child_set_rect,
		rvn_win32_pump,
		rvn_win32_wake,
		rvn_win32_clipboard_set,
		rvn_win32_clipboard_get,
		rvn_win32_wait_service
	};
	return &platform;
}

#endif /* _WIN32 */
