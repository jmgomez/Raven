/**
 * HUI - widgets/box_panel.c
 * VBox and HBox: children stacked along one axis (Slate SVerticalBox /
 * SHorizontalBox).
 *
 *   auto slot : takes the desired size of the child along the axis
 *   fill slot : shares the remaining space in proportion to its coefficient
 * Across the axis, every slot spans the whole panel and the child is placed
 * in it according to its alignment.
 */

#include "common.h"

static int hui_box_is_vertical(const hui_widget_t* widget) {
	return widget->cls->type == HUI_WIDGET_VBOX;
}

/* Size of a child along the axis, slot padding excluded, limited by the slot maximum. */
static float hui_box_child_main(const hui_widget_t* child, int vertical) {
	float size = vertical ? child->desired.y : child->desired.x;
	if (child->slot.max_size > 0.0f && size > child->slot.max_size) size = child->slot.max_size;
	return size;
}

static float hui_box_padding_main(const hui_widget_t* child, int vertical) {
	return vertical ? child->slot.padding.t + child->slot.padding.b : child->slot.padding.l + child->slot.padding.r;
}

static hui_vec2_t hui_box_measure(hui_widget_t* widget) {
	hui_spaced_panel_t* panel = (hui_spaced_panel_t*)widget;
	int vertical = hui_box_is_vertical(widget);
	float main = 0.0f, cross = 0.0f;
	uint32_t i, visible = 0;

	for (i = 0; i < widget->child_count; i++) {
		hui_widget_t* child = widget->children[i];
		hui_vec2_t size;
		if (hui_widget_collapsed(child)) continue;
		size = hui_widget_measure_slot(child);
		main += hui_box_child_main(child, vertical) + hui_box_padding_main(child, vertical);
		cross = hui_maxf(cross, vertical ? size.x : size.y);
		visible++;
	}
	if (visible > 1) main += panel->spacing * (float)(visible - 1);
	return vertical ? hui_vec2(cross, main) : hui_vec2(main, cross);
}

static void hui_box_arrange(hui_widget_t* widget) {
	hui_spaced_panel_t* panel = (hui_spaced_panel_t*)widget;
	int vertical = hui_box_is_vertical(widget);
	float main_size = vertical ? widget->rect.h : widget->rect.w;
	float cross_size = vertical ? widget->rect.w : widget->rect.h;
	float fixed = 0.0f, fill_total = 0.0f, remaining, position = 0.0f;
	uint32_t i, visible = 0;

	/* first pass: space taken by the auto slots, the paddings and the gaps */
	for (i = 0; i < widget->child_count; i++) {
		hui_widget_t* child = widget->children[i];
		if (hui_widget_collapsed(child)) continue;
		fixed += hui_box_padding_main(child, vertical);
		if (child->slot.is_fill) fill_total += child->slot.fill;
		else fixed += hui_box_child_main(child, vertical);
		visible++;
	}
	if (visible > 1) fixed += panel->spacing * (float)(visible - 1);
	remaining = hui_maxf(0.0f, main_size - fixed);

	/* second pass: place the children */
	for (i = 0; i < widget->child_count; i++) {
		hui_widget_t* child = widget->children[i];
		float size, padding;
		hui_rect_t slot;
		if (hui_widget_collapsed(child)) continue;
		padding = hui_box_padding_main(child, vertical);
		if (child->slot.is_fill) {
			size = fill_total > 0.0f ? remaining * child->slot.fill / fill_total : 0.0f;
			if (child->slot.max_size > 0.0f && size > child->slot.max_size) size = child->slot.max_size;
		} else {
			size = hui_box_child_main(child, vertical);
		}
		slot = vertical ? hui_rect(0.0f, position, cross_size, size + padding)
			: hui_rect(position, 0.0f, size + padding, cross_size);
		hui_widget_arrange_slot(child, slot);
		position += size + padding + panel->spacing;
	}
}

static int hui_box_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_spaced_panel_t* panel = (hui_spaced_panel_t*)widget;
	if (HUI_ATTR("Spacing")) {
		if (!hui_parse_float(value, &panel->spacing)) return HUI_FALSE;
		hui_widget_invalidate_layout(widget);
		return HUI_TRUE;
	}
	return HUI_FALSE;
}

const hui_widget_class_t hui_class_vbox = {
	.type = HUI_WIDGET_VBOX,
	.name = "VBox",
	.size = sizeof(hui_spaced_panel_t),
	.flags = HUI_CF_NO_HIT,
	.measure = hui_box_measure,
	.arrange = hui_box_arrange,
	.set_attribute = hui_box_set_attribute
};

const hui_widget_class_t hui_class_hbox = {
	.type = HUI_WIDGET_HBOX,
	.name = "HBox",
	.size = sizeof(hui_spaced_panel_t),
	.flags = HUI_CF_NO_HIT,
	.measure = hui_box_measure,
	.arrange = hui_box_arrange,
	.set_attribute = hui_box_set_attribute
};

/* ---------------------------------------------------------------------------
 *  Public API shared by the panels that have a spacing
 * ------------------------------------------------------------------------- */

void HUI_SetPanelSpacing(HUI_id id, float spacing) {
	HUI_WIDGET_VOID(widget, id, 0);
	switch (widget->cls->type) {
		case HUI_WIDGET_VBOX:
		case HUI_WIDGET_HBOX:
		case HUI_WIDGET_WRAP_BOX:
		case HUI_WIDGET_GRID:
		case HUI_WIDGET_UNIFORM_GRID:
		case HUI_WIDGET_SCROLL_BOX:
			((hui_spaced_panel_t*)widget)->spacing = spacing > 0.0f ? spacing : 0.0f;
			hui_widget_invalidate_layout(widget);
			break;
		default:
			hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_SetPanelSpacing: a %s has no spacing", widget->cls->name);
			break;
	}
}
