/**
 * HUI - widgets/button.c
 * Button (Slate SButton): a clickable frame around one child, usually a label.
 */

#include "common.h"

typedef struct hui_button_t {
	hui_widget_t base;
	HUI_EButtonStyle style;
	/** While pressed: HUI_TRUE when the cursor is still over the button. */
	int armed;
	HUI_CButtonClicked callback;
	void* user_data;
} hui_button_t;

static void hui_button_init(hui_widget_t* widget) {
	((hui_button_t*)widget)->style = HUI_BUTTON_DEFAULT;
}

static hui_margin_t hui_button_padding(const hui_widget_t* widget) {
	float px = hui_widget_metric(widget, HUI_METRIC_PADDING_X), py = hui_widget_metric(widget, HUI_METRIC_PADDING_Y);
	return hui_margin(px, py, px, py);
}

static hui_vec2_t hui_button_measure(hui_widget_t* widget) {
	hui_margin_t padding = hui_button_padding(widget);
	hui_vec2_t desired = hui_vec2(0.0f, 0.0f);
	if (widget->child_count > 0) desired = hui_widget_measure_slot(widget->children[0]);
	desired.x += padding.l + padding.r;
	desired.y = hui_maxf(desired.y + padding.t + padding.b, hui_widget_metric(widget, HUI_METRIC_CONTROL_HEIGHT));
	return desired;
}

static void hui_button_arrange(hui_widget_t* widget) {
	if (widget->child_count > 0 && !hui_widget_collapsed(widget->children[0])) {
		hui_widget_arrange_slot(widget->children[0],
			hui_rect_inset(hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h), hui_button_padding(widget)));
	}
}

static void hui_button_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_button_t* button = (hui_button_t*)widget;
	hui_rect_t rect = hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h);
	float radius = hui_widget_metric(widget, HUI_METRIC_CORNER_RADIUS);
	int pressed = (widget->flags & HUI_WF_PRESSED) && button->armed;
	int hovered = (widget->flags & HUI_WF_HOVERED) && painter->enabled;
	hui_color_t fill, outline = 0;

	if (button->style == HUI_BUTTON_PRIMARY) {
		fill = hui_widget_color(widget, pressed ? HUI_COLOR_ACCENT_PRESSED : (hovered ? HUI_COLOR_ACCENT_HOVERED : HUI_COLOR_ACCENT));
		if (!painter->enabled) fill = hui_color_fade(fill, 0.4f);
		/* the content of a primary button is drawn in the accent text color */
		if (painter->enabled) painter->text_color = hui_widget_color(widget, HUI_COLOR_ACCENT_TEXT);
	} else if (button->style == HUI_BUTTON_FLAT) {
		fill = pressed ? hui_widget_color(widget, HUI_COLOR_CONTROL_PRESSED)
			: (hovered ? hui_widget_color(widget, HUI_COLOR_CONTROL_HOVERED) : 0);
	} else {
		fill = hui_widget_color(widget, pressed ? HUI_COLOR_CONTROL_PRESSED : (hovered ? HUI_COLOR_CONTROL_HOVERED : HUI_COLOR_CONTROL));
		outline = hui_widget_color(widget, HUI_COLOR_CONTROL_BORDER);
		if (!painter->enabled) {
			fill = hui_color_fade(fill, 0.5f);
			outline = hui_color_fade(outline, 0.5f);
		}
	}
	hui_paint_box(painter, rect, radius, fill, outline ? hui_widget_metric(widget, HUI_METRIC_BORDER_WIDTH) : 0.0f, outline);
}

static void hui_button_paint_overlay(hui_widget_t* widget, HUI_Painter* painter) {
	hui_paint_focus(widget, painter, hui_widget_metric(widget, HUI_METRIC_CORNER_RADIUS));
}

static void hui_button_click(hui_widget_t* widget) {
	hui_button_t* button = (hui_button_t*)widget;
	if (button->callback) button->callback(widget->id, button->user_data);
	if (hui_widget_alive(widget)) hui_widget_fire_action(widget);
}

static int hui_button_event(hui_widget_t* widget, const HUI_Event* event) {
	hui_button_t* button = (hui_button_t*)widget;
	int inside = event->local_x >= 0.0f && event->local_y >= 0.0f &&
		event->local_x < widget->rect.w && event->local_y < widget->rect.h;

	switch (event->type) {
		case HUI_EVENT_MOUSE_DOWN:
			if (event->button != HUI_MOUSE_LEFT) return HUI_FALSE;
			widget->flags |= HUI_WF_PRESSED;
			button->armed = HUI_TRUE;
			hui_widget_capture_mouse(widget);
			hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_MOVE:
			if (!(widget->flags & HUI_WF_PRESSED)) return HUI_FALSE;
			if (button->armed != inside) {
				button->armed = inside;
				hui_widget_invalidate_paint(widget);
			}
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_UP:
			if (event->button != HUI_MOUSE_LEFT || !(widget->flags & HUI_WF_PRESSED)) return HUI_FALSE;
			widget->flags &= ~HUI_WF_PRESSED;
			hui_widget_release_mouse(widget);
			hui_widget_invalidate_paint(widget);
			/* a click is a press and a release on the button */
			if (inside) hui_button_click(widget);
			return HUI_TRUE;
		case HUI_EVENT_KEY_DOWN:
			if (!(widget->flags & HUI_WF_FOCUSED) || event->repeat) return HUI_FALSE;
			if (event->key == HUI_KEY_ENTER || event->key == HUI_KEY_KP_ENTER || event->key == HUI_KEY_SPACE) {
				hui_button_click(widget);
				return HUI_TRUE;
			}
			return HUI_FALSE;
		default:
			return HUI_FALSE;
	}
}

static void hui_button_set_text(hui_widget_t* widget, const char* text) {
	hui_widget_t* label;
	/* reuse the label when the content already is one */
	if (widget->child_count > 0 && widget->children[0]->cls->type == HUI_WIDGET_LABEL) {
		hui_widget_set_attribute(widget->children[0], "Text", text ? text : "");
		return;
	}
	label = hui_widget_create(HUI_WIDGET_LABEL);
	if (!label) return;
	hui_widget_set_attribute(label, "Text", text ? text : "");
	label->slot.halign = HUI_HALIGN_CENTER;
	label->slot.valign = HUI_VALIGN_CENTER;
	hui_widget_add_child(widget, label, -1);
}

static int hui_button_parse_style(const char* value, HUI_EButtonStyle* style) {
	if (hui_stricmp(value, "Default") == 0) *style = HUI_BUTTON_DEFAULT;
	else if (hui_stricmp(value, "Primary") == 0) *style = HUI_BUTTON_PRIMARY;
	else if (hui_stricmp(value, "Flat") == 0) *style = HUI_BUTTON_FLAT;
	else return HUI_FALSE;
	return HUI_TRUE;
}

static int hui_button_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_button_t* button = (hui_button_t*)widget;
	if (HUI_ATTR("Text")) {
		hui_button_set_text(widget, value);
		return HUI_TRUE;
	}
	if (HUI_ATTR("Style")) {
		if (!hui_button_parse_style(value, &button->style)) return HUI_FALSE;
		hui_widget_invalidate_paint(widget);
		return HUI_TRUE;
	}
	return HUI_FALSE;
}

const hui_widget_class_t hui_class_button = {
	.type = HUI_WIDGET_BUTTON,
	.name = "Button",
	.size = sizeof(hui_button_t),
	.flags = HUI_CF_SINGLE_CHILD | HUI_CF_FOCUSABLE,
	.init = hui_button_init,
	.measure = hui_button_measure,
	.arrange = hui_button_arrange,
	.paint = hui_button_paint,
	.paint_overlay = hui_button_paint_overlay,
	.event = hui_button_event,
	.set_attribute = hui_button_set_attribute
};

HUI_id HUI_CreateButton(const char* text) {
	hui_widget_t* widget;
	HUI_API_GUARD(HUI_INVALID_ID);
	widget = hui_widget_create(HUI_WIDGET_BUTTON);
	if (!widget) return HUI_INVALID_ID;
	if (text) hui_button_set_text(widget, text);
	return widget->id;
}

void HUI_SetButtonText(HUI_id id, const char* text) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_BUTTON);
	hui_button_set_text(widget, text);
}

void HUI_SetButtonStyle(HUI_id id, HUI_EButtonStyle style) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_BUTTON);
	if (style < HUI_BUTTON_DEFAULT || style > HUI_BUTTON_FLAT) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetButtonStyle: invalid style");
		return;
	}
	((hui_button_t*)widget)->style = style;
	hui_widget_invalidate_paint(widget);
}

void HUI_SetButtonClickedCallback(HUI_id id, HUI_CButtonClicked callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_BUTTON);
	((hui_button_t*)widget)->callback = callback;
	((hui_button_t*)widget)->user_data = user_data;
}
