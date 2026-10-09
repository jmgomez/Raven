/**
 * HUI - widgets/list_view.c
 * ListView (Slate SListView): a scrollable list of text rows with one
 * selection. Rows are virtualized: only the visible ones are measured and
 * drawn, so a list of a million rows costs the same as a list of ten.
 *
 * The rows are either stored by the list (HUI_AddListItem) or pulled from the
 * application when they are drawn (HUI_SetListVirtualItems).
 */

#include "common.h"
#include "../xml/xml.h"

typedef struct hui_list_view_t {
	hui_widget_t base;
	char** items;
	uint32_t count;
	uint32_t cap;
	/* virtual mode */
	int is_virtual;
	int virtual_count;
	HUI_CListItemText text_callback;
	void* text_user;

	int selected;
	int hot;
	hui_scrollbar_t bar;
	HUI_CListEvent selection_callback;
	void* selection_user;
	HUI_CListEvent activated_callback;
	void* activated_user;
} hui_list_view_t;

static void hui_list_view_init(hui_widget_t* widget) {
	hui_list_view_t* list = (hui_list_view_t*)widget;
	list->selected = -1;
	list->hot = -1;
}

static void hui_list_view_destroy(hui_widget_t* widget) {
	hui_list_view_t* list = (hui_list_view_t*)widget;
	uint32_t i;
	for (i = 0; i < list->count; i++) hui_free(list->items[i]);
	hui_free(list->items);
}

static int hui_list_count(const hui_list_view_t* list) {
	return list->is_virtual ? list->virtual_count : (int)list->count;
}

static const char* hui_list_text(hui_list_view_t* list, int index) {
	const char* text;
	if (index < 0 || index >= hui_list_count(list)) return "";
	if (!list->is_virtual) return list->items[index];
	text = list->text_callback ? list->text_callback(list->base.id, index, list->text_user) : NULL;
	return text ? text : "";
}

static float hui_list_row_height(const hui_widget_t* widget) {
	return hui_widget_metric(widget, HUI_METRIC_ROW_HEIGHT);
}

static hui_vec2_t hui_list_view_measure(hui_widget_t* widget) {
	hui_list_view_t* list = (hui_list_view_t*)widget;
	/* a list is usually given its size by a fill slot; this is only a sensible default */
	return hui_vec2(160.0f, hui_list_row_height(widget) * (float)hui_clampi(hui_list_count(list), 3, 8) + 2.0f);
}

static void hui_list_view_arrange(hui_widget_t* widget) {
	hui_list_view_t* list = (hui_list_view_t*)widget;
	list->bar.content = hui_list_row_height(widget) * (float)hui_list_count(list);
	list->bar.viewport = hui_maxf(0.0f, widget->rect.h - 2.0f);
	hui_scrollbar_clamp(&list->bar);
}

static hui_rect_t hui_list_track(const hui_widget_t* widget) {
	float bar = hui_widget_metric(widget, HUI_METRIC_SCROLLBAR_SIZE);
	return hui_rect(widget->rect.w - bar - 1.0f, 1.0f, bar, hui_maxf(0.0f, widget->rect.h - 2.0f));
}

static int hui_list_row_at(const hui_widget_t* widget, float y) {
	const hui_list_view_t* list = (const hui_list_view_t*)widget;
	int row;
	if (y < 1.0f || y >= widget->rect.h - 1.0f) return -1;
	row = (int)floorf((y - 1.0f + list->bar.offset) / hui_list_row_height(widget));
	return (row >= 0 && row < hui_list_count(list)) ? row : -1;
}

static void hui_list_reveal(hui_widget_t* widget, int row) {
	hui_list_view_t* list = (hui_list_view_t*)widget;
	float height = hui_list_row_height(widget), top = height * (float)row;
	if (row < 0) return;
	/* the content length may have changed since the last arrange */
	hui_list_view_arrange(widget);
	if (top < list->bar.offset) hui_scrollbar_set(&list->bar, top);
	else if (top + height > list->bar.offset + list->bar.viewport) hui_scrollbar_set(&list->bar, top + height - list->bar.viewport);
	hui_widget_invalidate_paint(widget);
}

static void hui_list_select(hui_widget_t* widget, int index, int notify) {
	hui_list_view_t* list = (hui_list_view_t*)widget;
	if (index < -1 || index >= hui_list_count(list)) index = -1;
	if (list->selected == index) return;
	list->selected = index;
	hui_list_reveal(widget, index);
	hui_widget_invalidate_paint(widget);
	if (notify && list->selection_callback) list->selection_callback(widget->id, index, list->selection_user);
}

static void hui_list_activate(hui_widget_t* widget) {
	hui_list_view_t* list = (hui_list_view_t*)widget;
	if (list->selected < 0) return;
	if (list->activated_callback) list->activated_callback(widget->id, list->selected, list->activated_user);
	if (hui_widget_alive(widget)) hui_widget_fire_action(widget);
}

static void hui_list_view_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_list_view_t* list = (hui_list_view_t*)widget;
	hui_rect_t rect = hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h);
	float row = hui_list_row_height(widget), padding = hui_widget_metric(widget, HUI_METRIC_PADDING_X) * 0.8f;
	int focused = (widget->flags & HUI_WF_FOCUSED) != 0;
	int count = hui_list_count(list);
	int first = (int)floorf(list->bar.offset / row), last, i;
	float content_width = rect.w - 2.0f - (hui_scrollbar_needed(&list->bar) ? hui_widget_metric(widget, HUI_METRIC_SCROLLBAR_SIZE) : 0.0f);
	hui_color_t text = hui_widget_text_color(widget, painter);
	hui_color_t selection = hui_widget_color(widget, HUI_COLOR_SELECTION);
	hui_vec2_t origin = hui_widget_origin(widget);

	hui_paint_box(painter, rect, hui_widget_metric(widget, HUI_METRIC_CORNER_RADIUS), hui_widget_color(widget, HUI_COLOR_INPUT),
		hui_widget_metric(widget, HUI_METRIC_BORDER_WIDTH), hui_widget_color(widget, focused ? HUI_COLOR_FOCUS : HUI_COLOR_BORDER));

	hui_paint_push_clip(painter, hui_rect(1.0f, 1.0f, rect.w - 2.0f, rect.h - 2.0f));
	last = hui_mini(count, first + (int)(list->bar.viewport / row) + 2);
	for (i = hui_maxi(first, 0); i < last; i++) {
		hui_rect_t line = hui_rect(1.0f, 1.0f + row * (float)i - list->bar.offset, content_width, row);
		if (i == list->selected) {
			/* a list without the keyboard focus shows a dimmer selection */
			hui_paint_rect(painter, line, focused ? selection : hui_color_mix(selection, hui_widget_color(widget, HUI_COLOR_INPUT), 0.45f));
		} else if (i == list->hot && (widget->flags & HUI_WF_HOVERED) && painter->enabled) {
			hui_paint_rect(painter, line, hui_widget_color(widget, HUI_COLOR_ROW_HOVERED));
		}
		hui_paint_text_in(painter, HUI_INVALID_ID, hui_widget_font_size(widget, 0.0f),
			hui_rect(line.x + padding, line.y, hui_maxf(0.0f, line.w - padding * 2.0f), row), 0.0f, hui_list_text(list, i), -1, text);
	}
	hui_paint_pop_clip(painter);
	hui_scrollbar_paint(&list->bar, widget, painter, hui_list_track(widget), HUI_TRUE,
		widget->window ? widget->window->mouse_x - origin.x : -1.0f, widget->window ? widget->window->mouse_y - origin.y : -1.0f);
}

static int hui_list_view_event(hui_widget_t* widget, const HUI_Event* event) {
	hui_list_view_t* list = (hui_list_view_t*)widget;
	int count = hui_list_count(list), changed = HUI_FALSE, row, page;

	if (event->type == HUI_EVENT_MOUSE_DOWN || event->type == HUI_EVENT_MOUSE_MOVE || event->type == HUI_EVENT_MOUSE_UP) {
		if (hui_scrollbar_event(&list->bar, widget, event, hui_list_track(widget), HUI_TRUE, &changed)) {
			hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		}
	}
	switch (event->type) {
		case HUI_EVENT_MOUSE_MOVE:
			row = hui_list_row_at(widget, event->local_y);
			if (row != list->hot) {
				list->hot = row;
				hui_widget_invalidate_paint(widget);
			}
			return HUI_FALSE;
		case HUI_EVENT_MOUSE_LEAVE:
			list->hot = -1;
			hui_widget_invalidate_paint(widget);
			return HUI_FALSE;
		case HUI_EVENT_MOUSE_DOWN:
			if (event->button != HUI_MOUSE_LEFT) return HUI_FALSE;
			row = hui_list_row_at(widget, event->local_y);
			if (row >= 0) {
				hui_list_select(widget, row, HUI_TRUE);
				if (event->clicks == 2 && hui_widget_alive(widget)) hui_list_activate(widget);
			}
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_WHEEL:
			if (!hui_scrollbar_set(&list->bar, list->bar.offset - event->wheel_y * hui_list_row_height(widget) * 3.0f)) return HUI_FALSE;
			list->hot = hui_list_row_at(widget, event->local_y);
			hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		case HUI_EVENT_FOCUS_GAINED:
		case HUI_EVENT_FOCUS_LOST:
			hui_widget_invalidate_paint(widget);
			return HUI_FALSE;
		case HUI_EVENT_KEY_DOWN:
			if (count == 0) return HUI_FALSE;
			page = hui_maxi(1, (int)(list->bar.viewport / hui_list_row_height(widget)) - 1);
			switch (event->key) {
				case HUI_KEY_UP:
					row = list->selected < 0 ? count - 1 : list->selected - 1;
					break;
				case HUI_KEY_DOWN:
					row = list->selected + 1;
					break;
				case HUI_KEY_PAGE_UP:
					row = list->selected - page;
					break;
				case HUI_KEY_PAGE_DOWN:
					row = list->selected + page;
					break;
				case HUI_KEY_HOME:
					row = 0;
					break;
				case HUI_KEY_END:
					row = count - 1;
					break;
				case HUI_KEY_ENTER:
				case HUI_KEY_KP_ENTER:
					hui_list_activate(widget);
					return HUI_TRUE;
				default:
					return HUI_FALSE;
			}
			hui_list_select(widget, hui_clampi(row, 0, count - 1), HUI_TRUE);
			return HUI_TRUE;
		default:
			return HUI_FALSE;
	}
}

static int hui_list_add(hui_widget_t* widget, const char* text) {
	hui_list_view_t* list = (hui_list_view_t*)widget;
	HUI_ARRAY_RESERVE(list->items, list->cap, list->count + 1);
	list->items[list->count++] = hui_strdup(text ? text : "");
	hui_widget_invalidate_layout(widget);
	return (int)list->count - 1;
}

static void hui_list_clear(hui_widget_t* widget) {
	hui_list_view_t* list = (hui_list_view_t*)widget;
	uint32_t i;
	for (i = 0; i < list->count; i++) hui_free(list->items[i]);
	list->count = 0;
	list->selected = -1;
	list->hot = -1;
	list->bar.offset = 0.0f;
	hui_widget_invalidate_layout(widget);
}

static int hui_list_view_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	int index;
	if (HUI_ATTR("Items")) {
		hui_list_clear(widget);
		while (*value) {
			size_t length = strcspn(value, "|");
			char* item = hui_strndup(value, length);
			hui_list_add(widget, item);
			hui_free(item);
			value += length;
			if (*value == '|') value++;
		}
		return HUI_TRUE;
	}
	if (HUI_ATTR("Selected")) {
		if (!hui_parse_int(value, &index)) return HUI_FALSE;
		hui_list_select(widget, index, HUI_FALSE);
		return HUI_TRUE;
	}
	return HUI_FALSE;
}

static int hui_list_view_xml_child(hui_widget_t* widget, const hui_xml_node_t* node) {
	const char* text;
	if (hui_stricmp(node->name, "Item") != 0) return HUI_FALSE;
	text = hui_xml_attribute(node, "Text");
	hui_list_add(widget, text ? text : node->text);
	return HUI_TRUE;
}

const hui_widget_class_t hui_class_list_view = {
	.type = HUI_WIDGET_LIST_VIEW,
	.name = "ListView",
	.size = sizeof(hui_list_view_t),
	/* the text of a virtual row comes from an application callback */
	.flags = HUI_CF_NO_CHILDREN | HUI_CF_FOCUSABLE | HUI_CF_UI_PAINT,
	.init = hui_list_view_init,
	.destroy = hui_list_view_destroy,
	.measure = hui_list_view_measure,
	.arrange = hui_list_view_arrange,
	.paint = hui_list_view_paint,
	.event = hui_list_view_event,
	.set_attribute = hui_list_view_set_attribute,
	.xml_child = hui_list_view_xml_child
};

/* ---------------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------------- */

int HUI_AddListItem(HUI_id id, const char* text) {
	HUI_WIDGET(widget, id, HUI_WIDGET_LIST_VIEW, -1);
	if (((hui_list_view_t*)widget)->is_virtual) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_AddListItem: the list is in virtual mode");
		return -1;
	}
	return hui_list_add(widget, text);
}

void HUI_SetListItemText(HUI_id id, int index, const char* text) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_LIST_VIEW);
	hui_list_view_t* list = (hui_list_view_t*)widget;
	if (list->is_virtual || index < 0 || index >= (int)list->count) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_SetListItemText: invalid index %d", index);
		return;
	}
	if (hui_str_set(&list->items[index], text ? text : "")) hui_widget_invalidate_paint(widget);
}

const char* HUI_GetListItemText(HUI_id id, int index) {
	HUI_WIDGET(widget, id, HUI_WIDGET_LIST_VIEW, "");
	return hui_list_text((hui_list_view_t*)widget, index);
}

void HUI_RemoveListItem(HUI_id id, int index) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_LIST_VIEW);
	hui_list_view_t* list = (hui_list_view_t*)widget;
	if (list->is_virtual || index < 0 || index >= (int)list->count) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_RemoveListItem: invalid index %d", index);
		return;
	}
	hui_free(list->items[index]);
	memmove(&list->items[index], &list->items[index + 1], sizeof(char*) * (list->count - (uint32_t)index - 1));
	list->count--;
	/* the selection follows the item it was on */
	if (list->selected == index) list->selected = -1;
	else if (list->selected > index) list->selected--;
	list->hot = -1;
	hui_widget_invalidate_layout(widget);
}

void HUI_ClearListItems(HUI_id id) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_LIST_VIEW);
	hui_list_clear(widget);
	((hui_list_view_t*)widget)->virtual_count = 0;
}

int HUI_GetListItemCount(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_LIST_VIEW, 0);
	return hui_list_count((hui_list_view_t*)widget);
}

void HUI_SetListVirtualItems(HUI_id id, int count, HUI_CListItemText callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_LIST_VIEW);
	hui_list_view_t* list = (hui_list_view_t*)widget;
	int selected = list->selected;
	float offset = list->bar.offset;
	hui_list_clear(widget);
	list->is_virtual = callback != NULL;
	list->virtual_count = callback && count > 0 ? count : 0;
	list->text_callback = callback;
	list->text_user = user_data;
	/* refreshing the count of a live list keeps its selection and scroll position */
	if (selected < list->virtual_count) list->selected = selected;
	list->bar.offset = offset;
	hui_widget_invalidate_layout(widget);
}

void HUI_SetListSelected(HUI_id id, int index) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_LIST_VIEW);
	hui_list_select(widget, index, HUI_FALSE);
}

int HUI_GetListSelected(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_LIST_VIEW, -1);
	return ((hui_list_view_t*)widget)->selected;
}

void HUI_ScrollToListItem(HUI_id id, int index) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_LIST_VIEW);
	if (index < 0 || index >= hui_list_count((hui_list_view_t*)widget)) return;
	if (widget->window && widget->window->needs_layout) hui_window_layout(widget->window);
	hui_list_reveal(widget, index);
}

void HUI_SetListSelectionChangedCallback(HUI_id id, HUI_CListEvent callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_LIST_VIEW);
	((hui_list_view_t*)widget)->selection_callback = callback;
	((hui_list_view_t*)widget)->selection_user = user_data;
}

void HUI_SetListActivatedCallback(HUI_id id, HUI_CListEvent callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_LIST_VIEW);
	((hui_list_view_t*)widget)->activated_callback = callback;
	((hui_list_view_t*)widget)->activated_user = user_data;
}
