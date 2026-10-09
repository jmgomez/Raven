/**
 * HUI - widgets/expander.c
 * Expander (Slate SExpandableArea): a clickable header that shows or hides
 * one child. Used for the sections of property panels.
 */

#include "common.h"

typedef struct hui_expander_t {
	hui_widget_t base;
	char* title;
	int expanded;
	HUI_CExpanderChanged callback;
	void* user_data;
} hui_expander_t;

static void hui_expander_init(hui_widget_t* widget) {
	((hui_expander_t*)widget)->expanded = HUI_TRUE;
}

static void hui_expander_destroy(hui_widget_t* widget) {
	hui_free(((hui_expander_t*)widget)->title);
}

static float hui_expander_header(const hui_widget_t* widget) {
	return hui_widget_metric(widget, HUI_METRIC_ROW_HEIGHT) + 4.0f;
}

static hui_vec2_t hui_expander_measure(hui_widget_t* widget) {
	hui_expander_t* expander = (hui_expander_t*)widget;
	float header = hui_expander_header(widget);
	hui_vec2_t desired;
	desired.x = header + hui_widget_text_width(widget, HUI_INVALID_ID, 0.0f, expander->title) +
		hui_widget_metric(widget, HUI_METRIC_PADDING_X);
	desired.y = header;
	if (expander->expanded && widget->child_count > 0) {
		hui_vec2_t child = hui_widget_measure_slot(widget->children[0]);
		desired.x = hui_maxf(desired.x, child.x);
		desired.y += child.y;
	}
	return desired;
}

static void hui_expander_arrange(hui_widget_t* widget) {
	hui_expander_t* expander = (hui_expander_t*)widget;
	float header = hui_expander_header(widget);
	if (expander->expanded && widget->child_count > 0 && !hui_widget_collapsed(widget->children[0])) {
		hui_widget_arrange_slot(widget->children[0],
			hui_rect(0.0f, header, widget->rect.w, hui_maxf(0.0f, widget->rect.h - header)));
	}
}

static int hui_expander_child_active(hui_widget_t* widget, hui_widget_t* child) {
	HUI_UNUSED(child);
	return ((hui_expander_t*)widget)->expanded;
}

static void hui_expander_set(hui_widget_t* widget, int expanded, int notify) {
	hui_expander_t* expander = (hui_expander_t*)widget;
	expanded = expanded != 0;
	if (expander->expanded == expanded) return;
	if (!expanded && widget->child_count > 0 && widget->window) {
		hui_window_forget_widget(widget->window, widget->children[0]);
	}
	expander->expanded = expanded;
	hui_widget_invalidate_layout(widget);
	if (notify) {
		if (expander->callback) expander->callback(widget->id, expanded, expander->user_data);
		if (hui_widget_alive(widget)) hui_widget_fire_action(widget);
	}
}

static int hui_expander_event(hui_widget_t* widget, const HUI_Event* event) {
	hui_expander_t* expander = (hui_expander_t*)widget;
	switch (event->type) {
		case HUI_EVENT_MOUSE_DOWN:
			if (event->button != HUI_MOUSE_LEFT || event->local_y >= hui_expander_header(widget)) return HUI_FALSE;
			hui_expander_set(widget, !expander->expanded, HUI_TRUE);
			return HUI_TRUE;
		case HUI_EVENT_KEY_DOWN:
			if (!(widget->flags & HUI_WF_FOCUSED)) return HUI_FALSE;
			if (event->key == HUI_KEY_ENTER || event->key == HUI_KEY_SPACE) {
				hui_expander_set(widget, !expander->expanded, HUI_TRUE);
				return HUI_TRUE;
			}
			return HUI_FALSE;
		case HUI_EVENT_MOUSE_MOVE:
		case HUI_EVENT_MOUSE_ENTER:
		case HUI_EVENT_MOUSE_LEAVE:
			hui_widget_invalidate_paint(widget);
			return HUI_FALSE;
		default:
			return HUI_FALSE;
	}
}

static void hui_expander_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_expander_t* expander = (hui_expander_t*)widget;
	float header = hui_expander_header(widget);
	hui_vec2_t origin = hui_widget_origin(widget);
	int hot = (widget->flags & HUI_WF_HOVERED) && widget->window &&
		widget->window->mouse_y - origin.y < header && widget->window->mouse_y - origin.y >= 0.0f;
	hui_color_t text = hui_widget_text_color(widget, painter);

	hui_paint_rect(painter, hui_rect(0.0f, 0.0f, widget->rect.w, header),
		hui_widget_color(widget, hot ? HUI_COLOR_HEADER_HOVERED : HUI_COLOR_HEADER));
	hui_paint_icon(painter, expander->expanded ? HUI_ICON_CHEVRON_DOWN : HUI_ICON_CHEVRON_RIGHT,
		header * 0.5f, header * 0.5f, 8.0f, text);
	if (expander->title) {
		hui_paint_text_in(painter, HUI_INVALID_ID, hui_widget_font_size(widget, 0.0f),
			hui_rect(header, 0.0f, hui_maxf(0.0f, widget->rect.w - header), header), 0.0f, expander->title, -1, text);
	}
}

static void hui_expander_paint_overlay(hui_widget_t* widget, HUI_Painter* painter) {
	hui_paint_focus(widget, painter, 0.0f);
}

static int hui_expander_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_expander_t* expander = (hui_expander_t*)widget;
	int flag;
	if (HUI_ATTR("Title") || HUI_ATTR("Text")) {
		hui_str_set(&expander->title, value);
		hui_widget_invalidate_layout(widget);
		return HUI_TRUE;
	}
	if (HUI_ATTR("Expanded")) {
		if (!hui_parse_bool(value, &flag)) return HUI_FALSE;
		hui_expander_set(widget, flag, HUI_FALSE);
		return HUI_TRUE;
	}
	return HUI_FALSE;
}

const hui_widget_class_t hui_class_expander = {
	.type = HUI_WIDGET_EXPANDER,
	.name = "Expander",
	.size = sizeof(hui_expander_t),
	.flags = HUI_CF_SINGLE_CHILD | HUI_CF_FOCUSABLE,
	.init = hui_expander_init,
	.destroy = hui_expander_destroy,
	.measure = hui_expander_measure,
	.arrange = hui_expander_arrange,
	.paint = hui_expander_paint,
	.paint_overlay = hui_expander_paint_overlay,
	.event = hui_expander_event,
	.set_attribute = hui_expander_set_attribute,
	.child_active = hui_expander_child_active
};

void HUI_SetExpanderTitle(HUI_id id, const char* title) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_EXPANDER);
	if (hui_str_set(&((hui_expander_t*)widget)->title, title)) hui_widget_invalidate_layout(widget);
}

void HUI_SetExpanderExpanded(HUI_id id, int expanded) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_EXPANDER);
	hui_expander_set(widget, expanded, HUI_FALSE);
}

int HUI_IsExpanderExpanded(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_EXPANDER, HUI_FALSE);
	return ((hui_expander_t*)widget)->expanded;
}

void HUI_SetExpanderChangedCallback(HUI_id id, HUI_CExpanderChanged callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_EXPANDER);
	((hui_expander_t*)widget)->callback = callback;
	((hui_expander_t*)widget)->user_data = user_data;
}
