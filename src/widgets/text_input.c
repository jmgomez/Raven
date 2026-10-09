/**
 * HUI - widgets/text_input.c
 * TextInput (Slate SEditableTextBox): single-line text field with caret,
 * selection, clipboard and placeholder.
 */

#include "common.h"

typedef struct hui_text_input_t {
	hui_widget_t base;
	hui_textedit_t edit;
	char* placeholder;
	HUI_CTextChanged changed;
	void* changed_user;
	HUI_CTextChanged committed;
	void* committed_user;
} hui_text_input_t;

static void hui_text_input_init(hui_widget_t* widget) {
	hui_textedit_init(&((hui_text_input_t*)widget)->edit);
	widget->cursor = HUI_CURSOR_IBEAM;
}

static void hui_text_input_destroy(hui_widget_t* widget) {
	hui_text_input_t* input = (hui_text_input_t*)widget;
	hui_textedit_free(&input->edit);
	hui_free(input->placeholder);
}

static hui_rect_t hui_text_input_area(const hui_widget_t* widget) {
	float px = hui_widget_metric(widget, HUI_METRIC_PADDING_X) * 0.7f;
	return hui_rect(px, 0.0f, hui_maxf(0.0f, widget->rect.w - px * 2.0f), widget->rect.h);
}

static hui_vec2_t hui_text_input_measure(hui_widget_t* widget) {
	return hui_vec2(140.0f, hui_widget_metric(widget, HUI_METRIC_CONTROL_HEIGHT));
}

static void hui_text_input_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_text_input_t* input = (hui_text_input_t*)widget;
	hui_rect_t rect = hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h);
	hui_rect_t area = hui_text_input_area(widget);
	int focused = (widget->flags & HUI_WF_FOCUSED) != 0;
	hui_color_t fill = hui_widget_color(widget, HUI_COLOR_INPUT);
	hui_color_t outline = hui_widget_color(widget, focused ? HUI_COLOR_FOCUS : HUI_COLOR_BORDER);

	if (!painter->enabled) fill = hui_color_fade(fill, 0.5f);
	hui_paint_box(painter, rect, hui_widget_metric(widget, HUI_METRIC_CORNER_RADIUS), fill,
		hui_widget_metric(widget, HUI_METRIC_BORDER_WIDTH), outline);
	if (input->edit.length == 0 && input->placeholder && !focused) {
		hui_paint_push_clip(painter, area);
		hui_paint_text_in(painter, HUI_INVALID_ID, hui_widget_font_size(widget, 0.0f), area, 0.0f, input->placeholder, -1,
			hui_widget_color(widget, HUI_COLOR_TEXT_PLACEHOLDER));
		hui_paint_pop_clip(painter);
		return;
	}
	hui_textedit_paint(&input->edit, widget, painter, area, HUI_INVALID_ID, 0.0f,
		hui_widget_text_color(widget, painter), focused);
}

/* Calls back after an edit. The text is copied: the callback may change it. */
static void hui_text_input_notify(hui_widget_t* widget, int flags) {
	hui_text_input_t* input = (hui_text_input_t*)widget;
	char* text;
	if (!(flags & (HUI_TEXTEDIT_CHANGED | HUI_TEXTEDIT_COMMITTED))) return;
	text = hui_strdup(input->edit.text);
	if ((flags & HUI_TEXTEDIT_CHANGED) && input->changed) input->changed(widget->id, text, input->changed_user);
	if ((flags & HUI_TEXTEDIT_COMMITTED) && hui_widget_alive(widget)) {
		if (input->committed) input->committed(widget->id, text, input->committed_user);
		if (hui_widget_alive(widget)) hui_widget_fire_action(widget);
	}
	hui_free(text);
}

static int hui_text_input_event(hui_widget_t* widget, const HUI_Event* event) {
	hui_text_input_t* input = (hui_text_input_t*)widget;
	hui_rect_t area = hui_text_input_area(widget);
	int flags;

	switch (event->type) {
		case HUI_EVENT_FOCUS_GAINED:
			hui_textedit_set_focused(&input->edit, widget, HUI_TRUE);
			/* reaching the field with Tab selects its content, like most toolkits */
			if (widget->window && widget->window->focus_visible) hui_textedit_select_all(&input->edit);
			return HUI_TRUE;
		case HUI_EVENT_FOCUS_LOST:
			hui_textedit_set_focused(&input->edit, widget, HUI_FALSE);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_DOWN:
			if (event->button != HUI_MOUSE_LEFT) return HUI_FALSE;
			hui_textedit_mouse_down(&input->edit, widget, HUI_INVALID_ID, 0.0f, event->local_x - area.x,
				event->clicks, (event->modifiers & HUI_MOD_SHIFT) != 0);
			hui_widget_capture_mouse(widget);
			hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_MOVE:
			if (!input->edit.mouse_selecting) return HUI_FALSE;
			hui_textedit_mouse_drag(&input->edit, widget, HUI_INVALID_ID, 0.0f, event->local_x - area.x);
			hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_UP:
			if (event->button != HUI_MOUSE_LEFT) return HUI_FALSE;
			input->edit.mouse_selecting = HUI_FALSE;
			hui_widget_release_mouse(widget);
			return HUI_TRUE;
		case HUI_EVENT_KEY_DOWN:
			flags = hui_textedit_key(&input->edit, event);
			if (!flags) return HUI_FALSE;
			hui_widget_invalidate_paint(widget);
			hui_text_input_notify(widget, flags);
			return HUI_TRUE;
		case HUI_EVENT_TEXT:
			flags = hui_textedit_input(&input->edit, event->codepoint);
			if (!flags) return HUI_FALSE;
			hui_widget_invalidate_paint(widget);
			hui_text_input_notify(widget, flags);
			return HUI_TRUE;
		default:
			return HUI_FALSE;
	}
}

static int hui_text_input_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_text_input_t* input = (hui_text_input_t*)widget;
	if (HUI_ATTR("Text")) {
		hui_textedit_set_text(&input->edit, value);
	} else if (HUI_ATTR("Placeholder")) {
		hui_str_set(&input->placeholder, value);
	} else if (HUI_ATTR("Password")) {
		if (!hui_parse_bool(value, &input->edit.password)) return HUI_FALSE;
	} else if (HUI_ATTR("ReadOnly")) {
		if (!hui_parse_bool(value, &input->edit.read_only)) return HUI_FALSE;
	} else if (HUI_ATTR("MaxLength")) {
		if (!hui_parse_int(value, &input->edit.max_chars)) return HUI_FALSE;
	} else {
		return HUI_FALSE;
	}
	hui_widget_invalidate_paint(widget);
	return HUI_TRUE;
}

const hui_widget_class_t hui_class_text_input = {
	.type = HUI_WIDGET_TEXT_INPUT,
	.name = "TextInput",
	.size = sizeof(hui_text_input_t),
	.flags = HUI_CF_NO_CHILDREN | HUI_CF_FOCUSABLE,
	.init = hui_text_input_init,
	.destroy = hui_text_input_destroy,
	.measure = hui_text_input_measure,
	.paint = hui_text_input_paint,
	.event = hui_text_input_event,
	.set_attribute = hui_text_input_set_attribute
};

void HUI_SetTextInputText(HUI_id id, const char* text) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_TEXT_INPUT);
	if (hui_textedit_set_text(&((hui_text_input_t*)widget)->edit, text)) hui_widget_invalidate_paint(widget);
}

const char* HUI_GetTextInputText(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_TEXT_INPUT, "");
	return ((hui_text_input_t*)widget)->edit.text;
}

void HUI_SetTextInputPlaceholder(HUI_id id, const char* text) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_TEXT_INPUT);
	if (hui_str_set(&((hui_text_input_t*)widget)->placeholder, text)) hui_widget_invalidate_paint(widget);
}

void HUI_SetTextInputPassword(HUI_id id, int password) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_TEXT_INPUT);
	((hui_text_input_t*)widget)->edit.password = password != 0;
	hui_widget_invalidate_paint(widget);
}

void HUI_SetTextInputReadOnly(HUI_id id, int read_only) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_TEXT_INPUT);
	((hui_text_input_t*)widget)->edit.read_only = read_only != 0;
	hui_widget_invalidate_paint(widget);
}

void HUI_SetTextInputMaxLength(HUI_id id, int max_length) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_TEXT_INPUT);
	((hui_text_input_t*)widget)->edit.max_chars = max_length > 0 ? max_length : 0;
}

void HUI_SelectAllText(HUI_id id) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_TEXT_INPUT);
	hui_textedit_select_all(&((hui_text_input_t*)widget)->edit);
	hui_widget_invalidate_paint(widget);
}

void HUI_SetTextInputChangedCallback(HUI_id id, HUI_CTextChanged callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_TEXT_INPUT);
	((hui_text_input_t*)widget)->changed = callback;
	((hui_text_input_t*)widget)->changed_user = user_data;
}

void HUI_SetTextInputCommittedCallback(HUI_id id, HUI_CTextChanged callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_TEXT_INPUT);
	((hui_text_input_t*)widget)->committed = callback;
	((hui_text_input_t*)widget)->committed_user = user_data;
}
