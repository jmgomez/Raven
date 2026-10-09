/**
 * Raven - widget/window.c
 * Windows: layout and paint scheduling, input routing (hover, focus, mouse
 * capture, popups, tooltips, shortcuts) and the public window API.
 */

#include "widget.h"
#include "../render/renderer.h"

#define RVN_MAX_FRAMES_IN_FLIGHT 2
/** Backdrop captures per frame (glass). The first one blurs the whole window, the others an area of it. */
#define RVN_MAX_BACKDROPS 24
#define RVN_DOUBLE_CLICK_TIME 0.4
#define RVN_DOUBLE_CLICK_DISTANCE 4.0f
#define RVN_TOOLTIP_DELAY 0.6
#define RVN_MAX_PATH_DEPTH 128

/* ---------------------------------------------------------------------------
 *  Lookup
 * ------------------------------------------------------------------------- */

rvn_window_t* rvn_window_get(Rid id) {
	rvn_window_t* window = (rvn_window_t*)rvn_pool_get(&g_rvn.windows, id);
	return (window && !window->dead) ? window : NULL;
}

rvn_window_t* rvn_window_check(Rid id, const char* function) {
	rvn_window_t* window;
	if (!rvn_api_check(function)) return NULL;
	window = rvn_window_get(id);
	if (!window) rvn_error(R_ERROR_INVALID_ID, R_SEVERITY_ERROR, "%s: invalid window handle 0x%08X", function, (unsigned)id);
	return window;
}

#define RVN_WINDOW(variable, id, result) \
	rvn_window_t* variable = rvn_window_check((id), __func__); \
	if (!variable) return result

#define RVN_WINDOW_VOID(variable, id) \
	rvn_window_t* variable = rvn_window_check((id), __func__); \
	if (!variable) return

rvn_vec2_t rvn_window_size(const rvn_window_t* window) {
	float scale = window->scale > 0.0f ? window->scale : 1.0f;
	return rvn_vec2((float)window->width_px / scale, (float)window->height_px / scale);
}

static REvent rvn_window_event(rvn_window_t* window, REventType type) {
	REvent event;
	memset(&event, 0, sizeof(event));
	event.type = type;
	event.window = window->id;
	event.target = R_INVALID_ID;
	event.x = window->mouse_x;
	event.y = window->mouse_y;
	event.modifiers = window->modifiers;
	return event;
}

/* ---------------------------------------------------------------------------
 *  Tooltip
 * ------------------------------------------------------------------------- */

static void rvn_window_tooltip_hide(rvn_window_t* window) {
	if (window->tooltip_timer != R_INVALID_ID) {
		Rtimer_destroy(window->tooltip_timer);
		window->tooltip_timer = R_INVALID_ID;
	}
	if (window->tooltip_visible) {
		window->tooltip_visible = R_FALSE;
		window->needs_paint = R_TRUE;
	}
	window->tooltip_widget = NULL;
}

static void rvn_window_tooltip_fire(Rid timer, void* user_data) {
	rvn_window_t* window = rvn_window_get((Rid)(uintptr_t)user_data);
	RVN_UNUSED(timer);
	if (!window) return;
	window->tooltip_timer = R_INVALID_ID;
	if (window->tooltip_widget && window->tooltip_widget->tooltip) {
		window->tooltip_visible = R_TRUE;
		window->tooltip_x = window->mouse_x;
		window->tooltip_y = window->mouse_y;
		window->needs_paint = R_TRUE;
	}
}

/* Arms the tooltip of the nearest widget with a tooltip text under the cursor. */
static void rvn_window_tooltip_arm(rvn_window_t* window) {
	rvn_widget_t* walk;
	rvn_window_tooltip_hide(window);
	for (walk = window->hover; walk; walk = walk->parent) {
		if (walk->tooltip) break;
		if (walk->flags & RVN_WF_POPUP) {
			walk = NULL;
			break;
		}
	}
	if (!walk) return;
	window->tooltip_widget = walk;
	window->tooltip_timer = Rtimer_create(RVN_TOOLTIP_DELAY, R_FALSE, rvn_window_tooltip_fire,
		(void*)(uintptr_t)window->id);
}

static void rvn_window_paint_tooltip(rvn_window_t* window, RPainter* painter) {
	const char* text;
	const char* line;
	rvn_font_t* font;
	rvn_font_metrics_t metrics;
	rvn_vec2_t size = rvn_window_size(window);
	float font_size = RVN_THEME_METRIC(R_METRIC_FONT_SIZE);
	float width = 0.0f, height, x, y;
	int lines = 0;

	if (!window->tooltip_visible || !window->tooltip_widget || !window->tooltip_widget->tooltip) return;
	text = window->tooltip_widget->tooltip;
	font = rvn_font_resolve(R_INVALID_ID);
	if (!font) return;
	rvn_font_metrics(font, font_size, &metrics);

	for (line = text;;) {
		const char* newline = strchr(line, '\n');
		int length = newline ? (int)(newline - line) : (int)strlen(line);
		width = rvn_maxf(width, rvn_text_width(font, font_size, line, length));
		lines++;
		if (!newline) break;
		line = newline + 1;
	}
	width += 16.0f;
	height = metrics.line_height * (float)lines + 10.0f;
	x = rvn_clampf(window->tooltip_x + 12.0f, 0.0f, rvn_maxf(0.0f, size.x - width));
	y = window->tooltip_y + 20.0f;
	if (y + height > size.y) y = window->tooltip_y - height - 6.0f;
	if (y < 0.0f) y = 0.0f;

	if (g_rvn.theme.glass) {
		rvn_glass_t glass;
		float radii[4];
		memset(&glass, 0, sizeof(glass));
		glass.tint = RVN_THEME_COLOR(R_COLOR_TOOLTIP);
		glass.rim = rvn_rgba(1.0f, 1.0f, 1.0f, g_rvn.theme.dark ? 0.5f : 0.8f);
		glass.frost = 1.0f;
		glass.refraction = 8.0f;
		glass.saturation = 1.6f;
		glass.shadow = 12.0f;
		glass.shadow_color = RVN_THEME_COLOR(R_COLOR_SHADOW);
		radii[0] = radii[1] = radii[2] = radii[3] = rvn_minf(height * 0.5f, 12.0f);
		rvn_paint_glass(painter, rvn_rect(x, y, width, height), radii, &glass);
	} else {
		rvn_paint_shadow(painter, rvn_rect(x, y + 2.0f, width, height), 4.0f, 8.0f, RVN_THEME_COLOR(R_COLOR_SHADOW));
		rvn_paint_box(painter, rvn_rect(x, y, width, height), 4.0f, RVN_THEME_COLOR(R_COLOR_TOOLTIP),
			1.0f, RVN_THEME_COLOR(R_COLOR_POPUP_BORDER));
	}
	y += 5.0f;
	for (line = text;;) {
		const char* newline = strchr(line, '\n');
		int length = newline ? (int)(newline - line) : (int)strlen(line);
		rvn_paint_text(painter, R_INVALID_ID, font_size, x + 8.0f, y, line, length, RVN_THEME_COLOR(R_COLOR_TOOLTIP_TEXT));
		y += metrics.line_height;
		if (!newline) break;
		line = newline + 1;
	}
}

/* ---------------------------------------------------------------------------
 *  Hover and cursor
 * ------------------------------------------------------------------------- */

/* Collects a widget and its ancestors, up to and including the popup root. */
static int rvn_hover_path(rvn_widget_t* from, rvn_widget_t** path) {
	int count = 0;
	rvn_widget_t* walk;
	for (walk = from; walk && count < RVN_MAX_PATH_DEPTH; walk = walk->parent) {
		path[count++] = walk;
		if (walk->flags & RVN_WF_POPUP) break;
	}
	return count;
}

static void rvn_hover_clear_flags(rvn_widget_t* from) {
	rvn_widget_t* walk;
	for (walk = from; walk; walk = walk->parent) walk->flags &= ~RVN_WF_HOVERED;
}

static void rvn_window_set_hover(rvn_window_t* window, rvn_widget_t* target) {
	rvn_widget_t* old_path[RVN_MAX_PATH_DEPTH];
	rvn_widget_t* new_path[RVN_MAX_PATH_DEPTH];
	Rid left[RVN_MAX_PATH_DEPTH];
	Rid entered[RVN_MAX_PATH_DEPTH];
	int old_count, new_count, left_count = 0, entered_count = 0, i, j;
	REvent event;

	if (window->hover == target) return;

	old_count = window->hover ? rvn_hover_path(window->hover, old_path) : 0;
	new_count = target ? rvn_hover_path(target, new_path) : 0;
	for (i = 0; i < old_count; i++) {
		for (j = 0; j < new_count && new_path[j] != old_path[i]; j++) {}
		if (j == new_count) left[left_count++] = old_path[i]->id;
	}
	for (i = 0; i < new_count; i++) {
		for (j = 0; j < old_count && old_path[j] != new_path[i]; j++) {}
		if (j == old_count) entered[entered_count++] = new_path[i]->id;
	}

	rvn_hover_clear_flags(window->hover);
	for (i = 0; i < new_count; i++) new_path[i]->flags |= RVN_WF_HOVERED;
	window->hover = target;
	window->needs_paint = R_TRUE;

	/* notify through handles: a callback may delete widgets of either path */
	for (i = 0; i < left_count; i++) {
		rvn_widget_t* widget = rvn_widget_get(left[i]);
		if (!widget) continue;
		event = rvn_window_event(window, R_EVENT_MOUSE_LEAVE);
		rvn_widget_send(widget, &event);
	}
	for (i = entered_count; i > 0; i--) {
		rvn_widget_t* widget = rvn_widget_get(entered[i - 1]);
		if (!widget) continue;
		event = rvn_window_event(window, R_EVENT_MOUSE_ENTER);
		rvn_widget_send(widget, &event);
	}
	if (!window->dead) rvn_window_tooltip_arm(window);
}

static rvn_widget_t* rvn_window_hit(rvn_window_t* window, float x, float y, int* popup_index) {
	uint32_t i;
	for (i = window->popup_count; i > 0; i--) {
		rvn_widget_t* hit = rvn_widget_hit_test(window->popups[i - 1].root, x, y);
		if (hit) {
			if (popup_index) *popup_index = (int)i - 1;
			return hit;
		}
	}
	if (popup_index) *popup_index = -1;
	return window->root ? rvn_widget_hit_test(window->root, x, y) : NULL;
}

static void rvn_window_update_cursor(rvn_window_t* window) {
	RCursor cursor = R_CURSOR_ARROW;
	rvn_widget_t* walk = window->capture ? window->capture : window->hover;
	for (; walk; walk = walk->parent) {
		if (walk->cursor != 0) {
			cursor = walk->cursor;
			break;
		}
		if (walk->flags & RVN_WF_POPUP) break;
	}
	if (cursor != window->cursor) {
		window->cursor = cursor;
		if (window->platform) g_rvn.platform->window_set_cursor(window->platform, cursor);
	}
}

/* Recomputes the hovered widget for a cursor that did not move (after a layout). */
static void rvn_window_refresh_hover(rvn_window_t* window) {
	if (window->capture || !window->mouse_inside) return;
	rvn_window_set_hover(window, rvn_window_hit(window, window->mouse_x, window->mouse_y, NULL));
	if (!window->dead) rvn_window_update_cursor(window);
}

/* ---------------------------------------------------------------------------
 *  Focus
 * ------------------------------------------------------------------------- */

void rvn_window_set_focus(rvn_window_t* window, rvn_widget_t* widget) {
	rvn_widget_t* old = window->focus;
	REvent event;
	if (old == widget) return;
	window->focus = widget;
	window->needs_paint = R_TRUE;
	if (old) {
		old->flags &= ~RVN_WF_FOCUSED;
		event = rvn_window_event(window, R_EVENT_FOCUS_LOST);
		rvn_widget_send(old, &event);
	}
	/* the callback above may have changed the focus again */
	if (widget && window->focus == widget && !(widget->flags & RVN_WF_DEAD)) {
		widget->flags |= RVN_WF_FOCUSED;
		event = rvn_window_event(window, R_EVENT_FOCUS_GAINED);
		rvn_widget_send(widget, &event);
	}
}

typedef struct rvn_focus_list_t {
	rvn_widget_t** items;
	uint32_t count;
	uint32_t cap;
} rvn_focus_list_t;

static void rvn_focus_collect(rvn_widget_t* widget, rvn_focus_list_t* list) {
	uint32_t i;
	if (widget->visibility != R_VISIBLE || (widget->flags & RVN_WF_DISABLED)) return;
	if (widget->flags & RVN_WF_FOCUSABLE) {
		RVN_ARRAY_RESERVE(list->items, list->cap, list->count + 1);
		list->items[list->count++] = widget;
	}
	for (i = 0; i < widget->child_count; i++) {
		rvn_widget_t* child = widget->children[i];
		if (widget->cls->child_active && !widget->cls->child_active(widget, child)) continue;
		rvn_focus_collect(child, list);
	}
}

static void rvn_window_focus_next(rvn_window_t* window, int direction) {
	rvn_focus_list_t list;
	uint32_t i, index = 0;
	int found = R_FALSE;

	memset(&list, 0, sizeof(list));
	if (window->root) rvn_focus_collect(window->root, &list);
	if (list.count == 0) {
		rvn_free(list.items);
		return;
	}
	for (i = 0; i < list.count; i++) {
		if (list.items[i] == window->focus) {
			index = i;
			found = R_TRUE;
			break;
		}
	}
	if (!found) index = direction > 0 ? 0 : list.count - 1;
	else index = (index + (direction > 0 ? 1u : list.count - 1u)) % list.count;
	window->focus_visible = R_TRUE;
	rvn_window_set_focus(window, list.items[index]);
	rvn_free(list.items);
}

/* ---------------------------------------------------------------------------
 *  Popups
 * ------------------------------------------------------------------------- */

int rvn_window_popup_index(const rvn_window_t* window, const rvn_widget_t* root) {
	uint32_t i;
	for (i = 0; i < window->popup_count; i++) {
		if (window->popups[i].root == root) return (int)i;
	}
	return -1;
}

void rvn_window_open_popup(rvn_window_t* window, rvn_widget_t* root, rvn_widget_t* owner,
	rvn_rect_t anchor, rvn_popup_placement_t placement, float min_width) {

	rvn_popup_t* popup;
	if (rvn_window_popup_index(window, root) >= 0) return;
	RVN_ARRAY_RESERVE(window->popups, window->popup_cap, window->popup_count + 1);
	popup = &window->popups[window->popup_count++];
	popup->root = root;
	popup->owner = owner;
	popup->anchor = anchor;
	popup->placement = placement;
	popup->min_width = min_width;
	root->flags |= RVN_WF_POPUP | RVN_WF_LAYOUT_DIRTY;
	if (root->window != window) rvn_widget_set_window(root, window);
	rvn_window_tooltip_hide(window);
	window->needs_layout = R_TRUE;
	window->needs_paint = R_TRUE;
}

void rvn_window_close_popup(rvn_window_t* window, rvn_widget_t* root) {
	int index = rvn_window_popup_index(window, root);
	if (index < 0) return;
	while ((int)window->popup_count > index) {
		rvn_popup_t popup = window->popups[--window->popup_count];
		rvn_widget_t* closed = popup.root;
		if (window->hover && rvn_widget_is_inside(window->hover, closed)) {
			rvn_hover_clear_flags(window->hover);
			window->hover = NULL;
		}
		if (window->capture && rvn_widget_is_inside(window->capture, closed)) window->capture = NULL;
		closed->flags &= ~RVN_WF_POPUP;
		/* a popup that is not part of the tree leaves the window */
		if (!closed->parent) rvn_widget_set_window(closed, NULL);
		if (closed->cls->popup_closed) closed->cls->popup_closed(closed);
	}
	window->needs_layout = R_TRUE;
	window->needs_paint = R_TRUE;
}

void rvn_window_close_popups(rvn_window_t* window) {
	if (window->popup_count > 0) rvn_window_close_popup(window, window->popups[0].root);
}

static void rvn_window_layout_popup(rvn_window_t* window, rvn_popup_t* popup) {
	rvn_vec2_t area = rvn_window_size(window);
	rvn_vec2_t desired = rvn_widget_measure(popup->root);
	float width = rvn_maxf(desired.x, popup->min_width);
	float height = desired.y;
	float x = popup->anchor.x, y = popup->anchor.y;

	if (width > area.x) width = area.x;
	if (height > area.y) height = area.y;

	switch (popup->placement) {
		case RVN_POPUP_BELOW:
			y = popup->anchor.y + popup->anchor.h;
			if (y + height > area.y && popup->anchor.y - height >= 0.0f) y = popup->anchor.y - height;
			break;
		case RVN_POPUP_RIGHT:
			x = popup->anchor.x + popup->anchor.w;
			if (x + width > area.x && popup->anchor.x - width >= 0.0f) x = popup->anchor.x - width;
			break;
		case RVN_POPUP_AT:
			break;
	}
	x = rvn_clampf(x, 0.0f, rvn_maxf(0.0f, area.x - width));
	y = rvn_clampf(y, 0.0f, rvn_maxf(0.0f, area.y - height));
	rvn_widget_arrange(popup->root, floorf(x), floorf(y), width, height);
}

/* ---------------------------------------------------------------------------
 *  References to widgets
 * ------------------------------------------------------------------------- */

void rvn_window_forget_widget(rvn_window_t* window, rvn_widget_t* widget) {
	uint32_t i;
	if (window->capture && rvn_widget_is_inside(window->capture, widget)) window->capture = NULL;
	if (window->focus && rvn_widget_is_inside(window->focus, widget)) {
		window->focus->flags &= ~RVN_WF_FOCUSED;
		window->focus = NULL;
	}
	if (window->hover && rvn_widget_is_inside(window->hover, widget)) {
		rvn_hover_clear_flags(window->hover);
		window->hover = NULL;
	}
	if (window->tooltip_widget && rvn_widget_is_inside(window->tooltip_widget, widget)) {
		rvn_window_tooltip_hide(window);
	}
	/* close the popups shown from this subtree, lowest first (it closes the ones above) */
	for (i = 0; i < window->popup_count; i++) {
		rvn_popup_t* popup = &window->popups[i];
		if (rvn_widget_is_inside(popup->root, widget) || (popup->owner && rvn_widget_is_inside(popup->owner, widget))) {
			rvn_window_close_popup(window, popup->root);
			break;
		}
	}
	window->needs_paint = R_TRUE;
}

void rvn_window_release_natives(rvn_window_t* window, rvn_widget_t* subtree) {
	uint32_t i;
	for (i = window->native_viewport_count; i > 0; i--) {
		rvn_widget_t* viewport = window->native_viewports[i - 1];
		if (rvn_widget_is_inside(viewport, subtree)) rvn_viewport_native_release(viewport, window);
	}
}

/* ---------------------------------------------------------------------------
 *  Layout
 * ------------------------------------------------------------------------- */

void rvn_window_layout(rvn_window_t* window) {
	double start = rvn_clock_seconds();
	rvn_vec2_t size = rvn_window_size(window);
	int pass;
	uint32_t i;

	/* a second pass is needed when the arrangement changes a desired size (wrapped text) */
	for (pass = 0; pass < 4 && window->needs_layout; pass++) {
		window->needs_layout = R_FALSE;
		if (window->root) {
			rvn_widget_measure(window->root);
			/* the content obeys its slot like any child: padding and alignment in the window */
			rvn_widget_arrange_slot(window->root, rvn_rect(0.0f, 0.0f, size.x, size.y));
		}
		for (i = 0; i < window->popup_count; i++) rvn_window_layout_popup(window, &window->popups[i]);
	}
	window->needs_layout = R_FALSE;
	window->needs_paint = R_TRUE;
	window->stats.layout_ms = (rvn_clock_seconds() - start) * 1000.0;
	rvn_window_refresh_hover(window);
}

/* ---------------------------------------------------------------------------
 *  Paint
 * ------------------------------------------------------------------------- */

/* The image behind the content: the one of the window, else the wallpaper of a glass theme. */
static void rvn_window_paint_background(rvn_window_t* window, RPainter* painter) {
	Rid texture = rvn_texture_get(window->background) ? window->background : R_INVALID_ID;
	rvn_texture_t* object;
	rvn_vec2_t size = rvn_window_size(window);
	float scale, visible_u, visible_v, uv[4];

	/* a window with its own clear color keeps it */
	if (texture == R_INVALID_ID && !window->has_clear_color) texture = g_rvn.theme.wallpaper;
	object = rvn_texture_get(texture);
	if (!object || !object->created || object->width <= 0 || object->height <= 0 || size.x <= 0.0f || size.y <= 0.0f) return;

	/* covers the window whatever its proportions: the centered part of the image with the same ratio */
	scale = rvn_maxf(size.x / (float)object->width, size.y / (float)object->height);
	visible_u = size.x / ((float)object->width * scale);
	visible_v = size.y / ((float)object->height * scale);
	uv[0] = 0.5f - visible_u * 0.5f;
	uv[1] = 0.5f - visible_v * 0.5f;
	uv[2] = 0.5f + visible_u * 0.5f;
	uv[3] = 0.5f + visible_v * 0.5f;
	rvn_paint_image(painter, rvn_rect(0.0f, 0.0f, size.x, size.y), texture, uv, rvn_rgba(1.0f, 1.0f, 1.0f, 1.0f), 0.0f);
}

static int rvn_window_can_paint(const rvn_window_t* window) {
	return window->surface_ready && window->width_px > 0 && window->height_px > 0;
}

static void rvn_window_paint(rvn_window_t* window, RCapture* capture) {
	rvn_frame_t* frame;
	RPainter painter;
	double start;
	uint32_t i, widgets = 0, backdrops;
	rvn_color_t clear;

	window->needs_paint = R_FALSE;
	if (!rvn_window_can_paint(window) || (!window->visible && !capture)) return;

	/* never queue more than a couple of frames: this is what paces a busy UI thread */
	rvn_renderer_throttle(&window->frames_in_flight, RVN_MAX_FRAMES_IN_FLIGHT);

	frame = rvn_renderer_acquire_frame();
	start = rvn_clock_seconds();

	memset(&painter, 0, sizeof(painter));
	painter.list = &frame->list;
	painter.window = window;
	painter.scale = window->scale;
	painter.clip = rvn_box(0.0f, 0.0f, (float)window->width_px, (float)window->height_px);
	painter.opacity = 1.0f;
	painter.text_color = RVN_THEME_COLOR(R_COLOR_TEXT);
	painter.enabled = R_TRUE;
	/* animations advance by the time since the last frame; after a pause they resume gently */
	painter.dt = (float)rvn_clampf((float)(start - window->paint_time), 0.001f, 1.0f / 30.0f);
	window->paint_time = start;

	rvn_atomic_store(&window->realtime_viewports, 0);
	rvn_atomic_store(&window->animating, 0);
	rvn_window_paint_background(window, &painter);
	if (window->root) {
		rvn_widget_paint(window->root, &painter);
		widgets += window->root->subtree_count;
	}
	for (i = 0; i < window->popup_count; i++) {
		rvn_widget_paint(window->popups[i].root, &painter);
		widgets += window->popups[i].root->subtree_count;
	}
	rvn_window_paint_tooltip(window, &painter);

	/* new glyphs reach the GPU before the frame that uses them */
	rvn_fonts_flush();
	rvn_fonts_resolve_pages(&frame->list);
	/* glass: only keep the backdrop captures that show something new */
	backdrops = rvn_drawlist_resolve_backdrops(&frame->list,
		rvn_box(0.0f, 0.0f, (float)window->width_px, (float)window->height_px), RVN_MAX_BACKDROPS);

	window->stats.paint_ms = (rvn_clock_seconds() - start) * 1000.0;
	window->stats.render_ms = (double)rvn_atomic_load(&window->render_us) / 1000.0;
	window->stats.widget_count = widgets;
	window->stats.instance_count = frame->list.instance_count;
	window->stats.draw_call_count = frame->list.command_count - backdrops;
	window->stats.backdrop_count = backdrops;
	window->stats.paint_job_count = (Ruint)rvn_paint_take_job_count();
	window->stats.frame_index = window->frame_index;

	clear = RVN_THEME_COLOR(R_COLOR_WINDOW);
	frame->data.window = window->id;
	frame->data.framebuffer_width = (Ruint)window->width_px;
	frame->data.framebuffer_height = (Ruint)window->height_px;
	if (window->has_clear_color) {
		memcpy(frame->data.clear_color, window->clear_color, sizeof(window->clear_color));
	} else {
		rvn_color_to_floats(clear, &frame->data.clear_color[0], &frame->data.clear_color[1],
			&frame->data.clear_color[2], &frame->data.clear_color[3]);
	}
	frame->data.frame_index = window->frame_index++;
	frame->data.scale = window->scale;
	frame->capture = capture;
	frame->frames_in_flight = &window->frames_in_flight;
	frame->render_us = &window->render_us;
	rvn_renderer_submit_frame(frame);

	/* native child windows follow the viewports that were just painted, the others hide */
	for (i = 0; i < window->native_viewport_count; i++) {
		rvn_viewport_native_sync(window->native_viewports[i], window->frame_index - 1);
	}
}

int rvn_window_wants_frame(rvn_window_t* window) {
	if (window->dead) return R_FALSE;
	if (window->needs_layout) return R_TRUE;
	if (!rvn_window_can_paint(window) || !window->visible) return R_FALSE;
	return window->needs_paint || window->continuous || rvn_atomic_load(&window->animating) != 0 ||
		rvn_atomic_load(&window->realtime_viewports) > 0 || rvn_atomic_load(&window->redraw_requested) != 0;
}

void rvn_window_update(rvn_window_t* window) {
	if (window->dead) return;
	if (rvn_atomic_exchange(&window->redraw_requested, 0)) window->needs_paint = R_TRUE;
	if (window->continuous || rvn_atomic_load(&window->realtime_viewports) > 0 || rvn_atomic_load(&window->animating) != 0) {
		window->needs_paint = R_TRUE;
	}
	if (window->needs_layout) rvn_window_layout(window);
	if (window->needs_paint) rvn_window_paint(window, NULL);
}

void rvn_invalidate_all_windows(void) {
	uint32_t i;
	for (i = 0; i < g_rvn.widgets.used; i++) {
		rvn_widget_t* widget = (rvn_widget_t*)rvn_pool_at(&g_rvn.widgets, i);
		if (widget) widget->flags |= RVN_WF_LAYOUT_DIRTY;
	}
	for (i = 0; i < g_rvn.windows.used; i++) {
		rvn_window_t* window = (rvn_window_t*)rvn_pool_at(&g_rvn.windows, i);
		if (window && !window->dead) {
			window->needs_layout = R_TRUE;
			window->needs_paint = R_TRUE;
		}
	}
}

void rvn_repaint_all_windows(void) {
	uint32_t i;
	for (i = 0; i < g_rvn.windows.used; i++) {
		rvn_window_t* window = (rvn_window_t*)rvn_pool_at(&g_rvn.windows, i);
		if (window && !window->dead) window->needs_paint = R_TRUE;
	}
}

/* ---------------------------------------------------------------------------
 *  Input (positions in logical units)
 * ------------------------------------------------------------------------- */

static void rvn_window_mouse_move(rvn_window_t* window, float x, float y) {
	REvent event;
	float dx = window->mouse_inside ? x - window->mouse_x : 0.0f;
	float dy = window->mouse_inside ? y - window->mouse_y : 0.0f;

	window->mouse_x = x;
	window->mouse_y = y;
	window->mouse_inside = R_TRUE;
	if (window->tooltip_visible && (fabsf(x - window->tooltip_x) > 6.0f || fabsf(y - window->tooltip_y) > 6.0f)) {
		rvn_window_tooltip_arm(window);
	}

	event = rvn_window_event(window, R_EVENT_MOUSE_MOVE);
	event.delta_x = dx;
	event.delta_y = dy;
	if (window->capture) {
		rvn_widget_send(window->capture, &event);
	} else {
		rvn_widget_t* hit = rvn_window_hit(window, x, y, NULL);
		rvn_window_set_hover(window, hit);
		if (window->dead) return;
		hit = window->hover;
		if (hit && rvn_widget_is_enabled(hit)) rvn_widget_dispatch(hit, &event);
	}
	if (!window->dead) rvn_window_update_cursor(window);
}

static void rvn_window_mouse_leave(rvn_window_t* window) {
	window->mouse_inside = R_FALSE;
	if (!window->capture) rvn_window_set_hover(window, NULL);
	if (!window->dead) rvn_window_tooltip_hide(window);
}

static rvn_widget_t* rvn_focusable_ancestor(rvn_widget_t* widget) {
	for (; widget; widget = widget->parent) {
		if (widget->flags & RVN_WF_FOCUSABLE) return widget;
		if (widget->flags & RVN_WF_POPUP) break;
	}
	return NULL;
}

static void rvn_window_open_context_menu(rvn_window_t* window, rvn_widget_t* target) {
	rvn_widget_t* walk;
	for (walk = target; walk; walk = walk->parent) {
		rvn_widget_t* menu;
		if (walk->context_menu == R_INVALID_ID) continue;
		menu = rvn_widget_get(walk->context_menu);
		if (menu && menu->cls->type == R_WIDGET_MENU) rvn_menu_popup(menu, window, window->mouse_x, window->mouse_y);
		return;
	}
}

static void rvn_window_mouse_button(rvn_window_t* window, RMouseButton button, int pressed) {
	REvent event;
	rvn_widget_t* target;
	rvn_widget_t* hit;
	int popup_index = -1;
	float x = window->mouse_x, y = window->mouse_y;

	if ((int)button < 0 || (int)button > R_MOUSE_X2) return;
	if (pressed) window->buttons |= 1u << button;
	else window->buttons &= ~(1u << button);
	rvn_window_tooltip_hide(window);

	hit = rvn_window_hit(window, x, y, &popup_index);

	if (pressed) {
		double now = rvn_clock_seconds();
		window->focus_visible = R_FALSE;
		if (button == (RMouseButton)window->click_button && now - window->click_time < RVN_DOUBLE_CLICK_TIME &&
			fabsf(x - window->click_x) < RVN_DOUBLE_CLICK_DISTANCE && fabsf(y - window->click_y) < RVN_DOUBLE_CLICK_DISTANCE) {
			window->click_count++;
		} else {
			window->click_count = 1;
		}
		window->click_time = now;
		window->click_x = x;
		window->click_y = y;
		window->click_button = (int)button;

		/* a click outside of the open popups closes them and is not passed on */
		if (window->popup_count > 0 && popup_index < 0 && !window->capture) {
			rvn_window_close_popups(window);
			rvn_window_refresh_hover(window);
			return;
		}

		target = window->capture ? window->capture : hit;
		if (!target) {
			rvn_window_set_focus(window, NULL);
			return;
		}
		if (!rvn_widget_is_enabled(target)) return;
		/* popups do not take the keyboard focus from the widget that opened them */
		if (popup_index < 0 && !window->capture) {
			rvn_window_set_focus(window, rvn_focusable_ancestor(target));
			if (window->dead || (target->flags & RVN_WF_DEAD)) return;
		}
		event = rvn_window_event(window, R_EVENT_MOUSE_DOWN);
		event.button = button;
		event.clicks = window->click_count;
		if (window->capture) rvn_widget_send(target, &event);
		else rvn_widget_dispatch(target, &event);
	} else {
		int was_captured = window->capture != NULL;
		target = window->capture ? window->capture : hit;
		if (target && rvn_widget_is_enabled(target)) {
			event = rvn_window_event(window, R_EVENT_MOUSE_UP);
			event.button = button;
			event.clicks = window->click_count;
			if (window->capture) rvn_widget_send(target, &event);
			else rvn_widget_dispatch(target, &event);
		}
		if (window->dead) return;
		if (window->buttons == 0) {
			/* the hover was frozen during the capture: bring it up to date */
			window->capture = NULL;
			if (was_captured) rvn_window_refresh_hover(window);
		}
		if (button == R_MOUSE_RIGHT && hit && !(hit->flags & RVN_WF_DEAD) && !window->dead) {
			rvn_window_open_context_menu(window, hit);
		}
	}
	if (!window->dead) rvn_window_update_cursor(window);
}

static void rvn_window_mouse_wheel(rvn_window_t* window, float wheel_x, float wheel_y) {
	REvent event;
	rvn_widget_t* target = window->capture ? window->capture : rvn_window_hit(window, window->mouse_x, window->mouse_y, NULL);
	rvn_window_tooltip_hide(window);
	if (!target || !rvn_widget_is_enabled(target)) return;
	event = rvn_window_event(window, R_EVENT_MOUSE_WHEEL);
	event.wheel_x = wheel_x;
	event.wheel_y = wheel_y;
	rvn_widget_dispatch(target, &event);
}

static void rvn_window_key(rvn_window_t* window, RKey key, int pressed, uint32_t modifiers, int repeat) {
	REvent event;
	uint32_t i;

	window->modifiers = modifiers;
	if (pressed) rvn_window_tooltip_hide(window);
	event = rvn_window_event(window, pressed ? R_EVENT_KEY_DOWN : R_EVENT_KEY_UP);
	event.key = key;
	event.repeat = repeat;

	/* the top popup sees the keyboard first (menu navigation) */
	if (window->popup_count > 0) {
		rvn_widget_t* top = window->popups[window->popup_count - 1].root;
		if (rvn_widget_dispatch(top, &event) || window->dead) return;
		if (pressed && key == R_KEY_ESCAPE && window->popup_count > 0) {
			rvn_window_close_popup(window, window->popups[window->popup_count - 1].root);
			return;
		}
	}
	if (window->focus && rvn_widget_is_enabled(window->focus)) {
		if (rvn_widget_dispatch(window->focus, &event) || window->dead) return;
	}
	if (!pressed) return;

	if (key == R_KEY_TAB && !(modifiers & (R_MOD_CTRL | R_MOD_ALT))) {
		rvn_window_focus_next(window, (modifiers & R_MOD_SHIFT) ? -1 : 1);
		return;
	}
	if (key == R_KEY_ESCAPE && window->focus) {
		rvn_window_set_focus(window, NULL);
		return;
	}
	for (i = 0; i < window->shortcut_count; i++) {
		rvn_shortcut_t* shortcut = &window->shortcuts[i];
		char* action;
		if (shortcut->key != key || shortcut->modifiers != (modifiers & 0xFu)) continue;
		/* copy: the callback may unregister the shortcut */
		action = rvn_strdup(shortcut->action);
		rvn_fire_action(window->id, action);
		rvn_free(action);
		return;
	}
	/* shortcuts shown in the menus of the window */
	{
		rvn_widget_t* item = rvn_menu_find_shortcut(window->root, key, modifiers & 0xFu);
		if (item) rvn_menu_item_trigger(item);
	}
}

static void rvn_window_text(rvn_window_t* window, uint32_t codepoint) {
	REvent event;
	if (!window->focus || !rvn_widget_is_enabled(window->focus)) return;
	event = rvn_window_event(window, R_EVENT_TEXT);
	event.codepoint = codepoint;
	rvn_widget_dispatch(window->focus, &event);
}

static void rvn_window_resized(rvn_window_t* window, int width_px, int height_px, float platform_scale) {
	float scale = (platform_scale > 0.0f ? platform_scale : 1.0f) * g_rvn.ui_scale;
	rvn_vec2_t size;
	if (window->width_px == width_px && window->height_px == height_px && window->scale == scale) return;
	window->width_px = width_px;
	window->height_px = height_px;
	window->scale = scale;
	rvn_window_close_popups(window);
	window->needs_layout = R_TRUE;
	window->needs_paint = R_TRUE;
	if (window->resize_callback) {
		size = rvn_window_size(window);
		window->resize_callback(window->id, (int)(size.x + 0.5f), (int)(size.y + 0.5f), window->resize_user);
	}
}

/* ---------------------------------------------------------------------------
 *  Platform events
 * ------------------------------------------------------------------------- */

void rvn_window_on_platform_event(const rvn_pevent_t* pevent) {
	rvn_window_t* window = (rvn_window_t*)pevent->user;
	float scale;

	if (pevent->type == RVN_PEVENT_MODAL_TICK) {
		Rupdate();
		return;
	}
	if (!window || window->dead) return;
	scale = window->scale > 0.0f ? window->scale : 1.0f;

	rvn_dispatch_begin();
	switch (pevent->type) {
		case RVN_PEVENT_CLOSE:
			if (!window->close_callback || window->close_callback(window->id, window->close_user)) {
				if (!window->dead) rvn_window_destroy(window);
			}
			break;
		case RVN_PEVENT_RESIZE:
			rvn_window_resized(window, pevent->width, pevent->height, pevent->scale);
			break;
		case RVN_PEVENT_EXPOSE:
			window->needs_paint = R_TRUE;
			break;
		case RVN_PEVENT_MOUSE_MOVE:
			window->modifiers = pevent->modifiers;
			rvn_window_mouse_move(window, (float)pevent->x / scale, (float)pevent->y / scale);
			break;
		case RVN_PEVENT_MOUSE_BUTTON:
			window->modifiers = pevent->modifiers;
			rvn_window_mouse_move(window, (float)pevent->x / scale, (float)pevent->y / scale);
			if (!window->dead) rvn_window_mouse_button(window, (RMouseButton)pevent->button, pevent->pressed);
			break;
		case RVN_PEVENT_MOUSE_WHEEL:
			window->modifiers = pevent->modifiers;
			rvn_window_mouse_wheel(window, pevent->wheel_x, pevent->wheel_y);
			break;
		case RVN_PEVENT_MOUSE_LEAVE:
			rvn_window_mouse_leave(window);
			break;
		case RVN_PEVENT_KEY:
			rvn_window_key(window, pevent->key, pevent->pressed, pevent->modifiers, pevent->repeat);
			break;
		case RVN_PEVENT_TEXT:
			rvn_window_text(window, pevent->codepoint);
			break;
		case RVN_PEVENT_FOCUS:
			if (!pevent->focused) {
				/* the window lost the keyboard: drop transient state */
				window->capture = NULL;
				window->buttons = 0;
				window->modifiers = 0;
				rvn_window_close_popups(window);
				rvn_window_tooltip_hide(window);
			}
			window->needs_paint = R_TRUE;
			break;
		case RVN_PEVENT_MODAL_TICK:
			break;
	}
	rvn_dispatch_end();
}

/* ---------------------------------------------------------------------------
 *  Shortcuts
 * ------------------------------------------------------------------------- */

void rvn_window_remove_shortcut(rvn_window_t* window, RKey key, uint32_t modifiers) {
	uint32_t i;
	for (i = 0; i < window->shortcut_count; i++) {
		if (window->shortcuts[i].key == key && window->shortcuts[i].modifiers == modifiers) {
			rvn_free(window->shortcuts[i].action);
			window->shortcuts[i] = window->shortcuts[--window->shortcut_count];
			return;
		}
	}
}

void rvn_window_add_shortcut(rvn_window_t* window, RKey key, uint32_t modifiers, const char* action) {
	rvn_shortcut_t* shortcut;
	rvn_window_remove_shortcut(window, key, modifiers);
	RVN_ARRAY_RESERVE(window->shortcuts, window->shortcut_cap, window->shortcut_count + 1);
	shortcut = &window->shortcuts[window->shortcut_count++];
	shortcut->key = key;
	shortcut->modifiers = modifiers;
	shortcut->action = rvn_strdup(action);
}

/* ---------------------------------------------------------------------------
 *  Destruction
 * ------------------------------------------------------------------------- */

static void rvn_window_free(rvn_window_t* window) {
	uint32_t i;
	/* waits for the frames still queued: they point into this structure */
	if (window->surface_ready) rvn_renderer_destroy_surface(window->id);
	if (window->platform) g_rvn.platform->window_destroy(window->platform);
	rvn_pool_remove(&g_rvn.windows, window->id);
	for (i = 0; i < window->shortcut_count; i++) rvn_free(window->shortcuts[i].action);
	rvn_free(window->shortcuts);
	rvn_free(window->native_viewports);
	rvn_free(window->popups);
	rvn_free(window->title);
	rvn_free(window);
}

void rvn_window_destroy(rvn_window_t* window) {
	rvn_widget_t* root;
	if (window->dead) return;
	rvn_window_close_popups(window);
	rvn_window_tooltip_hide(window);
	root = window->root;
	if (root) rvn_widget_destroy(root);
	window->root = NULL;
	window->hover = window->focus = window->capture = NULL;
	/* the handle stops resolving now; the slot stays reserved until the surface is gone */
	window->dead = R_TRUE;
	if (g_rvn.dispatch_depth > 0) {
		RVN_ARRAY_RESERVE(g_rvn.dead_windows, g_rvn.dead_window_cap, g_rvn.dead_window_count + 1);
		g_rvn.dead_windows[g_rvn.dead_window_count++] = window;
	} else {
		rvn_window_free(window);
	}
}

void rvn_windows_collect(void) {
	uint32_t i;
	for (i = 0; i < g_rvn.dead_window_count; i++) rvn_window_free(g_rvn.dead_windows[i]);
	g_rvn.dead_window_count = 0;
}

void rvn_windows_destroy_all(void) {
	uint32_t i;
	for (i = 0; i < g_rvn.windows.used; i++) {
		rvn_window_t* window = (rvn_window_t*)rvn_pool_at(&g_rvn.windows, i);
		if (window && !window->dead) rvn_window_destroy(window);
	}
	rvn_windows_collect();
}

/* ---------------------------------------------------------------------------
 *  Public API: windows
 * ------------------------------------------------------------------------- */

Rid Rwindow_create(const char* title, int width, int height, Ruint flags) {
	rvn_window_t* window;
	rvn_pwindow_desc_t desc;
	RNativeWindow native;

	RVN_API_GUARD(R_INVALID_ID);
	if (width <= 0 || height <= 0) {
		rvn_error(R_ERROR_INVALID_VALUE, R_SEVERITY_ERROR, "Rwindow_create: invalid size %d x %d", width, height);
		return R_INVALID_ID;
	}

	window = (rvn_window_t*)rvn_calloc(sizeof(rvn_window_t));
	window->title = rvn_strdup(title ? title : "");
	window->tooltip_timer = R_INVALID_ID;
	window->cursor = R_CURSOR_ARROW;
	window->click_button = -1;
	window->id = rvn_pool_add(&g_rvn.windows, window);
	if (window->id == R_INVALID_ID) {
		rvn_free(window->title);
		rvn_free(window);
		return R_INVALID_ID;
	}

	desc.title = window->title;
	desc.width = (int)((float)width * g_rvn.ui_scale + 0.5f);
	desc.height = (int)((float)height * g_rvn.ui_scale + 0.5f);
	desc.flags = flags;
	desc.user = window;
	window->platform = g_rvn.platform->window_create(&desc);
	if (!window->platform) {
		rvn_error(R_ERROR_PLATFORM, R_SEVERITY_ERROR, "Rwindow_create: the window system refused to create the window");
		rvn_pool_remove(&g_rvn.windows, window->id);
		rvn_free(window->title);
		rvn_free(window);
		return R_INVALID_ID;
	}
	g_rvn.platform->window_get_size(window->platform, &window->width_px, &window->height_px);
	window->scale = g_rvn.platform->window_get_scale(window->platform) * g_rvn.ui_scale;
	if (window->scale <= 0.0f) window->scale = 1.0f;

	memset(&native, 0, sizeof(native));
	g_rvn.platform->window_get_native(window->platform, &native);
	if (!rvn_renderer_create_surface(window->id, &native, (uint32_t)rvn_maxi(window->width_px, 1),
		(uint32_t)rvn_maxi(window->height_px, 1))) {
		rvn_error(R_ERROR_BACKEND, R_SEVERITY_ERROR, "Rwindow_create: cannot create the rendering surface");
		g_rvn.platform->window_destroy(window->platform);
		rvn_pool_remove(&g_rvn.windows, window->id);
		rvn_free(window->title);
		rvn_free(window);
		return R_INVALID_ID;
	}
	window->surface_ready = R_TRUE;
	window->needs_layout = R_TRUE;
	window->needs_paint = R_TRUE;
	window->visible = (flags & R_WINDOW_HIDDEN) == 0;
	g_rvn.had_window = R_TRUE;
	if (window->visible) g_rvn.platform->window_show(window->platform, R_TRUE);
	return window->id;
}

void Rwindow_destroy(Rid id) {
	RVN_WINDOW_VOID(window, id);
	rvn_window_destroy(window);
}

int Rwindow_is_valid(Rid window) {
	return g_rvn.initialized && rvn_window_get(window) != NULL;
}

void Rwindow_set_content(Rid id, Rid widget_id) {
	rvn_widget_t* widget = NULL;
	RVN_WINDOW_VOID(window, id);
	if (widget_id != R_INVALID_ID) {
		widget = rvn_widget_check(widget_id, 0, __func__);
		if (!widget) return;
	}
	if (window->root == widget) return;
	if (window->root) rvn_widget_destroy(window->root);
	window->root = NULL;
	if (widget) {
		if (widget->parent || widget->window) rvn_widget_detach(widget);
		window->root = widget;
		rvn_widget_set_window(widget, window);
		widget->flags |= RVN_WF_LAYOUT_DIRTY;
	}
	window->needs_layout = R_TRUE;
	window->needs_paint = R_TRUE;
}

Rid Rwindow_get_content(Rid id) {
	RVN_WINDOW(window, id, R_INVALID_ID);
	return window->root ? window->root->id : R_INVALID_ID;
}

void Rwindow_set_title(Rid id, const char* title) {
	RVN_WINDOW_VOID(window, id);
	if (!rvn_str_set(&window->title, title ? title : "")) return;
	g_rvn.platform->window_set_title(window->platform, window->title);
}

void Rwindow_set_size(Rid id, int width, int height) {
	RVN_WINDOW_VOID(window, id);
	if (width <= 0 || height <= 0) {
		rvn_error(R_ERROR_INVALID_VALUE, R_SEVERITY_ERROR, "Rwindow_set_size: invalid size");
		return;
	}
	g_rvn.platform->window_set_size(window->platform, (int)((float)width * window->scale + 0.5f),
		(int)((float)height * window->scale + 0.5f));
}

void Rwindow_get_size(Rid id, int* width, int* height) {
	rvn_window_t* window;
	rvn_vec2_t size;
	if (width) *width = 0;
	if (height) *height = 0;
	window = rvn_window_check(id, __func__);
	if (!window) return;
	size = rvn_window_size(window);
	if (width) *width = (int)(size.x + 0.5f);
	if (height) *height = (int)(size.y + 0.5f);
}

void Rwindow_set_min_size(Rid id, int width, int height) {
	RVN_WINDOW_VOID(window, id);
	g_rvn.platform->window_set_min_size(window->platform, (int)((float)rvn_maxi(width, 0) * window->scale + 0.5f),
		(int)((float)rvn_maxi(height, 0) * window->scale + 0.5f));
}

void Rwindow_set_position(Rid id, int x, int y) {
	RVN_WINDOW_VOID(window, id);
	g_rvn.platform->window_set_position(window->platform, x, y);
}

void Rwindow_get_position(Rid id, int* x, int* y) {
	rvn_window_t* window;
	int px = 0, py = 0;
	if (x) *x = 0;
	if (y) *y = 0;
	window = rvn_window_check(id, __func__);
	if (!window) return;
	g_rvn.platform->window_get_position(window->platform, &px, &py);
	if (x) *x = px;
	if (y) *y = py;
}

void Rwindow_get_framebuffer_size(Rid id, int* width, int* height) {
	rvn_window_t* window;
	if (width) *width = 0;
	if (height) *height = 0;
	window = rvn_window_check(id, __func__);
	if (!window) return;
	if (width) *width = window->width_px;
	if (height) *height = window->height_px;
}

float Rwindow_get_scale(Rid id) {
	RVN_WINDOW(window, id, 1.0f);
	return window->scale;
}

void Rwindow_show(Rid id) {
	RVN_WINDOW_VOID(window, id);
	window->visible = R_TRUE;
	window->needs_paint = R_TRUE;
	g_rvn.platform->window_show(window->platform, R_TRUE);
}

void Rwindow_hide(Rid id) {
	RVN_WINDOW_VOID(window, id);
	window->visible = R_FALSE;
	g_rvn.platform->window_show(window->platform, R_FALSE);
}

void Rwindow_maximize(Rid id) {
	RVN_WINDOW_VOID(window, id);
	g_rvn.platform->window_set_state(window->platform, RVN_PWINDOW_MAXIMIZED);
}

void Rwindow_minimize(Rid id) {
	RVN_WINDOW_VOID(window, id);
	g_rvn.platform->window_set_state(window->platform, RVN_PWINDOW_MINIMIZED);
}

void Rwindow_restore(Rid id) {
	RVN_WINDOW_VOID(window, id);
	g_rvn.platform->window_set_state(window->platform, RVN_PWINDOW_NORMAL);
}

void Rwindow_set_clear_color(Rid id, float r, float g, float b, float a) {
	RVN_WINDOW_VOID(window, id);
	window->has_clear_color = R_TRUE;
	window->clear_color[0] = r;
	window->clear_color[1] = g;
	window->clear_color[2] = b;
	window->clear_color[3] = a;
	window->needs_paint = R_TRUE;
}

void Rwindow_set_background(Rid id, Rid texture) {
	RVN_WINDOW_VOID(window, id);
	if (texture != R_INVALID_ID && !rvn_texture_get(texture)) {
		rvn_error(R_ERROR_INVALID_ID, R_SEVERITY_ERROR, "Rwindow_set_background: invalid texture");
		return;
	}
	window->background = texture;
	window->needs_paint = R_TRUE;
}

void Rwindow_request_redraw(Rid id) {
	if (!g_rvn.initialized) return;
	if (!Ris_ui_thread()) {
		/* the window table belongs to the UI thread: ask for every window instead */
		rvn_atomic_store(&g_rvn.redraw_all, 1);
		rvn_wake();
		return;
	}
	{
		rvn_window_t* window = rvn_window_get(id);
		if (window) rvn_atomic_store(&window->redraw_requested, 1);
	}
}

void Rwindow_set_continuous_redraw(Rid id, int enabled) {
	RVN_WINDOW_VOID(window, id);
	window->continuous = enabled != 0;
	if (enabled) window->needs_paint = R_TRUE;
}

void Rwindow_set_close_callback(Rid id, RWindowCloseCallback callback, void* user_data) {
	RVN_WINDOW_VOID(window, id);
	window->close_callback = callback;
	window->close_user = user_data;
}

void Rwindow_set_resize_callback(Rid id, RWindowResizeCallback callback, void* user_data) {
	RVN_WINDOW_VOID(window, id);
	window->resize_callback = callback;
	window->resize_user = user_data;
}

void Rwindow_set_user_handle(Rid id, void* handle) {
	RVN_WINDOW_VOID(window, id);
	window->user_handle = handle;
}

void* Rwindow_get_user_handle(Rid id) {
	RVN_WINDOW(window, id, NULL);
	return window->user_handle;
}

int Rwindow_get_native_handle(Rid id, RNativeWindow* native) {
	RVN_WINDOW(window, id, R_FALSE);
	if (!native) return R_FALSE;
	memset(native, 0, sizeof(*native));
	g_rvn.platform->window_get_native(window->platform, native);
	return native->system != R_WINDOW_SYSTEM_NONE;
}

uint8_t* Rwindow_capture(Rid id, int* width, int* height) {
	RCapture capture;
	rvn_window_t* window;
	if (width) *width = 0;
	if (height) *height = 0;
	window = rvn_window_check(id, __func__);
	if (!window) return NULL;
	memset(&capture, 0, sizeof(capture));
	if (window->needs_layout) rvn_window_layout(window);
	rvn_window_paint(window, &capture);
	/* the capture structure lives on this stack: wait for the render thread */
	rvn_renderer_sync();
	if (width) *width = capture.width;
	if (height) *height = capture.height;
	return capture.pixels;
}

void Rwindow_get_frame_stats(Rid id, RFrameStats* stats) {
	rvn_window_t* window;
	if (!stats) return;
	memset(stats, 0, sizeof(*stats));
	window = rvn_window_check(id, __func__);
	if (window) *stats = window->stats;
}

/* ---------------------------------------------------------------------------
 *  Public API: input state and synthetic input
 * ------------------------------------------------------------------------- */

Rid Rwindow_get_focused_widget(Rid id) {
	RVN_WINDOW(window, id, R_INVALID_ID);
	return window->focus ? window->focus->id : R_INVALID_ID;
}

Rid Rwindow_get_hovered_widget(Rid id) {
	RVN_WINDOW(window, id, R_INVALID_ID);
	return window->hover ? window->hover->id : R_INVALID_ID;
}

void Rwindow_get_mouse_position(Rid id, float* x, float* y) {
	rvn_window_t* window;
	if (x) *x = 0.0f;
	if (y) *y = 0.0f;
	window = rvn_window_check(id, __func__);
	if (!window) return;
	if (x) *x = window->mouse_x;
	if (y) *y = window->mouse_y;
}

/* Synthetic input acts on the current layout, like real input does. */
static rvn_window_t* rvn_window_inject_begin(Rid id, const char* function) {
	rvn_window_t* window = rvn_window_check(id, function);
	if (!window) return NULL;
	if (window->needs_layout) rvn_window_layout(window);
	rvn_dispatch_begin();
	return window;
}

void Rwindow_inject_mouse_move(Rid id, float x, float y) {
	rvn_window_t* window = rvn_window_inject_begin(id, __func__);
	if (!window) return;
	rvn_window_mouse_move(window, x, y);
	rvn_dispatch_end();
}

void Rwindow_inject_mouse_button(Rid id, RMouseButton button, int pressed) {
	rvn_window_t* window = rvn_window_inject_begin(id, __func__);
	if (!window) return;
	rvn_window_mouse_button(window, button, pressed);
	rvn_dispatch_end();
}

void Rwindow_inject_mouse_wheel(Rid id, float wheel_x, float wheel_y) {
	rvn_window_t* window = rvn_window_inject_begin(id, __func__);
	if (!window) return;
	rvn_window_mouse_wheel(window, wheel_x, wheel_y);
	rvn_dispatch_end();
}

void Rwindow_inject_key(Rid id, RKey key, int pressed, Ruint modifiers) {
	rvn_window_t* window = rvn_window_inject_begin(id, __func__);
	if (!window) return;
	rvn_window_key(window, key, pressed, modifiers, R_FALSE);
	rvn_dispatch_end();
}

void Rwindow_inject_text(Rid id, const char* text) {
	rvn_window_t* window = rvn_window_inject_begin(id, __func__);
	const char* cursor = text;
	const char* end;
	if (!window) return;
	end = text ? text + strlen(text) : NULL;
	while (text && cursor < end && !window->dead) {
		rvn_window_text(window, rvn_utf8_decode(&cursor, end));
	}
	rvn_dispatch_end();
}

void Rwindow_register_shortcut(Rid id, RKey key, Ruint modifiers, const char* action) {
	RVN_WINDOW_VOID(window, id);
	if (!action || !action[0]) {
		rvn_error(R_ERROR_INVALID_VALUE, R_SEVERITY_ERROR, "Rwindow_register_shortcut: empty action name");
		return;
	}
	rvn_window_add_shortcut(window, key, modifiers & 0xFu, action);
}

void Rwindow_unregister_shortcut(Rid id, RKey key, Ruint modifiers) {
	RVN_WINDOW_VOID(window, id);
	rvn_window_remove_shortcut(window, key, modifiers & 0xFu);
}

void Rwindow_close_popups(Rid id) {
	RVN_WINDOW_VOID(window, id);
	rvn_window_close_popups(window);
}
