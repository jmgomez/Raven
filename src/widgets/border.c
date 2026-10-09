/**
 * HUI - widgets/border.c
 * Border (Slate SBorder): one child with a background, an optional outline,
 * rounded corners, a drop shadow and inner padding.
 */

#include "common.h"

typedef struct hui_border_t {
	hui_widget_t base;
	hui_margin_t padding;
	float radii[4];
	int has_radii;
	float outline_width;
	hui_color_t outline_color;
	float shadow_blur;
	hui_vec2_t shadow_offset;
} hui_border_t;

static hui_vec2_t hui_border_measure(hui_widget_t* widget) {
	hui_border_t* border = (hui_border_t*)widget;
	hui_vec2_t desired = hui_vec2(0.0f, 0.0f);
	if (widget->child_count > 0) desired = hui_widget_measure_slot(widget->children[0]);
	desired.x += border->padding.l + border->padding.r;
	desired.y += border->padding.t + border->padding.b;
	return desired;
}

static void hui_border_arrange(hui_widget_t* widget) {
	hui_border_t* border = (hui_border_t*)widget;
	if (widget->child_count > 0 && !hui_widget_collapsed(widget->children[0])) {
		hui_widget_arrange_slot(widget->children[0],
			hui_rect_inset(hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h), border->padding));
	}
}

static void hui_border_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_border_t* border = (hui_border_t*)widget;
	hui_rect_t rect = hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h);
	float radii[4];
	int i;

	for (i = 0; i < 4; i++) radii[i] = border->has_radii ? border->radii[i] : 0.0f;
	if (border->shadow_blur > 0.0f) {
		hui_paint_shadow(painter, hui_rect(border->shadow_offset.x, border->shadow_offset.y, rect.w, rect.h),
			radii[0], border->shadow_blur, hui_widget_color(widget, HUI_COLOR_SHADOW));
	}
	hui_paint_box4(painter, rect, radii, hui_widget_color(widget, HUI_COLOR_PANEL),
		border->outline_width, border->outline_width > 0.0f ? border->outline_color : 0);
}

static int hui_border_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_border_t* border = (hui_border_t*)widget;
	hui_color_t color;
	float values[4];
	int count;

	if (HUI_ATTR("Padding")) {
		if (!hui_parse_margin(value, &border->padding)) return HUI_FALSE;
		hui_widget_invalidate_layout(widget);
	} else if (HUI_ATTR("Background")) {
		if (!hui_parse_color(value, &color)) return HUI_FALSE;
		hui_widget_set_color(widget, HUI_COLOR_PANEL, color);
	} else if (HUI_ATTR("BorderColor")) {
		if (!hui_parse_color(value, &border->outline_color)) return HUI_FALSE;
		if (border->outline_width <= 0.0f) border->outline_width = 1.0f;
	} else if (HUI_ATTR("BorderWidth")) {
		if (!hui_parse_float(value, &border->outline_width)) return HUI_FALSE;
		if (hui_color_a(border->outline_color) == 0) border->outline_color = hui_widget_color(widget, HUI_COLOR_BORDER);
	} else if (HUI_ATTR("CornerRadius")) {
		count = hui_parse_floats(value, values, 4);
		if (count != 1 && count != 4) return HUI_FALSE;
		border->radii[0] = values[0];
		border->radii[1] = count == 4 ? values[1] : values[0];
		border->radii[2] = count == 4 ? values[2] : values[0];
		border->radii[3] = count == 4 ? values[3] : values[0];
		border->has_radii = HUI_TRUE;
	} else if (HUI_ATTR("Shadow")) {
		count = hui_parse_floats(value, values, 3);
		if (count < 1) return HUI_FALSE;
		border->shadow_blur = values[0];
		border->shadow_offset = hui_vec2(count > 1 ? values[1] : 0.0f, count > 2 ? values[2] : 2.0f);
	} else {
		return HUI_FALSE;
	}
	hui_widget_invalidate_paint(widget);
	return HUI_TRUE;
}

const hui_widget_class_t hui_class_border = {
	.type = HUI_WIDGET_BORDER,
	.name = "Border",
	.size = sizeof(hui_border_t),
	.flags = HUI_CF_SINGLE_CHILD,
	.measure = hui_border_measure,
	.arrange = hui_border_arrange,
	.paint = hui_border_paint,
	.set_attribute = hui_border_set_attribute
};

void HUI_SetBorderPadding(HUI_id id, float left, float top, float right, float bottom) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_BORDER);
	((hui_border_t*)widget)->padding = hui_margin(left, top, right, bottom);
	hui_widget_invalidate_layout(widget);
}

void HUI_SetBorderBackground(HUI_id id, float r, float g, float b, float a) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_BORDER);
	hui_widget_set_color(widget, HUI_COLOR_PANEL, hui_rgba(r, g, b, a));
}

void HUI_SetBorderOutline(HUI_id id, float width, float r, float g, float b, float a) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_BORDER);
	((hui_border_t*)widget)->outline_width = width > 0.0f ? width : 0.0f;
	((hui_border_t*)widget)->outline_color = hui_rgba(r, g, b, a);
	hui_widget_invalidate_paint(widget);
}

void HUI_SetBorderCornerRadius(HUI_id id, float top_left, float top_right, float bottom_right, float bottom_left) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_BORDER);
	hui_border_t* border = (hui_border_t*)widget;
	border->radii[0] = top_left;
	border->radii[1] = top_right;
	border->radii[2] = bottom_right;
	border->radii[3] = bottom_left;
	border->has_radii = HUI_TRUE;
	hui_widget_invalidate_paint(widget);
}

void HUI_SetBorderShadow(HUI_id id, float blur, float offset_x, float offset_y) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_BORDER);
	((hui_border_t*)widget)->shadow_blur = blur > 0.0f ? blur : 0.0f;
	((hui_border_t*)widget)->shadow_offset = hui_vec2(offset_x, offset_y);
	hui_widget_invalidate_paint(widget);
}
