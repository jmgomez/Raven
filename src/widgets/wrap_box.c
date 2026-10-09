/**
 * HUI - widgets/wrap_box.c
 * WrapBox (Slate SWrapBox): children flow left to right and continue on a
 * new line when the width is used up.
 *
 * The height depends on the width the panel receives, which is only known
 * when it is arranged: the panel remembers that width and, when it changes,
 * asks for one more layout pass.
 */

#include "common.h"

typedef struct hui_wrap_box_t {
	hui_widget_t base;
	float spacing;
	/** Width received at the last arrange; 0 before the first one. */
	float wrap_width;
} hui_wrap_box_t;

/**
 * Walks the children as they flow in `width`. With `place`, gives them their
 * bounds. Returns the size used.
 */
static hui_vec2_t hui_wrap_box_flow(hui_widget_t* widget, float width, int place) {
	hui_wrap_box_t* box = (hui_wrap_box_t*)widget;
	float x = 0.0f, y = 0.0f, line_height = 0.0f, used_width = 0.0f;
	uint32_t i;

	for (i = 0; i < widget->child_count; i++) {
		hui_widget_t* child = widget->children[i];
		hui_vec2_t size;
		if (hui_widget_collapsed(child)) continue;
		size = hui_widget_measure_slot(child);
		if (x > 0.0f && width > 0.0f && x + size.x > width + 0.01f) {
			x = 0.0f;
			y += line_height + box->spacing;
			line_height = 0.0f;
		}
		if (place) hui_widget_arrange_slot(child, hui_rect(x, y, size.x, size.y));
		x += size.x;
		used_width = hui_maxf(used_width, x);
		x += box->spacing;
		line_height = hui_maxf(line_height, size.y);
	}
	return hui_vec2(used_width, y + line_height);
}

static hui_vec2_t hui_wrap_box_measure(hui_widget_t* widget) {
	hui_wrap_box_t* box = (hui_wrap_box_t*)widget;
	return hui_wrap_box_flow(widget, box->wrap_width, HUI_FALSE);
}

static void hui_wrap_box_arrange(hui_widget_t* widget) {
	hui_wrap_box_t* box = (hui_wrap_box_t*)widget;
	if (box->wrap_width != widget->rect.w) {
		box->wrap_width = widget->rect.w;
		/* the height we announced was computed for another width */
		hui_widget_invalidate_layout(widget);
	}
	hui_wrap_box_flow(widget, widget->rect.w, HUI_TRUE);
}

static int hui_wrap_box_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_wrap_box_t* box = (hui_wrap_box_t*)widget;
	if (HUI_ATTR("Spacing")) {
		if (!hui_parse_float(value, &box->spacing)) return HUI_FALSE;
		hui_widget_invalidate_layout(widget);
		return HUI_TRUE;
	}
	return HUI_FALSE;
}

const hui_widget_class_t hui_class_wrap_box = {
	.type = HUI_WIDGET_WRAP_BOX,
	.name = "WrapBox",
	.size = sizeof(hui_wrap_box_t),
	.flags = HUI_CF_NO_HIT,
	.measure = hui_wrap_box_measure,
	.arrange = hui_wrap_box_arrange,
	.set_attribute = hui_wrap_box_set_attribute
};
