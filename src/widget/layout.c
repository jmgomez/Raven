/**
 * HUI - widget/layout.c
 * The two layout passes and their invalidation.
 *
 *   measure  (bottom-up)  every widget computes the size it would like,
 *                         from the desired sizes of its children;
 *   arrange  (top-down)   every panel gives its children their final bounds.
 *
 * Both results are cached per widget. A change marks the widget and its
 * ancestors dirty, and the next pass only visits the dirty branches:
 *   - a clean widget returns its cached desired size without recursion,
 *   - a widget whose size did not change and that is not dirty keeps the
 *     arrangement of its whole subtree (moving a subtree costs nothing).
 */

#include "widget.h"

void hui_widget_invalidate_layout(hui_widget_t* widget) {
	hui_widget_t* walk;
	/* Invariant: a dirty widget inside the laid-out tree has dirty ancestors,
	 * so the walk can stop at the first widget that is already fully dirty. */
	for (walk = widget; walk; walk = walk->parent) {
		if ((walk->flags & HUI_WF_LAYOUT_DIRTY) == HUI_WF_LAYOUT_DIRTY) break;
		walk->flags |= HUI_WF_LAYOUT_DIRTY;
	}
	if (widget->window) {
		widget->window->needs_layout = HUI_TRUE;
		widget->window->needs_paint = HUI_TRUE;
	}
}

void hui_widget_invalidate_paint(hui_widget_t* widget) {
	if (widget->window) widget->window->needs_paint = HUI_TRUE;
}

/* ---------------------------------------------------------------------------
 *  Measure
 * ------------------------------------------------------------------------- */

static hui_vec2_t hui_widget_measure_default(hui_widget_t* widget) {
	hui_vec2_t desired = hui_vec2(0.0f, 0.0f);
	uint32_t i;
	for (i = 0; i < widget->child_count; i++) {
		hui_vec2_t child = hui_widget_measure_slot(widget->children[i]);
		desired.x = hui_maxf(desired.x, child.x);
		desired.y = hui_maxf(desired.y, child.y);
	}
	return desired;
}

hui_vec2_t hui_widget_measure(hui_widget_t* widget) {
	hui_vec2_t desired;

	/* A collapsed widget is skipped and keeps its dirty state for later. */
	if (widget->visibility == HUI_COLLAPSED) return hui_vec2(0.0f, 0.0f);
	if (!(widget->flags & HUI_WF_MEASURE_DIRTY)) return widget->desired;

	/* cleared first: an invalidation raised while measuring must survive */
	widget->flags &= ~HUI_WF_MEASURE_DIRTY;
	desired = widget->cls->measure ? widget->cls->measure(widget) : hui_widget_measure_default(widget);

	if (widget->fixed_size.x >= 0.0f) desired.x = widget->fixed_size.x;
	if (widget->fixed_size.y >= 0.0f) desired.y = widget->fixed_size.y;
	if (widget->min_size.x >= 0.0f) desired.x = hui_maxf(desired.x, widget->min_size.x);
	if (widget->min_size.y >= 0.0f) desired.y = hui_maxf(desired.y, widget->min_size.y);
	if (widget->max_size.x >= 0.0f) desired.x = hui_minf(desired.x, widget->max_size.x);
	if (widget->max_size.y >= 0.0f) desired.y = hui_minf(desired.y, widget->max_size.y);

	widget->desired = desired;
	return desired;
}

hui_vec2_t hui_widget_measure_slot(hui_widget_t* child) {
	hui_vec2_t desired;
	if (child->visibility == HUI_COLLAPSED) return hui_vec2(0.0f, 0.0f);
	desired = hui_widget_measure(child);
	desired.x += child->slot.padding.l + child->slot.padding.r;
	desired.y += child->slot.padding.t + child->slot.padding.b;
	return desired;
}

/* ---------------------------------------------------------------------------
 *  Arrange
 * ------------------------------------------------------------------------- */

static void hui_widget_arrange_default(hui_widget_t* widget) {
	hui_rect_t area = hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h);
	uint32_t i;
	for (i = 0; i < widget->child_count; i++) {
		if (!hui_widget_collapsed(widget->children[i])) hui_widget_arrange_slot(widget->children[i], area);
	}
}

void hui_widget_arrange(hui_widget_t* widget, float x, float y, float width, float height) {
	int resized;
	if (width < 0.0f) width = 0.0f;
	if (height < 0.0f) height = 0.0f;
	resized = widget->rect.w != width || widget->rect.h != height;
	widget->rect = hui_rect(x, y, width, height);
	if (resized || (widget->flags & HUI_WF_ARRANGE_DIRTY)) {
		/* cleared first: an invalidation raised while arranging must survive */
		widget->flags &= ~HUI_WF_ARRANGE_DIRTY;
		if (widget->cls->arrange) widget->cls->arrange(widget);
		else hui_widget_arrange_default(widget);
	}
}

void hui_widget_arrange_slot(hui_widget_t* child, hui_rect_t area) {
	hui_vec2_t desired = child->desired;
	float x, y, width, height;

	area = hui_rect_inset(area, child->slot.padding);

	switch (child->slot.halign) {
		case HUI_HALIGN_LEFT:
			width = hui_minf(desired.x, area.w);
			x = area.x;
			break;
		case HUI_HALIGN_CENTER:
			width = hui_minf(desired.x, area.w);
			x = area.x + (area.w - width) * 0.5f;
			break;
		case HUI_HALIGN_RIGHT:
			width = hui_minf(desired.x, area.w);
			x = area.x + area.w - width;
			break;
		case HUI_HALIGN_FILL:
		default:
			width = area.w;
			x = area.x;
			break;
	}
	switch (child->slot.valign) {
		case HUI_VALIGN_TOP:
			height = hui_minf(desired.y, area.h);
			y = area.y;
			break;
		case HUI_VALIGN_CENTER:
			height = hui_minf(desired.y, area.h);
			y = area.y + (area.h - height) * 0.5f;
			break;
		case HUI_VALIGN_BOTTOM:
			height = hui_minf(desired.y, area.h);
			y = area.y + area.h - height;
			break;
		case HUI_VALIGN_FILL:
		default:
			height = area.h;
			y = area.y;
			break;
	}
	hui_widget_arrange(child, x, y, width, height);
}
