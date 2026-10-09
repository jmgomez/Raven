/**
 * Raven - widgets/expander.c
 * Expander (Slate SExpandableArea): a clickable header that shows or hides
 * one child. Used for the sections of property panels.
 */

#include "common.h"

typedef struct rvn_expander_t {
	rvn_widget_t base;
	char* title;
	int expanded;
	RExpanderChangedCallback callback;
	void* user_data;
} rvn_expander_t;

static void rvn_expander_init(rvn_widget_t* widget) {
	((rvn_expander_t*)widget)->expanded = R_TRUE;
}

static void rvn_expander_destroy(rvn_widget_t* widget) {
	rvn_free(((rvn_expander_t*)widget)->title);
}

static float rvn_expander_header(const rvn_widget_t* widget) {
	return rvn_widget_metric(widget, R_METRIC_ROW_HEIGHT) + 4.0f;
}

static rvn_vec2_t rvn_expander_measure(rvn_widget_t* widget) {
	rvn_expander_t* expander = (rvn_expander_t*)widget;
	float header = rvn_expander_header(widget);
	rvn_vec2_t desired;
	desired.x = header + rvn_widget_text_width(widget, R_INVALID_ID, 0.0f, expander->title) +
		rvn_widget_metric(widget, R_METRIC_PADDING_X);
	desired.y = header;
	if (expander->expanded && widget->child_count > 0) {
		rvn_vec2_t child = rvn_widget_measure_slot(widget->children[0]);
		desired.x = rvn_maxf(desired.x, child.x);
		desired.y += child.y;
	}
	return desired;
}

static void rvn_expander_arrange(rvn_widget_t* widget) {
	rvn_expander_t* expander = (rvn_expander_t*)widget;
	float header = rvn_expander_header(widget);
	if (expander->expanded && widget->child_count > 0 && !rvn_widget_collapsed(widget->children[0])) {
		rvn_widget_arrange_slot(widget->children[0],
			rvn_rect(0.0f, header, widget->rect.w, rvn_maxf(0.0f, widget->rect.h - header)));
	}
}

static int rvn_expander_child_active(rvn_widget_t* widget, rvn_widget_t* child) {
	RVN_UNUSED(child);
	return ((rvn_expander_t*)widget)->expanded;
}

static void rvn_expander_set(rvn_widget_t* widget, int expanded, int notify) {
	rvn_expander_t* expander = (rvn_expander_t*)widget;
	expanded = expanded != 0;
	if (expander->expanded == expanded) return;
	if (!expanded && widget->child_count > 0 && widget->window) {
		rvn_window_forget_widget(widget->window, widget->children[0]);
	}
	expander->expanded = expanded;
	rvn_widget_invalidate_layout(widget);
	if (notify) {
		if (expander->callback) expander->callback(widget->id, expanded, expander->user_data);
		if (rvn_widget_alive(widget)) rvn_widget_fire_action(widget);
	}
}

static int rvn_expander_event(rvn_widget_t* widget, const REvent* event) {
	rvn_expander_t* expander = (rvn_expander_t*)widget;
	switch (event->type) {
		case R_EVENT_MOUSE_DOWN:
			if (event->button != R_MOUSE_LEFT || event->local_y >= rvn_expander_header(widget)) return R_FALSE;
			rvn_expander_set(widget, !expander->expanded, R_TRUE);
			return R_TRUE;
		case R_EVENT_KEY_DOWN:
			if (!(widget->flags & RVN_WF_FOCUSED)) return R_FALSE;
			if (event->key == R_KEY_ENTER || event->key == R_KEY_SPACE) {
				rvn_expander_set(widget, !expander->expanded, R_TRUE);
				return R_TRUE;
			}
			return R_FALSE;
		case R_EVENT_MOUSE_MOVE:
		case R_EVENT_MOUSE_ENTER:
		case R_EVENT_MOUSE_LEAVE:
			rvn_widget_invalidate_paint(widget);
			return R_FALSE;
		default:
			return R_FALSE;
	}
}

static void rvn_expander_paint(rvn_widget_t* widget, RPainter* painter) {
	rvn_expander_t* expander = (rvn_expander_t*)widget;
	float header = rvn_expander_header(widget);
	rvn_vec2_t origin = rvn_widget_origin(widget);
	int hot = (widget->flags & RVN_WF_HOVERED) && widget->window &&
		widget->window->mouse_y - origin.y < header && widget->window->mouse_y - origin.y >= 0.0f;
	rvn_color_t text = rvn_widget_text_color(widget, painter);

	rvn_paint_box(painter, rvn_rect(0.0f, 0.0f, widget->rect.w, header), g_rvn.theme.glass ? 10.0f : 0.0f,
		rvn_widget_color(widget, hot ? R_COLOR_HEADER_HOVERED : R_COLOR_HEADER), 0.0f, 0);
	rvn_paint_icon(painter, expander->expanded ? RVN_ICON_CHEVRON_DOWN : RVN_ICON_CHEVRON_RIGHT,
		header * 0.5f, header * 0.5f, 8.0f, text);
	if (expander->title) {
		rvn_paint_text_in(painter, R_INVALID_ID, rvn_widget_font_size(widget, 0.0f),
			rvn_rect(header, 0.0f, rvn_maxf(0.0f, widget->rect.w - header), header), 0.0f, expander->title, -1, text);
	}
}

static void rvn_expander_paint_overlay(rvn_widget_t* widget, RPainter* painter) {
	rvn_paint_focus(widget, painter, g_rvn.theme.glass ? 10.0f : 0.0f);
}

static int rvn_expander_set_attribute(rvn_widget_t* widget, const char* name, const char* value) {
	rvn_expander_t* expander = (rvn_expander_t*)widget;
	int flag;
	if (RVN_ATTR("Title") || RVN_ATTR("Text")) {
		rvn_str_set(&expander->title, value);
		rvn_widget_invalidate_layout(widget);
		return R_TRUE;
	}
	if (RVN_ATTR("Expanded")) {
		if (!rvn_parse_bool(value, &flag)) return R_FALSE;
		rvn_expander_set(widget, flag, R_FALSE);
		return R_TRUE;
	}
	return R_FALSE;
}

const rvn_widget_class_t rvn_class_expander = {
	.type = R_WIDGET_EXPANDER,
	.name = "Expander",
	.size = sizeof(rvn_expander_t),
	.flags = RVN_CF_SINGLE_CHILD | RVN_CF_FOCUSABLE,
	.init = rvn_expander_init,
	.destroy = rvn_expander_destroy,
	.measure = rvn_expander_measure,
	.arrange = rvn_expander_arrange,
	.paint = rvn_expander_paint,
	.paint_overlay = rvn_expander_paint_overlay,
	.event = rvn_expander_event,
	.set_attribute = rvn_expander_set_attribute,
	.child_active = rvn_expander_child_active
};

void Rexpander_set_title(Rid id, const char* title) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_EXPANDER);
	if (rvn_str_set(&((rvn_expander_t*)widget)->title, title)) rvn_widget_invalidate_layout(widget);
}

void Rexpander_set_expanded(Rid id, int expanded) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_EXPANDER);
	rvn_expander_set(widget, expanded, R_FALSE);
}

int Rexpander_is_expanded(Rid id) {
	RVN_WIDGET(widget, id, R_WIDGET_EXPANDER, R_FALSE);
	return ((rvn_expander_t*)widget)->expanded;
}

void Rexpander_set_changed_callback(Rid id, RExpanderChangedCallback callback, void* user_data) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_EXPANDER);
	((rvn_expander_t*)widget)->callback = callback;
	((rvn_expander_t*)widget)->user_data = user_data;
}
