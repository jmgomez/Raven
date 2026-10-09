/**
 * HUI - widgets/scroll_box.c
 * ScrollBox (Slate SScrollBox): children stacked along one axis inside a
 * clipped, scrollable area.
 *
 * Scrolling only moves the children: their own layout is untouched, so the
 * cost of a scroll step does not depend on what the children contain.
 */

#include "common.h"

#define HUI_SCROLL_WHEEL_STEP 48.0f

typedef struct hui_scroll_box_t {
	hui_widget_t base;
	float spacing;
	HUI_EOrientation orientation;
	int scroll_both;
	/** Along the stacking direction / across it. */
	hui_scrollbar_t main_bar;
	hui_scrollbar_t cross_bar;
} hui_scroll_box_t;

static void hui_scroll_box_init(hui_widget_t* widget) {
	((hui_scroll_box_t*)widget)->orientation = HUI_VERTICAL;
}

static int hui_scroll_box_vertical(const hui_widget_t* widget) {
	return ((const hui_scroll_box_t*)widget)->orientation == HUI_VERTICAL;
}

/* Size of the stacked children: x = along the axis, y = across. */
static hui_vec2_t hui_scroll_box_content(hui_widget_t* widget) {
	hui_scroll_box_t* box = (hui_scroll_box_t*)widget;
	int vertical = hui_scroll_box_vertical(widget);
	float main = 0.0f, cross = 0.0f;
	uint32_t i, visible = 0;
	for (i = 0; i < widget->child_count; i++) {
		hui_widget_t* child = widget->children[i];
		hui_vec2_t size;
		if (hui_widget_collapsed(child)) continue;
		size = hui_widget_measure_slot(child);
		main += vertical ? size.y : size.x;
		cross = hui_maxf(cross, vertical ? size.x : size.y);
		visible++;
	}
	if (visible > 1) main += box->spacing * (float)(visible - 1);
	return hui_vec2(main, cross);
}

static hui_vec2_t hui_scroll_box_measure(hui_widget_t* widget) {
	/* asks for the whole content; it scrolls when the parent gives less */
	hui_vec2_t content = hui_scroll_box_content(widget);
	return hui_scroll_box_vertical(widget) ? hui_vec2(content.y, content.x) : hui_vec2(content.x, content.y);
}

static void hui_scroll_box_arrange(hui_widget_t* widget) {
	hui_scroll_box_t* box = (hui_scroll_box_t*)widget;
	int vertical = hui_scroll_box_vertical(widget);
	float bar_size = hui_widget_metric(widget, HUI_METRIC_SCROLLBAR_SIZE);
	float view_main = vertical ? widget->rect.h : widget->rect.w;
	float view_cross = vertical ? widget->rect.w : widget->rect.h;
	hui_vec2_t content = hui_scroll_box_content(widget);
	float cross_size, position;
	uint32_t i;

	/* a visible scrollbar takes its thickness from the other axis */
	if (content.x > view_main + 0.5f) view_cross -= bar_size;
	if (box->scroll_both && content.y > view_cross + 0.5f) {
		view_main -= bar_size;
		if (content.x > view_main + 0.5f && view_cross == (vertical ? widget->rect.w : widget->rect.h)) view_cross -= bar_size;
	}
	view_main = hui_maxf(view_main, 0.0f);
	view_cross = hui_maxf(view_cross, 0.0f);

	box->main_bar.content = content.x;
	box->main_bar.viewport = view_main;
	hui_scrollbar_clamp(&box->main_bar);
	/* without cross scrolling the children are simply as wide as the view */
	cross_size = box->scroll_both ? hui_maxf(content.y, view_cross) : view_cross;
	box->cross_bar.content = cross_size;
	box->cross_bar.viewport = view_cross;
	hui_scrollbar_clamp(&box->cross_bar);

	position = -box->main_bar.offset;
	for (i = 0; i < widget->child_count; i++) {
		hui_widget_t* child = widget->children[i];
		hui_vec2_t size;
		float length;
		if (hui_widget_collapsed(child)) continue;
		size = hui_widget_measure_slot(child);
		length = vertical ? size.y : size.x;
		if (vertical) hui_widget_arrange_slot(child, hui_rect(-box->cross_bar.offset, position, cross_size, length));
		else hui_widget_arrange_slot(child, hui_rect(position, -box->cross_bar.offset, length, cross_size));
		position += length + box->spacing;
	}
}

/* Applies new scroll offsets at once: only the positions of the children change. */
static void hui_scroll_box_apply(hui_widget_t* widget) {
	hui_scroll_box_arrange(widget);
	hui_widget_invalidate_paint(widget);
}

static void hui_scroll_box_tracks(const hui_widget_t* widget, hui_rect_t* main_track, hui_rect_t* cross_track) {
	const hui_scroll_box_t* box = (const hui_scroll_box_t*)widget;
	float bar_size = hui_widget_metric(widget, HUI_METRIC_SCROLLBAR_SIZE);
	int cross_needed = hui_scrollbar_needed(&box->cross_bar);
	int main_needed = hui_scrollbar_needed(&box->main_bar);
	if (hui_scroll_box_vertical(widget)) {
		*main_track = hui_rect(widget->rect.w - bar_size, 0.0f, bar_size, widget->rect.h - (cross_needed ? bar_size : 0.0f));
		*cross_track = hui_rect(0.0f, widget->rect.h - bar_size, widget->rect.w - (main_needed ? bar_size : 0.0f), bar_size);
	} else {
		*main_track = hui_rect(0.0f, widget->rect.h - bar_size, widget->rect.w - (cross_needed ? bar_size : 0.0f), bar_size);
		*cross_track = hui_rect(widget->rect.w - bar_size, 0.0f, bar_size, widget->rect.h - (main_needed ? bar_size : 0.0f));
	}
}

static void hui_scroll_box_paint_overlay(hui_widget_t* widget, HUI_Painter* painter) {
	hui_scroll_box_t* box = (hui_scroll_box_t*)widget;
	hui_rect_t main_track, cross_track;
	hui_vec2_t origin = hui_widget_origin(widget);
	float mouse_x = widget->window ? widget->window->mouse_x - origin.x : -1.0f;
	float mouse_y = widget->window ? widget->window->mouse_y - origin.y : -1.0f;
	int vertical = hui_scroll_box_vertical(widget);

	hui_scroll_box_tracks(widget, &main_track, &cross_track);
	hui_scrollbar_paint(&box->main_bar, widget, painter, main_track, vertical, mouse_x, mouse_y);
	hui_scrollbar_paint(&box->cross_bar, widget, painter, cross_track, !vertical, mouse_x, mouse_y);
}

static int hui_scroll_box_event(hui_widget_t* widget, const HUI_Event* event) {
	hui_scroll_box_t* box = (hui_scroll_box_t*)widget;
	hui_rect_t main_track, cross_track;
	int vertical = hui_scroll_box_vertical(widget);
	int changed = HUI_FALSE, used;

	switch (event->type) {
		case HUI_EVENT_MOUSE_WHEEL: {
			/* the wheel scrolls vertically; with Shift, or on a horizontal box, horizontally */
			float along = vertical ? event->wheel_y : (event->wheel_y != 0.0f ? event->wheel_y : event->wheel_x);
			float across = vertical ? event->wheel_x : 0.0f;
			if (event->modifiers & HUI_MOD_SHIFT) {
				across = along;
				along = 0.0f;
			}
			if (along != 0.0f) changed |= hui_scrollbar_set(&box->main_bar, box->main_bar.offset - along * HUI_SCROLL_WHEEL_STEP);
			if (across != 0.0f) changed |= hui_scrollbar_set(&box->cross_bar, box->cross_bar.offset - across * HUI_SCROLL_WHEEL_STEP);
			if (changed) hui_scroll_box_apply(widget);
			/* at the end of its range, the wheel goes to the scroll box above */
			return changed;
		}
		case HUI_EVENT_MOUSE_DOWN:
		case HUI_EVENT_MOUSE_MOVE:
		case HUI_EVENT_MOUSE_UP:
			hui_scroll_box_tracks(widget, &main_track, &cross_track);
			used = hui_scrollbar_event(&box->main_bar, widget, event, main_track, vertical, &changed);
			if (!used) used = hui_scrollbar_event(&box->cross_bar, widget, event, cross_track, !vertical, &changed);
			if (changed) hui_scroll_box_apply(widget);
			else if (used || event->type == HUI_EVENT_MOUSE_MOVE) hui_widget_invalidate_paint(widget);
			return used;
		default:
			return HUI_FALSE;
	}
}

static int hui_scroll_box_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_scroll_box_t* box = (hui_scroll_box_t*)widget;
	if (HUI_ATTR("Orientation")) {
		if (!hui_parse_orientation(value, &box->orientation)) return HUI_FALSE;
	} else if (HUI_ATTR("ScrollBoth")) {
		if (!hui_parse_bool(value, &box->scroll_both)) return HUI_FALSE;
	} else if (HUI_ATTR("Spacing")) {
		if (!hui_parse_float(value, &box->spacing)) return HUI_FALSE;
	} else {
		return HUI_FALSE;
	}
	hui_widget_invalidate_layout(widget);
	return HUI_TRUE;
}

const hui_widget_class_t hui_class_scroll_box = {
	.type = HUI_WIDGET_SCROLL_BOX,
	.name = "ScrollBox",
	.size = sizeof(hui_scroll_box_t),
	.flags = HUI_CF_CLIP,
	.init = hui_scroll_box_init,
	.measure = hui_scroll_box_measure,
	.arrange = hui_scroll_box_arrange,
	.paint_overlay = hui_scroll_box_paint_overlay,
	.event = hui_scroll_box_event,
	.set_attribute = hui_scroll_box_set_attribute
};

/* ---------------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------------- */

/* Offsets are validated against the current layout. */
static void hui_scroll_box_sync(hui_widget_t* widget) {
	if (widget->window && widget->window->needs_layout) hui_window_layout(widget->window);
}

void HUI_SetScrollBoxOrientation(HUI_id id, HUI_EOrientation orientation) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SCROLL_BOX);
	if (orientation != HUI_HORIZONTAL && orientation != HUI_VERTICAL) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetScrollBoxOrientation: invalid orientation");
		return;
	}
	((hui_scroll_box_t*)widget)->orientation = orientation;
	hui_widget_invalidate_layout(widget);
}

void HUI_SetScrollBoxScrollBoth(HUI_id id, int enabled) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SCROLL_BOX);
	((hui_scroll_box_t*)widget)->scroll_both = enabled != 0;
	hui_widget_invalidate_layout(widget);
}

void HUI_SetScrollOffset(HUI_id id, float x, float y) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SCROLL_BOX);
	hui_scroll_box_t* box = (hui_scroll_box_t*)widget;
	int vertical = hui_scroll_box_vertical(widget);
	hui_scroll_box_sync(widget);
	hui_scrollbar_set(&box->main_bar, vertical ? y : x);
	hui_scrollbar_set(&box->cross_bar, vertical ? x : y);
	hui_scroll_box_apply(widget);
}

void HUI_GetScrollOffset(HUI_id id, float* x, float* y) {
	hui_widget_t* widget;
	hui_scroll_box_t* box;
	int vertical;
	if (x) *x = 0.0f;
	if (y) *y = 0.0f;
	widget = hui_widget_check(id, HUI_WIDGET_SCROLL_BOX, __func__);
	if (!widget) return;
	box = (hui_scroll_box_t*)widget;
	vertical = hui_scroll_box_vertical(widget);
	if (x) *x = vertical ? box->cross_bar.offset : box->main_bar.offset;
	if (y) *y = vertical ? box->main_bar.offset : box->cross_bar.offset;
}

void HUI_ScrollToEnd(HUI_id id) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SCROLL_BOX);
	hui_scroll_box_t* box = (hui_scroll_box_t*)widget;
	hui_scroll_box_sync(widget);
	hui_scrollbar_set(&box->main_bar, hui_scrollbar_max(&box->main_bar));
	hui_scroll_box_apply(widget);
}

void HUI_ScrollIntoView(HUI_id id) {
	hui_widget_t* ancestor;
	hui_widget_t* target;
	HUI_WIDGET_VOID(widget, id, 0);
	hui_scroll_box_sync(widget);
	target = widget;
	/* every scroll box above the widget brings it into its own view */
	for (ancestor = widget->parent; ancestor; ancestor = ancestor->parent) {
		hui_scroll_box_t* box;
		hui_vec2_t inner, outer;
		float start, end;
		int vertical;
		if (ancestor->cls->type != HUI_WIDGET_SCROLL_BOX) continue;
		box = (hui_scroll_box_t*)ancestor;
		vertical = hui_scroll_box_vertical(ancestor);
		inner = hui_widget_origin(target);
		outer = hui_widget_origin(ancestor);
		start = (vertical ? inner.y - outer.y : inner.x - outer.x) + box->main_bar.offset;
		end = start + (vertical ? target->rect.h : target->rect.w);
		if (start < box->main_bar.offset) hui_scrollbar_set(&box->main_bar, start);
		else if (end > box->main_bar.offset + box->main_bar.viewport) hui_scrollbar_set(&box->main_bar, end - box->main_bar.viewport);
		hui_scroll_box_apply(ancestor);
		target = ancestor;
	}
}
