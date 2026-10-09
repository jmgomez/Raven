/**
 * HUI - widgets/splitter.c
 * Splitter (Slate SSplitter): panes separated by handles the user can drag.
 * The relative size of a pane is the fill coefficient of its slot.
 */

#include "common.h"

typedef struct hui_splitter_t {
	hui_widget_t base;
	HUI_EOrientation orientation;
	float min_pane;
	/** Handle under the cursor / being dragged (index of the pane before it), or -1. */
	int hot_handle;
	int drag_handle;
	/** Cursor position along the axis when the drag started, and the two pane sizes then. */
	float drag_origin;
	float drag_before;
	float drag_after;
} hui_splitter_t;

static void hui_splitter_init(hui_widget_t* widget) {
	hui_splitter_t* splitter = (hui_splitter_t*)widget;
	splitter->orientation = HUI_HORIZONTAL;
	splitter->min_pane = 24.0f;
	splitter->hot_handle = -1;
	splitter->drag_handle = -1;
}

static int hui_splitter_horizontal(const hui_widget_t* widget) {
	return ((const hui_splitter_t*)widget)->orientation == HUI_HORIZONTAL;
}

static uint32_t hui_splitter_pane_count(const hui_widget_t* widget) {
	uint32_t i, count = 0;
	for (i = 0; i < widget->child_count; i++) {
		if (!hui_widget_collapsed(widget->children[i])) count++;
	}
	return count;
}

static float hui_splitter_weight(const hui_widget_t* child) {
	return child->slot.fill > 0.0f ? child->slot.fill : 0.0001f;
}

static hui_vec2_t hui_splitter_measure(hui_widget_t* widget) {
	int horizontal = hui_splitter_horizontal(widget);
	float handle = hui_widget_metric(widget, HUI_METRIC_SPLITTER_SIZE);
	float main = 0.0f, cross = 0.0f;
	uint32_t i, panes = 0;
	for (i = 0; i < widget->child_count; i++) {
		hui_widget_t* child = widget->children[i];
		hui_vec2_t size;
		if (hui_widget_collapsed(child)) continue;
		size = hui_widget_measure_slot(child);
		main += horizontal ? size.x : size.y;
		cross = hui_maxf(cross, horizontal ? size.y : size.x);
		panes++;
	}
	if (panes > 1) main += handle * (float)(panes - 1);
	return horizontal ? hui_vec2(main, cross) : hui_vec2(cross, main);
}

static void hui_splitter_arrange(hui_widget_t* widget) {
	int horizontal = hui_splitter_horizontal(widget);
	float handle = hui_widget_metric(widget, HUI_METRIC_SPLITTER_SIZE);
	float main_size = horizontal ? widget->rect.w : widget->rect.h;
	float cross_size = horizontal ? widget->rect.h : widget->rect.w;
	uint32_t panes = hui_splitter_pane_count(widget), i, seen = 0;
	float available, total = 0.0f, position = 0.0f;

	if (panes == 0) return;
	available = hui_maxf(0.0f, main_size - handle * (float)(panes - 1));
	for (i = 0; i < widget->child_count; i++) {
		if (!hui_widget_collapsed(widget->children[i])) total += hui_splitter_weight(widget->children[i]);
	}
	for (i = 0; i < widget->child_count; i++) {
		hui_widget_t* child = widget->children[i];
		float size;
		if (hui_widget_collapsed(child)) continue;
		seen++;
		/* the last pane takes what is left, so rounding never leaves a gap */
		size = seen == panes ? hui_maxf(0.0f, main_size - position) : floorf(available * hui_splitter_weight(child) / total + 0.5f);
		if (horizontal) hui_widget_arrange_slot(child, hui_rect(position, 0.0f, size, cross_size));
		else hui_widget_arrange_slot(child, hui_rect(0.0f, position, cross_size, size));
		position += size + handle;
	}
}

/* Rectangle of the handle that follows the visible pane number `index`. */
static int hui_splitter_handle_rect(const hui_widget_t* widget, int index, hui_rect_t* rect, hui_widget_t** before, hui_widget_t** after) {
	int horizontal = hui_splitter_horizontal(widget);
	float handle = hui_widget_metric(widget, HUI_METRIC_SPLITTER_SIZE);
	hui_widget_t* previous = NULL;
	uint32_t i;
	int seen = -1;
	for (i = 0; i < widget->child_count; i++) {
		hui_widget_t* child = widget->children[i];
		if (hui_widget_collapsed(child)) continue;
		if (seen == index && previous) {
			/* slot paddings are part of the pane: the handle starts where the slot ends */
			float edge = horizontal ? previous->rect.x + previous->rect.w + previous->slot.padding.r
				: previous->rect.y + previous->rect.h + previous->slot.padding.b;
			*rect = horizontal ? hui_rect(edge, 0.0f, handle, widget->rect.h) : hui_rect(0.0f, edge, widget->rect.w, handle);
			if (before) *before = previous;
			if (after) *after = child;
			return HUI_TRUE;
		}
		previous = child;
		seen++;
	}
	return HUI_FALSE;
}

static int hui_splitter_handle_at(const hui_widget_t* widget, float x, float y) {
	int count = (int)hui_splitter_pane_count(widget) - 1, i;
	for (i = 0; i < count; i++) {
		hui_rect_t rect;
		if (!hui_splitter_handle_rect(widget, i, &rect, NULL, NULL)) continue;
		/* a slightly larger target than what is drawn */
		rect.x -= 2.0f;
		rect.y -= 2.0f;
		rect.w += 4.0f;
		rect.h += 4.0f;
		if (hui_rect_contains(rect, x, y)) return i;
	}
	return -1;
}

static float hui_splitter_pane_size(const hui_widget_t* widget, const hui_widget_t* child) {
	return hui_splitter_horizontal(widget) ? child->rect.w + child->slot.padding.l + child->slot.padding.r
		: child->rect.h + child->slot.padding.t + child->slot.padding.b;
}

static int hui_splitter_event(hui_widget_t* widget, const HUI_Event* event) {
	hui_splitter_t* splitter = (hui_splitter_t*)widget;
	int horizontal = hui_splitter_horizontal(widget);
	float along = horizontal ? event->local_x : event->local_y;
	hui_widget_t* before = NULL;
	hui_widget_t* after = NULL;
	hui_rect_t rect;
	uint32_t i;

	switch (event->type) {
		case HUI_EVENT_MOUSE_DOWN: {
			int handle = hui_splitter_handle_at(widget, event->local_x, event->local_y);
			if (event->button != HUI_MOUSE_LEFT || handle < 0) return HUI_FALSE;
			if (!hui_splitter_handle_rect(widget, handle, &rect, &before, &after)) return HUI_FALSE;
			/* from now on the weights are the pane sizes in logical units */
			for (i = 0; i < widget->child_count; i++) {
				hui_widget_t* child = widget->children[i];
				if (!hui_widget_collapsed(child)) child->slot.fill = hui_maxf(hui_splitter_pane_size(widget, child), 0.0001f);
			}
			splitter->drag_handle = handle;
			splitter->drag_origin = along;
			splitter->drag_before = before->slot.fill;
			splitter->drag_after = after->slot.fill;
			hui_widget_capture_mouse(widget);
			hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		}
		case HUI_EVENT_MOUSE_MOVE:
			if (splitter->drag_handle >= 0) {
				float delta = along - splitter->drag_origin;
				float total = splitter->drag_before + splitter->drag_after;
				float minimum = hui_minf(splitter->min_pane, total * 0.5f);
				float size;
				if (!hui_splitter_handle_rect(widget, splitter->drag_handle, &rect, &before, &after)) return HUI_TRUE;
				size = hui_clampf(splitter->drag_before + delta, minimum, total - minimum);
				before->slot.fill = hui_maxf(size, 0.0001f);
				after->slot.fill = hui_maxf(total - size, 0.0001f);
				hui_widget_invalidate_layout(widget);
				return HUI_TRUE;
			} else {
				int handle = hui_splitter_handle_at(widget, event->local_x, event->local_y);
				if (handle != splitter->hot_handle) {
					splitter->hot_handle = handle;
					hui_widget_invalidate_paint(widget);
				}
				widget->cursor = handle >= 0 ? (horizontal ? HUI_CURSOR_RESIZE_H : HUI_CURSOR_RESIZE_V) : (HUI_ECursor)0;
				return HUI_FALSE;
			}
		case HUI_EVENT_MOUSE_UP:
			if (splitter->drag_handle < 0 || event->button != HUI_MOUSE_LEFT) return HUI_FALSE;
			splitter->drag_handle = -1;
			hui_widget_release_mouse(widget);
			hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_LEAVE:
			if (splitter->hot_handle >= 0 && splitter->drag_handle < 0) {
				splitter->hot_handle = -1;
				widget->cursor = (HUI_ECursor)0;
				hui_widget_invalidate_paint(widget);
			}
			return HUI_FALSE;
		default:
			return HUI_FALSE;
	}
}

static void hui_splitter_paint_overlay(hui_widget_t* widget, HUI_Painter* painter) {
	hui_splitter_t* splitter = (hui_splitter_t*)widget;
	int horizontal = hui_splitter_horizontal(widget);
	int count = (int)hui_splitter_pane_count(widget) - 1, i;
	for (i = 0; i < count; i++) {
		hui_rect_t rect, line;
		int hot = i == splitter->drag_handle || (i == splitter->hot_handle && splitter->drag_handle < 0);
		if (!hui_splitter_handle_rect(widget, i, &rect, NULL, NULL)) continue;
		if (hot) {
			hui_paint_rect(painter, rect, hui_widget_color(widget, HUI_COLOR_ACCENT));
		} else {
			/* at rest, a thin line in the middle of the handle */
			line = horizontal ? hui_rect(rect.x + floorf(rect.w * 0.5f), rect.y, 1.0f, rect.h)
				: hui_rect(rect.x, rect.y + floorf(rect.h * 0.5f), rect.w, 1.0f);
			hui_paint_rect(painter, line, hui_widget_color(widget, HUI_COLOR_SEPARATOR));
		}
	}
}

static int hui_splitter_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_splitter_t* splitter = (hui_splitter_t*)widget;
	if (HUI_ATTR("Orientation")) {
		if (!hui_parse_orientation(value, &splitter->orientation)) return HUI_FALSE;
	} else if (HUI_ATTR("MinPaneSize")) {
		if (!hui_parse_float(value, &splitter->min_pane)) return HUI_FALSE;
	} else {
		return HUI_FALSE;
	}
	hui_widget_invalidate_layout(widget);
	return HUI_TRUE;
}

const hui_widget_class_t hui_class_splitter = {
	.type = HUI_WIDGET_SPLITTER,
	.name = "Splitter",
	.size = sizeof(hui_splitter_t),
	.init = hui_splitter_init,
	.measure = hui_splitter_measure,
	.arrange = hui_splitter_arrange,
	.paint_overlay = hui_splitter_paint_overlay,
	.event = hui_splitter_event,
	.set_attribute = hui_splitter_set_attribute
};

void HUI_SetSplitterOrientation(HUI_id id, HUI_EOrientation orientation) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SPLITTER);
	if (orientation != HUI_HORIZONTAL && orientation != HUI_VERTICAL) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetSplitterOrientation: invalid orientation");
		return;
	}
	((hui_splitter_t*)widget)->orientation = orientation;
	hui_widget_invalidate_layout(widget);
}

void HUI_SetSplitterMinPaneSize(HUI_id id, float size) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SPLITTER);
	((hui_splitter_t*)widget)->min_pane = size > 0.0f ? size : 0.0f;
}
