/**
 * HUI - widgets/tab_view.c
 * TabView: a bar of tabs above one visible page. Every child is a page; the
 * title of its tab is the title of its slot (HUI_SetSlotTitle).
 */

#include "common.h"

typedef struct hui_tab_view_t {
	hui_widget_t base;
	int active;
	int hot;
	HUI_CTabChanged callback;
	void* user_data;
} hui_tab_view_t;

static void hui_tab_view_init(hui_widget_t* widget) {
	((hui_tab_view_t*)widget)->hot = -1;
}

static float hui_tab_bar_height(const hui_widget_t* widget) {
	return hui_widget_metric(widget, HUI_METRIC_CONTROL_HEIGHT) + 2.0f;
}

static const char* hui_tab_title(const hui_widget_t* child) {
	return (child->slot.title && child->slot.title[0]) ? child->slot.title : "Tab";
}

static float hui_tab_width(const hui_widget_t* widget, const hui_widget_t* child) {
	return hui_widget_text_width(widget, HUI_INVALID_ID, 0.0f, hui_tab_title(child)) +
		hui_widget_metric(widget, HUI_METRIC_PADDING_X) * 2.0f + 4.0f;
}

static hui_widget_t* hui_tab_view_page(hui_widget_t* widget) {
	hui_tab_view_t* tabs = (hui_tab_view_t*)widget;
	if (tabs->active < 0 || tabs->active >= (int)widget->child_count) return NULL;
	return widget->children[tabs->active];
}

static hui_vec2_t hui_tab_view_measure(hui_widget_t* widget) {
	hui_widget_t* page = hui_tab_view_page(widget);
	hui_vec2_t desired = page ? hui_widget_measure_slot(page) : hui_vec2(0.0f, 0.0f);
	float bar = 0.0f;
	uint32_t i;
	for (i = 0; i < widget->child_count; i++) bar += hui_tab_width(widget, widget->children[i]);
	desired.x = hui_maxf(desired.x, bar);
	desired.y += hui_tab_bar_height(widget);
	return desired;
}

static void hui_tab_view_arrange(hui_widget_t* widget) {
	hui_widget_t* page = hui_tab_view_page(widget);
	float bar = hui_tab_bar_height(widget);
	if (page && !hui_widget_collapsed(page)) {
		hui_widget_arrange_slot(page, hui_rect(0.0f, bar, widget->rect.w, hui_maxf(0.0f, widget->rect.h - bar)));
	}
}

static int hui_tab_view_child_active(hui_widget_t* widget, hui_widget_t* child) {
	return hui_tab_view_page(widget) == child;
}

static void hui_tab_view_children_changed(hui_widget_t* widget) {
	hui_tab_view_t* tabs = (hui_tab_view_t*)widget;
	if (tabs->active >= (int)widget->child_count) tabs->active = (int)widget->child_count - 1;
	if (tabs->active < 0 && widget->child_count > 0) tabs->active = 0;
}

static int hui_tab_at(const hui_widget_t* widget, float x, float y) {
	float position = 0.0f;
	uint32_t i;
	if (y < 0.0f || y >= hui_tab_bar_height(widget)) return -1;
	for (i = 0; i < widget->child_count; i++) {
		float width = hui_tab_width(widget, widget->children[i]);
		if (x >= position && x < position + width) return (int)i;
		position += width;
	}
	return -1;
}

static void hui_tab_view_activate(hui_widget_t* widget, int index, int notify) {
	hui_tab_view_t* tabs = (hui_tab_view_t*)widget;
	hui_widget_t* previous = hui_tab_view_page(widget);
	if (index < 0 || index >= (int)widget->child_count || index == tabs->active) return;
	if (previous && widget->window) hui_window_forget_widget(widget->window, previous);
	tabs->active = index;
	hui_widget_invalidate_layout(widget);
	if (notify) {
		if (tabs->callback) tabs->callback(widget->id, index, tabs->user_data);
		if (hui_widget_alive(widget)) hui_widget_fire_action(widget);
	}
}

static int hui_tab_view_event(hui_widget_t* widget, const HUI_Event* event) {
	hui_tab_view_t* tabs = (hui_tab_view_t*)widget;
	int index;
	switch (event->type) {
		case HUI_EVENT_MOUSE_MOVE:
			index = hui_tab_at(widget, event->local_x, event->local_y);
			if (index != tabs->hot) {
				tabs->hot = index;
				hui_widget_invalidate_paint(widget);
			}
			return HUI_FALSE;
		case HUI_EVENT_MOUSE_LEAVE:
			if (tabs->hot >= 0) {
				tabs->hot = -1;
				hui_widget_invalidate_paint(widget);
			}
			return HUI_FALSE;
		case HUI_EVENT_MOUSE_DOWN:
			index = hui_tab_at(widget, event->local_x, event->local_y);
			if (event->button != HUI_MOUSE_LEFT || index < 0) return HUI_FALSE;
			hui_tab_view_activate(widget, index, HUI_TRUE);
			return HUI_TRUE;
		default:
			return HUI_FALSE;
	}
}

static void hui_tab_view_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_tab_view_t* tabs = (hui_tab_view_t*)widget;
	float bar = hui_tab_bar_height(widget), position = 0.0f;
	hui_color_t text = hui_widget_text_color(widget, painter);
	uint32_t i;

	hui_paint_rect(painter, hui_rect(0.0f, 0.0f, widget->rect.w, bar), hui_widget_color(widget, HUI_COLOR_HEADER));
	hui_paint_rect(painter, hui_rect(0.0f, bar - 1.0f, widget->rect.w, 1.0f), hui_widget_color(widget, HUI_COLOR_SEPARATOR));
	for (i = 0; i < widget->child_count; i++) {
		float width = hui_tab_width(widget, widget->children[i]);
		hui_rect_t rect = hui_rect(position, 0.0f, width, bar);
		if ((int)i == tabs->active) {
			hui_paint_rect(painter, rect, hui_widget_color(widget, HUI_COLOR_TAB_ACTIVE));
			hui_paint_rect(painter, hui_rect(position, 0.0f, width, 2.0f), hui_widget_color(widget, HUI_COLOR_ACCENT));
		} else if ((int)i == tabs->hot && (widget->flags & HUI_WF_HOVERED)) {
			hui_paint_rect(painter, rect, hui_widget_color(widget, HUI_COLOR_HEADER_HOVERED));
		}
		hui_paint_text_in(painter, HUI_INVALID_ID, hui_widget_font_size(widget, 0.0f), rect, 0.5f,
			hui_tab_title(widget->children[i]), -1,
			(int)i == tabs->active ? text : hui_color_mix(text, hui_widget_color(widget, HUI_COLOR_HEADER), 0.3f));
		position += width;
	}
}

static int hui_tab_view_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	int index;
	if (HUI_ATTR("ActiveTab")) {
		if (!hui_parse_int(value, &index)) return HUI_FALSE;
		hui_tab_view_activate(widget, index, HUI_FALSE);
		return HUI_TRUE;
	}
	return HUI_FALSE;
}

const hui_widget_class_t hui_class_tab_view = {
	.type = HUI_WIDGET_TAB_VIEW,
	.name = "TabView",
	.size = sizeof(hui_tab_view_t),
	.init = hui_tab_view_init,
	.measure = hui_tab_view_measure,
	.arrange = hui_tab_view_arrange,
	.paint = hui_tab_view_paint,
	.event = hui_tab_view_event,
	.set_attribute = hui_tab_view_set_attribute,
	.child_active = hui_tab_view_child_active,
	.children_changed = hui_tab_view_children_changed
};

int HUI_AddTab(HUI_id id, const char* title, HUI_id content) {
	hui_widget_t* page;
	HUI_WIDGET(widget, id, HUI_WIDGET_TAB_VIEW, -1);
	page = hui_widget_check(content, 0, __func__);
	if (!page) return -1;
	if (hui_widget_is_inside(widget, page)) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_AddTab: the content contains the tab view");
		return -1;
	}
	hui_str_set(&page->slot.title, title);
	hui_widget_add_child(widget, page, -1);
	return (int)widget->child_count - 1;
}

void HUI_SetActiveTab(HUI_id id, int index) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_TAB_VIEW);
	hui_tab_view_activate(widget, index, HUI_FALSE);
}

int HUI_GetActiveTab(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_TAB_VIEW, -1);
	return ((hui_tab_view_t*)widget)->active;
}

void HUI_SetTabChangedCallback(HUI_id id, HUI_CTabChanged callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_TAB_VIEW);
	((hui_tab_view_t*)widget)->callback = callback;
	((hui_tab_view_t*)widget)->user_data = user_data;
}
