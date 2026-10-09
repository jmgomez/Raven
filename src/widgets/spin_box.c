/**
 * HUI - widgets/spin_box.c
 * SpinBox (Slate SSpinBox): numeric field. Drag it sideways to scrub the
 * value, click it to type, use the arrow keys or the wheel to step.
 */

#include "common.h"

#define HUI_SPIN_DRAG_THRESHOLD 3.0f
#define HUI_SPIN_PIXELS_PER_STEP 4.0f

typedef struct hui_spin_box_t {
	hui_widget_t base;
	hui_textedit_t edit;
	double value;
	double minimum;
	double maximum;
	double step;
	int decimals;
	int editing;
	int dragging;
	float press_x;
	double press_value;
	HUI_CSpinBoxChanged callback;
	void* user_data;
} hui_spin_box_t;

static void hui_spin_box_init(hui_widget_t* widget) {
	hui_spin_box_t* spin = (hui_spin_box_t*)widget;
	hui_textedit_init(&spin->edit);
	spin->minimum = -1e30;
	spin->maximum = 1e30;
	spin->step = 1.0;
	spin->decimals = 2;
	widget->cursor = HUI_CURSOR_RESIZE_H;
}

static void hui_spin_box_destroy(hui_widget_t* widget) {
	hui_textedit_free(&((hui_spin_box_t*)widget)->edit);
}

static void hui_spin_box_format(const hui_spin_box_t* spin, char* buffer, size_t size) {
	snprintf(buffer, size, "%.*f", spin->decimals, spin->value);
}

static void hui_spin_box_set(hui_widget_t* widget, double value, int notify) {
	hui_spin_box_t* spin = (hui_spin_box_t*)widget;
	double low = spin->minimum < spin->maximum ? spin->minimum : spin->maximum;
	double high = spin->minimum < spin->maximum ? spin->maximum : spin->minimum;
	if (value != value) return; /* NaN */
	if (value < low) value = low;
	if (value > high) value = high;
	if (value == spin->value) return;
	spin->value = value;
	hui_widget_invalidate_paint(widget);
	if (notify) {
		if (spin->callback) spin->callback(widget->id, value, spin->user_data);
		if (hui_widget_alive(widget)) hui_widget_fire_action(widget);
	}
}

static hui_rect_t hui_spin_box_area(const hui_widget_t* widget) {
	float px = hui_widget_metric(widget, HUI_METRIC_PADDING_X) * 0.7f;
	return hui_rect(px, 0.0f, hui_maxf(0.0f, widget->rect.w - px * 2.0f), widget->rect.h);
}

static void hui_spin_box_begin_edit(hui_widget_t* widget, int select_all) {
	hui_spin_box_t* spin = (hui_spin_box_t*)widget;
	char buffer[64];
	if (spin->editing) return;
	hui_spin_box_format(spin, buffer, sizeof(buffer));
	hui_textedit_set_text(&spin->edit, buffer);
	if (select_all) hui_textedit_select_all(&spin->edit);
	else spin->edit.caret = spin->edit.anchor = 0;
	hui_textedit_set_focused(&spin->edit, widget, HUI_TRUE);
	spin->editing = HUI_TRUE;
	widget->cursor = HUI_CURSOR_IBEAM;
}

static void hui_spin_box_end_edit(hui_widget_t* widget, int apply) {
	hui_spin_box_t* spin = (hui_spin_box_t*)widget;
	if (!spin->editing) return;
	spin->editing = HUI_FALSE;
	widget->cursor = HUI_CURSOR_RESIZE_H;
	hui_textedit_set_focused(&spin->edit, widget, HUI_FALSE);
	if (apply) {
		char* end = NULL;
		double value;
		/* accept a decimal comma as well */
		char* text = hui_strdup(spin->edit.text);
		char* comma = strchr(text, ',');
		if (comma) *comma = '.';
		value = strtod(text, &end);
		if (end != text) hui_spin_box_set(widget, value, HUI_TRUE);
		hui_free(text);
	}
	if (hui_widget_alive(widget)) hui_widget_invalidate_paint(widget);
}

static hui_vec2_t hui_spin_box_measure(hui_widget_t* widget) {
	return hui_vec2(80.0f, hui_widget_metric(widget, HUI_METRIC_CONTROL_HEIGHT));
}

static void hui_spin_box_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_spin_box_t* spin = (hui_spin_box_t*)widget;
	hui_rect_t rect = hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h);
	int focused = (widget->flags & HUI_WF_FOCUSED) != 0;
	hui_color_t fill = hui_widget_color(widget, HUI_COLOR_INPUT);
	hui_color_t text = hui_widget_text_color(widget, painter);
	char buffer[64];

	if (!painter->enabled) fill = hui_color_fade(fill, 0.5f);
	hui_paint_box(painter, rect, hui_widget_metric(widget, HUI_METRIC_CORNER_RADIUS), fill,
		hui_widget_metric(widget, HUI_METRIC_BORDER_WIDTH), hui_widget_color(widget, focused ? HUI_COLOR_FOCUS : HUI_COLOR_BORDER));
	if (spin->editing) {
		hui_textedit_paint(&spin->edit, widget, painter, hui_spin_box_area(widget), HUI_INVALID_ID, 0.0f, text, HUI_TRUE);
		return;
	}
	hui_spin_box_format(spin, buffer, sizeof(buffer));
	hui_paint_push_clip(painter, rect);
	hui_paint_text_in(painter, HUI_INVALID_ID, hui_widget_font_size(widget, 0.0f), rect, 0.5f, buffer, -1, text);
	hui_paint_pop_clip(painter);
}

static int hui_spin_box_event(hui_widget_t* widget, const HUI_Event* event) {
	hui_spin_box_t* spin = (hui_spin_box_t*)widget;
	hui_rect_t area = hui_spin_box_area(widget);
	int flags;

	switch (event->type) {
		case HUI_EVENT_FOCUS_GAINED:
			/* reaching the field with Tab starts typing right away */
			if (widget->window && widget->window->focus_visible) hui_spin_box_begin_edit(widget, HUI_TRUE);
			return HUI_TRUE;
		case HUI_EVENT_FOCUS_LOST:
			hui_spin_box_end_edit(widget, HUI_TRUE);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_DOWN:
			if (event->button != HUI_MOUSE_LEFT) return HUI_FALSE;
			hui_widget_capture_mouse(widget);
			if (spin->editing) {
				hui_textedit_mouse_down(&spin->edit, widget, HUI_INVALID_ID, 0.0f, event->local_x - area.x,
					event->clicks, (event->modifiers & HUI_MOD_SHIFT) != 0);
			} else {
				widget->flags |= HUI_WF_PRESSED;
				spin->dragging = HUI_FALSE;
				spin->press_x = event->x;
				spin->press_value = spin->value;
			}
			hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_MOVE:
			if (spin->editing) {
				if (!spin->edit.mouse_selecting) return HUI_FALSE;
				hui_textedit_mouse_drag(&spin->edit, widget, HUI_INVALID_ID, 0.0f, event->local_x - area.x);
				hui_widget_invalidate_paint(widget);
				return HUI_TRUE;
			}
			if (!(widget->flags & HUI_WF_PRESSED)) return HUI_FALSE;
			if (!spin->dragging && fabsf(event->x - spin->press_x) > HUI_SPIN_DRAG_THRESHOLD) spin->dragging = HUI_TRUE;
			if (spin->dragging) {
				/* Shift scrubs ten times finer */
				double scale = (event->modifiers & HUI_MOD_SHIFT) ? 0.1 : 1.0;
				double steps = floor((double)(event->x - spin->press_x) / HUI_SPIN_PIXELS_PER_STEP);
				hui_spin_box_set(widget, spin->press_value + steps * spin->step * scale, HUI_TRUE);
			}
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_UP:
			if (event->button != HUI_MOUSE_LEFT) return HUI_FALSE;
			hui_widget_release_mouse(widget);
			if (spin->editing) {
				spin->edit.mouse_selecting = HUI_FALSE;
				return HUI_TRUE;
			}
			if (!(widget->flags & HUI_WF_PRESSED)) return HUI_FALSE;
			widget->flags &= ~HUI_WF_PRESSED;
			/* a click without a drag switches to typing */
			if (!spin->dragging) hui_spin_box_begin_edit(widget, HUI_TRUE);
			spin->dragging = HUI_FALSE;
			hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_WHEEL:
			/* only a focused field steps with the wheel: it must not hijack the scrolling of a panel */
			if (!(widget->flags & HUI_WF_FOCUSED) || spin->editing || event->wheel_y == 0.0f) return HUI_FALSE;
			hui_spin_box_set(widget, spin->value + (event->wheel_y > 0.0f ? spin->step : -spin->step), HUI_TRUE);
			return HUI_TRUE;
		case HUI_EVENT_KEY_DOWN:
			if (spin->editing) {
				if (event->key == HUI_KEY_ESCAPE) {
					hui_spin_box_end_edit(widget, HUI_FALSE);
					return HUI_TRUE;
				}
				flags = hui_textedit_key(&spin->edit, event);
				if (flags & HUI_TEXTEDIT_COMMITTED) hui_spin_box_end_edit(widget, HUI_TRUE);
				else if (flags) hui_widget_invalidate_paint(widget);
				return flags != 0;
			}
			switch (event->key) {
				case HUI_KEY_UP:
				case HUI_KEY_RIGHT:
					hui_spin_box_set(widget, spin->value + spin->step, HUI_TRUE);
					return HUI_TRUE;
				case HUI_KEY_DOWN:
				case HUI_KEY_LEFT:
					hui_spin_box_set(widget, spin->value - spin->step, HUI_TRUE);
					return HUI_TRUE;
				case HUI_KEY_ENTER:
				case HUI_KEY_KP_ENTER:
					hui_spin_box_begin_edit(widget, HUI_TRUE);
					return HUI_TRUE;
				default:
					return HUI_FALSE;
			}
		case HUI_EVENT_TEXT:
			/* typing a number starts an edit that replaces the value */
			if (!spin->editing) {
				uint32_t c = event->codepoint;
				if (!((c >= '0' && c <= '9') || c == '-' || c == '.' || c == ',')) return HUI_FALSE;
				hui_spin_box_begin_edit(widget, HUI_TRUE);
			}
			if (hui_textedit_input(&spin->edit, event->codepoint)) hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		default:
			return HUI_FALSE;
	}
}

static int hui_spin_box_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_spin_box_t* spin = (hui_spin_box_t*)widget;
	float number;
	if (!hui_parse_float(value, &number)) return HUI_FALSE;
	if (HUI_ATTR("Min")) spin->minimum = number;
	else if (HUI_ATTR("Max")) spin->maximum = number;
	else if (HUI_ATTR("Step")) spin->step = number;
	else if (HUI_ATTR("Decimals")) spin->decimals = hui_clampi((int)number, 0, 9);
	else if (HUI_ATTR("Value")) {
		hui_spin_box_set(widget, number, HUI_FALSE);
		return HUI_TRUE;
	} else {
		return HUI_FALSE;
	}
	hui_spin_box_set(widget, spin->value, HUI_FALSE);
	hui_widget_invalidate_paint(widget);
	return HUI_TRUE;
}

const hui_widget_class_t hui_class_spin_box = {
	.type = HUI_WIDGET_SPIN_BOX,
	.name = "SpinBox",
	.size = sizeof(hui_spin_box_t),
	.flags = HUI_CF_NO_CHILDREN | HUI_CF_FOCUSABLE,
	.init = hui_spin_box_init,
	.destroy = hui_spin_box_destroy,
	.measure = hui_spin_box_measure,
	.paint = hui_spin_box_paint,
	.event = hui_spin_box_event,
	.set_attribute = hui_spin_box_set_attribute
};

void HUI_SetSpinBoxRange(HUI_id id, double minimum, double maximum) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SPIN_BOX);
	hui_spin_box_t* spin = (hui_spin_box_t*)widget;
	spin->minimum = minimum;
	spin->maximum = maximum;
	hui_spin_box_set(widget, spin->value, HUI_FALSE);
}

void HUI_SetSpinBoxValue(HUI_id id, double value) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SPIN_BOX);
	hui_spin_box_set(widget, value, HUI_FALSE);
}

double HUI_GetSpinBoxValue(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_SPIN_BOX, 0.0);
	return ((hui_spin_box_t*)widget)->value;
}

void HUI_SetSpinBoxStep(HUI_id id, double step) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SPIN_BOX);
	((hui_spin_box_t*)widget)->step = step;
}

void HUI_SetSpinBoxDecimals(HUI_id id, int decimals) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SPIN_BOX);
	((hui_spin_box_t*)widget)->decimals = hui_clampi(decimals, 0, 9);
	hui_widget_invalidate_paint(widget);
}

void HUI_SetSpinBoxChangedCallback(HUI_id id, HUI_CSpinBoxChanged callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SPIN_BOX);
	((hui_spin_box_t*)widget)->callback = callback;
	((hui_spin_box_t*)widget)->user_data = user_data;
}
