/**
 * Raven - widgets/common.c
 * Helpers shared by the widget implementations.
 */

#include "common.h"

/* ---------------------------------------------------------------------------
 *  Text
 * ------------------------------------------------------------------------- */

float rvn_widget_font_size(const rvn_widget_t* widget, float size) {
	return size > 0.0f ? size : rvn_widget_metric(widget, R_METRIC_FONT_SIZE);
}

float rvn_widget_text_width(const rvn_widget_t* widget, Rid font, float size, const char* text) {
	rvn_font_t* resolved = rvn_font_resolve(font);
	if (!resolved || !text) return 0.0f;
	return rvn_text_width(resolved, rvn_widget_font_size(widget, size), text, -1);
}

float rvn_widget_line_height(const rvn_widget_t* widget, Rid font, float size) {
	rvn_font_t* resolved = rvn_font_resolve(font);
	rvn_font_metrics_t metrics;
	if (!resolved) return 0.0f;
	rvn_font_metrics(resolved, rvn_widget_font_size(widget, size), &metrics);
	return metrics.line_height;
}

int rvn_widget_alive(const rvn_widget_t* widget) {
	return (widget->flags & RVN_WF_DEAD) == 0;
}

/* ---------------------------------------------------------------------------
 *  Glass themes
 * ------------------------------------------------------------------------- */

float rvn_control_radius(const rvn_widget_t* widget, float height) {
	if (g_rvn.theme.glass) return height * 0.5f;
	return rvn_widget_metric(widget, R_METRIC_CORNER_RADIUS);
}

/* Color between two roles, with the alpha interpolated too. */
static rvn_color_t rvn_role_mix(const rvn_widget_t* widget, RColorRole from, RColorRole to, float t) {
	return rvn_color_mix(rvn_widget_color(widget, from), rvn_widget_color(widget, to), rvn_clampf(t, 0.0f, 1.0f));
}

void rvn_glass_style(const rvn_widget_t* widget, rvn_surface_t kind, float hover, float press, rvn_glass_t* glass) {
	const int dark = g_rvn.theme.dark;
	/* the rim catches more light on a pale surface */
	float rim = dark ? 0.55f : 0.85f;
	rvn_color_t tint;

	memset(glass, 0, sizeof(*glass));
	glass->frost = 1.0f;
	glass->refraction = 10.0f;
	glass->saturation = 1.5f;
	glass->shadow_color = rvn_widget_color(widget, R_COLOR_SHADOW);

	switch (kind) {
		case RVN_SURFACE_ACCENT:
			tint = rvn_role_mix(widget, R_COLOR_ACCENT, R_COLOR_ACCENT_HOVERED, hover);
			tint = rvn_color_mix(tint, rvn_widget_color(widget, R_COLOR_ACCENT_PRESSED), press);
			/* tinted glass: the color dominates but what is behind still shows */
			tint = rvn_color_fade(tint, 0.84f);
			glass->saturation = 1.3f;
			glass->shadow = 7.0f;
			break;
		case RVN_SURFACE_INPUT:
			tint = rvn_widget_color(widget, R_COLOR_INPUT);
			rim *= 0.55f;
			glass->refraction = 6.0f;
			break;
		case RVN_SURFACE_BAR:
			tint = rvn_widget_color(widget, R_COLOR_PANEL);
			rim *= 0.6f;
			glass->refraction = 8.0f;
			glass->bevel = 6.0f;
			break;
		case RVN_SURFACE_PANEL:
			tint = rvn_widget_color(widget, R_COLOR_PANEL);
			glass->refraction = 14.0f;
			glass->bevel = 12.0f;
			break;
		case RVN_SURFACE_POPUP:
			tint = rvn_widget_color(widget, R_COLOR_POPUP);
			glass->refraction = 14.0f;
			glass->bevel = 10.0f;
			glass->saturation = 1.7f;
			glass->shadow = 22.0f;
			break;
		case RVN_SURFACE_LENS:
			tint = rvn_rgba(1.0f, 1.0f, 1.0f, dark ? 0.06f : 0.16f);
			rim = 1.0f;
			glass->frost = 0.0f;
			glass->refraction = 16.0f;
			glass->saturation = 1.15f;
			glass->shadow = 9.0f;
			break;
		case RVN_SURFACE_CONTROL:
		default:
			tint = rvn_role_mix(widget, R_COLOR_CONTROL, R_COLOR_CONTROL_HOVERED, hover);
			tint = rvn_color_mix(tint, rvn_widget_color(widget, R_COLOR_CONTROL_PRESSED), press);
			glass->frost = 0.9f;
			glass->shadow = 6.0f;
			break;
	}
	glass->tint = tint;
	glass->rim = rvn_rgba(1.0f, 1.0f, 1.0f, rvn_clampf(rim + 0.25f * press, 0.0f, 1.0f));
	/* touched glass lights up */
	glass->glow = (dark ? 0.05f : 0.03f) * press;
}

void rvn_paint_surface(RPainter* painter, const rvn_widget_t* widget, rvn_rect_t rect, float radius,
	rvn_surface_t kind, float hover, float press) {
	rvn_glass_t glass;
	float radii[4];
	float opacity = painter->opacity;

	rvn_glass_style(widget, kind, hover, press, &glass);
	radii[0] = radii[1] = radii[2] = radii[3] = radius;
	if (!painter->enabled) {
		painter->opacity *= 0.5f;
		glass.shadow = 0.0f;
	}
	rvn_paint_glass(painter, rect, radii, &glass);
	painter->opacity = opacity;
}

void rvn_paint_row(RPainter* painter, rvn_rect_t row, rvn_color_t color) {
	if (g_rvn.theme.glass) {
		rvn_paint_box(painter, rvn_rect(row.x + 4.0f, row.y + 1.0f, row.w - 8.0f, row.h - 2.0f), 8.0f, color, 0.0f, 0);
	} else {
		rvn_paint_rect(painter, row, color);
	}
}

float rvn_widget_hover(rvn_widget_t* widget, RPainter* painter, int hovered) {
	return rvn_paint_animate(painter, &widget->anim[0], hovered ? 1.0f : 0.0f, 16.0f);
}

float rvn_widget_press(rvn_widget_t* widget, RPainter* painter, int pressed) {
	/* goes down at once, comes back up slowly */
	return rvn_paint_animate(painter, &widget->anim[1], pressed ? 1.0f : 0.0f, pressed ? 40.0f : 12.0f);
}

void rvn_paint_knob(RPainter* painter, const rvn_widget_t* widget, rvn_rect_t rect, float press) {
	float radius = rvn_minf(rect.w, rect.h) * 0.5f;
	if (press > 0.02f && painter->enabled) {
		/* picked up: the knob grows and turns into a drop of clear glass */
		float grow = 3.0f * press;
		rvn_rect_t lens = rvn_rect(rect.x - grow * 1.4f, rect.y - grow, rect.w + grow * 2.8f, rect.h + grow * 2.0f);
		float opacity = painter->opacity;
		rvn_paint_surface(painter, widget, lens, radius + grow, RVN_SURFACE_LENS, 0.0f, 1.0f);
		/* the solid knob fades into it */
		painter->opacity *= 1.0f - press;
		rvn_paint_box(painter, rect, radius, rvn_rgba(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, 0);
		painter->opacity = opacity;
		return;
	}
	rvn_paint_shadow(painter, rvn_rect(rect.x, rect.y + 1.5f, rect.w, rect.h), radius, 5.0f, rvn_rgba(0.0f, 0.0f, 0.0f, 0.28f));
	rvn_paint_box(painter, rect, radius, painter->enabled ? rvn_rgba(1.0f, 1.0f, 1.0f, 1.0f) : rvn_rgba(0.85f, 0.85f, 0.87f, 1.0f), 0.0f, 0);
}

void rvn_paint_focus(const rvn_widget_t* widget, RPainter* painter, float radius) {
	if (!(widget->flags & RVN_WF_FOCUSED) || !widget->window || !widget->window->focus_visible) return;
	rvn_paint_box(painter, rvn_rect(-2.0f, -2.0f, widget->rect.w + 4.0f, widget->rect.h + 4.0f), radius + 2.0f,
		0, 2.0f, rvn_widget_color(widget, R_COLOR_FOCUS));
}

/* ---------------------------------------------------------------------------
 *  Scrollbar
 * ------------------------------------------------------------------------- */

float rvn_scrollbar_max(const rvn_scrollbar_t* bar) {
	return rvn_maxf(0.0f, bar->content - bar->viewport);
}

int rvn_scrollbar_needed(const rvn_scrollbar_t* bar) {
	return bar->content > bar->viewport + 0.5f;
}

int rvn_scrollbar_set(rvn_scrollbar_t* bar, float offset) {
	float clamped = rvn_clampf(offset, 0.0f, rvn_scrollbar_max(bar));
	/* whole logical units keep scrolled content on the pixel grid at scale 1 */
	clamped = floorf(clamped + 0.5f);
	if (clamped > rvn_scrollbar_max(bar)) clamped = rvn_scrollbar_max(bar);
	if (clamped == bar->offset) return R_FALSE;
	bar->offset = clamped;
	return R_TRUE;
}

int rvn_scrollbar_clamp(rvn_scrollbar_t* bar) {
	return rvn_scrollbar_set(bar, bar->offset);
}

static void rvn_scrollbar_thumb(const rvn_scrollbar_t* bar, float track_length, float* start, float* length) {
	float ratio = bar->content > 0.0f ? rvn_clampf(bar->viewport / bar->content, 0.0f, 1.0f) : 1.0f;
	float max = rvn_scrollbar_max(bar);
	*length = rvn_maxf(track_length * ratio, rvn_minf(24.0f, track_length));
	*start = max > 0.0f ? (track_length - *length) * (bar->offset / max) : 0.0f;
}

void rvn_scrollbar_paint(const rvn_scrollbar_t* bar, const rvn_widget_t* widget, RPainter* painter,
	rvn_rect_t track, int vertical, float mouse_x, float mouse_y) {

	float start, length, thickness = vertical ? track.w : track.h;
	rvn_rect_t thumb;
	int hot;

	if (!rvn_scrollbar_needed(bar)) return;
	rvn_scrollbar_thumb(bar, vertical ? track.h : track.w, &start, &length);
	if (vertical) thumb = rvn_rect(track.x + 2.0f, track.y + start + 2.0f, thickness - 4.0f, length - 4.0f);
	else thumb = rvn_rect(track.x + start + 2.0f, track.y + 2.0f, length - 4.0f, thickness - 4.0f);

	hot = bar->dragging || ((widget->flags & RVN_WF_HOVERED) && rvn_rect_contains(track, mouse_x, mouse_y));
	if (g_rvn.theme.glass) {
		/* no track: a thin capsule over the content, thicker under the pointer */
		float slim = hot ? 1.0f : 2.0f;
		if (vertical) thumb = rvn_rect(thumb.x + slim, thumb.y, thumb.w - slim * 2.0f, thumb.h);
		else thumb = rvn_rect(thumb.x, thumb.y + slim, thumb.w, thumb.h - slim * 2.0f);
		rvn_paint_box(painter, thumb, thickness, rvn_widget_color(widget, hot ? R_COLOR_HANDLE_HOVERED : R_COLOR_HANDLE), 0.0f, 0);
		return;
	}
	rvn_paint_rect(painter, track, rvn_color_fade(rvn_widget_color(widget, R_COLOR_TRACK), 0.6f));
	rvn_paint_box(painter, thumb, (thickness - 4.0f) * 0.5f,
		rvn_widget_color(widget, hot ? R_COLOR_HANDLE_HOVERED : R_COLOR_HANDLE), 0.0f, 0);
}

int rvn_scrollbar_event(rvn_scrollbar_t* bar, rvn_widget_t* widget, const REvent* event,
	rvn_rect_t track, int vertical, int* changed) {

	float track_length = vertical ? track.h : track.w;
	float position = vertical ? event->local_y - track.y : event->local_x - track.x;
	float start, length, travel;

	*changed = R_FALSE;
	if (!bar->dragging && !rvn_scrollbar_needed(bar)) return R_FALSE;
	rvn_scrollbar_thumb(bar, track_length, &start, &length);
	travel = track_length - length;

	switch (event->type) {
		case R_EVENT_MOUSE_DOWN:
			if (event->button != R_MOUSE_LEFT || !rvn_rect_contains(track, event->local_x, event->local_y)) return R_FALSE;
			if (position < start || position > start + length) {
				/* click in the track: bring the middle of the thumb under the cursor */
				if (travel > 0.0f) *changed = rvn_scrollbar_set(bar, (position - length * 0.5f) / travel * rvn_scrollbar_max(bar));
				rvn_scrollbar_thumb(bar, track_length, &start, &length);
			}
			bar->dragging = R_TRUE;
			bar->grab = position - start;
			rvn_widget_capture_mouse(widget);
			return R_TRUE;
		case R_EVENT_MOUSE_MOVE:
			if (!bar->dragging) return R_FALSE;
			if (travel > 0.0f) *changed = rvn_scrollbar_set(bar, (position - bar->grab) / travel * rvn_scrollbar_max(bar));
			return R_TRUE;
		case R_EVENT_MOUSE_UP:
			if (!bar->dragging || event->button != R_MOUSE_LEFT) return R_FALSE;
			bar->dragging = R_FALSE;
			rvn_widget_release_mouse(widget);
			return R_TRUE;
		default:
			return R_FALSE;
	}
}
