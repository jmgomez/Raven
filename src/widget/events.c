/**
 * HUI - widget/events.c
 * Delivery of input events to widgets (bubbling) and actions.
 */

#include "widget.h"

static int hui_event_bubbles(HUI_EEventType type) {
	switch (type) {
		case HUI_EVENT_MOUSE_MOVE:
		case HUI_EVENT_MOUSE_DOWN:
		case HUI_EVENT_MOUSE_UP:
		case HUI_EVENT_MOUSE_WHEEL:
		case HUI_EVENT_KEY_DOWN:
		case HUI_EVENT_KEY_UP:
		case HUI_EVENT_TEXT:
			return HUI_TRUE;
		default:
			return HUI_FALSE;
	}
}

static int hui_widget_deliver(hui_widget_t* target, HUI_Event* event, int bubbles) {
	hui_widget_t* widget = target;
	int handled = HUI_FALSE;

	if (!target || (target->flags & HUI_WF_DEAD)) return HUI_FALSE;
	event->target = target->id;
	if (target->window) event->window = target->window->id;

	hui_dispatch_begin();
	while (widget && !(widget->flags & HUI_WF_DEAD)) {
		hui_vec2_t origin = hui_widget_origin(widget);
		int is_popup_root = (widget->flags & HUI_WF_POPUP) != 0;
		event->local_x = event->x - origin.x;
		event->local_y = event->y - origin.y;

		/* the application sees the event first and may consume it */
		if (widget->event_callback && widget->event_callback(widget->id, event, widget->event_user)) {
			handled = HUI_TRUE;
			break;
		}
		if (!(widget->flags & HUI_WF_DEAD) && widget->cls->event && widget->cls->event(widget, event)) {
			handled = HUI_TRUE;
			break;
		}
		/* events do not leave a popup, and a deleted widget has no parent any more */
		if (!bubbles || is_popup_root || (widget->flags & HUI_WF_DEAD)) break;
		widget = widget->parent;
	}
	hui_dispatch_end();
	return handled;
}

int hui_widget_dispatch(hui_widget_t* target, HUI_Event* event) {
	return hui_widget_deliver(target, event, hui_event_bubbles(event->type));
}

int hui_widget_send(hui_widget_t* target, HUI_Event* event) {
	return hui_widget_deliver(target, event, HUI_FALSE);
}

void hui_widget_fire_action(hui_widget_t* widget) {
	char* action;
	HUI_id sender;
	if (!widget->action) return;
	/* the callback may delete the widget or change its action: work on a copy */
	action = hui_strdup(widget->action);
	sender = widget->id;
	hui_dispatch_begin();
	hui_fire_action(sender, action);
	hui_dispatch_end();
	hui_free(action);
}

void hui_widget_capture_mouse(hui_widget_t* widget) {
	if (widget->window) widget->window->capture = widget;
}

void hui_widget_release_mouse(hui_widget_t* widget) {
	if (widget->window && widget->window->capture == widget) widget->window->capture = NULL;
}

void hui_widget_focus(hui_widget_t* widget) {
	if (!widget->window) return;
	if (!(widget->flags & HUI_WF_FOCUSABLE) || !hui_widget_is_enabled(widget) || !hui_widget_is_shown(widget)) return;
	hui_window_set_focus(widget->window, widget);
}
