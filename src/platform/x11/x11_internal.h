/**
 * HUI - platform/x11/x11_internal.h
 * Shared by the files of the X11 platform.
 */

#ifndef HUI_X11_INTERNAL_H
#define HUI_X11_INTERNAL_H

#include "../platform.h"

#include <X11/Xlib.h>

/** Maps an X11 key symbol to a HUI key, or HUI_KEY_UNKNOWN (x11_keys.c). */
HUI_EKey hui_x11_translate_keysym(KeySym symbol);

/** Converts the modifier bits of an X11 event state (x11_keys.c). */
uint32_t hui_x11_translate_modifiers(unsigned int state);

#endif /* HUI_X11_INTERNAL_H */
