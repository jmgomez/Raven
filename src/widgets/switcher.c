/**
 * HUI - widgets/switcher.c
 * Switcher (Slate SWidgetSwitcher): shows one of its children. The others
 * are neither laid out, painted nor reachable by the mouse or the keyboard.
 */

#include "common.h"

typedef struct hui_switcher_t {
	hui_widget_t base;
	int index;
} hui_switcher_t;

static hui_widget_t* hui_switcher_active(hui_widget_t* widget) {
	hui_switcher_t* switcher = (hui_switcher_t*)widget;
	if (switcher->index < 0 || switcher->index >= (int)widget->child_count) return NULL;
	return widget->children[switcher->index];
}

static hui_vec2_t hui_switcher_measure(hui_widget_t* widget) {
	hui_widget_t* active = hui_switcher_active(widget);
	return active ? hui_widget_measure_slot(active) : hui_vec2(0.0f, 0.0f);
}

static void hui_switcher_arrange(hui_widget_t* widget) {
	hui_widget_t* active = hui_switcher_active(widget);
	if (active && !hui_widget_collapsed(active)) {
		hui_widget_arrange_slot(active, hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h));
	}
}

static int hui_switcher_child_active(hui_widget_t* widget, hui_widget_t* child) {
	return hui_switcher_active(widget) == child;
}

static void hui_switcher_set(hui_widget_t* widget, int index) {
	hui_switcher_t* switcher = (hui_switcher_t*)widget;
	hui_widget_t* previous = hui_switcher_active(widget);
	if (switcher->index == index) return;
	/* the page being hidden may hold the focus, the hover or an open popup */
	if (previous && widget->window) hui_window_forget_widget(widget->window, previous);
	switcher->index = index;
	hui_widget_invalidate_layout(widget);
}

static int hui_switcher_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	int index;
	if (HUI_ATTR("Index")) {
		if (!hui_parse_int(value, &index)) return HUI_FALSE;
		hui_switcher_set(widget, index);
		return HUI_TRUE;
	}
	return HUI_FALSE;
}

const hui_widget_class_t hui_class_switcher = {
	.type = HUI_WIDGET_SWITCHER,
	.name = "Switcher",
	.size = sizeof(hui_switcher_t),
	.flags = HUI_CF_NO_HIT,
	.measure = hui_switcher_measure,
	.arrange = hui_switcher_arrange,
	.child_active = hui_switcher_child_active,
	.set_attribute = hui_switcher_set_attribute
};

void HUI_SetSwitcherIndex(HUI_id id, int index) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SWITCHER);
	hui_switcher_set(widget, index);
}

int HUI_GetSwitcherIndex(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_SWITCHER, -1);
	return ((hui_switcher_t*)widget)->index;
}
