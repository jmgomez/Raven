/**
 * HUI - widgets/size_box.c
 * SizeBox (Slate SBox): one child with size constraints. The fixed, minimum
 * and maximum sizes are the constraints every widget has (HUI_SetWidgetSize
 * ...); this panel adds a fixed aspect ratio, typically around a viewport.
 */

#include "common.h"

typedef struct hui_size_box_t {
	hui_widget_t base;
	/** Width / height. 0 = no constraint. */
	float aspect_ratio;
} hui_size_box_t;

static void hui_size_box_arrange(hui_widget_t* widget) {
	hui_size_box_t* box = (hui_size_box_t*)widget;
	hui_rect_t area = hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h);
	hui_widget_t* child;

	if (widget->child_count == 0 || hui_widget_collapsed(widget->children[0])) return;
	child = widget->children[0];
	area = hui_rect_inset(area, child->slot.padding);

	if (box->aspect_ratio > 0.0f && area.w > 0.0f && area.h > 0.0f) {
		/* largest rectangle of the requested ratio that fits, placed by the slot alignment */
		float width = area.w, height = area.w / box->aspect_ratio;
		float fx = 0.5f, fy = 0.5f;
		if (height > area.h) {
			height = area.h;
			width = area.h * box->aspect_ratio;
		}
		if (child->slot.halign == HUI_HALIGN_LEFT) fx = 0.0f;
		else if (child->slot.halign == HUI_HALIGN_RIGHT) fx = 1.0f;
		if (child->slot.valign == HUI_VALIGN_TOP) fy = 0.0f;
		else if (child->slot.valign == HUI_VALIGN_BOTTOM) fy = 1.0f;
		hui_widget_arrange(child, area.x + (area.w - width) * fx, area.y + (area.h - height) * fy, width, height);
		return;
	}
	hui_widget_arrange_slot(child, hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h));
}

static int hui_size_box_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_size_box_t* box = (hui_size_box_t*)widget;
	if (HUI_ATTR("AspectRatio")) {
		float values[2];
		int count = hui_parse_floats(value, values, 2);
		/* "1.777" or "16,9" */
		if (count == 1) box->aspect_ratio = values[0];
		else if (count == 2 && values[1] > 0.0f) box->aspect_ratio = values[0] / values[1];
		else return HUI_FALSE;
		hui_widget_invalidate_layout(widget);
		return HUI_TRUE;
	}
	return HUI_FALSE;
}

const hui_widget_class_t hui_class_size_box = {
	.type = HUI_WIDGET_SIZE_BOX,
	.name = "SizeBox",
	.size = sizeof(hui_size_box_t),
	.flags = HUI_CF_SINGLE_CHILD | HUI_CF_NO_HIT,
	.arrange = hui_size_box_arrange,
	.set_attribute = hui_size_box_set_attribute
};

void HUI_SetSizeBoxAspectRatio(HUI_id id, float ratio) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SIZE_BOX);
	((hui_size_box_t*)widget)->aspect_ratio = ratio > 0.0f ? ratio : 0.0f;
	hui_widget_invalidate_layout(widget);
}
