/**
 * HUI - widget/window.c
 * Windows: layout and paint scheduling, input routing (hover, focus, mouse
 * capture, popups, tooltips, shortcuts) and the public window API.
 */

#include "widget.h"
#include "../render/renderer.h"

#define HUI_MAX_FRAMES_IN_FLIGHT 2
#define HUI_DOUBLE_CLICK_TIME 0.4
#define HUI_DOUBLE_CLICK_DISTANCE 4.0f
#define HUI_TOOLTIP_DELAY 0.6
#define HUI_MAX_PATH_DEPTH 128

/* ---------------------------------------------------------------------------
 *  Lookup
 * ------------------------------------------------------------------------- */

hui_window_t* hui_window_get(HUI_id id) {
	hui_window_t* window = (hui_window_t*)hui_pool_get(&g_hui.windows, id);
	return (window && !window->dead) ? window : NULL;
}

hui_window_t* hui_window_check(HUI_id id, const char* function) {
	hui_window_t* window;
	if (!hui_api_check(function)) return NULL;
	window = hui_window_get(id);
	if (!window) hui_error(HUI_ERROR_INVALID_ID, HUI_SEVERITY_ERROR, "%s: invalid window handle 0x%08X", function, (unsigned)id);
	return window;
}

#define HUI_WINDOW(variable, id, result) \
	hui_window_t* variable = hui_window_check((id), __func__); \
	if (!variable) return result

#define HUI_WINDOW_VOID(variable, id) \
	hui_window_t* variable = hui_window_check((id), __func__); \
	if (!variable) return

hui_vec2_t hui_window_size(const hui_window_t* window) {
	float scale = window->scale > 0.0f ? window->scale : 1.0f;
	return hui_vec2((float)window->width_px / scale, (float)window->height_px / scale);
}

static HUI_Event hui_window_event(hui_window_t* window, HUI_EEventType type) {
	HUI_Event event;
	memset(&event, 0, sizeof(event));
	event.type = type;
	event.window = window->id;
	event.target = HUI_INVALID_ID;
	event.x = window->mouse_x;
	event.y = window->mouse_y;
	event.modifiers = window->modifiers;
	return event;
}

/* ---------------------------------------------------------------------------
 *  Tooltip
 * ------------------------------------------------------------------------- */

static void hui_window_tooltip_hide(hui_window_t* window) {
	if (window->tooltip_timer != HUI_INVALID_ID) {
		HUI_DeleteTimer(window->tooltip_timer);
		window->tooltip_timer = HUI_INVALID_ID;
	}
	if (window->tooltip_visible) {
		window->tooltip_visible = HUI_FALSE;
		window->needs_paint = HUI_TRUE;
	}
	window->tooltip_widget = NULL;
}

static void hui_window_tooltip_fire(HUI_id timer, void* user_data) {
	hui_window_t* window = hui_window_get((HUI_id)(uintptr_t)user_data);
	HUI_UNUSED(timer);
	if (!window) return;
	window->tooltip_timer = HUI_INVALID_ID;
	if (window->tooltip_widget && window->tooltip_widget->tooltip) {
		window->tooltip_visible = HUI_TRUE;
		window->tooltip_x = window->mouse_x;
		window->tooltip_y = window->mouse_y;
		window->needs_paint = HUI_TRUE;
	}
}

/* Arms the tooltip of the nearest widget with a tooltip text under the cursor. */
static void hui_window_tooltip_arm(hui_window_t* window) {
	hui_widget_t* walk;
	hui_window_tooltip_hide(window);
	for (walk = window->hover; walk; walk = walk->parent) {
		if (walk->tooltip) break;
		if (walk->flags & HUI_WF_POPUP) {
			walk = NULL;
			break;
		}
	}
	if (!walk) return;
	window->tooltip_widget = walk;
	window->tooltip_timer = HUI_CreateTimer(HUI_TOOLTIP_DELAY, HUI_FALSE, hui_window_tooltip_fire,
		(void*)(uintptr_t)window->id);
}

static void hui_window_paint_tooltip(hui_window_t* window, HUI_Painter* painter) {
	const char* text;
	const char* line;
	hui_font_t* font;
	hui_font_metrics_t metrics;
	hui_vec2_t size = hui_window_size(window);
	float font_size = HUI_THEME_METRIC(HUI_METRIC_FONT_SIZE);
	float width = 0.0f, height, x, y;
	int lines = 0;

	if (!window->tooltip_visible || !window->tooltip_widget || !window->tooltip_widget->tooltip) return;
	text = window->tooltip_widget->tooltip;
	font = hui_font_resolve(HUI_INVALID_ID);
	if (!font) return;
	hui_font_metrics(font, font_size, &metrics);

	for (line = text;;) {
		const char* newline = strchr(line, '\n');
		int length = newline ? (int)(newline - line) : (int)strlen(line);
		width = hui_maxf(width, hui_text_width(font, font_size, line, length));
		lines++;
		if (!newline) break;
		line = newline + 1;
	}
	width += 16.0f;
	height = metrics.line_height * (float)lines + 10.0f;
	x = hui_clampf(window->tooltip_x + 12.0f, 0.0f, hui_maxf(0.0f, size.x - width));
	y = window->tooltip_y + 20.0f;
	if (y + height > size.y) y = window->tooltip_y - height - 6.0f;
	if (y < 0.0f) y = 0.0f;

	hui_paint_shadow(painter, hui_rect(x, y + 2.0f, width, height), 4.0f, 8.0f, HUI_THEME_COLOR(HUI_COLOR_SHADOW));
	hui_paint_box(painter, hui_rect(x, y, width, height), 4.0f, HUI_THEME_COLOR(HUI_COLOR_TOOLTIP),
		1.0f, HUI_THEME_COLOR(HUI_COLOR_POPUP_BORDER));
	y += 5.0f;
	for (line = text;;) {
		const char* newline = strchr(line, '\n');
		int length = newline ? (int)(newline - line) : (int)strlen(line);
		hui_paint_text(painter, HUI_INVALID_ID, font_size, x + 8.0f, y, line, length, HUI_THEME_COLOR(HUI_COLOR_TOOLTIP_TEXT));
		y += metrics.line_height;
		if (!newline) break;
		line = newline + 1;
	}
}

/* ---------------------------------------------------------------------------
 *  Hover and cursor
 * ------------------------------------------------------------------------- */

/* Collects a widget and its ancestors, up to and including the popup root. */
static int hui_hover_path(hui_widget_t* from, hui_widget_t** path) {
	int count = 0;
	hui_widget_t* walk;
	for (walk = from; walk && count < HUI_MAX_PATH_DEPTH; walk = walk->parent) {
		path[count++] = walk;
		if (walk->flags & HUI_WF_POPUP) break;
	}
	return count;
}

static void hui_hover_clear_flags(hui_widget_t* from) {
	hui_widget_t* walk;
	for (walk = from; walk; walk = walk->parent) walk->flags &= ~HUI_WF_HOVERED;
}

static void hui_window_set_hover(hui_window_t* window, hui_widget_t* target) {
	hui_widget_t* old_path[HUI_MAX_PATH_DEPTH];
	hui_widget_t* new_path[HUI_MAX_PATH_DEPTH];
	HUI_id left[HUI_MAX_PATH_DEPTH];
	HUI_id entered[HUI_MAX_PATH_DEPTH];
	int old_count, new_count, left_count = 0, entered_count = 0, i, j;
	HUI_Event event;

	if (window->hover == target) return;

	old_count = window->hover ? hui_hover_path(window->hover, old_path) : 0;
	new_count = target ? hui_hover_path(target, new_path) : 0;
	for (i = 0; i < old_count; i++) {
		for (j = 0; j < new_count && new_path[j] != old_path[i]; j++) {}
		if (j == new_count) left[left_count++] = old_path[i]->id;
	}
	for (i = 0; i < new_count; i++) {
		for (j = 0; j < old_count && old_path[j] != new_path[i]; j++) {}
		if (j == old_count) entered[entered_count++] = new_path[i]->id;
	}

	hui_hover_clear_flags(window->hover);
	for (i = 0; i < new_count; i++) new_path[i]->flags |= HUI_WF_HOVERED;
	window->hover = target;
	window->needs_paint = HUI_TRUE;

	/* notify through handles: a callback may delete widgets of either path */
	for (i = 0; i < left_count; i++) {
		hui_widget_t* widget = hui_widget_get(left[i]);
		if (!widget) continue;
		event = hui_window_event(window, HUI_EVENT_MOUSE_LEAVE);
		hui_widget_send(widget, &event);
	}
	for (i = entered_count; i > 0; i--) {
		hui_widget_t* widget = hui_widget_get(entered[i - 1]);
		if (!widget) continue;
		event = hui_window_event(window, HUI_EVENT_MOUSE_ENTER);
		hui_widget_send(widget, &event);
	}
	if (!window->dead) hui_window_tooltip_arm(window);
}

static hui_widget_t* hui_window_hit(hui_window_t* window, float x, float y, int* popup_index) {
	uint32_t i;
	for (i = window->popup_count; i > 0; i--) {
		hui_widget_t* hit = hui_widget_hit_test(window->popups[i - 1].root, x, y);
		if (hit) {
			if (popup_index) *popup_index = (int)i - 1;
			return hit;
		}
	}
	if (popup_index) *popup_index = -1;
	return window->root ? hui_widget_hit_test(window->root, x, y) : NULL;
}

static void hui_window_update_cursor(hui_window_t* window) {
	HUI_ECursor cursor = HUI_CURSOR_ARROW;
	hui_widget_t* walk = window->capture ? window->capture : window->hover;
	for (; walk; walk = walk->parent) {
		if (walk->cursor != 0) {
			cursor = walk->cursor;
			break;
		}
		if (walk->flags & HUI_WF_POPUP) break;
	}
	if (cursor != window->cursor) {
		window->cursor = cursor;
		if (window->platform) g_hui.platform->window_set_cursor(window->platform, cursor);
	}
}

/* Recomputes the hovered widget for a cursor that did not move (after a layout). */
static void hui_window_refresh_hover(hui_window_t* window) {
	if (window->capture || !window->mouse_inside) return;
	hui_window_set_hover(window, hui_window_hit(window, window->mouse_x, window->mouse_y, NULL));
	if (!window->dead) hui_window_update_cursor(window);
}

/* ---------------------------------------------------------------------------
 *  Focus
 * ------------------------------------------------------------------------- */

void hui_window_set_focus(hui_window_t* window, hui_widget_t* widget) {
	hui_widget_t* old = window->focus;
	HUI_Event event;
	if (old == widget) return;
	window->focus = widget;
	window->needs_paint = HUI_TRUE;
	if (old) {
		old->flags &= ~HUI_WF_FOCUSED;
		event = hui_window_event(window, HUI_EVENT_FOCUS_LOST);
		hui_widget_send(old, &event);
	}
	/* the callback above may have changed the focus again */
	if (widget && window->focus == widget && !(widget->flags & HUI_WF_DEAD)) {
		widget->flags |= HUI_WF_FOCUSED;
		event = hui_window_event(window, HUI_EVENT_FOCUS_GAINED);
		hui_widget_send(widget, &event);
	}
}

typedef struct hui_focus_list_t {
	hui_widget_t** items;
	uint32_t count;
	uint32_t cap;
} hui_focus_list_t;

static void hui_focus_collect(hui_widget_t* widget, hui_focus_list_t* list) {
	uint32_t i;
	if (widget->visibility != HUI_VISIBLE || (widget->flags & HUI_WF_DISABLED)) return;
	if (widget->flags & HUI_WF_FOCUSABLE) {
		HUI_ARRAY_RESERVE(list->items, list->cap, list->count + 1);
		list->items[list->count++] = widget;
	}
	for (i = 0; i < widget->child_count; i++) {
		hui_widget_t* child = widget->children[i];
		if (widget->cls->child_active && !widget->cls->child_active(widget, child)) continue;
		hui_focus_collect(child, list);
	}
}

static void hui_window_focus_next(hui_window_t* window, int direction) {
	hui_focus_list_t list;
	uint32_t i, index = 0;
	int found = HUI_FALSE;

	memset(&list, 0, sizeof(list));
	if (window->root) hui_focus_collect(window->root, &list);
	if (list.count == 0) {
		hui_free(list.items);
		return;
	}
	for (i = 0; i < list.count; i++) {
		if (list.items[i] == window->focus) {
			index = i;
			found = HUI_TRUE;
			break;
		}
	}
	if (!found) index = direction > 0 ? 0 : list.count - 1;
	else index = (index + (direction > 0 ? 1u : list.count - 1u)) % list.count;
	window->focus_visible = HUI_TRUE;
	hui_window_set_focus(window, list.items[index]);
	hui_free(list.items);
}

/* ---------------------------------------------------------------------------
 *  Popups
 * ------------------------------------------------------------------------- */

int hui_window_popup_index(const hui_window_t* window, const hui_widget_t* root) {
	uint32_t i;
	for (i = 0; i < window->popup_count; i++) {
		if (window->popups[i].root == root) return (int)i;
	}
	return -1;
}

void hui_window_open_popup(hui_window_t* window, hui_widget_t* root, hui_widget_t* owner,
	hui_rect_t anchor, hui_popup_placement_t placement, float min_width) {

	hui_popup_t* popup;
	if (hui_window_popup_index(window, root) >= 0) return;
	HUI_ARRAY_RESERVE(window->popups, window->popup_cap, window->popup_count + 1);
	popup = &window->popups[window->popup_count++];
	popup->root = root;
	popup->owner = owner;
	popup->anchor = anchor;
	popup->placement = placement;
	popup->min_width = min_width;
	root->flags |= HUI_WF_POPUP | HUI_WF_LAYOUT_DIRTY;
	if (root->window != window) hui_widget_set_window(root, window);
	hui_window_tooltip_hide(window);
	window->needs_layout = HUI_TRUE;
	window->needs_paint = HUI_TRUE;
}

void hui_window_close_popup(hui_window_t* window, hui_widget_t* root) {
	int index = hui_window_popup_index(window, root);
	if (index < 0) return;
	while ((int)window->popup_count > index) {
		hui_popup_t popup = window->popups[--window->popup_count];
		hui_widget_t* closed = popup.root;
		if (window->hover && hui_widget_is_inside(window->hover, closed)) {
			hui_hover_clear_flags(window->hover);
			window->hover = NULL;
		}
		if (window->capture && hui_widget_is_inside(window->capture, closed)) window->capture = NULL;
		closed->flags &= ~HUI_WF_POPUP;
		/* a popup that is not part of the tree leaves the window */
		if (!closed->parent) hui_widget_set_window(closed, NULL);
		if (closed->cls->popup_closed) closed->cls->popup_closed(closed);
	}
	window->needs_layout = HUI_TRUE;
	window->needs_paint = HUI_TRUE;
}

void hui_window_close_popups(hui_window_t* window) {
	if (window->popup_count > 0) hui_window_close_popup(window, window->popups[0].root);
}

static void hui_window_layout_popup(hui_window_t* window, hui_popup_t* popup) {
	hui_vec2_t area = hui_window_size(window);
	hui_vec2_t desired = hui_widget_measure(popup->root);
	float width = hui_maxf(desired.x, popup->min_width);
	float height = desired.y;
	float x = popup->anchor.x, y = popup->anchor.y;

	if (width > area.x) width = area.x;
	if (height > area.y) height = area.y;

	switch (popup->placement) {
		case HUI_POPUP_BELOW:
			y = popup->anchor.y + popup->anchor.h;
			if (y + height > area.y && popup->anchor.y - height >= 0.0f) y = popup->anchor.y - height;
			break;
		case HUI_POPUP_RIGHT:
			x = popup->anchor.x + popup->anchor.w;
			if (x + width > area.x && popup->anchor.x - width >= 0.0f) x = popup->anchor.x - width;
			break;
		case HUI_POPUP_AT:
			break;
	}
	x = hui_clampf(x, 0.0f, hui_maxf(0.0f, area.x - width));
	y = hui_clampf(y, 0.0f, hui_maxf(0.0f, area.y - height));
	hui_widget_arrange(popup->root, floorf(x), floorf(y), width, height);
}

/* ---------------------------------------------------------------------------
 *  References to widgets
 * ------------------------------------------------------------------------- */

void hui_window_forget_widget(hui_window_t* window, hui_widget_t* widget) {
	uint32_t i;
	if (window->capture && hui_widget_is_inside(window->capture, widget)) window->capture = NULL;
	if (window->focus && hui_widget_is_inside(window->focus, widget)) {
		window->focus->flags &= ~HUI_WF_FOCUSED;
		window->focus = NULL;
	}
	if (window->hover && hui_widget_is_inside(window->hover, widget)) {
		hui_hover_clear_flags(window->hover);
		window->hover = NULL;
	}
	if (window->tooltip_widget && hui_widget_is_inside(window->tooltip_widget, widget)) {
		hui_window_tooltip_hide(window);
	}
	/* close the popups shown from this subtree, lowest first (it closes the ones above) */
	for (i = 0; i < window->popup_count; i++) {
		hui_popup_t* popup = &window->popups[i];
		if (hui_widget_is_inside(popup->root, widget) || (popup->owner && hui_widget_is_inside(popup->owner, widget))) {
			hui_window_close_popup(window, popup->root);
			break;
		}
	}
	window->needs_paint = HUI_TRUE;
}

void hui_window_release_natives(hui_window_t* window, hui_widget_t* subtree) {
	uint32_t i;
	for (i = window->native_viewport_count; i > 0; i--) {
		hui_widget_t* viewport = window->native_viewports[i - 1];
		if (hui_widget_is_inside(viewport, subtree)) hui_viewport_native_release(viewport, window);
	}
}

/* ---------------------------------------------------------------------------
 *  Layout
 * ------------------------------------------------------------------------- */

void hui_window_layout(hui_window_t* window) {
	double start = hui_clock_seconds();
	hui_vec2_t size = hui_window_size(window);
	int pass;
	uint32_t i;

	/* a second pass is needed when the arrangement changes a desired size (wrapped text) */
	for (pass = 0; pass < 4 && window->needs_layout; pass++) {
		window->needs_layout = HUI_FALSE;
		if (window->root) {
			hui_widget_measure(window->root);
			/* the content obeys its slot like any child: padding and alignment in the window */
			hui_widget_arrange_slot(window->root, hui_rect(0.0f, 0.0f, size.x, size.y));
		}
		for (i = 0; i < window->popup_count; i++) hui_window_layout_popup(window, &window->popups[i]);
	}
	window->needs_layout = HUI_FALSE;
	window->needs_paint = HUI_TRUE;
	window->stats.layout_ms = (hui_clock_seconds() - start) * 1000.0;
	hui_window_refresh_hover(window);
}

/* ---------------------------------------------------------------------------
 *  Paint
 * ------------------------------------------------------------------------- */

static int hui_window_can_paint(const hui_window_t* window) {
	return window->surface_ready && window->width_px > 0 && window->height_px > 0;
}

static void hui_window_paint(hui_window_t* window, HUI_Capture* capture) {
	hui_frame_t* frame;
	HUI_Painter painter;
	double start;
	uint32_t i, widgets = 0;
	hui_color_t clear;

	window->needs_paint = HUI_FALSE;
	if (!hui_window_can_paint(window) || (!window->visible && !capture)) return;

	/* never queue more than a couple of frames: this is what paces a busy UI thread */
	hui_renderer_throttle(&window->frames_in_flight, HUI_MAX_FRAMES_IN_FLIGHT);

	frame = hui_renderer_acquire_frame();
	start = hui_clock_seconds();

	memset(&painter, 0, sizeof(painter));
	painter.list = &frame->list;
	painter.window = window;
	painter.scale = window->scale;
	painter.clip = hui_box(0.0f, 0.0f, (float)window->width_px, (float)window->height_px);
	painter.opacity = 1.0f;
	painter.text_color = HUI_THEME_COLOR(HUI_COLOR_TEXT);
	painter.enabled = HUI_TRUE;

	hui_atomic_store(&window->realtime_viewports, 0);
	if (window->root) {
		hui_widget_paint(window->root, &painter);
		widgets += window->root->subtree_count;
	}
	for (i = 0; i < window->popup_count; i++) {
		hui_widget_paint(window->popups[i].root, &painter);
		widgets += window->popups[i].root->subtree_count;
	}
	hui_window_paint_tooltip(window, &painter);

	/* new glyphs reach the GPU before the frame that uses them */
	hui_fonts_flush();
	hui_fonts_resolve_pages(&frame->list);

	window->stats.paint_ms = (hui_clock_seconds() - start) * 1000.0;
	window->stats.render_ms = (double)hui_atomic_load(&window->render_us) / 1000.0;
	window->stats.widget_count = widgets;
	window->stats.instance_count = frame->list.instance_count;
	window->stats.draw_call_count = frame->list.command_count;
	window->stats.paint_job_count = (HUI_uint)hui_paint_take_job_count();
	window->stats.frame_index = window->frame_index;

	clear = HUI_THEME_COLOR(HUI_COLOR_WINDOW);
	frame->data.window = window->id;
	frame->data.framebuffer_width = (HUI_uint)window->width_px;
	frame->data.framebuffer_height = (HUI_uint)window->height_px;
	if (window->has_clear_color) {
		memcpy(frame->data.clear_color, window->clear_color, sizeof(window->clear_color));
	} else {
		hui_color_to_floats(clear, &frame->data.clear_color[0], &frame->data.clear_color[1],
			&frame->data.clear_color[2], &frame->data.clear_color[3]);
	}
	frame->data.frame_index = window->frame_index++;
	frame->capture = capture;
	frame->frames_in_flight = &window->frames_in_flight;
	frame->render_us = &window->render_us;
	hui_renderer_submit_frame(frame);

	/* native child windows follow the viewports that were just painted, the others hide */
	for (i = 0; i < window->native_viewport_count; i++) {
		hui_viewport_native_sync(window->native_viewports[i], window->frame_index - 1);
	}
}

int hui_window_wants_frame(hui_window_t* window) {
	if (window->dead) return HUI_FALSE;
	if (window->needs_layout) return HUI_TRUE;
	if (!hui_window_can_paint(window) || !window->visible) return HUI_FALSE;
	return window->needs_paint || window->continuous ||
		hui_atomic_load(&window->realtime_viewports) > 0 || hui_atomic_load(&window->redraw_requested) != 0;
}

void hui_window_update(hui_window_t* window) {
	if (window->dead) return;
	if (hui_atomic_exchange(&window->redraw_requested, 0)) window->needs_paint = HUI_TRUE;
	if (window->continuous || hui_atomic_load(&window->realtime_viewports) > 0) window->needs_paint = HUI_TRUE;
	if (window->needs_layout) hui_window_layout(window);
	if (window->needs_paint) hui_window_paint(window, NULL);
}

void hui_invalidate_all_windows(void) {
	uint32_t i;
	for (i = 0; i < g_hui.widgets.used; i++) {
		hui_widget_t* widget = (hui_widget_t*)hui_pool_at(&g_hui.widgets, i);
		if (widget) widget->flags |= HUI_WF_LAYOUT_DIRTY;
	}
	for (i = 0; i < g_hui.windows.used; i++) {
		hui_window_t* window = (hui_window_t*)hui_pool_at(&g_hui.windows, i);
		if (window && !window->dead) {
			window->needs_layout = HUI_TRUE;
			window->needs_paint = HUI_TRUE;
		}
	}
}

void hui_repaint_all_windows(void) {
	uint32_t i;
	for (i = 0; i < g_hui.windows.used; i++) {
		hui_window_t* window = (hui_window_t*)hui_pool_at(&g_hui.windows, i);
		if (window && !window->dead) window->needs_paint = HUI_TRUE;
	}
}

/* ---------------------------------------------------------------------------
 *  Input (positions in logical units)
 * ------------------------------------------------------------------------- */

static void hui_window_mouse_move(hui_window_t* window, float x, float y) {
	HUI_Event event;
	float dx = window->mouse_inside ? x - window->mouse_x : 0.0f;
	float dy = window->mouse_inside ? y - window->mouse_y : 0.0f;

	window->mouse_x = x;
	window->mouse_y = y;
	window->mouse_inside = HUI_TRUE;
	if (window->tooltip_visible && (fabsf(x - window->tooltip_x) > 6.0f || fabsf(y - window->tooltip_y) > 6.0f)) {
		hui_window_tooltip_arm(window);
	}

	event = hui_window_event(window, HUI_EVENT_MOUSE_MOVE);
	event.delta_x = dx;
	event.delta_y = dy;
	if (window->capture) {
		hui_widget_send(window->capture, &event);
	} else {
		hui_widget_t* hit = hui_window_hit(window, x, y, NULL);
		hui_window_set_hover(window, hit);
		if (window->dead) return;
		hit = window->hover;
		if (hit && hui_widget_is_enabled(hit)) hui_widget_dispatch(hit, &event);
	}
	if (!window->dead) hui_window_update_cursor(window);
}

static void hui_window_mouse_leave(hui_window_t* window) {
	window->mouse_inside = HUI_FALSE;
	if (!window->capture) hui_window_set_hover(window, NULL);
	if (!window->dead) hui_window_tooltip_hide(window);
}

static hui_widget_t* hui_focusable_ancestor(hui_widget_t* widget) {
	for (; widget; widget = widget->parent) {
		if (widget->flags & HUI_WF_FOCUSABLE) return widget;
		if (widget->flags & HUI_WF_POPUP) break;
	}
	return NULL;
}

static void hui_window_open_context_menu(hui_window_t* window, hui_widget_t* target) {
	hui_widget_t* walk;
	for (walk = target; walk; walk = walk->parent) {
		hui_widget_t* menu;
		if (walk->context_menu == HUI_INVALID_ID) continue;
		menu = hui_widget_get(walk->context_menu);
		if (menu && menu->cls->type == HUI_WIDGET_MENU) hui_menu_popup(menu, window, window->mouse_x, window->mouse_y);
		return;
	}
}

static void hui_window_mouse_button(hui_window_t* window, HUI_EMouseButton button, int pressed) {
	HUI_Event event;
	hui_widget_t* target;
	hui_widget_t* hit;
	int popup_index = -1;
	float x = window->mouse_x, y = window->mouse_y;

	if ((int)button < 0 || (int)button > HUI_MOUSE_X2) return;
	if (pressed) window->buttons |= 1u << button;
	else window->buttons &= ~(1u << button);
	hui_window_tooltip_hide(window);

	hit = hui_window_hit(window, x, y, &popup_index);

	if (pressed) {
		double now = hui_clock_seconds();
		window->focus_visible = HUI_FALSE;
		if (button == (HUI_EMouseButton)window->click_button && now - window->click_time < HUI_DOUBLE_CLICK_TIME &&
			fabsf(x - window->click_x) < HUI_DOUBLE_CLICK_DISTANCE && fabsf(y - window->click_y) < HUI_DOUBLE_CLICK_DISTANCE) {
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
			hui_window_close_popups(window);
			hui_window_refresh_hover(window);
			return;
		}

		target = window->capture ? window->capture : hit;
		if (!target) {
			hui_window_set_focus(window, NULL);
			return;
		}
		if (!hui_widget_is_enabled(target)) return;
		/* popups do not take the keyboard focus from the widget that opened them */
		if (popup_index < 0 && !window->capture) {
			hui_window_set_focus(window, hui_focusable_ancestor(target));
			if (window->dead || (target->flags & HUI_WF_DEAD)) return;
		}
		event = hui_window_event(window, HUI_EVENT_MOUSE_DOWN);
		event.button = button;
		event.clicks = window->click_count;
		if (window->capture) hui_widget_send(target, &event);
		else hui_widget_dispatch(target, &event);
	} else {
		int was_captured = window->capture != NULL;
		target = window->capture ? window->capture : hit;
		if (target && hui_widget_is_enabled(target)) {
			event = hui_window_event(window, HUI_EVENT_MOUSE_UP);
			event.button = button;
			event.clicks = window->click_count;
			if (window->capture) hui_widget_send(target, &event);
			else hui_widget_dispatch(target, &event);
		}
		if (window->dead) return;
		if (window->buttons == 0) {
			/* the hover was frozen during the capture: bring it up to date */
			window->capture = NULL;
			if (was_captured) hui_window_refresh_hover(window);
		}
		if (button == HUI_MOUSE_RIGHT && hit && !(hit->flags & HUI_WF_DEAD) && !window->dead) {
			hui_window_open_context_menu(window, hit);
		}
	}
	if (!window->dead) hui_window_update_cursor(window);
}

static void hui_window_mouse_wheel(hui_window_t* window, float wheel_x, float wheel_y) {
	HUI_Event event;
	hui_widget_t* target = window->capture ? window->capture : hui_window_hit(window, window->mouse_x, window->mouse_y, NULL);
	hui_window_tooltip_hide(window);
	if (!target || !hui_widget_is_enabled(target)) return;
	event = hui_window_event(window, HUI_EVENT_MOUSE_WHEEL);
	event.wheel_x = wheel_x;
	event.wheel_y = wheel_y;
	hui_widget_dispatch(target, &event);
}

static void hui_window_key(hui_window_t* window, HUI_EKey key, int pressed, uint32_t modifiers, int repeat) {
	HUI_Event event;
	uint32_t i;

	window->modifiers = modifiers;
	if (pressed) hui_window_tooltip_hide(window);
	event = hui_window_event(window, pressed ? HUI_EVENT_KEY_DOWN : HUI_EVENT_KEY_UP);
	event.key = key;
	event.repeat = repeat;

	/* the top popup sees the keyboard first (menu navigation) */
	if (window->popup_count > 0) {
		hui_widget_t* top = window->popups[window->popup_count - 1].root;
		if (hui_widget_dispatch(top, &event) || window->dead) return;
		if (pressed && key == HUI_KEY_ESCAPE && window->popup_count > 0) {
			hui_window_close_popup(window, window->popups[window->popup_count - 1].root);
			return;
		}
	}
	if (window->focus && hui_widget_is_enabled(window->focus)) {
		if (hui_widget_dispatch(window->focus, &event) || window->dead) return;
	}
	if (!pressed) return;

	if (key == HUI_KEY_TAB && !(modifiers & (HUI_MOD_CTRL | HUI_MOD_ALT))) {
		hui_window_focus_next(window, (modifiers & HUI_MOD_SHIFT) ? -1 : 1);
		return;
	}
	if (key == HUI_KEY_ESCAPE && window->focus) {
		hui_window_set_focus(window, NULL);
		return;
	}
	for (i = 0; i < window->shortcut_count; i++) {
		hui_shortcut_t* shortcut = &window->shortcuts[i];
		char* action;
		if (shortcut->key != key || shortcut->modifiers != (modifiers & 0xFu)) continue;
		/* copy: the callback may unregister the shortcut */
		action = hui_strdup(shortcut->action);
		hui_fire_action(window->id, action);
		hui_free(action);
		return;
	}
	/* shortcuts shown in the menus of the window */
	{
		hui_widget_t* item = hui_menu_find_shortcut(window->root, key, modifiers & 0xFu);
		if (item) hui_menu_item_trigger(item);
	}
}

static void hui_window_text(hui_window_t* window, uint32_t codepoint) {
	HUI_Event event;
	if (!window->focus || !hui_widget_is_enabled(window->focus)) return;
	event = hui_window_event(window, HUI_EVENT_TEXT);
	event.codepoint = codepoint;
	hui_widget_dispatch(window->focus, &event);
}

static void hui_window_resized(hui_window_t* window, int width_px, int height_px, float platform_scale) {
	float scale = (platform_scale > 0.0f ? platform_scale : 1.0f) * g_hui.ui_scale;
	hui_vec2_t size;
	if (window->width_px == width_px && window->height_px == height_px && window->scale == scale) return;
	window->width_px = width_px;
	window->height_px = height_px;
	window->scale = scale;
	hui_window_close_popups(window);
	window->needs_layout = HUI_TRUE;
	window->needs_paint = HUI_TRUE;
	if (window->resize_callback) {
		size = hui_window_size(window);
		window->resize_callback(window->id, (int)(size.x + 0.5f), (int)(size.y + 0.5f), window->resize_user);
	}
}

/* ---------------------------------------------------------------------------
 *  Platform events
 * ------------------------------------------------------------------------- */

void hui_window_on_platform_event(const hui_pevent_t* pevent) {
	hui_window_t* window = (hui_window_t*)pevent->user;
	float scale;

	if (pevent->type == HUI_PEVENT_MODAL_TICK) {
		HUI_Update();
		return;
	}
	if (!window || window->dead) return;
	scale = window->scale > 0.0f ? window->scale : 1.0f;

	hui_dispatch_begin();
	switch (pevent->type) {
		case HUI_PEVENT_CLOSE:
			if (!window->close_callback || window->close_callback(window->id, window->close_user)) {
				if (!window->dead) hui_window_destroy(window);
			}
			break;
		case HUI_PEVENT_RESIZE:
			hui_window_resized(window, pevent->width, pevent->height, pevent->scale);
			break;
		case HUI_PEVENT_EXPOSE:
			window->needs_paint = HUI_TRUE;
			break;
		case HUI_PEVENT_MOUSE_MOVE:
			window->modifiers = pevent->modifiers;
			hui_window_mouse_move(window, (float)pevent->x / scale, (float)pevent->y / scale);
			break;
		case HUI_PEVENT_MOUSE_BUTTON:
			window->modifiers = pevent->modifiers;
			hui_window_mouse_move(window, (float)pevent->x / scale, (float)pevent->y / scale);
			if (!window->dead) hui_window_mouse_button(window, (HUI_EMouseButton)pevent->button, pevent->pressed);
			break;
		case HUI_PEVENT_MOUSE_WHEEL:
			window->modifiers = pevent->modifiers;
			hui_window_mouse_wheel(window, pevent->wheel_x, pevent->wheel_y);
			break;
		case HUI_PEVENT_MOUSE_LEAVE:
			hui_window_mouse_leave(window);
			break;
		case HUI_PEVENT_KEY:
			hui_window_key(window, pevent->key, pevent->pressed, pevent->modifiers, pevent->repeat);
			break;
		case HUI_PEVENT_TEXT:
			hui_window_text(window, pevent->codepoint);
			break;
		case HUI_PEVENT_FOCUS:
			if (!pevent->focused) {
				/* the window lost the keyboard: drop transient state */
				window->capture = NULL;
				window->buttons = 0;
				window->modifiers = 0;
				hui_window_close_popups(window);
				hui_window_tooltip_hide(window);
			}
			window->needs_paint = HUI_TRUE;
			break;
		case HUI_PEVENT_MODAL_TICK:
			break;
	}
	hui_dispatch_end();
}

/* ---------------------------------------------------------------------------
 *  Shortcuts
 * ------------------------------------------------------------------------- */

void hui_window_remove_shortcut(hui_window_t* window, HUI_EKey key, uint32_t modifiers) {
	uint32_t i;
	for (i = 0; i < window->shortcut_count; i++) {
		if (window->shortcuts[i].key == key && window->shortcuts[i].modifiers == modifiers) {
			hui_free(window->shortcuts[i].action);
			window->shortcuts[i] = window->shortcuts[--window->shortcut_count];
			return;
		}
	}
}

void hui_window_add_shortcut(hui_window_t* window, HUI_EKey key, uint32_t modifiers, const char* action) {
	hui_shortcut_t* shortcut;
	hui_window_remove_shortcut(window, key, modifiers);
	HUI_ARRAY_RESERVE(window->shortcuts, window->shortcut_cap, window->shortcut_count + 1);
	shortcut = &window->shortcuts[window->shortcut_count++];
	shortcut->key = key;
	shortcut->modifiers = modifiers;
	shortcut->action = hui_strdup(action);
}

/* ---------------------------------------------------------------------------
 *  Destruction
 * ------------------------------------------------------------------------- */

static void hui_window_free(hui_window_t* window) {
	uint32_t i;
	/* waits for the frames still queued: they point into this structure */
	if (window->surface_ready) hui_renderer_destroy_surface(window->id);
	if (window->platform) g_hui.platform->window_destroy(window->platform);
	hui_pool_remove(&g_hui.windows, window->id);
	for (i = 0; i < window->shortcut_count; i++) hui_free(window->shortcuts[i].action);
	hui_free(window->shortcuts);
	hui_free(window->native_viewports);
	hui_free(window->popups);
	hui_free(window->title);
	hui_free(window);
}

void hui_window_destroy(hui_window_t* window) {
	hui_widget_t* root;
	if (window->dead) return;
	hui_window_close_popups(window);
	hui_window_tooltip_hide(window);
	root = window->root;
	if (root) hui_widget_destroy(root);
	window->root = NULL;
	window->hover = window->focus = window->capture = NULL;
	/* the handle stops resolving now; the slot stays reserved until the surface is gone */
	window->dead = HUI_TRUE;
	if (g_hui.dispatch_depth > 0) {
		HUI_ARRAY_RESERVE(g_hui.dead_windows, g_hui.dead_window_cap, g_hui.dead_window_count + 1);
		g_hui.dead_windows[g_hui.dead_window_count++] = window;
	} else {
		hui_window_free(window);
	}
}

void hui_windows_collect(void) {
	uint32_t i;
	for (i = 0; i < g_hui.dead_window_count; i++) hui_window_free(g_hui.dead_windows[i]);
	g_hui.dead_window_count = 0;
}

void hui_windows_destroy_all(void) {
	uint32_t i;
	for (i = 0; i < g_hui.windows.used; i++) {
		hui_window_t* window = (hui_window_t*)hui_pool_at(&g_hui.windows, i);
		if (window && !window->dead) hui_window_destroy(window);
	}
	hui_windows_collect();
}

/* ---------------------------------------------------------------------------
 *  Public API: windows
 * ------------------------------------------------------------------------- */

HUI_id HUI_CreateWindow(const char* title, int width, int height, HUI_uint flags) {
	hui_window_t* window;
	hui_pwindow_desc_t desc;
	HUI_NativeWindow native;

	HUI_API_GUARD(HUI_INVALID_ID);
	if (width <= 0 || height <= 0) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_CreateWindow: invalid size %d x %d", width, height);
		return HUI_INVALID_ID;
	}

	window = (hui_window_t*)hui_calloc(sizeof(hui_window_t));
	window->title = hui_strdup(title ? title : "");
	window->tooltip_timer = HUI_INVALID_ID;
	window->cursor = HUI_CURSOR_ARROW;
	window->click_button = -1;
	window->id = hui_pool_add(&g_hui.windows, window);
	if (window->id == HUI_INVALID_ID) {
		hui_free(window->title);
		hui_free(window);
		return HUI_INVALID_ID;
	}

	desc.title = window->title;
	desc.width = (int)((float)width * g_hui.ui_scale + 0.5f);
	desc.height = (int)((float)height * g_hui.ui_scale + 0.5f);
	desc.flags = flags;
	desc.user = window;
	window->platform = g_hui.platform->window_create(&desc);
	if (!window->platform) {
		hui_error(HUI_ERROR_PLATFORM, HUI_SEVERITY_ERROR, "HUI_CreateWindow: the window system refused to create the window");
		hui_pool_remove(&g_hui.windows, window->id);
		hui_free(window->title);
		hui_free(window);
		return HUI_INVALID_ID;
	}
	g_hui.platform->window_get_size(window->platform, &window->width_px, &window->height_px);
	window->scale = g_hui.platform->window_get_scale(window->platform) * g_hui.ui_scale;
	if (window->scale <= 0.0f) window->scale = 1.0f;

	memset(&native, 0, sizeof(native));
	g_hui.platform->window_get_native(window->platform, &native);
	if (!hui_renderer_create_surface(window->id, &native, (uint32_t)hui_maxi(window->width_px, 1),
		(uint32_t)hui_maxi(window->height_px, 1))) {
		hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR, "HUI_CreateWindow: cannot create the rendering surface");
		g_hui.platform->window_destroy(window->platform);
		hui_pool_remove(&g_hui.windows, window->id);
		hui_free(window->title);
		hui_free(window);
		return HUI_INVALID_ID;
	}
	window->surface_ready = HUI_TRUE;
	window->needs_layout = HUI_TRUE;
	window->needs_paint = HUI_TRUE;
	window->visible = (flags & HUI_WINDOW_HIDDEN) == 0;
	g_hui.had_window = HUI_TRUE;
	if (window->visible) g_hui.platform->window_show(window->platform, HUI_TRUE);
	return window->id;
}

void HUI_DeleteWindow(HUI_id id) {
	HUI_WINDOW_VOID(window, id);
	hui_window_destroy(window);
}

int HUI_IsValidWindow(HUI_id window) {
	return g_hui.initialized && hui_window_get(window) != NULL;
}

void HUI_SetWindowContent(HUI_id id, HUI_id widget_id) {
	hui_widget_t* widget = NULL;
	HUI_WINDOW_VOID(window, id);
	if (widget_id != HUI_INVALID_ID) {
		widget = hui_widget_check(widget_id, 0, __func__);
		if (!widget) return;
	}
	if (window->root == widget) return;
	if (window->root) hui_widget_destroy(window->root);
	window->root = NULL;
	if (widget) {
		if (widget->parent || widget->window) hui_widget_detach(widget);
		window->root = widget;
		hui_widget_set_window(widget, window);
		widget->flags |= HUI_WF_LAYOUT_DIRTY;
	}
	window->needs_layout = HUI_TRUE;
	window->needs_paint = HUI_TRUE;
}

HUI_id HUI_GetWindowContent(HUI_id id) {
	HUI_WINDOW(window, id, HUI_INVALID_ID);
	return window->root ? window->root->id : HUI_INVALID_ID;
}

void HUI_SetWindowTitle(HUI_id id, const char* title) {
	HUI_WINDOW_VOID(window, id);
	if (!hui_str_set(&window->title, title ? title : "")) return;
	g_hui.platform->window_set_title(window->platform, window->title);
}

void HUI_SetWindowSize(HUI_id id, int width, int height) {
	HUI_WINDOW_VOID(window, id);
	if (width <= 0 || height <= 0) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_SetWindowSize: invalid size");
		return;
	}
	g_hui.platform->window_set_size(window->platform, (int)((float)width * window->scale + 0.5f),
		(int)((float)height * window->scale + 0.5f));
}

void HUI_GetWindowSize(HUI_id id, int* width, int* height) {
	hui_window_t* window;
	hui_vec2_t size;
	if (width) *width = 0;
	if (height) *height = 0;
	window = hui_window_check(id, __func__);
	if (!window) return;
	size = hui_window_size(window);
	if (width) *width = (int)(size.x + 0.5f);
	if (height) *height = (int)(size.y + 0.5f);
}

void HUI_SetWindowMinSize(HUI_id id, int width, int height) {
	HUI_WINDOW_VOID(window, id);
	g_hui.platform->window_set_min_size(window->platform, (int)((float)hui_maxi(width, 0) * window->scale + 0.5f),
		(int)((float)hui_maxi(height, 0) * window->scale + 0.5f));
}

void HUI_SetWindowPosition(HUI_id id, int x, int y) {
	HUI_WINDOW_VOID(window, id);
	g_hui.platform->window_set_position(window->platform, x, y);
}

void HUI_GetWindowPosition(HUI_id id, int* x, int* y) {
	hui_window_t* window;
	int px = 0, py = 0;
	if (x) *x = 0;
	if (y) *y = 0;
	window = hui_window_check(id, __func__);
	if (!window) return;
	g_hui.platform->window_get_position(window->platform, &px, &py);
	if (x) *x = px;
	if (y) *y = py;
}

void HUI_GetWindowFramebufferSize(HUI_id id, int* width, int* height) {
	hui_window_t* window;
	if (width) *width = 0;
	if (height) *height = 0;
	window = hui_window_check(id, __func__);
	if (!window) return;
	if (width) *width = window->width_px;
	if (height) *height = window->height_px;
}

float HUI_GetWindowScale(HUI_id id) {
	HUI_WINDOW(window, id, 1.0f);
	return window->scale;
}

void HUI_ShowWindow(HUI_id id) {
	HUI_WINDOW_VOID(window, id);
	window->visible = HUI_TRUE;
	window->needs_paint = HUI_TRUE;
	g_hui.platform->window_show(window->platform, HUI_TRUE);
}

void HUI_HideWindow(HUI_id id) {
	HUI_WINDOW_VOID(window, id);
	window->visible = HUI_FALSE;
	g_hui.platform->window_show(window->platform, HUI_FALSE);
}

void HUI_MaximizeWindow(HUI_id id) {
	HUI_WINDOW_VOID(window, id);
	g_hui.platform->window_set_state(window->platform, HUI_PWINDOW_MAXIMIZED);
}

void HUI_MinimizeWindow(HUI_id id) {
	HUI_WINDOW_VOID(window, id);
	g_hui.platform->window_set_state(window->platform, HUI_PWINDOW_MINIMIZED);
}

void HUI_RestoreWindow(HUI_id id) {
	HUI_WINDOW_VOID(window, id);
	g_hui.platform->window_set_state(window->platform, HUI_PWINDOW_NORMAL);
}

void HUI_SetWindowClearColor(HUI_id id, float r, float g, float b, float a) {
	HUI_WINDOW_VOID(window, id);
	window->has_clear_color = HUI_TRUE;
	window->clear_color[0] = r;
	window->clear_color[1] = g;
	window->clear_color[2] = b;
	window->clear_color[3] = a;
	window->needs_paint = HUI_TRUE;
}

void HUI_RequestWindowRedraw(HUI_id id) {
	if (!g_hui.initialized) return;
	if (!HUI_IsUIThread()) {
		/* the window table belongs to the UI thread: ask for every window instead */
		hui_atomic_store(&g_hui.redraw_all, 1);
		hui_wake();
		return;
	}
	{
		hui_window_t* window = hui_window_get(id);
		if (window) hui_atomic_store(&window->redraw_requested, 1);
	}
}

void HUI_SetWindowContinuousRedraw(HUI_id id, int enabled) {
	HUI_WINDOW_VOID(window, id);
	window->continuous = enabled != 0;
	if (enabled) window->needs_paint = HUI_TRUE;
}

void HUI_SetWindowCloseCallback(HUI_id id, HUI_CWindowClose callback, void* user_data) {
	HUI_WINDOW_VOID(window, id);
	window->close_callback = callback;
	window->close_user = user_data;
}

void HUI_SetWindowResizeCallback(HUI_id id, HUI_CWindowResize callback, void* user_data) {
	HUI_WINDOW_VOID(window, id);
	window->resize_callback = callback;
	window->resize_user = user_data;
}

void HUI_SetWindowUserHandle(HUI_id id, void* handle) {
	HUI_WINDOW_VOID(window, id);
	window->user_handle = handle;
}

void* HUI_GetWindowUserHandle(HUI_id id) {
	HUI_WINDOW(window, id, NULL);
	return window->user_handle;
}

int HUI_GetWindowNativeHandle(HUI_id id, HUI_NativeWindow* native) {
	HUI_WINDOW(window, id, HUI_FALSE);
	if (!native) return HUI_FALSE;
	memset(native, 0, sizeof(*native));
	g_hui.platform->window_get_native(window->platform, native);
	return native->system != HUI_WINDOW_SYSTEM_NONE;
}

uint8_t* HUI_CaptureWindow(HUI_id id, int* width, int* height) {
	HUI_Capture capture;
	hui_window_t* window;
	if (width) *width = 0;
	if (height) *height = 0;
	window = hui_window_check(id, __func__);
	if (!window) return NULL;
	memset(&capture, 0, sizeof(capture));
	if (window->needs_layout) hui_window_layout(window);
	hui_window_paint(window, &capture);
	/* the capture structure lives on this stack: wait for the render thread */
	hui_renderer_sync();
	if (width) *width = capture.width;
	if (height) *height = capture.height;
	return capture.pixels;
}

void HUI_GetFrameStats(HUI_id id, HUI_FrameStats* stats) {
	hui_window_t* window;
	if (!stats) return;
	memset(stats, 0, sizeof(*stats));
	window = hui_window_check(id, __func__);
	if (window) *stats = window->stats;
}

/* ---------------------------------------------------------------------------
 *  Public API: input state and synthetic input
 * ------------------------------------------------------------------------- */

HUI_id HUI_GetFocusedWidget(HUI_id id) {
	HUI_WINDOW(window, id, HUI_INVALID_ID);
	return window->focus ? window->focus->id : HUI_INVALID_ID;
}

HUI_id HUI_GetHoveredWidget(HUI_id id) {
	HUI_WINDOW(window, id, HUI_INVALID_ID);
	return window->hover ? window->hover->id : HUI_INVALID_ID;
}

void HUI_GetMousePosition(HUI_id id, float* x, float* y) {
	hui_window_t* window;
	if (x) *x = 0.0f;
	if (y) *y = 0.0f;
	window = hui_window_check(id, __func__);
	if (!window) return;
	if (x) *x = window->mouse_x;
	if (y) *y = window->mouse_y;
}

/* Synthetic input acts on the current layout, like real input does. */
static hui_window_t* hui_window_inject_begin(HUI_id id, const char* function) {
	hui_window_t* window = hui_window_check(id, function);
	if (!window) return NULL;
	if (window->needs_layout) hui_window_layout(window);
	hui_dispatch_begin();
	return window;
}

void HUI_InjectMouseMove(HUI_id id, float x, float y) {
	hui_window_t* window = hui_window_inject_begin(id, __func__);
	if (!window) return;
	hui_window_mouse_move(window, x, y);
	hui_dispatch_end();
}

void HUI_InjectMouseButton(HUI_id id, HUI_EMouseButton button, int pressed) {
	hui_window_t* window = hui_window_inject_begin(id, __func__);
	if (!window) return;
	hui_window_mouse_button(window, button, pressed);
	hui_dispatch_end();
}

void HUI_InjectMouseWheel(HUI_id id, float wheel_x, float wheel_y) {
	hui_window_t* window = hui_window_inject_begin(id, __func__);
	if (!window) return;
	hui_window_mouse_wheel(window, wheel_x, wheel_y);
	hui_dispatch_end();
}

void HUI_InjectKey(HUI_id id, HUI_EKey key, int pressed, HUI_uint modifiers) {
	hui_window_t* window = hui_window_inject_begin(id, __func__);
	if (!window) return;
	hui_window_key(window, key, pressed, modifiers, HUI_FALSE);
	hui_dispatch_end();
}

void HUI_InjectText(HUI_id id, const char* text) {
	hui_window_t* window = hui_window_inject_begin(id, __func__);
	const char* cursor = text;
	const char* end;
	if (!window) return;
	end = text ? text + strlen(text) : NULL;
	while (text && cursor < end && !window->dead) {
		hui_window_text(window, hui_utf8_decode(&cursor, end));
	}
	hui_dispatch_end();
}

void HUI_RegisterShortcut(HUI_id id, HUI_EKey key, HUI_uint modifiers, const char* action) {
	HUI_WINDOW_VOID(window, id);
	if (!action || !action[0]) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_RegisterShortcut: empty action name");
		return;
	}
	hui_window_add_shortcut(window, key, modifiers & 0xFu, action);
}

void HUI_UnregisterShortcut(HUI_id id, HUI_EKey key, HUI_uint modifiers) {
	HUI_WINDOW_VOID(window, id);
	hui_window_remove_shortcut(window, key, modifiers & 0xFu);
}

void HUI_ClosePopups(HUI_id id) {
	HUI_WINDOW_VOID(window, id);
	hui_window_close_popups(window);
}
