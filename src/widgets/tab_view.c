/**
 * Raven - widgets/tab_view.c
 * TabView: a bar of tabs above one visible page. Every child is a page; the
 * title of its tab is the title of its slot (Rslot_set_title).
 */

#include "common.h"

typedef struct rvn_tab_view_t {
	rvn_widget_t base;
	int active;
	int hot;
	RTabChangedCallback callback;
	void* user_data;
} rvn_tab_view_t;

static void rvn_tab_view_init(rvn_widget_t* widget) {
	((rvn_tab_view_t*)widget)->hot = -1;
}

static float rvn_tab_bar_height(const rvn_widget_t* widget) {
	/* the segmented control of the glass themes keeps some air under it */
	return rvn_widget_metric(widget, R_METRIC_CONTROL_HEIGHT) + (g_rvn.theme.glass ? 8.0f : 2.0f);
}

static const char* rvn_tab_title(const rvn_widget_t* child) {
	return (child->slot.title && child->slot.title[0]) ? child->slot.title : "Tab";
}

static float rvn_tab_width(const rvn_widget_t* widget, const rvn_widget_t* child) {
	return rvn_widget_text_width(widget, R_INVALID_ID, 0.0f, rvn_tab_title(child)) +
		rvn_widget_metric(widget, R_METRIC_PADDING_X) * 2.0f + 4.0f;
}

static rvn_widget_t* rvn_tab_view_page(rvn_widget_t* widget) {
	rvn_tab_view_t* tabs = (rvn_tab_view_t*)widget;
	if (tabs->active < 0 || tabs->active >= (int)widget->child_count) return NULL;
	return widget->children[tabs->active];
}

static rvn_vec2_t rvn_tab_view_measure(rvn_widget_t* widget) {
	rvn_widget_t* page = rvn_tab_view_page(widget);
	rvn_vec2_t desired = page ? rvn_widget_measure_slot(page) : rvn_vec2(0.0f, 0.0f);
	float bar = 0.0f;
	uint32_t i;
	for (i = 0; i < widget->child_count; i++) bar += rvn_tab_width(widget, widget->children[i]);
	desired.x = rvn_maxf(desired.x, bar);
	desired.y += rvn_tab_bar_height(widget);
	return desired;
}

static void rvn_tab_view_arrange(rvn_widget_t* widget) {
	rvn_widget_t* page = rvn_tab_view_page(widget);
	float bar = rvn_tab_bar_height(widget);
	if (page && !rvn_widget_collapsed(page)) {
		rvn_widget_arrange_slot(page, rvn_rect(0.0f, bar, widget->rect.w, rvn_maxf(0.0f, widget->rect.h - bar)));
	}
}

static int rvn_tab_view_child_active(rvn_widget_t* widget, rvn_widget_t* child) {
	return rvn_tab_view_page(widget) == child;
}

static void rvn_tab_view_children_changed(rvn_widget_t* widget) {
	rvn_tab_view_t* tabs = (rvn_tab_view_t*)widget;
	if (tabs->active >= (int)widget->child_count) tabs->active = (int)widget->child_count - 1;
	if (tabs->active < 0 && widget->child_count > 0) tabs->active = 0;
}

static int rvn_tab_at(const rvn_widget_t* widget, float x, float y) {
	float position = 0.0f;
	uint32_t i;
	if (y < 0.0f || y >= rvn_tab_bar_height(widget)) return -1;
	for (i = 0; i < widget->child_count; i++) {
		float width = rvn_tab_width(widget, widget->children[i]);
		if (x >= position && x < position + width) return (int)i;
		position += width;
	}
	return -1;
}

static void rvn_tab_view_activate(rvn_widget_t* widget, int index, int notify) {
	rvn_tab_view_t* tabs = (rvn_tab_view_t*)widget;
	rvn_widget_t* previous = rvn_tab_view_page(widget);
	if (index < 0 || index >= (int)widget->child_count || index == tabs->active) return;
	if (previous && widget->window) rvn_window_forget_widget(widget->window, previous);
	tabs->active = index;
	rvn_widget_invalidate_layout(widget);
	if (notify) {
		if (tabs->callback) tabs->callback(widget->id, index, tabs->user_data);
		if (rvn_widget_alive(widget)) rvn_widget_fire_action(widget);
	}
}

static int rvn_tab_view_event(rvn_widget_t* widget, const REvent* event) {
	rvn_tab_view_t* tabs = (rvn_tab_view_t*)widget;
	int index;
	switch (event->type) {
		case R_EVENT_MOUSE_MOVE:
			index = rvn_tab_at(widget, event->local_x, event->local_y);
			if (index != tabs->hot) {
				tabs->hot = index;
				rvn_widget_invalidate_paint(widget);
			}
			return R_FALSE;
		case R_EVENT_MOUSE_LEAVE:
			if (tabs->hot >= 0) {
				tabs->hot = -1;
				rvn_widget_invalidate_paint(widget);
			}
			return R_FALSE;
		case R_EVENT_MOUSE_DOWN:
			index = rvn_tab_at(widget, event->local_x, event->local_y);
			if (event->button != R_MOUSE_LEFT || index < 0) return R_FALSE;
			rvn_tab_view_activate(widget, index, R_TRUE);
			return R_TRUE;
		default:
			return R_FALSE;
	}
}

static void rvn_tab_view_paint(rvn_widget_t* widget, RPainter* painter) {
	rvn_tab_view_t* tabs = (rvn_tab_view_t*)widget;
	float bar = rvn_tab_bar_height(widget), position = 0.0f;
	rvn_color_t text = rvn_widget_text_color(widget, painter);
	uint32_t i;

	if (g_rvn.theme.glass) {
		/* a segmented control: a capsule holding the titles, the active one on a lighter pill of glass */
		float total = 0.0f;
		for (i = 0; i < widget->child_count; i++) total += rvn_tab_width(widget, widget->children[i]);
		if (total > 0.0f) {
			rvn_paint_box(painter, rvn_rect(0.0f, 2.0f, total, bar - 6.0f), (bar - 6.0f) * 0.5f,
				rvn_widget_color(widget, R_COLOR_TRACK), 0.0f, 0);
		}
		for (i = 0; i < widget->child_count; i++) {
			float width = rvn_tab_width(widget, widget->children[i]);
			rvn_rect_t rect = rvn_rect(position, 2.0f, width, bar - 6.0f);
			rvn_rect_t pill = rvn_rect(rect.x + 2.0f, rect.y + 2.0f, rect.w - 4.0f, rect.h - 4.0f);
			if ((int)i == tabs->active) {
				rvn_paint_surface(painter, widget, pill, pill.h * 0.5f, RVN_SURFACE_CONTROL, 1.0f, 0.6f);
			} else if ((int)i == tabs->hot && (widget->flags & RVN_WF_HOVERED)) {
				rvn_paint_box(painter, pill, pill.h * 0.5f, rvn_widget_color(widget, R_COLOR_HEADER_HOVERED), 0.0f, 0);
			}
			rvn_paint_text_in(painter, R_INVALID_ID, rvn_widget_font_size(widget, 0.0f), rect, 0.5f,
				rvn_tab_title(widget->children[i]), -1, (int)i == tabs->active ? text : rvn_color_fade(text, 0.62f));
			position += width;
		}
		return;
	}
	rvn_paint_rect(painter, rvn_rect(0.0f, 0.0f, widget->rect.w, bar), rvn_widget_color(widget, R_COLOR_HEADER));
	rvn_paint_rect(painter, rvn_rect(0.0f, bar - 1.0f, widget->rect.w, 1.0f), rvn_widget_color(widget, R_COLOR_SEPARATOR));
	for (i = 0; i < widget->child_count; i++) {
		float width = rvn_tab_width(widget, widget->children[i]);
		rvn_rect_t rect = rvn_rect(position, 0.0f, width, bar);
		if ((int)i == tabs->active) {
			rvn_paint_rect(painter, rect, rvn_widget_color(widget, R_COLOR_TAB_ACTIVE));
			rvn_paint_rect(painter, rvn_rect(position, 0.0f, width, 2.0f), rvn_widget_color(widget, R_COLOR_ACCENT));
		} else if ((int)i == tabs->hot && (widget->flags & RVN_WF_HOVERED)) {
			rvn_paint_rect(painter, rect, rvn_widget_color(widget, R_COLOR_HEADER_HOVERED));
		}
		rvn_paint_text_in(painter, R_INVALID_ID, rvn_widget_font_size(widget, 0.0f), rect, 0.5f,
			rvn_tab_title(widget->children[i]), -1,
			(int)i == tabs->active ? text : rvn_color_mix(text, rvn_widget_color(widget, R_COLOR_HEADER), 0.3f));
		position += width;
	}
}

static int rvn_tab_view_set_attribute(rvn_widget_t* widget, const char* name, const char* value) {
	int index;
	if (RVN_ATTR("ActiveTab")) {
		if (!rvn_parse_int(value, &index)) return R_FALSE;
		rvn_tab_view_activate(widget, index, R_FALSE);
		return R_TRUE;
	}
	return R_FALSE;
}

const rvn_widget_class_t rvn_class_tab_view = {
	.type = R_WIDGET_TAB_VIEW,
	.name = "TabView",
	.size = sizeof(rvn_tab_view_t),
	.init = rvn_tab_view_init,
	.measure = rvn_tab_view_measure,
	.arrange = rvn_tab_view_arrange,
	.paint = rvn_tab_view_paint,
	.event = rvn_tab_view_event,
	.set_attribute = rvn_tab_view_set_attribute,
	.child_active = rvn_tab_view_child_active,
	.children_changed = rvn_tab_view_children_changed
};

int Rtab_view_add_tab(Rid id, const char* title, Rid content) {
	rvn_widget_t* page;
	RVN_WIDGET(widget, id, R_WIDGET_TAB_VIEW, -1);
	page = rvn_widget_check(content, 0, __func__);
	if (!page) return -1;
	if (rvn_widget_is_inside(widget, page)) {
		rvn_error(R_ERROR_INVALID_OPERATION, R_SEVERITY_ERROR, "Rtab_view_add_tab: the content contains the tab view");
		return -1;
	}
	rvn_str_set(&page->slot.title, title);
	rvn_widget_add_child(widget, page, -1);
	return (int)widget->child_count - 1;
}

void Rtab_view_set_active(Rid id, int index) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_TAB_VIEW);
	rvn_tab_view_activate(widget, index, R_FALSE);
}

int Rtab_view_get_active(Rid id) {
	RVN_WIDGET(widget, id, R_WIDGET_TAB_VIEW, -1);
	return ((rvn_tab_view_t*)widget)->active;
}

void Rtab_view_set_changed_callback(Rid id, RTabChangedCallback callback, void* user_data) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_TAB_VIEW);
	((rvn_tab_view_t*)widget)->callback = callback;
	((rvn_tab_view_t*)widget)->user_data = user_data;
}
