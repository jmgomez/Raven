/**
 * HUI - widgets/textedit.c
 * Single-line text editing model: buffer, caret, selection, clipboard and
 * painting. TextInput and SpinBox embed one.
 */

#include "common.h"

#define HUI_BULLET "\xE2\x80\xA2" /* U+2022, shown instead of password characters */

void hui_textedit_init(hui_textedit_t* edit) {
	memset(edit, 0, sizeof(*edit));
	edit->cap = 32;
	edit->text = (char*)hui_alloc((size_t)edit->cap);
	edit->text[0] = '\0';
	edit->blink_timer = HUI_INVALID_ID;
}

void hui_textedit_free(hui_textedit_t* edit) {
	if (edit->blink_timer != HUI_INVALID_ID && HUI_IsValidTimer(edit->blink_timer)) HUI_DeleteTimer(edit->blink_timer);
	hui_free(edit->text);
	memset(edit, 0, sizeof(*edit));
	edit->blink_timer = HUI_INVALID_ID;
}

/* ---------------------------------------------------------------------------
 *  Buffer
 * ------------------------------------------------------------------------- */

static int hui_te_min(const hui_textedit_t* edit) { return hui_mini(edit->caret, edit->anchor); }
static int hui_te_max(const hui_textedit_t* edit) { return hui_maxi(edit->caret, edit->anchor); }
static int hui_te_has_selection(const hui_textedit_t* edit) { return edit->caret != edit->anchor; }

static void hui_te_delete(hui_textedit_t* edit, int from, int to) {
	if (to <= from) return;
	memmove(edit->text + from, edit->text + to, (size_t)(edit->length - to) + 1);
	edit->length -= to - from;
	edit->caret = edit->anchor = from;
}

/* Inserts at the caret, replacing the selection. Returns HUI_TRUE when something was inserted. */
static int hui_te_insert(hui_textedit_t* edit, const char* text, int length) {
	if (hui_te_has_selection(edit)) hui_te_delete(edit, hui_te_min(edit), hui_te_max(edit));
	if (edit->max_chars > 0) {
		/* cut the insertion at a character boundary */
		int room = edit->max_chars - hui_utf8_count(edit->text, edit->length);
		int fit = 0;
		while (room > 0 && fit < length) {
			fit = hui_utf8_next(text, length, fit);
			room--;
		}
		length = fit;
	}
	if (length <= 0) return HUI_FALSE;
	if (edit->length + length + 1 > edit->cap) {
		while (edit->length + length + 1 > edit->cap) edit->cap *= 2;
		edit->text = (char*)hui_realloc(edit->text, (size_t)edit->cap);
	}
	memmove(edit->text + edit->caret + length, edit->text + edit->caret, (size_t)(edit->length - edit->caret) + 1);
	memcpy(edit->text + edit->caret, text, (size_t)length);
	edit->length += length;
	edit->caret += length;
	edit->anchor = edit->caret;
	return HUI_TRUE;
}

int hui_textedit_set_text(hui_textedit_t* edit, const char* text) {
	int length;
	if (!text) text = "";
	if (strcmp(edit->text, text) == 0) return HUI_FALSE;
	length = (int)strlen(text);
	if (length + 1 > edit->cap) {
		while (length + 1 > edit->cap) edit->cap *= 2;
		edit->text = (char*)hui_realloc(edit->text, (size_t)edit->cap);
	}
	memcpy(edit->text, text, (size_t)length + 1);
	edit->length = length;
	edit->caret = edit->anchor = length;
	edit->scroll = 0.0f;
	return HUI_TRUE;
}

void hui_textedit_select_all(hui_textedit_t* edit) {
	edit->anchor = 0;
	edit->caret = edit->length;
}

static int hui_te_is_space(char c) {
	return c == ' ' || c == '\t';
}

static int hui_te_word_left(const hui_textedit_t* edit, int offset) {
	while (offset > 0 && hui_te_is_space(edit->text[offset - 1])) offset--;
	while (offset > 0 && !hui_te_is_space(edit->text[offset - 1])) offset--;
	return offset;
}

static int hui_te_word_right(const hui_textedit_t* edit, int offset) {
	while (offset < edit->length && !hui_te_is_space(edit->text[offset])) offset++;
	while (offset < edit->length && hui_te_is_space(edit->text[offset])) offset++;
	return offset;
}

/* ---------------------------------------------------------------------------
 *  Focus and caret blinking
 * ------------------------------------------------------------------------- */

static void hui_te_blink(HUI_id timer, void* user_data) {
	hui_widget_t* widget = hui_widget_get((HUI_id)(uintptr_t)user_data);
	HUI_UNUSED(timer);
	if (widget) hui_widget_invalidate_paint(widget);
}

void hui_textedit_set_focused(hui_textedit_t* edit, hui_widget_t* widget, int focused) {
	if (focused) {
		edit->blink_start = hui_clock_seconds();
		if (edit->blink_timer == HUI_INVALID_ID) {
			edit->blink_timer = HUI_CreateTimer(0.5, HUI_TRUE, hui_te_blink, (void*)(uintptr_t)widget->id);
		}
	} else {
		if (edit->blink_timer != HUI_INVALID_ID) {
			if (HUI_IsValidTimer(edit->blink_timer)) HUI_DeleteTimer(edit->blink_timer);
			edit->blink_timer = HUI_INVALID_ID;
		}
		edit->mouse_selecting = HUI_FALSE;
		edit->anchor = edit->caret;
		edit->scroll = 0.0f;
	}
	hui_widget_invalidate_paint(widget);
}

/* ---------------------------------------------------------------------------
 *  Keyboard
 * ------------------------------------------------------------------------- */

static void hui_te_copy(const hui_textedit_t* edit) {
	char* copy;
	if (edit->password || !hui_te_has_selection(edit)) return;
	copy = hui_strndup(edit->text + hui_te_min(edit), (size_t)(hui_te_max(edit) - hui_te_min(edit)));
	HUI_SetClipboardText(copy);
	hui_free(copy);
}

static int hui_te_paste(hui_textedit_t* edit) {
	const char* clip = HUI_GetClipboardText();
	char* line;
	size_t i, length;
	int changed;
	if (!clip || !clip[0]) return HUI_FALSE;
	/* a single-line editor keeps the first line and turns tabs into spaces */
	length = strcspn(clip, "\r\n");
	line = hui_strndup(clip, length);
	for (i = 0; i < length; i++) {
		if ((unsigned char)line[i] < 32) line[i] = ' ';
	}
	changed = hui_te_insert(edit, line, (int)length);
	hui_free(line);
	return changed;
}

int hui_textedit_key(hui_textedit_t* edit, const HUI_Event* event) {
	int shift = (event->modifiers & HUI_MOD_SHIFT) != 0;
	int command = (event->modifiers & (HUI_MOD_CTRL | HUI_MOD_SUPER)) != 0;
	int alt = (event->modifiers & HUI_MOD_ALT) != 0;
	int result = HUI_TEXTEDIT_HANDLED;
	int target;

	if (event->type != HUI_EVENT_KEY_DOWN) return 0;

	switch (event->key) {
		case HUI_KEY_LEFT:
			if (!shift && hui_te_has_selection(edit)) target = hui_te_min(edit);
			else target = command ? hui_te_word_left(edit, edit->caret) : hui_utf8_prev(edit->text, edit->caret);
			edit->caret = target;
			if (!shift) edit->anchor = target;
			break;
		case HUI_KEY_RIGHT:
			if (!shift && hui_te_has_selection(edit)) target = hui_te_max(edit);
			else target = command ? hui_te_word_right(edit, edit->caret) : hui_utf8_next(edit->text, edit->length, edit->caret);
			edit->caret = target;
			if (!shift) edit->anchor = target;
			break;
		case HUI_KEY_HOME:
			edit->caret = 0;
			if (!shift) edit->anchor = 0;
			break;
		case HUI_KEY_END:
			edit->caret = edit->length;
			if (!shift) edit->anchor = edit->length;
			break;
		case HUI_KEY_BACKSPACE:
			if (edit->read_only) break;
			if (hui_te_has_selection(edit)) {
				hui_te_delete(edit, hui_te_min(edit), hui_te_max(edit));
				result |= HUI_TEXTEDIT_CHANGED;
			} else if (edit->caret > 0) {
				hui_te_delete(edit, command ? hui_te_word_left(edit, edit->caret) : hui_utf8_prev(edit->text, edit->caret), edit->caret);
				result |= HUI_TEXTEDIT_CHANGED;
			}
			break;
		case HUI_KEY_DELETE:
			if (edit->read_only) break;
			if (hui_te_has_selection(edit)) {
				hui_te_delete(edit, hui_te_min(edit), hui_te_max(edit));
				result |= HUI_TEXTEDIT_CHANGED;
			} else if (edit->caret < edit->length) {
				hui_te_delete(edit, edit->caret, command ? hui_te_word_right(edit, edit->caret) : hui_utf8_next(edit->text, edit->length, edit->caret));
				result |= HUI_TEXTEDIT_CHANGED;
			}
			break;
		case HUI_KEY_ENTER:
		case HUI_KEY_KP_ENTER:
			result |= HUI_TEXTEDIT_COMMITTED;
			break;
		case HUI_KEY_A:
			if (!command) goto printable;
			hui_textedit_select_all(edit);
			break;
		case HUI_KEY_C:
			if (!command) goto printable;
			hui_te_copy(edit);
			break;
		case HUI_KEY_X:
			if (!command) goto printable;
			hui_te_copy(edit);
			if (!edit->read_only && !edit->password && hui_te_has_selection(edit)) {
				hui_te_delete(edit, hui_te_min(edit), hui_te_max(edit));
				result |= HUI_TEXTEDIT_CHANGED;
			}
			break;
		case HUI_KEY_V:
			if (!command) goto printable;
			if (!edit->read_only && hui_te_paste(edit)) result |= HUI_TEXTEDIT_CHANGED;
			break;
		default:
		printable:
			/* keys that type a character are consumed so that they never trigger
			 * a window shortcut; the character itself arrives as a text event */
			if (command || alt || (int)event->key >= 256 || event->key == HUI_KEY_UNKNOWN) return 0;
			return HUI_TEXTEDIT_HANDLED;
	}
	edit->blink_start = hui_clock_seconds();
	return result;
}

int hui_textedit_input(hui_textedit_t* edit, uint32_t codepoint) {
	char encoded[4];
	int length;
	if (edit->read_only || codepoint < 32 || codepoint == 127) return 0;
	length = hui_utf8_encode(codepoint, encoded);
	edit->blink_start = hui_clock_seconds();
	return hui_te_insert(edit, encoded, length) ? (HUI_TEXTEDIT_HANDLED | HUI_TEXTEDIT_CHANGED) : HUI_TEXTEDIT_HANDLED;
}

/* ---------------------------------------------------------------------------
 *  Geometry (password mode shows one bullet per character)
 * ------------------------------------------------------------------------- */

static float hui_te_offset_to_x(const hui_textedit_t* edit, hui_font_t* font, float size, int offset) {
	if (edit->password) {
		return hui_text_width(font, size, HUI_BULLET, -1) * (float)hui_utf8_count(edit->text, offset);
	}
	return hui_text_width(font, size, edit->text, offset);
}

static int hui_te_x_to_offset(const hui_textedit_t* edit, hui_font_t* font, float size, float x) {
	if (edit->password) {
		float bullet = hui_text_width(font, size, HUI_BULLET, -1);
		int count = hui_utf8_count(edit->text, edit->length);
		int index = bullet > 0.0f ? hui_clampi((int)(x / bullet + 0.5f), 0, count) : 0;
		int offset = 0;
		while (index-- > 0) offset = hui_utf8_next(edit->text, edit->length, offset);
		return offset;
	}
	return hui_text_offset_at(font, size, edit->text, edit->length, x);
}

void hui_textedit_mouse_down(hui_textedit_t* edit, const hui_widget_t* widget, HUI_id font_id, float size,
	float x, int clicks, int extend) {
	hui_font_t* font = hui_font_resolve(font_id);
	int offset;
	if (!font) return;
	size = hui_widget_font_size(widget, size);
	offset = hui_te_x_to_offset(edit, font, size, x + edit->scroll);
	if (clicks >= 3) {
		hui_textedit_select_all(edit);
	} else if (clicks == 2 && !edit->password) {
		edit->anchor = hui_te_word_left(edit, hui_mini(offset + 1, edit->length));
		edit->caret = hui_te_word_right(edit, edit->anchor);
		/* a double click selects the word, not the spaces after it */
		while (edit->caret > edit->anchor && hui_te_is_space(edit->text[edit->caret - 1])) edit->caret--;
	} else {
		edit->caret = offset;
		if (!extend) edit->anchor = offset;
		edit->mouse_selecting = HUI_TRUE;
	}
	edit->blink_start = hui_clock_seconds();
}

void hui_textedit_mouse_drag(hui_textedit_t* edit, const hui_widget_t* widget, HUI_id font_id, float size, float x) {
	hui_font_t* font = hui_font_resolve(font_id);
	if (!font || !edit->mouse_selecting) return;
	size = hui_widget_font_size(widget, size);
	edit->caret = hui_te_x_to_offset(edit, font, size, x + edit->scroll);
}

/* ---------------------------------------------------------------------------
 *  Paint
 * ------------------------------------------------------------------------- */

void hui_textedit_paint(hui_textedit_t* edit, const hui_widget_t* widget, HUI_Painter* painter,
	hui_rect_t area, HUI_id font_id, float size, hui_color_t color, int focused) {

	hui_font_t* font = hui_font_resolve(font_id);
	hui_font_metrics_t metrics;
	float caret_x, text_width, max_scroll, text_y;

	if (!font) return;
	size = hui_widget_font_size(widget, size);
	hui_font_metrics(font, size, &metrics);
	text_y = area.y + (area.h - metrics.line_height) * 0.5f;
	caret_x = hui_te_offset_to_x(edit, font, size, edit->caret);
	text_width = hui_te_offset_to_x(edit, font, size, edit->length);

	/* scroll horizontally so that the caret stays visible */
	if (focused) {
		if (caret_x - edit->scroll > area.w - 1.0f) edit->scroll = caret_x - area.w + 1.0f;
		if (caret_x - edit->scroll < 0.0f) edit->scroll = caret_x;
	}
	max_scroll = hui_maxf(0.0f, text_width - area.w + 1.0f);
	edit->scroll = hui_clampf(edit->scroll, 0.0f, max_scroll);

	hui_paint_push_clip(painter, area);
	if (focused && hui_te_has_selection(edit)) {
		float x0 = hui_te_offset_to_x(edit, font, size, hui_te_min(edit));
		float x1 = hui_te_offset_to_x(edit, font, size, hui_te_max(edit));
		hui_paint_rect(painter, hui_rect(area.x + x0 - edit->scroll, text_y, x1 - x0, metrics.line_height),
			hui_widget_color(widget, HUI_COLOR_SELECTION));
	}
	if (edit->password) {
		int count = hui_utf8_count(edit->text, edit->length), i;
		char* bullets = (char*)hui_alloc((size_t)count * 3 + 1);
		for (i = 0; i < count; i++) memcpy(bullets + i * 3, HUI_BULLET, 3);
		bullets[count * 3] = '\0';
		hui_paint_text(painter, font_id, size, area.x - edit->scroll, text_y, bullets, count * 3, color);
		hui_free(bullets);
	} else {
		hui_paint_text(painter, font_id, size, area.x - edit->scroll, text_y, edit->text, edit->length, color);
	}
	if (focused && !edit->read_only && fmod(hui_clock_seconds() - edit->blink_start, 1.0) < 0.5) {
		hui_paint_rect(painter, hui_rect(area.x + caret_x - edit->scroll, text_y, 1.0f, metrics.line_height), color);
	}
	hui_paint_pop_clip(painter);
}
