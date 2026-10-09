/**
 * HUI - platform/platform.c
 * Selection of the window system compiled into the library.
 */

#include "platform.h"

const hui_platform_t* hui_platform_select(int headless) {
	if (headless) return hui_platform_null();
#if defined(HUI_PLATFORM_WIN32)
	return hui_platform_win32();
#elif defined(HUI_PLATFORM_X11)
	return hui_platform_x11();
#else
	/* no window system for this OS yet: the library still works headless */
	return hui_platform_null();
#endif
}
