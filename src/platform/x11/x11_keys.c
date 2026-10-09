/**
 * HUI - platform/x11/x11_keys.c
 * X11 key symbols and modifier masks to HUI keys.
 */

#include "x11_internal.h"

#include <X11/keysym.h>

HUI_EKey hui_x11_translate_keysym(KeySym symbol) {
	/* Latin letters and digits share their codes with ASCII */
	if (symbol >= XK_a && symbol <= XK_z) return (HUI_EKey)(HUI_KEY_A + (int)(symbol - XK_a));
	if (symbol >= XK_A && symbol <= XK_Z) return (HUI_EKey)(HUI_KEY_A + (int)(symbol - XK_A));
	if (symbol >= XK_0 && symbol <= XK_9) return (HUI_EKey)(HUI_KEY_0 + (int)(symbol - XK_0));
	if (symbol >= XK_F1 && symbol <= XK_F12) return (HUI_EKey)(HUI_KEY_F1 + (int)(symbol - XK_F1));
	if (symbol >= XK_KP_0 && symbol <= XK_KP_9) return (HUI_EKey)(HUI_KEY_KP_0 + (int)(symbol - XK_KP_0));

	switch (symbol) {
		case XK_space: return HUI_KEY_SPACE;
		case XK_apostrophe: return HUI_KEY_APOSTROPHE;
		case XK_comma: return HUI_KEY_COMMA;
		case XK_minus: return HUI_KEY_MINUS;
		case XK_period: return HUI_KEY_PERIOD;
		case XK_slash: return HUI_KEY_SLASH;
		case XK_semicolon: return HUI_KEY_SEMICOLON;
		case XK_equal: return HUI_KEY_EQUAL;
		case XK_bracketleft: return HUI_KEY_LEFT_BRACKET;
		case XK_backslash: return HUI_KEY_BACKSLASH;
		case XK_bracketright: return HUI_KEY_RIGHT_BRACKET;
		case XK_grave: return HUI_KEY_GRAVE;

		case XK_Escape: return HUI_KEY_ESCAPE;
		case XK_Return: return HUI_KEY_ENTER;
		case XK_Tab: return HUI_KEY_TAB;
		case XK_ISO_Left_Tab: return HUI_KEY_TAB;
		case XK_BackSpace: return HUI_KEY_BACKSPACE;
		case XK_Insert: return HUI_KEY_INSERT;
		case XK_Delete: return HUI_KEY_DELETE;
		case XK_Right: return HUI_KEY_RIGHT;
		case XK_Left: return HUI_KEY_LEFT;
		case XK_Down: return HUI_KEY_DOWN;
		case XK_Up: return HUI_KEY_UP;
		case XK_Page_Up: return HUI_KEY_PAGE_UP;
		case XK_Page_Down: return HUI_KEY_PAGE_DOWN;
		case XK_Home: return HUI_KEY_HOME;
		case XK_End: return HUI_KEY_END;
		case XK_Caps_Lock: return HUI_KEY_CAPS_LOCK;
		case XK_Print: return HUI_KEY_PRINT_SCREEN;
		case XK_Pause: return HUI_KEY_PAUSE;
		case XK_Menu: return HUI_KEY_MENU;

		/* keypad: the same keys with and without Num Lock */
		case XK_KP_Insert: return HUI_KEY_KP_0;
		case XK_KP_End: return HUI_KEY_KP_1;
		case XK_KP_Down: return HUI_KEY_KP_2;
		case XK_KP_Page_Down: return HUI_KEY_KP_3;
		case XK_KP_Left: return HUI_KEY_KP_4;
		case XK_KP_Begin: return HUI_KEY_KP_5;
		case XK_KP_Right: return HUI_KEY_KP_6;
		case XK_KP_Home: return HUI_KEY_KP_7;
		case XK_KP_Up: return HUI_KEY_KP_8;
		case XK_KP_Page_Up: return HUI_KEY_KP_9;
		case XK_KP_Decimal: return HUI_KEY_KP_DECIMAL;
		case XK_KP_Delete: return HUI_KEY_KP_DECIMAL;
		case XK_KP_Divide: return HUI_KEY_KP_DIVIDE;
		case XK_KP_Multiply: return HUI_KEY_KP_MULTIPLY;
		case XK_KP_Subtract: return HUI_KEY_KP_SUBTRACT;
		case XK_KP_Add: return HUI_KEY_KP_ADD;
		case XK_KP_Enter: return HUI_KEY_KP_ENTER;

		case XK_Shift_L: return HUI_KEY_LEFT_SHIFT;
		case XK_Control_L: return HUI_KEY_LEFT_CTRL;
		case XK_Alt_L: return HUI_KEY_LEFT_ALT;
		case XK_Meta_L: return HUI_KEY_LEFT_ALT;
		case XK_Super_L: return HUI_KEY_LEFT_SUPER;
		case XK_Shift_R: return HUI_KEY_RIGHT_SHIFT;
		case XK_Control_R: return HUI_KEY_RIGHT_CTRL;
		case XK_Alt_R: return HUI_KEY_RIGHT_ALT;
		case XK_Meta_R: return HUI_KEY_RIGHT_ALT;
		case XK_ISO_Level3_Shift: return HUI_KEY_RIGHT_ALT;
		case XK_Super_R: return HUI_KEY_RIGHT_SUPER;
		default: return HUI_KEY_UNKNOWN;
	}
}

uint32_t hui_x11_translate_modifiers(unsigned int state) {
	uint32_t modifiers = 0;
	if (state & ShiftMask) modifiers |= HUI_MOD_SHIFT;
	if (state & ControlMask) modifiers |= HUI_MOD_CTRL;
	if (state & Mod1Mask) modifiers |= HUI_MOD_ALT;
	if (state & Mod4Mask) modifiers |= HUI_MOD_SUPER;
	return modifiers;
}
