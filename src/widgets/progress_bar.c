/**
 * HUI - widgets/progress_bar.c
 * ProgressBar (Slate SProgressBar) and Separator (SSeparator): two widgets
 * that only draw.
 */

#include "common.h"

/* ===========================================================================
 *  ProgressBar
 * ========================================================================= */

typedef struct hui_progress_bar_t {
	hui_widget_t base;
	float value;
} hui_progress_bar_t;

static hui_vec2_t hui_progress_bar_measure(hui_widget_t* widget) {
	HUI_UNUSED(widget);
	return hui_vec2(120.0f, 8.0f);
}

static void hui_progress_bar_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_progress_bar_t* bar = (hui_progress_bar_t*)widget;
	hui_rect_t rect = hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h);
	float radius = hui_minf(widget->rect.h * 0.5f, hui_widget_metric(widget, HUI_METRIC_CORNER_RADIUS));
	float filled = floorf(widget->rect.w * hui_clampf(bar->value, 0.0f, 1.0f) + 0.5f);
	hui_color_t fill = hui_widget_color(widget, HUI_COLOR_ACCENT);

	if (!painter->enabled) fill = hui_color_fade(fill, 0.4f);
	hui_paint_box(painter, rect, radius, hui_widget_color(widget, HUI_COLOR_TRACK), 0.0f, 0);
	if (filled > 0.0f) {
		/* clipping the full-size shape keeps the rounded ends at any progress */
		hui_paint_push_clip(painter, hui_rect(0.0f, 0.0f, filled, widget->rect.h));
		hui_paint_box(painter, rect, radius, fill, 0.0f, 0);
		hui_paint_pop_clip(painter);
	}
}

static int hui_progress_bar_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	if (HUI_ATTR("Value")) {
		if (!hui_parse_float(value, &((hui_progress_bar_t*)widget)->value)) return HUI_FALSE;
		hui_widget_invalidate_paint(widget);
		return HUI_TRUE;
	}
	return HUI_FALSE;
}

const hui_widget_class_t hui_class_progress_bar = {
	.type = HUI_WIDGET_PROGRESSBAR,
	.name = "ProgressBar",
	.size = sizeof(hui_progress_bar_t),
	.flags = HUI_CF_NO_CHILDREN | HUI_CF_NO_HIT,
	.measure = hui_progress_bar_measure,
	.paint = hui_progress_bar_paint,
	.set_attribute = hui_progress_bar_set_attribute
};

void HUI_SetProgressBarValue(HUI_id id, float value) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_PROGRESSBAR);
	value = hui_clampf(value, 0.0f, 1.0f);
	if (((hui_progress_bar_t*)widget)->value == value) return;
	((hui_progress_bar_t*)widget)->value = value;
	hui_widget_invalidate_paint(widget);
}

float HUI_GetProgressBarValue(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_PROGRESSBAR, 0.0f);
	return ((hui_progress_bar_t*)widget)->value;
}

/* ===========================================================================
 *  Separator
 * ========================================================================= */

typedef struct hui_separator_t {
	hui_widget_t base;
	HUI_EOrientation orientation;
} hui_separator_t;

static void hui_separator_init(hui_widget_t* widget) {
	((hui_separator_t*)widget)->orientation = HUI_HORIZONTAL;
}

static hui_vec2_t hui_separator_measure(hui_widget_t* widget) {
	HUI_UNUSED(widget);
	/* one unit thick in both cases: it stretches along the other axis by its fill alignment */
	return hui_vec2(1.0f, 1.0f);
}

static void hui_separator_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_separator_t* separator = (hui_separator_t*)widget;
	hui_color_t color = hui_widget_color(widget, HUI_COLOR_SEPARATOR);
	if (separator->orientation == HUI_HORIZONTAL) {
		hui_paint_rect(painter, hui_rect(0.0f, floorf((widget->rect.h - 1.0f) * 0.5f), widget->rect.w, 1.0f), color);
	} else {
		hui_paint_rect(painter, hui_rect(floorf((widget->rect.w - 1.0f) * 0.5f), 0.0f, 1.0f, widget->rect.h), color);
	}
}

static int hui_separator_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	if (HUI_ATTR("Orientation")) {
		if (!hui_parse_orientation(value, &((hui_separator_t*)widget)->orientation)) return HUI_FALSE;
		hui_widget_invalidate_paint(widget);
		return HUI_TRUE;
	}
	return HUI_FALSE;
}

const hui_widget_class_t hui_class_separator = {
	.type = HUI_WIDGET_SEPARATOR,
	.name = "Separator",
	.size = sizeof(hui_separator_t),
	.flags = HUI_CF_NO_CHILDREN | HUI_CF_NO_HIT,
	.init = hui_separator_init,
	.measure = hui_separator_measure,
	.paint = hui_separator_paint,
	.set_attribute = hui_separator_set_attribute
};

void HUI_SetSeparatorOrientation(HUI_id id, HUI_EOrientation orientation) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SEPARATOR);
	if (orientation != HUI_HORIZONTAL && orientation != HUI_VERTICAL) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetSeparatorOrientation: invalid orientation");
		return;
	}
	((hui_separator_t*)widget)->orientation = orientation;
	hui_widget_invalidate_paint(widget);
}
