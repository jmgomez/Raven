/**
 * HUI - widgets/checkbox.c
 * Checkbox (Slate SCheckBox): a two-state mark with a text, drawn as a check
 * box, a radio button or an on / off switch.
 */

#include "common.h"

typedef struct hui_checkbox_t {
	hui_widget_t base;
	char* text;
	int checked;
	int armed;
	HUI_ECheckboxStyle style;
	HUI_CCheckboxChanged callback;
	void* user_data;
} hui_checkbox_t;

static void hui_checkbox_init(hui_widget_t* widget) {
	((hui_checkbox_t*)widget)->style = HUI_CHECKBOX_CHECK;
}

static void hui_checkbox_destroy(hui_widget_t* widget) {
	hui_free(((hui_checkbox_t*)widget)->text);
}

/* Size of the mark: a square, or a wider pill for the switch style. */
static hui_vec2_t hui_checkbox_mark(const hui_widget_t* widget) {
	float size = hui_widget_metric(widget, HUI_METRIC_CHECK_SIZE);
	if (((const hui_checkbox_t*)widget)->style == HUI_CHECKBOX_SWITCH) return hui_vec2(floorf(size * 1.9f), size);
	return hui_vec2(size, size);
}

static hui_vec2_t hui_checkbox_measure(hui_widget_t* widget) {
	hui_checkbox_t* checkbox = (hui_checkbox_t*)widget;
	hui_vec2_t mark = hui_checkbox_mark(widget);
	hui_vec2_t desired = mark;
	if (checkbox->text && checkbox->text[0]) {
		desired.x += hui_widget_metric(widget, HUI_METRIC_SPACING) +
			ceilf(hui_widget_text_width(widget, HUI_INVALID_ID, 0.0f, checkbox->text));
		desired.y = hui_maxf(desired.y, ceilf(hui_widget_line_height(widget, HUI_INVALID_ID, 0.0f)));
	}
	desired.y = hui_maxf(desired.y, hui_widget_metric(widget, HUI_METRIC_ROW_HEIGHT));
	return desired;
}

static void hui_checkbox_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_checkbox_t* checkbox = (hui_checkbox_t*)widget;
	hui_vec2_t mark = hui_checkbox_mark(widget);
	hui_rect_t box = hui_rect(0.0f, floorf((widget->rect.h - mark.y) * 0.5f), mark.x, mark.y);
	int hovered = (widget->flags & HUI_WF_HOVERED) && painter->enabled;
	int pressed = (widget->flags & HUI_WF_PRESSED) && checkbox->armed;
	hui_color_t accent = hui_widget_color(widget, hovered ? HUI_COLOR_ACCENT_HOVERED : HUI_COLOR_ACCENT);
	hui_color_t off = hui_widget_color(widget, pressed ? HUI_COLOR_CONTROL_PRESSED : (hovered ? HUI_COLOR_CONTROL_HOVERED : HUI_COLOR_CONTROL));
	hui_color_t outline = hui_widget_color(widget, HUI_COLOR_CONTROL_BORDER);
	hui_color_t on_mark = hui_widget_color(widget, HUI_COLOR_ACCENT_TEXT);
	float border = hui_widget_metric(widget, HUI_METRIC_BORDER_WIDTH);

	if (!painter->enabled) {
		accent = hui_color_fade(accent, 0.4f);
		off = hui_color_fade(off, 0.5f);
		outline = hui_color_fade(outline, 0.5f);
	}

	switch (checkbox->style) {
		case HUI_CHECKBOX_RADIO:
			hui_paint_box(painter, box, mark.y * 0.5f, checkbox->checked ? accent : off, checkbox->checked ? 0.0f : border, outline);
			if (checkbox->checked) {
				float dot = floorf(mark.y * 0.4f);
				hui_paint_box(painter, hui_rect(box.x + (mark.x - dot) * 0.5f, box.y + (mark.y - dot) * 0.5f, dot, dot),
					dot * 0.5f, on_mark, 0.0f, 0);
			}
			break;
		case HUI_CHECKBOX_SWITCH: {
			float knob = mark.y - 4.0f;
			float knob_x = checkbox->checked ? box.x + mark.x - knob - 2.0f : box.x + 2.0f;
			hui_paint_box(painter, box, mark.y * 0.5f, checkbox->checked ? accent : off, checkbox->checked ? 0.0f : border, outline);
			hui_paint_box(painter, hui_rect(knob_x, box.y + 2.0f, knob, knob), knob * 0.5f,
				checkbox->checked ? on_mark : hui_widget_color(widget, HUI_COLOR_HANDLE_HOVERED), 0.0f, 0);
			break;
		}
		case HUI_CHECKBOX_CHECK:
		default:
			hui_paint_box(painter, box, 3.0f, checkbox->checked ? accent : off, checkbox->checked ? 0.0f : border, outline);
			if (checkbox->checked) {
				hui_paint_icon(painter, HUI_ICON_CHECK, box.x + mark.x * 0.5f, box.y + mark.y * 0.5f, mark.y * 0.55f, on_mark);
			}
			break;
	}
	if (checkbox->text && checkbox->text[0]) {
		float x = mark.x + hui_widget_metric(widget, HUI_METRIC_SPACING);
		hui_paint_text_in(painter, HUI_INVALID_ID, hui_widget_font_size(widget, 0.0f),
			hui_rect(x, 0.0f, hui_maxf(0.0f, widget->rect.w - x), widget->rect.h), 0.0f, checkbox->text, -1,
			hui_widget_text_color(widget, painter));
	}
}

static void hui_checkbox_paint_overlay(hui_widget_t* widget, HUI_Painter* painter) {
	hui_paint_focus(widget, painter, 3.0f);
}

static void hui_checkbox_set(hui_widget_t* widget, int checked, int notify) {
	hui_checkbox_t* checkbox = (hui_checkbox_t*)widget;
	checked = checked != 0;
	if (checkbox->checked == checked) return;
	checkbox->checked = checked;
	hui_widget_invalidate_paint(widget);

	/* radio boxes of one parent are exclusive */
	if (checked && checkbox->style == HUI_CHECKBOX_RADIO && widget->parent) {
		uint32_t i;
		for (i = 0; i < widget->parent->child_count; i++) {
			hui_widget_t* sibling = widget->parent->children[i];
			if (sibling != widget && sibling->cls->type == HUI_WIDGET_CHECKBOX &&
				((hui_checkbox_t*)sibling)->style == HUI_CHECKBOX_RADIO) {
				hui_checkbox_set(sibling, HUI_FALSE, notify);
			}
		}
	}
	if (notify && hui_widget_alive(widget)) {
		if (checkbox->callback) checkbox->callback(widget->id, checked, checkbox->user_data);
		if (hui_widget_alive(widget)) hui_widget_fire_action(widget);
	}
}

static void hui_checkbox_toggle(hui_widget_t* widget) {
	hui_checkbox_t* checkbox = (hui_checkbox_t*)widget;
	/* a radio box can only be unchecked by checking another one */
	if (checkbox->style == HUI_CHECKBOX_RADIO && checkbox->checked) return;
	hui_checkbox_set(widget, !checkbox->checked, HUI_TRUE);
}

static int hui_checkbox_event(hui_widget_t* widget, const HUI_Event* event) {
	hui_checkbox_t* checkbox = (hui_checkbox_t*)widget;
	int inside = event->local_x >= 0.0f && event->local_y >= 0.0f &&
		event->local_x < widget->rect.w && event->local_y < widget->rect.h;

	switch (event->type) {
		case HUI_EVENT_MOUSE_DOWN:
			if (event->button != HUI_MOUSE_LEFT) return HUI_FALSE;
			widget->flags |= HUI_WF_PRESSED;
			checkbox->armed = HUI_TRUE;
			hui_widget_capture_mouse(widget);
			hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_MOVE:
			if (!(widget->flags & HUI_WF_PRESSED)) return HUI_FALSE;
			if (checkbox->armed != inside) {
				checkbox->armed = inside;
				hui_widget_invalidate_paint(widget);
			}
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_UP:
			if (event->button != HUI_MOUSE_LEFT || !(widget->flags & HUI_WF_PRESSED)) return HUI_FALSE;
			widget->flags &= ~HUI_WF_PRESSED;
			hui_widget_release_mouse(widget);
			hui_widget_invalidate_paint(widget);
			if (inside) hui_checkbox_toggle(widget);
			return HUI_TRUE;
		case HUI_EVENT_KEY_DOWN:
			if (!(widget->flags & HUI_WF_FOCUSED) || event->repeat) return HUI_FALSE;
			if (event->key == HUI_KEY_SPACE || event->key == HUI_KEY_ENTER) {
				hui_checkbox_toggle(widget);
				return HUI_TRUE;
			}
			return HUI_FALSE;
		default:
			return HUI_FALSE;
	}
}

static int hui_checkbox_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_checkbox_t* checkbox = (hui_checkbox_t*)widget;
	int flag;
	if (HUI_ATTR("Text")) {
		hui_str_set(&checkbox->text, value);
	} else if (HUI_ATTR("Checked")) {
		if (!hui_parse_bool(value, &flag)) return HUI_FALSE;
		hui_checkbox_set(widget, flag, HUI_FALSE);
	} else if (HUI_ATTR("Style")) {
		if (hui_stricmp(value, "Check") == 0) checkbox->style = HUI_CHECKBOX_CHECK;
		else if (hui_stricmp(value, "Radio") == 0) checkbox->style = HUI_CHECKBOX_RADIO;
		else if (hui_stricmp(value, "Switch") == 0) checkbox->style = HUI_CHECKBOX_SWITCH;
		else return HUI_FALSE;
	} else {
		return HUI_FALSE;
	}
	hui_widget_invalidate_layout(widget);
	return HUI_TRUE;
}

const hui_widget_class_t hui_class_checkbox = {
	.type = HUI_WIDGET_CHECKBOX,
	.name = "Checkbox",
	.size = sizeof(hui_checkbox_t),
	.flags = HUI_CF_NO_CHILDREN | HUI_CF_FOCUSABLE,
	.init = hui_checkbox_init,
	.destroy = hui_checkbox_destroy,
	.measure = hui_checkbox_measure,
	.paint = hui_checkbox_paint,
	.paint_overlay = hui_checkbox_paint_overlay,
	.event = hui_checkbox_event,
	.set_attribute = hui_checkbox_set_attribute
};

HUI_id HUI_CreateCheckbox(const char* text) {
	hui_widget_t* widget;
	HUI_API_GUARD(HUI_INVALID_ID);
	widget = hui_widget_create(HUI_WIDGET_CHECKBOX);
	if (!widget) return HUI_INVALID_ID;
	((hui_checkbox_t*)widget)->text = hui_strdup(text);
	return widget->id;
}

void HUI_SetCheckboxText(HUI_id id, const char* text) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_CHECKBOX);
	if (hui_str_set(&((hui_checkbox_t*)widget)->text, text)) hui_widget_invalidate_layout(widget);
}

void HUI_SetCheckboxChecked(HUI_id id, int checked) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_CHECKBOX);
	hui_checkbox_set(widget, checked, HUI_FALSE);
}

int HUI_IsCheckboxChecked(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_CHECKBOX, HUI_FALSE);
	return ((hui_checkbox_t*)widget)->checked;
}

void HUI_SetCheckboxStyle(HUI_id id, HUI_ECheckboxStyle style) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_CHECKBOX);
	if (style < HUI_CHECKBOX_CHECK || style > HUI_CHECKBOX_SWITCH) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetCheckboxStyle: invalid style");
		return;
	}
	((hui_checkbox_t*)widget)->style = style;
	hui_widget_invalidate_layout(widget);
}

void HUI_SetCheckboxChangedCallback(HUI_id id, HUI_CCheckboxChanged callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_CHECKBOX);
	((hui_checkbox_t*)widget)->callback = callback;
	((hui_checkbox_t*)widget)->user_data = user_data;
}
