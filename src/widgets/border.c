/**
 * Raven - widgets/border.c
 * Border (Slate SBorder): one child with a background, an optional outline,
 * rounded corners, a drop shadow and inner padding.
 */

#include "common.h"

typedef struct rvn_border_t {
	rvn_widget_t base;
	rvn_margin_t padding;
	float radii[4];
	int has_radii;
	float outline_width;
	rvn_color_t outline_color;
	float shadow_blur;
	rvn_vec2_t shadow_offset;
	RMaterial material;
} rvn_border_t;

static rvn_vec2_t rvn_border_measure(rvn_widget_t* widget) {
	rvn_border_t* border = (rvn_border_t*)widget;
	rvn_vec2_t desired = rvn_vec2(0.0f, 0.0f);
	if (widget->child_count > 0) desired = rvn_widget_measure_slot(widget->children[0]);
	desired.x += border->padding.l + border->padding.r;
	desired.y += border->padding.t + border->padding.b;
	return desired;
}

static void rvn_border_arrange(rvn_widget_t* widget) {
	rvn_border_t* border = (rvn_border_t*)widget;
	if (widget->child_count > 0 && !rvn_widget_collapsed(widget->children[0])) {
		rvn_widget_arrange_slot(widget->children[0],
			rvn_rect_inset(rvn_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h), border->padding));
	}
}

static void rvn_border_paint(rvn_widget_t* widget, RPainter* painter) {
	rvn_border_t* border = (rvn_border_t*)widget;
	rvn_rect_t rect = rvn_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h);
	float radii[4];
	int i;

	if (border->material == R_MATERIAL_GLASS) {
		rvn_glass_t glass;
		rvn_glass_style(widget, RVN_SURFACE_PANEL, 0.0f, 0.0f, &glass);
		/* a panel of glass is rounded unless told otherwise */
		for (i = 0; i < 4; i++) radii[i] = border->has_radii ? border->radii[i] : 20.0f;
		glass.shadow = border->shadow_blur;
		rvn_paint_glass(painter, rect, radii, &glass);
		return;
	}
	for (i = 0; i < 4; i++) radii[i] = border->has_radii ? border->radii[i] : 0.0f;
	if (border->shadow_blur > 0.0f) {
		rvn_paint_shadow(painter, rvn_rect(border->shadow_offset.x, border->shadow_offset.y, rect.w, rect.h),
			radii[0], border->shadow_blur, rvn_widget_color(widget, R_COLOR_SHADOW));
	}
	rvn_paint_box4(painter, rect, radii, rvn_widget_color(widget, R_COLOR_PANEL),
		border->outline_width, border->outline_width > 0.0f ? border->outline_color : 0);
}

static int rvn_border_set_attribute(rvn_widget_t* widget, const char* name, const char* value) {
	rvn_border_t* border = (rvn_border_t*)widget;
	rvn_color_t color;
	float values[4];
	int count;

	if (RVN_ATTR("Padding")) {
		if (!rvn_parse_margin(value, &border->padding)) return R_FALSE;
		rvn_widget_invalidate_layout(widget);
	} else if (RVN_ATTR("Background")) {
		if (!rvn_parse_color(value, &color)) return R_FALSE;
		rvn_widget_set_color(widget, R_COLOR_PANEL, color);
	} else if (RVN_ATTR("BorderColor")) {
		if (!rvn_parse_color(value, &border->outline_color)) return R_FALSE;
		if (border->outline_width <= 0.0f) border->outline_width = 1.0f;
	} else if (RVN_ATTR("BorderWidth")) {
		if (!rvn_parse_float(value, &border->outline_width)) return R_FALSE;
		if (rvn_color_a(border->outline_color) == 0) border->outline_color = rvn_widget_color(widget, R_COLOR_BORDER);
	} else if (RVN_ATTR("CornerRadius")) {
		count = rvn_parse_floats(value, values, 4);
		if (count != 1 && count != 4) return R_FALSE;
		border->radii[0] = values[0];
		border->radii[1] = count == 4 ? values[1] : values[0];
		border->radii[2] = count == 4 ? values[2] : values[0];
		border->radii[3] = count == 4 ? values[3] : values[0];
		border->has_radii = R_TRUE;
	} else if (RVN_ATTR("Material")) {
		if (rvn_stricmp(value, "Glass") == 0) border->material = R_MATERIAL_GLASS;
		else if (rvn_stricmp(value, "Solid") == 0) border->material = R_MATERIAL_SOLID;
		else return R_FALSE;
	} else if (RVN_ATTR("Shadow")) {
		count = rvn_parse_floats(value, values, 3);
		if (count < 1) return R_FALSE;
		border->shadow_blur = values[0];
		border->shadow_offset = rvn_vec2(count > 1 ? values[1] : 0.0f, count > 2 ? values[2] : 2.0f);
	} else {
		return R_FALSE;
	}
	rvn_widget_invalidate_paint(widget);
	return R_TRUE;
}

const rvn_widget_class_t rvn_class_border = {
	.type = R_WIDGET_BORDER,
	.name = "Border",
	.size = sizeof(rvn_border_t),
	.flags = RVN_CF_SINGLE_CHILD,
	.measure = rvn_border_measure,
	.arrange = rvn_border_arrange,
	.paint = rvn_border_paint,
	.set_attribute = rvn_border_set_attribute
};

void Rborder_set_padding(Rid id, float left, float top, float right, float bottom) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_BORDER);
	((rvn_border_t*)widget)->padding = rvn_margin(left, top, right, bottom);
	rvn_widget_invalidate_layout(widget);
}

void Rborder_set_background(Rid id, float r, float g, float b, float a) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_BORDER);
	rvn_widget_set_color(widget, R_COLOR_PANEL, rvn_rgba(r, g, b, a));
}

void Rborder_set_outline(Rid id, float width, float r, float g, float b, float a) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_BORDER);
	((rvn_border_t*)widget)->outline_width = width > 0.0f ? width : 0.0f;
	((rvn_border_t*)widget)->outline_color = rvn_rgba(r, g, b, a);
	rvn_widget_invalidate_paint(widget);
}

void Rborder_set_corner_radius(Rid id, float top_left, float top_right, float bottom_right, float bottom_left) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_BORDER);
	rvn_border_t* border = (rvn_border_t*)widget;
	border->radii[0] = top_left;
	border->radii[1] = top_right;
	border->radii[2] = bottom_right;
	border->radii[3] = bottom_left;
	border->has_radii = R_TRUE;
	rvn_widget_invalidate_paint(widget);
}

void Rborder_set_material(Rid id, RMaterial material) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_BORDER);
	if (material != R_MATERIAL_SOLID && material != R_MATERIAL_GLASS) {
		rvn_error(R_ERROR_INVALID_ENUM, R_SEVERITY_ERROR, "Rborder_set_material: invalid material");
		return;
	}
	((rvn_border_t*)widget)->material = material;
	rvn_widget_invalidate_paint(widget);
}

void Rborder_set_shadow(Rid id, float blur, float offset_x, float offset_y) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_BORDER);
	((rvn_border_t*)widget)->shadow_blur = blur > 0.0f ? blur : 0.0f;
	((rvn_border_t*)widget)->shadow_offset = rvn_vec2(offset_x, offset_y);
	rvn_widget_invalidate_paint(widget);
}
