/**
 * HUI - widgets/tree_view.c
 * TreeView (Slate STreeView): hierarchy of text items that expand and
 * collapse, with one selection. Like the list, it is virtualized: the
 * expanded items are flattened into rows and only the visible rows are drawn.
 *
 * Items are objects with their own HUI_id handles (HUI_AddTreeItem), so the
 * application can keep references to them (scene nodes, assets, ...).
 */

#include "common.h"
#include "../xml/xml.h"

typedef struct hui_tree_view_t hui_tree_view_t;

typedef struct hui_tree_item_t {
	HUI_id id;
	hui_tree_view_t* tree;
	struct hui_tree_item_t* parent;
	struct hui_tree_item_t** children;
	uint32_t child_count;
	uint32_t child_cap;
	char* text;
	int expanded;
	/** Nesting level, filled when the rows are rebuilt. */
	int depth;
	void* user_handle;
} hui_tree_item_t;

struct hui_tree_view_t {
	hui_widget_t base;
	/** Invisible item holding the top-level items. */
	hui_tree_item_t root;
	/** Items currently shown, top to bottom. */
	hui_tree_item_t** rows;
	uint32_t row_count;
	uint32_t row_cap;
	int rows_dirty;
	hui_tree_item_t* selected;
	int hot;
	hui_scrollbar_t bar;
	HUI_CTreeEvent selection_callback;
	void* selection_user;
	HUI_CTreeEvent activated_callback;
	void* activated_user;
};

/* ---------------------------------------------------------------------------
 *  Items
 * ------------------------------------------------------------------------- */

static hui_tree_item_t* hui_tree_item_get(HUI_id id) {
	return (hui_tree_item_t*)hui_pool_get(&g_hui.tree_items, id);
}

static hui_tree_item_t* hui_tree_item_check(HUI_id id, const char* function) {
	hui_tree_item_t* item;
	if (!hui_api_check(function)) return NULL;
	item = hui_tree_item_get(id);
	if (!item) hui_error(HUI_ERROR_INVALID_ID, HUI_SEVERITY_ERROR, "%s: invalid tree item handle 0x%08X", function, (unsigned)id);
	return item;
}

static void hui_tree_changed(hui_tree_view_t* tree) {
	tree->rows_dirty = HUI_TRUE;
	hui_widget_invalidate_layout(&tree->base);
}

static hui_tree_item_t* hui_tree_item_add(hui_tree_view_t* tree, hui_tree_item_t* parent, const char* text) {
	hui_tree_item_t* item = (hui_tree_item_t*)hui_calloc(sizeof(hui_tree_item_t));
	item->tree = tree;
	item->parent = parent;
	item->text = hui_strdup(text ? text : "");
	item->id = hui_pool_add(&g_hui.tree_items, item);
	if (item->id == HUI_INVALID_ID) {
		hui_free(item->text);
		hui_free(item);
		return NULL;
	}
	HUI_ARRAY_RESERVE(parent->children, parent->child_cap, parent->child_count + 1);
	parent->children[parent->child_count++] = item;
	hui_tree_changed(tree);
	return item;
}

static int hui_tree_item_is_inside(const hui_tree_item_t* item, const hui_tree_item_t* ancestor) {
	for (; item; item = item->parent) {
		if (item == ancestor) return HUI_TRUE;
	}
	return HUI_FALSE;
}

/* Releases the descendants of an item, and the item itself unless it is the root. */
static void hui_tree_item_free(hui_tree_item_t* item) {
	uint32_t i;
	for (i = 0; i < item->child_count; i++) hui_tree_item_free(item->children[i]);
	hui_free(item->children);
	item->children = NULL;
	item->child_count = item->child_cap = 0;
	if (item->id != 0) {
		hui_pool_remove(&g_hui.tree_items, item->id);
		hui_free(item->text);
		hui_free(item);
	}
}

static void hui_tree_item_delete(hui_tree_item_t* item) {
	hui_tree_view_t* tree = item->tree;
	hui_tree_item_t* parent = item->parent;
	uint32_t i;
	if (tree->selected && hui_tree_item_is_inside(tree->selected, item)) tree->selected = NULL;
	for (i = 0; parent && i < parent->child_count; i++) {
		if (parent->children[i] != item) continue;
		memmove(&parent->children[i], &parent->children[i + 1], sizeof(hui_tree_item_t*) * (parent->child_count - i - 1));
		parent->child_count--;
		break;
	}
	hui_tree_item_free(item);
	tree->hot = -1;
	hui_tree_changed(tree);
}

/* ---------------------------------------------------------------------------
 *  Rows
 * ------------------------------------------------------------------------- */

static void hui_tree_flatten(hui_tree_view_t* tree, hui_tree_item_t* item, int depth) {
	uint32_t i;
	for (i = 0; i < item->child_count; i++) {
		hui_tree_item_t* child = item->children[i];
		child->depth = depth;
		HUI_ARRAY_RESERVE(tree->rows, tree->row_cap, tree->row_count + 1);
		tree->rows[tree->row_count++] = child;
		if (child->expanded) hui_tree_flatten(tree, child, depth + 1);
	}
}

static void hui_tree_update_rows(hui_tree_view_t* tree) {
	if (!tree->rows_dirty) return;
	tree->rows_dirty = HUI_FALSE;
	tree->row_count = 0;
	hui_tree_flatten(tree, &tree->root, 0);
}

static int hui_tree_row_of(hui_tree_view_t* tree, const hui_tree_item_t* item) {
	uint32_t i;
	hui_tree_update_rows(tree);
	for (i = 0; i < tree->row_count; i++) {
		if (tree->rows[i] == item) return (int)i;
	}
	return -1;
}

static float hui_tree_row_height(const hui_widget_t* widget) {
	return hui_widget_metric(widget, HUI_METRIC_ROW_HEIGHT);
}

/* ---------------------------------------------------------------------------
 *  Class
 * ------------------------------------------------------------------------- */

static void hui_tree_view_init(hui_widget_t* widget) {
	hui_tree_view_t* tree = (hui_tree_view_t*)widget;
	tree->root.tree = tree;
	tree->root.expanded = HUI_TRUE;
	tree->hot = -1;
}

static void hui_tree_view_destroy(hui_widget_t* widget) {
	hui_tree_view_t* tree = (hui_tree_view_t*)widget;
	hui_tree_item_free(&tree->root);
	hui_free(tree->rows);
}

static hui_vec2_t hui_tree_view_measure(hui_widget_t* widget) {
	hui_tree_view_t* tree = (hui_tree_view_t*)widget;
	hui_tree_update_rows(tree);
	return hui_vec2(160.0f, hui_tree_row_height(widget) * (float)hui_clampi((int)tree->row_count, 3, 10) + 2.0f);
}

static void hui_tree_view_arrange(hui_widget_t* widget) {
	hui_tree_view_t* tree = (hui_tree_view_t*)widget;
	hui_tree_update_rows(tree);
	tree->bar.content = hui_tree_row_height(widget) * (float)tree->row_count;
	tree->bar.viewport = hui_maxf(0.0f, widget->rect.h - 2.0f);
	hui_scrollbar_clamp(&tree->bar);
}

static hui_rect_t hui_tree_track(const hui_widget_t* widget) {
	float bar = hui_widget_metric(widget, HUI_METRIC_SCROLLBAR_SIZE);
	return hui_rect(widget->rect.w - bar - 1.0f, 1.0f, bar, hui_maxf(0.0f, widget->rect.h - 2.0f));
}

static int hui_tree_row_at(hui_widget_t* widget, float y) {
	hui_tree_view_t* tree = (hui_tree_view_t*)widget;
	int row;
	if (y < 1.0f || y >= widget->rect.h - 1.0f) return -1;
	hui_tree_update_rows(tree);
	row = (int)floorf((y - 1.0f + tree->bar.offset) / hui_tree_row_height(widget));
	return (row >= 0 && row < (int)tree->row_count) ? row : -1;
}

static void hui_tree_reveal(hui_widget_t* widget, int row) {
	hui_tree_view_t* tree = (hui_tree_view_t*)widget;
	float height = hui_tree_row_height(widget), top = height * (float)row;
	if (row < 0) return;
	hui_tree_view_arrange(widget);
	if (top < tree->bar.offset) hui_scrollbar_set(&tree->bar, top);
	else if (top + height > tree->bar.offset + tree->bar.viewport) hui_scrollbar_set(&tree->bar, top + height - tree->bar.viewport);
	hui_widget_invalidate_paint(widget);
}

static void hui_tree_select(hui_widget_t* widget, hui_tree_item_t* item, int notify) {
	hui_tree_view_t* tree = (hui_tree_view_t*)widget;
	hui_tree_item_t* walk;
	if (tree->selected == item) return;
	tree->selected = item;
	/* an item hidden in a collapsed branch becomes visible */
	for (walk = item ? item->parent : NULL; walk; walk = walk->parent) {
		if (!walk->expanded) {
			walk->expanded = HUI_TRUE;
			hui_tree_changed(tree);
		}
	}
	if (item) hui_tree_reveal(widget, hui_tree_row_of(tree, item));
	hui_widget_invalidate_paint(widget);
	if (notify && tree->selection_callback) {
		tree->selection_callback(widget->id, item ? item->id : HUI_INVALID_ID, tree->selection_user);
	}
}

static void hui_tree_set_expanded(hui_tree_item_t* item, int expanded) {
	hui_tree_view_t* tree = item->tree;
	expanded = expanded != 0;
	if (item->expanded == expanded) return;
	item->expanded = expanded;
	/* the selection never stays inside a branch that is being closed */
	if (!expanded && tree->selected && tree->selected != item && hui_tree_item_is_inside(tree->selected, item)) {
		tree->selected = item;
	}
	hui_tree_changed(tree);
}

static void hui_tree_activate(hui_widget_t* widget) {
	hui_tree_view_t* tree = (hui_tree_view_t*)widget;
	if (!tree->selected) return;
	if (tree->activated_callback) tree->activated_callback(widget->id, tree->selected->id, tree->activated_user);
	if (hui_widget_alive(widget)) hui_widget_fire_action(widget);
}

static void hui_tree_view_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_tree_view_t* tree = (hui_tree_view_t*)widget;
	hui_rect_t rect = hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h);
	float row = hui_tree_row_height(widget), indent = hui_widget_metric(widget, HUI_METRIC_INDENT);
	int focused = (widget->flags & HUI_WF_FOCUSED) != 0;
	int first = (int)floorf(tree->bar.offset / row), last, i;
	float content_width = rect.w - 2.0f - (hui_scrollbar_needed(&tree->bar) ? hui_widget_metric(widget, HUI_METRIC_SCROLLBAR_SIZE) : 0.0f);
	hui_color_t text = hui_widget_text_color(widget, painter);
	hui_color_t selection = hui_widget_color(widget, HUI_COLOR_SELECTION);
	hui_vec2_t origin = hui_widget_origin(widget);

	hui_paint_box(painter, rect, hui_widget_metric(widget, HUI_METRIC_CORNER_RADIUS), hui_widget_color(widget, HUI_COLOR_INPUT),
		hui_widget_metric(widget, HUI_METRIC_BORDER_WIDTH), hui_widget_color(widget, focused ? HUI_COLOR_FOCUS : HUI_COLOR_BORDER));

	hui_paint_push_clip(painter, hui_rect(1.0f, 1.0f, rect.w - 2.0f, rect.h - 2.0f));
	last = hui_mini((int)tree->row_count, first + (int)(tree->bar.viewport / row) + 2);
	for (i = hui_maxi(first, 0); i < last; i++) {
		hui_tree_item_t* item = tree->rows[i];
		hui_rect_t line = hui_rect(1.0f, 1.0f + row * (float)i - tree->bar.offset, content_width, row);
		float x = 4.0f + indent * (float)item->depth;
		if (item == tree->selected) {
			hui_paint_rect(painter, line, focused ? selection : hui_color_mix(selection, hui_widget_color(widget, HUI_COLOR_INPUT), 0.45f));
		} else if (i == tree->hot && (widget->flags & HUI_WF_HOVERED) && painter->enabled) {
			hui_paint_rect(painter, line, hui_widget_color(widget, HUI_COLOR_ROW_HOVERED));
		}
		if (item->child_count > 0) {
			hui_paint_icon(painter, item->expanded ? HUI_ICON_CHEVRON_DOWN : HUI_ICON_CHEVRON_RIGHT,
				x + indent * 0.5f, line.y + row * 0.5f, 7.0f, hui_color_fade(text, 0.75f));
		}
		hui_paint_text_in(painter, HUI_INVALID_ID, hui_widget_font_size(widget, 0.0f),
			hui_rect(x + indent, line.y, hui_maxf(0.0f, line.w - x - indent), row), 0.0f, item->text, -1, text);
	}
	hui_paint_pop_clip(painter);
	hui_scrollbar_paint(&tree->bar, widget, painter, hui_tree_track(widget), HUI_TRUE,
		widget->window ? widget->window->mouse_x - origin.x : -1.0f, widget->window ? widget->window->mouse_y - origin.y : -1.0f);
}

static int hui_tree_view_event(hui_widget_t* widget, const HUI_Event* event) {
	hui_tree_view_t* tree = (hui_tree_view_t*)widget;
	float indent = hui_widget_metric(widget, HUI_METRIC_INDENT);
	int changed = HUI_FALSE, row, page;
	hui_tree_item_t* item;

	if (event->type == HUI_EVENT_MOUSE_DOWN || event->type == HUI_EVENT_MOUSE_MOVE || event->type == HUI_EVENT_MOUSE_UP) {
		if (hui_scrollbar_event(&tree->bar, widget, event, hui_tree_track(widget), HUI_TRUE, &changed)) {
			hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		}
	}
	switch (event->type) {
		case HUI_EVENT_MOUSE_MOVE:
			row = hui_tree_row_at(widget, event->local_y);
			if (row != tree->hot) {
				tree->hot = row;
				hui_widget_invalidate_paint(widget);
			}
			return HUI_FALSE;
		case HUI_EVENT_MOUSE_LEAVE:
			tree->hot = -1;
			hui_widget_invalidate_paint(widget);
			return HUI_FALSE;
		case HUI_EVENT_MOUSE_DOWN: {
			float arrow_x;
			if (event->button != HUI_MOUSE_LEFT) return HUI_FALSE;
			row = hui_tree_row_at(widget, event->local_y);
			if (row < 0) return HUI_TRUE;
			item = tree->rows[row];
			arrow_x = 5.0f + indent * (float)item->depth;
			if (item->child_count > 0 && event->local_x >= arrow_x && event->local_x < arrow_x + indent) {
				/* the arrow only opens and closes the branch */
				hui_tree_set_expanded(item, !item->expanded);
				return HUI_TRUE;
			}
			hui_tree_select(widget, item, HUI_TRUE);
			if (event->clicks == 2 && hui_widget_alive(widget) && tree->selected == item) {
				if (item->child_count > 0) hui_tree_set_expanded(item, !item->expanded);
				hui_tree_activate(widget);
			}
			return HUI_TRUE;
		}
		case HUI_EVENT_MOUSE_WHEEL:
			if (!hui_scrollbar_set(&tree->bar, tree->bar.offset - event->wheel_y * hui_tree_row_height(widget) * 3.0f)) return HUI_FALSE;
			tree->hot = hui_tree_row_at(widget, event->local_y);
			hui_widget_invalidate_paint(widget);
			return HUI_TRUE;
		case HUI_EVENT_FOCUS_GAINED:
		case HUI_EVENT_FOCUS_LOST:
			hui_widget_invalidate_paint(widget);
			return HUI_FALSE;
		case HUI_EVENT_KEY_DOWN:
			hui_tree_update_rows(tree);
			if (tree->row_count == 0) return HUI_FALSE;
			row = tree->selected ? hui_tree_row_of(tree, tree->selected) : -1;
			page = hui_maxi(1, (int)(tree->bar.viewport / hui_tree_row_height(widget)) - 1);
			item = tree->selected;
			switch (event->key) {
				case HUI_KEY_UP:
					row = row < 0 ? (int)tree->row_count - 1 : row - 1;
					break;
				case HUI_KEY_DOWN:
					row = row + 1;
					break;
				case HUI_KEY_PAGE_UP:
					row -= page;
					break;
				case HUI_KEY_PAGE_DOWN:
					row += page;
					break;
				case HUI_KEY_HOME:
					row = 0;
					break;
				case HUI_KEY_END:
					row = (int)tree->row_count - 1;
					break;
				case HUI_KEY_LEFT:
					/* close the branch, or go up to the parent */
					if (!item) return HUI_FALSE;
					if (item->expanded && item->child_count > 0) hui_tree_set_expanded(item, HUI_FALSE);
					else if (item->parent && item->parent != &tree->root) hui_tree_select(widget, item->parent, HUI_TRUE);
					return HUI_TRUE;
				case HUI_KEY_RIGHT:
					/* open the branch, or go down to its first item */
					if (!item || item->child_count == 0) return HUI_FALSE;
					if (!item->expanded) hui_tree_set_expanded(item, HUI_TRUE);
					else hui_tree_select(widget, item->children[0], HUI_TRUE);
					return HUI_TRUE;
				case HUI_KEY_ENTER:
				case HUI_KEY_KP_ENTER:
					hui_tree_activate(widget);
					return HUI_TRUE;
				default:
					return HUI_FALSE;
			}
			hui_tree_select(widget, tree->rows[hui_clampi(row, 0, (int)tree->row_count - 1)], HUI_TRUE);
			return HUI_TRUE;
		default:
			return HUI_FALSE;
	}
}

/* <Item Text="..." Expanded="true"> ... nested <Item> ... </Item> */
static int hui_tree_xml_item(hui_tree_view_t* tree, hui_tree_item_t* parent, const hui_xml_node_t* node) {
	const hui_xml_node_t* child;
	const char* text;
	const char* expanded;
	hui_tree_item_t* item;
	int flag = HUI_FALSE;

	if (hui_stricmp(node->name, "Item") != 0) return HUI_FALSE;
	text = hui_xml_attribute(node, "Text");
	item = hui_tree_item_add(tree, parent, text ? text : node->text);
	if (!item) return HUI_FALSE;
	expanded = hui_xml_attribute(node, "Expanded");
	if (expanded && hui_parse_bool(expanded, &flag)) item->expanded = flag;
	for (child = node->first_child; child; child = child->next) {
		if (!hui_tree_xml_item(tree, item, child)) return HUI_FALSE;
	}
	return HUI_TRUE;
}

static int hui_tree_view_xml_child(hui_widget_t* widget, const hui_xml_node_t* node) {
	hui_tree_view_t* tree = (hui_tree_view_t*)widget;
	return hui_tree_xml_item(tree, &tree->root, node);
}

const hui_widget_class_t hui_class_tree_view = {
	.type = HUI_WIDGET_TREE_VIEW,
	.name = "TreeView",
	.size = sizeof(hui_tree_view_t),
	.flags = HUI_CF_NO_CHILDREN | HUI_CF_FOCUSABLE,
	.init = hui_tree_view_init,
	.destroy = hui_tree_view_destroy,
	.measure = hui_tree_view_measure,
	.arrange = hui_tree_view_arrange,
	.paint = hui_tree_view_paint,
	.event = hui_tree_view_event,
	.xml_child = hui_tree_view_xml_child
};

/* ---------------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------------- */

HUI_id HUI_AddTreeItem(HUI_id id, HUI_id parent_item, const char* text) {
	hui_tree_view_t* tree;
	hui_tree_item_t* parent;
	hui_tree_item_t* item;
	HUI_WIDGET(widget, id, HUI_WIDGET_TREE_VIEW, HUI_INVALID_ID);
	tree = (hui_tree_view_t*)widget;
	parent = &tree->root;
	if (parent_item != HUI_INVALID_ID) {
		parent = hui_tree_item_check(parent_item, __func__);
		if (!parent) return HUI_INVALID_ID;
		if (parent->tree != tree) {
			hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_AddTreeItem: the parent item belongs to another tree");
			return HUI_INVALID_ID;
		}
	}
	item = hui_tree_item_add(tree, parent, text);
	return item ? item->id : HUI_INVALID_ID;
}

void HUI_DeleteTreeItem(HUI_id id) {
	hui_tree_item_t* item = hui_tree_item_check(id, __func__);
	if (item) hui_tree_item_delete(item);
}

void HUI_ClearTreeItems(HUI_id id) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_TREE_VIEW);
	hui_tree_view_t* tree = (hui_tree_view_t*)widget;
	hui_tree_item_free(&tree->root);
	tree->selected = NULL;
	tree->hot = -1;
	tree->bar.offset = 0.0f;
	hui_tree_changed(tree);
}

int HUI_IsValidTreeItem(HUI_id item) {
	return g_hui.initialized && hui_tree_item_get(item) != NULL;
}

void HUI_SetTreeItemText(HUI_id id, const char* text) {
	hui_tree_item_t* item = hui_tree_item_check(id, __func__);
	if (item && hui_str_set(&item->text, text ? text : "")) hui_widget_invalidate_paint(&item->tree->base);
}

const char* HUI_GetTreeItemText(HUI_id id) {
	hui_tree_item_t* item = hui_tree_item_check(id, __func__);
	return item ? item->text : "";
}

void HUI_SetTreeItemExpanded(HUI_id id, int expanded) {
	hui_tree_item_t* item = hui_tree_item_check(id, __func__);
	if (item) hui_tree_set_expanded(item, expanded);
}

int HUI_IsTreeItemExpanded(HUI_id id) {
	hui_tree_item_t* item = hui_tree_item_check(id, __func__);
	return item ? item->expanded : HUI_FALSE;
}

void HUI_SetTreeItemUserHandle(HUI_id id, void* handle) {
	hui_tree_item_t* item = hui_tree_item_check(id, __func__);
	if (item) item->user_handle = handle;
}

void* HUI_GetTreeItemUserHandle(HUI_id id) {
	hui_tree_item_t* item = hui_tree_item_check(id, __func__);
	return item ? item->user_handle : NULL;
}

int HUI_GetTreeItemCount(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_TREE_VIEW, 0);
	return (int)((hui_tree_view_t*)widget)->root.child_count;
}

HUI_id HUI_GetTreeItem(HUI_id id, int index) {
	HUI_WIDGET(widget, id, HUI_WIDGET_TREE_VIEW, HUI_INVALID_ID);
	hui_tree_view_t* tree = (hui_tree_view_t*)widget;
	if (index < 0 || index >= (int)tree->root.child_count) return HUI_INVALID_ID;
	return tree->root.children[index]->id;
}

HUI_id HUI_GetTreeItemParent(HUI_id id) {
	hui_tree_item_t* item = hui_tree_item_check(id, __func__);
	if (!item || !item->parent || item->parent == &item->tree->root) return HUI_INVALID_ID;
	return item->parent->id;
}

int HUI_GetTreeItemChildCount(HUI_id id) {
	hui_tree_item_t* item = hui_tree_item_check(id, __func__);
	return item ? (int)item->child_count : 0;
}

HUI_id HUI_GetTreeItemChild(HUI_id id, int index) {
	hui_tree_item_t* item = hui_tree_item_check(id, __func__);
	if (!item || index < 0 || index >= (int)item->child_count) return HUI_INVALID_ID;
	return item->children[index]->id;
}

void HUI_SetTreeSelectedItem(HUI_id id, HUI_id item_id) {
	hui_tree_item_t* item = NULL;
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_TREE_VIEW);
	if (item_id != HUI_INVALID_ID) {
		item = hui_tree_item_check(item_id, __func__);
		if (!item) return;
		if (item->tree != (hui_tree_view_t*)widget) {
			hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_SetTreeSelectedItem: the item belongs to another tree");
			return;
		}
	}
	hui_tree_select(widget, item, HUI_FALSE);
}

HUI_id HUI_GetTreeSelectedItem(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_TREE_VIEW, HUI_INVALID_ID);
	hui_tree_view_t* tree = (hui_tree_view_t*)widget;
	return tree->selected ? tree->selected->id : HUI_INVALID_ID;
}

void HUI_SetTreeSelectionChangedCallback(HUI_id id, HUI_CTreeEvent callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_TREE_VIEW);
	((hui_tree_view_t*)widget)->selection_callback = callback;
	((hui_tree_view_t*)widget)->selection_user = user_data;
}

void HUI_SetTreeActivatedCallback(HUI_id id, HUI_CTreeEvent callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_TREE_VIEW);
	((hui_tree_view_t*)widget)->activated_callback = callback;
	((hui_tree_view_t*)widget)->activated_user = user_data;
}
