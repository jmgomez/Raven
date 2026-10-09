/**
 * HUI - widgets/common.c
 * Helpers shared by the widget implementations.
 */

#include "common.h"

/* ---------------------------------------------------------------------------
 *  Text
 * ------------------------------------------------------------------------- */

float hui_widget_font_size(const hui_widget_t* widget, float size) {
	return size > 0.0f ? size : hui_widget_metric(widget, HUI_METRIC_FONT_SIZE);
}

float hui_widget_text_width(const hui_widget_t* widget, HUI_id font, float size, const char* text) {
	hui_font_t* resolved = hui_font_resolve(font);
	if (!resolved || !text) return 0.0f;
	return hui_text_width(resolved, hui_widget_font_size(widget, size), text, -1);
}

float hui_widget_line_height(const hui_widget_t* widget, HUI_id font, float size) {
	hui_font_t* resolved = hui_font_resolve(font);
	hui_font_metrics_t metrics;
	if (!resolved) return 0.0f;
	hui_font_metrics(resolved, hui_widget_font_size(widget, size), &metrics);
	return metrics.line_height;
}

int hui_widget_alive(const hui_widget_t* widget) {
	return (widget->flags & HUI_WF_DEAD) == 0;
}

void hui_paint_focus(const hui_widget_t* widget, HUI_Painter* painter, float radius) {
	if (!(widget->flags & HUI_WF_FOCUSED) || !widget->window || !widget->window->focus_visible) return;
	hui_paint_box(painter, hui_rect(-2.0f, -2.0f, widget->rect.w + 4.0f, widget->rect.h + 4.0f), radius + 2.0f,
		0, 2.0f, hui_widget_color(widget, HUI_COLOR_FOCUS));
}

/* ---------------------------------------------------------------------------
 *  Scrollbar
 * ------------------------------------------------------------------------- */

float hui_scrollbar_max(const hui_scrollbar_t* bar) {
	return hui_maxf(0.0f, bar->content - bar->viewport);
}

int hui_scrollbar_needed(const hui_scrollbar_t* bar) {
	return bar->content > bar->viewport + 0.5f;
}

int hui_scrollbar_set(hui_scrollbar_t* bar, float offset) {
	float clamped = hui_clampf(offset, 0.0f, hui_scrollbar_max(bar));
	/* whole logical units keep scrolled content on the pixel grid at scale 1 */
	clamped = floorf(clamped + 0.5f);
	if (clamped > hui_scrollbar_max(bar)) clamped = hui_scrollbar_max(bar);
	if (clamped == bar->offset) return HUI_FALSE;
	bar->offset = clamped;
	return HUI_TRUE;
}

int hui_scrollbar_clamp(hui_scrollbar_t* bar) {
	return hui_scrollbar_set(bar, bar->offset);
}

static void hui_scrollbar_thumb(const hui_scrollbar_t* bar, float track_length, float* start, float* length) {
	float ratio = bar->content > 0.0f ? hui_clampf(bar->viewport / bar->content, 0.0f, 1.0f) : 1.0f;
	float max = hui_scrollbar_max(bar);
	*length = hui_maxf(track_length * ratio, hui_minf(24.0f, track_length));
	*start = max > 0.0f ? (track_length - *length) * (bar->offset / max) : 0.0f;
}

void hui_scrollbar_paint(const hui_scrollbar_t* bar, const hui_widget_t* widget, HUI_Painter* painter,
	hui_rect_t track, int vertical, float mouse_x, float mouse_y) {

	float start, length, thickness = vertical ? track.w : track.h;
	hui_rect_t thumb;
	int hot;

	if (!hui_scrollbar_needed(bar)) return;
	hui_scrollbar_thumb(bar, vertical ? track.h : track.w, &start, &length);
	if (vertical) thumb = hui_rect(track.x + 2.0f, track.y + start + 2.0f, thickness - 4.0f, length - 4.0f);
	else thumb = hui_rect(track.x + start + 2.0f, track.y + 2.0f, length - 4.0f, thickness - 4.0f);

	hot = bar->dragging || ((widget->flags & HUI_WF_HOVERED) && hui_rect_contains(track, mouse_x, mouse_y));
	hui_paint_rect(painter, track, hui_color_fade(hui_widget_color(widget, HUI_COLOR_TRACK), 0.6f));
	hui_paint_box(painter, thumb, (thickness - 4.0f) * 0.5f,
		hui_widget_color(widget, hot ? HUI_COLOR_HANDLE_HOVERED : HUI_COLOR_HANDLE), 0.0f, 0);
}

int hui_scrollbar_event(hui_scrollbar_t* bar, hui_widget_t* widget, const HUI_Event* event,
	hui_rect_t track, int vertical, int* changed) {

	float track_length = vertical ? track.h : track.w;
	float position = vertical ? event->local_y - track.y : event->local_x - track.x;
	float start, length, travel;

	*changed = HUI_FALSE;
	if (!bar->dragging && !hui_scrollbar_needed(bar)) return HUI_FALSE;
	hui_scrollbar_thumb(bar, track_length, &start, &length);
	travel = track_length - length;

	switch (event->type) {
		case HUI_EVENT_MOUSE_DOWN:
			if (event->button != HUI_MOUSE_LEFT || !hui_rect_contains(track, event->local_x, event->local_y)) return HUI_FALSE;
			if (position < start || position > start + length) {
				/* click in the track: bring the middle of the thumb under the cursor */
				if (travel > 0.0f) *changed = hui_scrollbar_set(bar, (position - length * 0.5f) / travel * hui_scrollbar_max(bar));
				hui_scrollbar_thumb(bar, track_length, &start, &length);
			}
			bar->dragging = HUI_TRUE;
			bar->grab = position - start;
			hui_widget_capture_mouse(widget);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_MOVE:
			if (!bar->dragging) return HUI_FALSE;
			if (travel > 0.0f) *changed = hui_scrollbar_set(bar, (position - bar->grab) / travel * hui_scrollbar_max(bar));
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_UP:
			if (!bar->dragging || event->button != HUI_MOUSE_LEFT) return HUI_FALSE;
			bar->dragging = HUI_FALSE;
			hui_widget_release_mouse(widget);
			return HUI_TRUE;
		default:
			return HUI_FALSE;
	}
}
