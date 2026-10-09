/**
 * HUI - widgets/combo_box.c
 * ComboBox (Slate SComboBox): a button showing the selected item, and a
 * popup list to pick another one.
 *
 * The list is a private widget class shown through the popup layer of the
 * window; it is owned by the combo box and is not one of its children.
 */

#include "common.h"
#include "../xml/xml.h"

#define HUI_COMBO_MAX_ROWS 10

typedef struct hui_combo_box_t {
	hui_widget_t base;
	char** items;
	uint32_t count;
	uint32_t cap;
	int selected;
	hui_widget_t* list;
	HUI_CComboBoxChanged callback;
	void* user_data;
} hui_combo_box_t;

typedef struct hui_combo_list_t {
	hui_widget_t base;
	hui_combo_box_t* combo;
	int hot;
	hui_scrollbar_t bar;
} hui_combo_list_t;

static const hui_widget_class_t hui_class_combo_list;

static void hui_combo_box_select(hui_widget_t* widget, int index, int notify) {
	hui_combo_box_t* combo = (hui_combo_box_t*)widget;
	if (index < -1 || index >= (int)combo->count) index = -1;
	if (combo->selected == index) return;
	combo->selected = index;
	hui_widget_invalidate_paint(widget);
	if (notify) {
		if (combo->callback) combo->callback(widget->id, index, combo->user_data);
		if (hui_widget_alive(widget)) hui_widget_fire_action(widget);
	}
}

/* ===========================================================================
 *  Popup list
 * ========================================================================= */

static float hui_combo_row_height(const hui_widget_t* widget) {
	return hui_widget_metric(widget, HUI_METRIC_ROW_HEIGHT);
}

static hui_vec2_t hui_combo_list_measure(hui_widget_t* widget) {
	hui_combo_list_t* list = (hui_combo_list_t*)widget;
	hui_combo_box_t* combo = list->combo;
	float width = 0.0f, row = hui_combo_row_height(widget);
	uint32_t i, rows = hui_minu(combo->count, HUI_COMBO_MAX_ROWS);
	for (i = 0; i < combo->count; i++) {
		width = hui_maxf(width, hui_widget_text_width(widget, HUI_INVALID_ID, 0.0f, combo->items[i]));
	}
	width += hui_widget_metric(widget, HUI_METRIC_PADDING_X) * 2.0f;
	if (combo->count > HUI_COMBO_MAX_ROWS) width += hui_widget_metric(widget, HUI_METRIC_SCROLLBAR_SIZE);
	return hui_vec2(ceilf(width), row * (float)hui_maxu(rows, 1) + 2.0f);
}

static void hui_combo_list_arrange(hui_widget_t* widget) {
	hui_combo_list_t* list = (hui_combo_list_t*)widget;
	list->bar.content = hui_combo_row_height(widget) * (float)list->combo->count;
	list->bar.viewport = hui_maxf(0.0f, widget->rect.h - 2.0f);
	hui_scrollbar_clamp(&list->bar);
}

static hui_rect_t hui_combo_list_track(const hui_widget_t* widget) {
	float bar = hui_widget_metric(widget, HUI_METRIC_SCROLLBAR_SIZE);
	return hui_rect(widget->rect.w - bar - 1.0f, 1.0f, bar, widget->rect.h - 2.0f);
}

static int hui_combo_list_row_at(const hui_widget_t* widget, float y) {
	const hui_combo_list_t* list = (const hui_combo_list_t*)widget;
	int row = (int)floorf((y - 1.0f + list->bar.offset) / hui_combo_row_height(widget));
	return (row >= 0 && row < (int)list->combo->count) ? row : -1;
}

static void hui_combo_list_reveal(hui_widget_t* widget, int row) {
	hui_combo_list_t* list = (hui_combo_list_t*)widget;
	float height = hui_combo_row_height(widget), top = height * (float)row;
	if (row < 0) return;
	if (top < list->bar.offset) hui_scrollbar_set(&list->bar, top);
	else if (top + height > list->bar.offset + list->bar.viewport) hui_scrollbar_set(&list->bar, top + height - list->bar.viewport);
}

static void hui_combo_list_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_combo_list_t* list = (hui_combo_list_t*)widget;
	hui_combo_box_t* combo = list->combo;
	hui_rect_t rect = hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h);
	float row = hui_combo_row_height(widget), padding = hui_widget_metric(widget, HUI_METRIC_PADDING_X);
	float radius = hui_widget_metric(widget, HUI_METRIC_CORNER_RADIUS);
	hui_color_t text = hui_widget_color(widget, HUI_COLOR_TEXT);
	int first = (int)floorf(list->bar.offset / row), last, i;
	hui_vec2_t origin = hui_widget_origin(widget);

	hui_paint_shadow(painter, hui_rect(0.0f, 3.0f, rect.w, rect.h), radius, 12.0f, hui_widget_color(widget, HUI_COLOR_SHADOW));
	hui_paint_box(painter, rect, radius, hui_widget_color(widget, HUI_COLOR_POPUP), 1.0f, hui_widget_color(widget, HUI_COLOR_POPUP_BORDER));

	hui_paint_push_clip(painter, hui_rect(1.0f, 1.0f, rect.w - 2.0f, rect.h - 2.0f));
	last = hui_mini((int)combo->count, first + (int)(list->bar.viewport / row) + 2);
	for (i = hui_maxi(first, 0); i < last; i++) {
		hui_rect_t line = hui_rect(1.0f, 1.0f + row * (float)i - list->bar.offset, rect.w - 2.0f, row);
		if (i == list->hot) hui_paint_rect(painter, line, hui_widget_color(widget, HUI_COLOR_SELECTION));
		hui_paint_text_in(painter, HUI_INVALID_ID, hui_widget_font_size(widget, 0.0f),
			hui_rect(padding, line.y, rect.w - padding * 2.0f, row), 0.0f, combo->items[i], -1,
			i == combo->selected ? hui_widget_color(widget, HUI_COLOR_ACCENT_HOVERED) : text);
	}
	hui_paint_pop_clip(painter);
	hui_scrollbar_paint(&list->bar, widget, painter, hui_combo_list_track(widget), HUI_TRUE,
		widget->window ? widget->window->mouse_x - origin.x : -1.0f, widget->window ? widget->window->mouse_y - origin.y : -1.0f);
}

static void hui_combo_list_pick(hui_widget_t* widget, int row) {
	hui_combo_list_t* list = (hui_combo_list_t*)widget;
	hui_widget_t* combo = &list->combo->base;
	hui_window_t* window = widget->window;
	/* close first: the callback may open something else, or delete the combo box */
	if (window) hui_window_close_popup(window, widget);
	if (row >= 0) hui_combo_box_select(combo, row, HUI_TRUE);
}

static int hui_combo_list_event(hui_widget_t* widget, const HUI_Event* event) {
	hui_combo_list_t* list = (hui_combo_list_t*)widget;
	int changed = HUI_FALSE, row;

	if (event->type == HUI_EVENT_MOUSE_DOWN || event->type == HUI_EVENT_MOUSE_MOVE || event->type == HUI_EVENT_MOUSE_UP) {
		if (hui_scrollbar_event(&list->bar, widget, event, hui_combo_list_track(widget), HUI_TRUE, &changed)) {
			hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		}
	}
	switch (event->type) {
		case HUI_EVENT_MOUSE_MOVE:
			row = hui_combo_list_row_at(widget, event->local_y);
			if (row != list->hot && row >= 0) {
				list->hot = row;
				hui_widget_invalidate_paint(widget);
			}
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_DOWN:
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_UP:
			if (event->button != HUI_MOUSE_LEFT) return HUI_TRUE;
			row = hui_combo_list_row_at(widget, event->local_y);
			if (row >= 0 && event->local_x >= 0.0f && event->local_x < widget->rect.w) hui_combo_list_pick(widget, row);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_WHEEL:
			if (hui_scrollbar_set(&list->bar, list->bar.offset - event->wheel_y * hui_combo_row_height(widget) * 2.0f)) {
				hui_widget_invalidate_paint(widget);
			}
			return HUI_TRUE;
		case HUI_EVENT_KEY_DOWN:
			switch (event->key) {
				case HUI_KEY_UP:
					list->hot = hui_maxi(list->hot - 1, 0);
					break;
				case HUI_KEY_DOWN:
					list->hot = hui_mini(list->hot + 1, (int)list->combo->count - 1);
					break;
				case HUI_KEY_HOME:
					list->hot = 0;
					break;
				case HUI_KEY_END:
					list->hot = (int)list->combo->count - 1;
					break;
				case HUI_KEY_ENTER:
				case HUI_KEY_KP_ENTER:
				case HUI_KEY_SPACE:
					hui_combo_list_pick(widget, list->hot);
					return HUI_TRUE;
				default:
					/* Escape is left to the window, which closes the popup */
					return HUI_FALSE;
			}
			hui_combo_list_reveal(widget, list->hot);
			hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		default:
			return HUI_FALSE;
	}
}

static void hui_combo_list_closed(hui_widget_t* widget) {
	hui_combo_list_t* list = (hui_combo_list_t*)widget;
	list->bar.dragging = HUI_FALSE;
	hui_widget_invalidate_paint(&list->combo->base);
}

static const hui_widget_class_t hui_class_combo_list = {
	.type = HUI_WIDGET_INTERNAL_COMBO_LIST,
	.name = "ComboBoxList",
	.size = sizeof(hui_combo_list_t),
	.flags = HUI_CF_NO_CHILDREN,
	.measure = hui_combo_list_measure,
	.arrange = hui_combo_list_arrange,
	.paint = hui_combo_list_paint,
	.event = hui_combo_list_event,
	.popup_closed = hui_combo_list_closed
};

/* ===========================================================================
 *  ComboBox
 * ========================================================================= */

static void hui_combo_box_init(hui_widget_t* widget) {
	hui_combo_box_t* combo = (hui_combo_box_t*)widget;
	combo->selected = -1;
	combo->list = hui_widget_create_class(&hui_class_combo_list);
	if (combo->list) {
		((hui_combo_list_t*)combo->list)->combo = combo;
		combo->list->flags |= HUI_WF_INTERNAL;
	}
}

static void hui_combo_box_destroy(hui_widget_t* widget) {
	hui_combo_box_t* combo = (hui_combo_box_t*)widget;
	uint32_t i;
	if (combo->list) hui_widget_destroy(combo->list);
	for (i = 0; i < combo->count; i++) hui_free(combo->items[i]);
	hui_free(combo->items);
}

static int hui_combo_box_is_open(const hui_widget_t* widget) {
	const hui_combo_box_t* combo = (const hui_combo_box_t*)widget;
	return combo->list && (combo->list->flags & HUI_WF_POPUP);
}

static void hui_combo_box_open(hui_widget_t* widget) {
	hui_combo_box_t* combo = (hui_combo_box_t*)widget;
	hui_combo_list_t* list = (hui_combo_list_t*)combo->list;
	hui_vec2_t origin;
	if (!widget->window || !list || combo->count == 0 || hui_combo_box_is_open(widget)) return;
	origin = hui_widget_origin(widget);
	list->hot = combo->selected;
	hui_window_open_popup(widget->window, &list->base, widget,
		hui_rect(origin.x, origin.y, widget->rect.w, widget->rect.h + 2.0f), HUI_POPUP_BELOW, widget->rect.w);
	/* lay the list out now so that the selected row can be scrolled into view */
	hui_window_layout(widget->window);
	hui_combo_list_reveal(&list->base, combo->selected);
}

static hui_vec2_t hui_combo_box_measure(hui_widget_t* widget) {
	hui_combo_box_t* combo = (hui_combo_box_t*)widget;
	float height = hui_widget_metric(widget, HUI_METRIC_CONTROL_HEIGHT);
	float width = 0.0f;
	uint32_t i;
	for (i = 0; i < combo->count; i++) {
		width = hui_maxf(width, hui_widget_text_width(widget, HUI_INVALID_ID, 0.0f, combo->items[i]));
	}
	/* text + padding + room for the arrow */
	return hui_vec2(ceilf(hui_maxf(width, 40.0f) + hui_widget_metric(widget, HUI_METRIC_PADDING_X) * 2.0f + height * 0.7f), height);
}

static void hui_combo_box_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_combo_box_t* combo = (hui_combo_box_t*)widget;
	hui_rect_t rect = hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h);
	float padding = hui_widget_metric(widget, HUI_METRIC_PADDING_X);
	float arrow = widget->rect.h * 0.7f;
	int open = hui_combo_box_is_open(widget);
	int hovered = (widget->flags & HUI_WF_HOVERED) && painter->enabled;
	hui_color_t fill = hui_widget_color(widget, open ? HUI_COLOR_CONTROL_PRESSED : (hovered ? HUI_COLOR_CONTROL_HOVERED : HUI_COLOR_CONTROL));
	hui_color_t outline = hui_widget_color(widget, HUI_COLOR_CONTROL_BORDER);
	hui_color_t text = hui_widget_text_color(widget, painter);

	if (!painter->enabled) {
		fill = hui_color_fade(fill, 0.5f);
		outline = hui_color_fade(outline, 0.5f);
	}
	hui_paint_box(painter, rect, hui_widget_metric(widget, HUI_METRIC_CORNER_RADIUS), fill,
		hui_widget_metric(widget, HUI_METRIC_BORDER_WIDTH), outline);
	if (combo->selected >= 0 && combo->selected < (int)combo->count) {
		hui_rect_t area = hui_rect(padding, 0.0f, hui_maxf(0.0f, rect.w - padding - arrow), rect.h);
		hui_paint_push_clip(painter, area);
		hui_paint_text_in(painter, HUI_INVALID_ID, hui_widget_font_size(widget, 0.0f), area, 0.0f,
			combo->items[combo->selected], -1, text);
		hui_paint_pop_clip(painter);
	}
	hui_paint_icon(painter, HUI_ICON_CHEVRON_DOWN, rect.w - arrow * 0.5f - 2.0f, rect.h * 0.5f, 7.0f, text);
}

static void hui_combo_box_paint_overlay(hui_widget_t* widget, HUI_Painter* painter) {
	hui_paint_focus(widget, painter, hui_widget_metric(widget, HUI_METRIC_CORNER_RADIUS));
}

static int hui_combo_box_event(hui_widget_t* widget, const HUI_Event* event) {
	hui_combo_box_t* combo = (hui_combo_box_t*)widget;
	switch (event->type) {
		case HUI_EVENT_MOUSE_DOWN:
			if (event->button != HUI_MOUSE_LEFT) return HUI_FALSE;
			/* when the list is open, the click that closes it never reaches us */
			hui_combo_box_open(widget);
			return HUI_TRUE;
		case HUI_EVENT_KEY_DOWN:
			if (!(widget->flags & HUI_WF_FOCUSED)) return HUI_FALSE;
			switch (event->key) {
				case HUI_KEY_UP:
					if (combo->selected > 0) hui_combo_box_select(widget, combo->selected - 1, HUI_TRUE);
					return HUI_TRUE;
				case HUI_KEY_DOWN:
					if (combo->selected + 1 < (int)combo->count) hui_combo_box_select(widget, combo->selected + 1, HUI_TRUE);
					return HUI_TRUE;
				case HUI_KEY_ENTER:
				case HUI_KEY_KP_ENTER:
				case HUI_KEY_SPACE:
					hui_combo_box_open(widget);
					return HUI_TRUE;
				default:
					return HUI_FALSE;
			}
		default:
			return HUI_FALSE;
	}
}

static int hui_combo_box_add(hui_widget_t* widget, const char* text) {
	hui_combo_box_t* combo = (hui_combo_box_t*)widget;
	HUI_ARRAY_RESERVE(combo->items, combo->cap, combo->count + 1);
	combo->items[combo->count++] = hui_strdup(text ? text : "");
	hui_widget_invalidate_layout(widget);
	if (combo->list) hui_widget_invalidate_layout(combo->list);
	return (int)combo->count - 1;
}

static void hui_combo_box_clear(hui_widget_t* widget) {
	hui_combo_box_t* combo = (hui_combo_box_t*)widget;
	uint32_t i;
	if (widget->window && combo->list) hui_window_close_popup(widget->window, combo->list);
	for (i = 0; i < combo->count; i++) hui_free(combo->items[i]);
	combo->count = 0;
	combo->selected = -1;
	hui_widget_invalidate_layout(widget);
}

static int hui_combo_box_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	int index;
	if (HUI_ATTR("Items")) {
		/* "First|Second|Third" */
		hui_combo_box_clear(widget);
		while (*value) {
			size_t length = strcspn(value, "|");
			char* item = hui_strndup(value, length);
			hui_combo_box_add(widget, item);
			hui_free(item);
			value += length;
			if (*value == '|') value++;
		}
		return HUI_TRUE;
	}
	if (HUI_ATTR("Selected")) {
		if (!hui_parse_int(value, &index)) return HUI_FALSE;
		hui_combo_box_select(widget, index, HUI_FALSE);
		return HUI_TRUE;
	}
	return HUI_FALSE;
}

static int hui_combo_box_xml_child(hui_widget_t* widget, const hui_xml_node_t* node) {
	const char* text;
	if (hui_stricmp(node->name, "Item") != 0) return HUI_FALSE;
	text = hui_xml_attribute(node, "Text");
	hui_combo_box_add(widget, text ? text : node->text);
	return HUI_TRUE;
}

const hui_widget_class_t hui_class_combo_box = {
	.type = HUI_WIDGET_COMBO_BOX,
	.name = "ComboBox",
	.size = sizeof(hui_combo_box_t),
	.flags = HUI_CF_NO_CHILDREN | HUI_CF_FOCUSABLE,
	.init = hui_combo_box_init,
	.destroy = hui_combo_box_destroy,
	.measure = hui_combo_box_measure,
	.paint = hui_combo_box_paint,
	.paint_overlay = hui_combo_box_paint_overlay,
	.event = hui_combo_box_event,
	.set_attribute = hui_combo_box_set_attribute,
	.xml_child = hui_combo_box_xml_child
};

int HUI_AddComboBoxItem(HUI_id id, const char* text) {
	HUI_WIDGET(widget, id, HUI_WIDGET_COMBO_BOX, -1);
	return hui_combo_box_add(widget, text);
}

void HUI_ClearComboBoxItems(HUI_id id) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_COMBO_BOX);
	hui_combo_box_clear(widget);
}

int HUI_GetComboBoxItemCount(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_COMBO_BOX, 0);
	return (int)((hui_combo_box_t*)widget)->count;
}

const char* HUI_GetComboBoxItemText(HUI_id id, int index) {
	HUI_WIDGET(widget, id, HUI_WIDGET_COMBO_BOX, "");
	hui_combo_box_t* combo = (hui_combo_box_t*)widget;
	return (index >= 0 && index < (int)combo->count) ? combo->items[index] : "";
}

void HUI_SetComboBoxSelected(HUI_id id, int index) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_COMBO_BOX);
	hui_combo_box_select(widget, index, HUI_FALSE);
}

int HUI_GetComboBoxSelected(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_COMBO_BOX, -1);
	return ((hui_combo_box_t*)widget)->selected;
}

void HUI_SetComboBoxChangedCallback(HUI_id id, HUI_CComboBoxChanged callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_COMBO_BOX);
	((hui_combo_box_t*)widget)->callback = callback;
	((hui_combo_box_t*)widget)->user_data = user_data;
}
