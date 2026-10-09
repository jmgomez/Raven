/**
 * HUI - widgets/slider.c
 * Slider (Slate SSlider): a value picked by dragging a handle along a track.
 */

#include "common.h"

typedef struct hui_slider_t {
	hui_widget_t base;
	float minimum;
	float maximum;
	float value;
	float step;
	HUI_EOrientation orientation;
	HUI_CSliderChanged callback;
	void* user_data;
} hui_slider_t;

static void hui_slider_init(hui_widget_t* widget) {
	hui_slider_t* slider = (hui_slider_t*)widget;
	slider->maximum = 1.0f;
	slider->orientation = HUI_HORIZONTAL;
}

static float hui_slider_fraction(const hui_slider_t* slider) {
	float range = slider->maximum - slider->minimum;
	return range != 0.0f ? hui_clampf((slider->value - slider->minimum) / range, 0.0f, 1.0f) : 0.0f;
}

static float hui_slider_snap(const hui_slider_t* slider, float value) {
	float low = hui_minf(slider->minimum, slider->maximum), high = hui_maxf(slider->minimum, slider->maximum);
	if (slider->step > 0.0f) value = slider->minimum + floorf((value - slider->minimum) / slider->step + 0.5f) * slider->step;
	return hui_clampf(value, low, high);
}

static void hui_slider_set(hui_widget_t* widget, float value, int notify) {
	hui_slider_t* slider = (hui_slider_t*)widget;
	value = hui_slider_snap(slider, value);
	if (value == slider->value) return;
	slider->value = value;
	hui_widget_invalidate_paint(widget);
	if (notify) {
		if (slider->callback) slider->callback(widget->id, value, slider->user_data);
		if (hui_widget_alive(widget)) hui_widget_fire_action(widget);
	}
}

static hui_vec2_t hui_slider_measure(hui_widget_t* widget) {
	hui_slider_t* slider = (hui_slider_t*)widget;
	float thickness = hui_maxf(hui_widget_metric(widget, HUI_METRIC_SLIDER_HANDLE_SIZE) + 4.0f,
		hui_widget_metric(widget, HUI_METRIC_ROW_HEIGHT));
	return slider->orientation == HUI_HORIZONTAL ? hui_vec2(120.0f, thickness) : hui_vec2(thickness, 120.0f);
}

static void hui_slider_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_slider_t* slider = (hui_slider_t*)widget;
	float handle = hui_widget_metric(widget, HUI_METRIC_SLIDER_HANDLE_SIZE);
	float track = 4.0f, fraction = hui_slider_fraction(slider);
	int hot = ((widget->flags & HUI_WF_HOVERED) || (widget->flags & HUI_WF_PRESSED)) && painter->enabled;
	hui_color_t fill = hui_widget_color(widget, HUI_COLOR_ACCENT);
	hui_color_t knob = hui_widget_color(widget, hot ? HUI_COLOR_ACCENT_HOVERED : HUI_COLOR_ACCENT);
	hui_color_t back = hui_widget_color(widget, HUI_COLOR_TRACK);

	if (!painter->enabled) {
		fill = hui_color_fade(fill, 0.4f);
		knob = hui_widget_color(widget, HUI_COLOR_HANDLE);
	}
	if (slider->orientation == HUI_HORIZONTAL) {
		float travel = hui_maxf(0.0f, widget->rect.w - handle);
		float center = handle * 0.5f + travel * fraction;
		float y = floorf((widget->rect.h - track) * 0.5f);
		hui_paint_box(painter, hui_rect(handle * 0.5f, y, travel, track), track * 0.5f, back, 0.0f, 0);
		hui_paint_box(painter, hui_rect(handle * 0.5f, y, travel * fraction, track), track * 0.5f, fill, 0.0f, 0);
		hui_paint_box(painter, hui_rect(center - handle * 0.5f, (widget->rect.h - handle) * 0.5f, handle, handle),
			handle * 0.5f, knob, 2.0f, hui_widget_color(widget, HUI_COLOR_ACCENT_TEXT));
	} else {
		/* a vertical slider grows upwards */
		float travel = hui_maxf(0.0f, widget->rect.h - handle);
		float center = handle * 0.5f + travel * (1.0f - fraction);
		float x = floorf((widget->rect.w - track) * 0.5f);
		hui_paint_box(painter, hui_rect(x, handle * 0.5f, track, travel), track * 0.5f, back, 0.0f, 0);
		hui_paint_box(painter, hui_rect(x, center, track, handle * 0.5f + travel - center), track * 0.5f, fill, 0.0f, 0);
		hui_paint_box(painter, hui_rect((widget->rect.w - handle) * 0.5f, center - handle * 0.5f, handle, handle),
			handle * 0.5f, knob, 2.0f, hui_widget_color(widget, HUI_COLOR_ACCENT_TEXT));
	}
}

static void hui_slider_paint_overlay(hui_widget_t* widget, HUI_Painter* painter) {
	hui_paint_focus(widget, painter, 4.0f);
}

static float hui_slider_value_at(const hui_widget_t* widget, float x, float y) {
	const hui_slider_t* slider = (const hui_slider_t*)widget;
	float handle = hui_widget_metric(widget, HUI_METRIC_SLIDER_HANDLE_SIZE);
	float fraction;
	if (slider->orientation == HUI_HORIZONTAL) {
		float travel = widget->rect.w - handle;
		fraction = travel > 0.0f ? (x - handle * 0.5f) / travel : 0.0f;
	} else {
		float travel = widget->rect.h - handle;
		fraction = travel > 0.0f ? 1.0f - (y - handle * 0.5f) / travel : 0.0f;
	}
	return slider->minimum + (slider->maximum - slider->minimum) * hui_clampf(fraction, 0.0f, 1.0f);
}

static int hui_slider_event(hui_widget_t* widget, const HUI_Event* event) {
	hui_slider_t* slider = (hui_slider_t*)widget;
	float step = slider->step > 0.0f ? slider->step : (slider->maximum - slider->minimum) / 100.0f;

	switch (event->type) {
		case HUI_EVENT_MOUSE_DOWN:
			if (event->button != HUI_MOUSE_LEFT) return HUI_FALSE;
			widget->flags |= HUI_WF_PRESSED;
			hui_widget_capture_mouse(widget);
			hui_slider_set(widget, hui_slider_value_at(widget, event->local_x, event->local_y), HUI_TRUE);
			if (hui_widget_alive(widget)) hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_MOVE:
			if (!(widget->flags & HUI_WF_PRESSED)) return HUI_FALSE;
			hui_slider_set(widget, hui_slider_value_at(widget, event->local_x, event->local_y), HUI_TRUE);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_UP:
			if (event->button != HUI_MOUSE_LEFT || !(widget->flags & HUI_WF_PRESSED)) return HUI_FALSE;
			widget->flags &= ~HUI_WF_PRESSED;
			hui_widget_release_mouse(widget);
			hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		case HUI_EVENT_KEY_DOWN:
			if (!(widget->flags & HUI_WF_FOCUSED)) return HUI_FALSE;
			switch (event->key) {
				case HUI_KEY_LEFT:
				case HUI_KEY_DOWN:
					hui_slider_set(widget, slider->value - step, HUI_TRUE);
					return HUI_TRUE;
				case HUI_KEY_RIGHT:
				case HUI_KEY_UP:
					hui_slider_set(widget, slider->value + step, HUI_TRUE);
					return HUI_TRUE;
				case HUI_KEY_HOME:
					hui_slider_set(widget, slider->minimum, HUI_TRUE);
					return HUI_TRUE;
				case HUI_KEY_END:
					hui_slider_set(widget, slider->maximum, HUI_TRUE);
					return HUI_TRUE;
				default:
					return HUI_FALSE;
			}
		default:
			return HUI_FALSE;
	}
}

static int hui_slider_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_slider_t* slider = (hui_slider_t*)widget;
	float number;
	if (HUI_ATTR("Orientation")) {
		if (!hui_parse_orientation(value, &slider->orientation)) return HUI_FALSE;
		hui_widget_invalidate_layout(widget);
		return HUI_TRUE;
	}
	if (!hui_parse_float(value, &number)) return HUI_FALSE;
	if (HUI_ATTR("Min")) slider->minimum = number;
	else if (HUI_ATTR("Max")) slider->maximum = number;
	else if (HUI_ATTR("Step")) slider->step = number > 0.0f ? number : 0.0f;
	else if (HUI_ATTR("Value")) {
		hui_slider_set(widget, number, HUI_FALSE);
		return HUI_TRUE;
	} else {
		return HUI_FALSE;
	}
	slider->value = hui_slider_snap(slider, slider->value);
	hui_widget_invalidate_paint(widget);
	return HUI_TRUE;
}

const hui_widget_class_t hui_class_slider = {
	.type = HUI_WIDGET_SLIDER,
	.name = "Slider",
	.size = sizeof(hui_slider_t),
	.flags = HUI_CF_NO_CHILDREN | HUI_CF_FOCUSABLE,
	.init = hui_slider_init,
	.measure = hui_slider_measure,
	.paint = hui_slider_paint,
	.paint_overlay = hui_slider_paint_overlay,
	.event = hui_slider_event,
	.set_attribute = hui_slider_set_attribute
};

void HUI_SetSliderRange(HUI_id id, float minimum, float maximum) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SLIDER);
	hui_slider_t* slider = (hui_slider_t*)widget;
	slider->minimum = minimum;
	slider->maximum = maximum;
	slider->value = hui_slider_snap(slider, slider->value);
	hui_widget_invalidate_paint(widget);
}

void HUI_SetSliderValue(HUI_id id, float value) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SLIDER);
	hui_slider_set(widget, value, HUI_FALSE);
}

float HUI_GetSliderValue(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_SLIDER, 0.0f);
	return ((hui_slider_t*)widget)->value;
}

void HUI_SetSliderStep(HUI_id id, float step) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SLIDER);
	((hui_slider_t*)widget)->step = step > 0.0f ? step : 0.0f;
}

void HUI_SetSliderOrientation(HUI_id id, HUI_EOrientation orientation) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SLIDER);
	if (orientation != HUI_HORIZONTAL && orientation != HUI_VERTICAL) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetSliderOrientation: invalid orientation");
		return;
	}
	((hui_slider_t*)widget)->orientation = orientation;
	hui_widget_invalidate_layout(widget);
}

void HUI_SetSliderChangedCallback(HUI_id id, HUI_CSliderChanged callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SLIDER);
	((hui_slider_t*)widget)->callback = callback;
	((hui_slider_t*)widget)->user_data = user_data;
}
