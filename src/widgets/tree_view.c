/**
 * Raven - widgets/tree_view.c
 * TreeView (Slate STreeView): hierarchy of text items that expand and
 * collapse, with one selection. Like the list, it is virtualized: the
 * expanded items are flattened into rows and only the visible rows are drawn.
 *
 * Items are objects with their own Rid handles (Rtree_view_add_item), so the
 * application can keep references to them (scene nodes, assets, ...).
 */

#include "common.h"
#include "../xml/xml.h"

typedef struct rvn_tree_view_t rvn_tree_view_t;

typedef struct rvn_tree_item_t {
	Rid id;
	rvn_tree_view_t* tree;
	struct rvn_tree_item_t* parent;
	struct rvn_tree_item_t** children;
	uint32_t child_count;
	uint32_t child_cap;
	char* text;
	int expanded;
	/** Nesting level, filled when the rows are rebuilt. */
	int depth;
	void* user_handle;
} rvn_tree_item_t;

struct rvn_tree_view_t {
	rvn_widget_t base;
	/** Invisible item holding the top-level items. */
	rvn_tree_item_t root;
	/** Items currently shown, top to bottom. */
	rvn_tree_item_t** rows;
	uint32_t row_count;
	uint32_t row_cap;
	int rows_dirty;
	rvn_tree_item_t* selected;
	int hot;
	rvn_scrollbar_t bar;
	RTreeEventCallback selection_callback;
	void* selection_user;
	RTreeEventCallback activated_callback;
	void* activated_user;
};

/* ---------------------------------------------------------------------------
 *  Items
 * ------------------------------------------------------------------------- */

static rvn_tree_item_t* rvn_tree_item_get(Rid id) {
	return (rvn_tree_item_t*)rvn_pool_get(&g_rvn.tree_items, id);
}

static rvn_tree_item_t* rvn_tree_item_check(Rid id, const char* function) {
	rvn_tree_item_t* item;
	if (!rvn_api_check(function)) return NULL;
	item = rvn_tree_item_get(id);
	if (!item) rvn_error(R_ERROR_INVALID_ID, R_SEVERITY_ERROR, "%s: invalid tree item handle 0x%08X", function, (unsigned)id);
	return item;
}

static void rvn_tree_changed(rvn_tree_view_t* tree) {
	tree->rows_dirty = R_TRUE;
	rvn_widget_invalidate_layout(&tree->base);
}

static rvn_tree_item_t* rvn_tree_item_add(rvn_tree_view_t* tree, rvn_tree_item_t* parent, const char* text) {
	rvn_tree_item_t* item = (rvn_tree_item_t*)rvn_calloc(sizeof(rvn_tree_item_t));
	item->tree = tree;
	item->parent = parent;
	item->text = rvn_strdup(text ? text : "");
	item->id = rvn_pool_add(&g_rvn.tree_items, item);
	if (item->id == R_INVALID_ID) {
		rvn_free(item->text);
		rvn_free(item);
		return NULL;
	}
	RVN_ARRAY_RESERVE(parent->children, parent->child_cap, parent->child_count + 1);
	parent->children[parent->child_count++] = item;
	rvn_tree_changed(tree);
	return item;
}

static int rvn_tree_item_is_inside(const rvn_tree_item_t* item, const rvn_tree_item_t* ancestor) {
	for (; item; item = item->parent) {
		if (item == ancestor) return R_TRUE;
	}
	return R_FALSE;
}

/* Releases the descendants of an item, and the item itself unless it is the root. */
static void rvn_tree_item_free(rvn_tree_item_t* item) {
	uint32_t i;
	for (i = 0; i < item->child_count; i++) rvn_tree_item_free(item->children[i]);
	rvn_free(item->children);
	item->children = NULL;
	item->child_count = item->child_cap = 0;
	if (item->id != 0) {
		rvn_pool_remove(&g_rvn.tree_items, item->id);
		rvn_free(item->text);
		rvn_free(item);
	}
}

static void rvn_tree_item_delete(rvn_tree_item_t* item) {
	rvn_tree_view_t* tree = item->tree;
	rvn_tree_item_t* parent = item->parent;
	uint32_t i;
	if (tree->selected && rvn_tree_item_is_inside(tree->selected, item)) tree->selected = NULL;
	for (i = 0; parent && i < parent->child_count; i++) {
		if (parent->children[i] != item) continue;
		memmove(&parent->children[i], &parent->children[i + 1], sizeof(rvn_tree_item_t*) * (parent->child_count - i - 1));
		parent->child_count--;
		break;
	}
	rvn_tree_item_free(item);
	tree->hot = -1;
	rvn_tree_changed(tree);
}

/* ---------------------------------------------------------------------------
 *  Rows
 * ------------------------------------------------------------------------- */

static void rvn_tree_flatten(rvn_tree_view_t* tree, rvn_tree_item_t* item, int depth) {
	uint32_t i;
	for (i = 0; i < item->child_count; i++) {
		rvn_tree_item_t* child = item->children[i];
		child->depth = depth;
		RVN_ARRAY_RESERVE(tree->rows, tree->row_cap, tree->row_count + 1);
		tree->rows[tree->row_count++] = child;
		if (child->expanded) rvn_tree_flatten(tree, child, depth + 1);
	}
}

static void rvn_tree_update_rows(rvn_tree_view_t* tree) {
	if (!tree->rows_dirty) return;
	tree->rows_dirty = R_FALSE;
	tree->row_count = 0;
	rvn_tree_flatten(tree, &tree->root, 0);
}

static int rvn_tree_row_of(rvn_tree_view_t* tree, const rvn_tree_item_t* item) {
	uint32_t i;
	rvn_tree_update_rows(tree);
	for (i = 0; i < tree->row_count; i++) {
		if (tree->rows[i] == item) return (int)i;
	}
	return -1;
}

static float rvn_tree_row_height(const rvn_widget_t* widget) {
	return rvn_widget_metric(widget, R_METRIC_ROW_HEIGHT);
}

/* ---------------------------------------------------------------------------
 *  Class
 * ------------------------------------------------------------------------- */

static void rvn_tree_view_init(rvn_widget_t* widget) {
	rvn_tree_view_t* tree = (rvn_tree_view_t*)widget;
	tree->root.tree = tree;
	tree->root.expanded = R_TRUE;
	tree->hot = -1;
}

static void rvn_tree_view_destroy(rvn_widget_t* widget) {
	rvn_tree_view_t* tree = (rvn_tree_view_t*)widget;
	rvn_tree_item_free(&tree->root);
	rvn_free(tree->rows);
}

static rvn_vec2_t rvn_tree_view_measure(rvn_widget_t* widget) {
	rvn_tree_view_t* tree = (rvn_tree_view_t*)widget;
	rvn_tree_update_rows(tree);
	return rvn_vec2(160.0f, rvn_tree_row_height(widget) * (float)rvn_clampi((int)tree->row_count, 3, 10) + 2.0f);
}

static void rvn_tree_view_arrange(rvn_widget_t* widget) {
	rvn_tree_view_t* tree = (rvn_tree_view_t*)widget;
	rvn_tree_update_rows(tree);
	tree->bar.content = rvn_tree_row_height(widget) * (float)tree->row_count;
	tree->bar.viewport = rvn_maxf(0.0f, widget->rect.h - 2.0f);
	rvn_scrollbar_clamp(&tree->bar);
}

static rvn_rect_t rvn_tree_track(const rvn_widget_t* widget) {
	float bar = rvn_widget_metric(widget, R_METRIC_SCROLLBAR_SIZE);
	return rvn_rect(widget->rect.w - bar - 1.0f, 1.0f, bar, rvn_maxf(0.0f, widget->rect.h - 2.0f));
}

static int rvn_tree_row_at(rvn_widget_t* widget, float y) {
	rvn_tree_view_t* tree = (rvn_tree_view_t*)widget;
	int row;
	if (y < 1.0f || y >= widget->rect.h - 1.0f) return -1;
	rvn_tree_update_rows(tree);
	row = (int)floorf((y - 1.0f + tree->bar.offset) / rvn_tree_row_height(widget));
	return (row >= 0 && row < (int)tree->row_count) ? row : -1;
}

static void rvn_tree_reveal(rvn_widget_t* widget, int row) {
	rvn_tree_view_t* tree = (rvn_tree_view_t*)widget;
	float height = rvn_tree_row_height(widget), top = height * (float)row;
	if (row < 0) return;
	rvn_tree_view_arrange(widget);
	if (top < tree->bar.offset) rvn_scrollbar_set(&tree->bar, top);
	else if (top + height > tree->bar.offset + tree->bar.viewport) rvn_scrollbar_set(&tree->bar, top + height - tree->bar.viewport);
	rvn_widget_invalidate_paint(widget);
}

static void rvn_tree_select(rvn_widget_t* widget, rvn_tree_item_t* item, int notify) {
	rvn_tree_view_t* tree = (rvn_tree_view_t*)widget;
	rvn_tree_item_t* walk;
	if (tree->selected == item) return;
	tree->selected = item;
	/* an item hidden in a collapsed branch becomes visible */
	for (walk = item ? item->parent : NULL; walk; walk = walk->parent) {
		if (!walk->expanded) {
			walk->expanded = R_TRUE;
			rvn_tree_changed(tree);
		}
	}
	if (item) rvn_tree_reveal(widget, rvn_tree_row_of(tree, item));
	rvn_widget_invalidate_paint(widget);
	if (notify && tree->selection_callback) {
		tree->selection_callback(widget->id, item ? item->id : R_INVALID_ID, tree->selection_user);
	}
}

static void rvn_tree_set_expanded(rvn_tree_item_t* item, int expanded) {
	rvn_tree_view_t* tree = item->tree;
	expanded = expanded != 0;
	if (item->expanded == expanded) return;
	item->expanded = expanded;
	/* the selection never stays inside a branch that is being closed */
	if (!expanded && tree->selected && tree->selected != item && rvn_tree_item_is_inside(tree->selected, item)) {
		tree->selected = item;
	}
	rvn_tree_changed(tree);
}

static void rvn_tree_activate(rvn_widget_t* widget) {
	rvn_tree_view_t* tree = (rvn_tree_view_t*)widget;
	if (!tree->selected) return;
	if (tree->activated_callback) tree->activated_callback(widget->id, tree->selected->id, tree->activated_user);
	if (rvn_widget_alive(widget)) rvn_widget_fire_action(widget);
}

static void rvn_tree_view_paint(rvn_widget_t* widget, RPainter* painter) {
	rvn_tree_view_t* tree = (rvn_tree_view_t*)widget;
	rvn_rect_t rect = rvn_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h);
	float row = rvn_tree_row_height(widget), indent = rvn_widget_metric(widget, R_METRIC_INDENT);
	int focused = (widget->flags & RVN_WF_FOCUSED) != 0;
	int first = (int)floorf(tree->bar.offset / row), last, i;
	float content_width = rect.w - 2.0f - (rvn_scrollbar_needed(&tree->bar) ? rvn_widget_metric(widget, R_METRIC_SCROLLBAR_SIZE) : 0.0f);
	rvn_color_t text = rvn_widget_text_color(widget, painter);
	rvn_color_t selection = rvn_widget_color(widget, R_COLOR_SELECTION);
	rvn_vec2_t origin = rvn_widget_origin(widget);

	rvn_paint_box(painter, rect, rvn_widget_metric(widget, R_METRIC_CORNER_RADIUS), rvn_widget_color(widget, R_COLOR_INPUT),
		rvn_widget_metric(widget, R_METRIC_BORDER_WIDTH), rvn_widget_color(widget, focused ? R_COLOR_FOCUS : R_COLOR_BORDER));

	rvn_paint_push_clip(painter, rvn_rect(1.0f, 1.0f, rect.w - 2.0f, rect.h - 2.0f));
	last = rvn_mini((int)tree->row_count, first + (int)(tree->bar.viewport / row) + 2);
	for (i = rvn_maxi(first, 0); i < last; i++) {
		rvn_tree_item_t* item = tree->rows[i];
		rvn_rect_t line = rvn_rect(1.0f, 1.0f + row * (float)i - tree->bar.offset, content_width, row);
		float x = 4.0f + indent * (float)item->depth;
		if (item == tree->selected) {
			rvn_paint_row(painter, line, focused ? selection : rvn_color_mix(selection, rvn_widget_color(widget, R_COLOR_INPUT), 0.45f));
		} else if (i == tree->hot && (widget->flags & RVN_WF_HOVERED) && painter->enabled) {
			rvn_paint_row(painter, line, rvn_widget_color(widget, R_COLOR_ROW_HOVERED));
		}
		if (item->child_count > 0) {
			rvn_paint_icon(painter, item->expanded ? RVN_ICON_CHEVRON_DOWN : RVN_ICON_CHEVRON_RIGHT,
				x + indent * 0.5f, line.y + row * 0.5f, 7.0f, rvn_color_fade(text, 0.75f));
		}
		rvn_paint_text_in(painter, R_INVALID_ID, rvn_widget_font_size(widget, 0.0f),
			rvn_rect(x + indent, line.y, rvn_maxf(0.0f, line.w - x - indent), row), 0.0f, item->text, -1, text);
	}
	rvn_paint_pop_clip(painter);
	rvn_scrollbar_paint(&tree->bar, widget, painter, rvn_tree_track(widget), R_TRUE,
		widget->window ? widget->window->mouse_x - origin.x : -1.0f, widget->window ? widget->window->mouse_y - origin.y : -1.0f);
}

static int rvn_tree_view_event(rvn_widget_t* widget, const REvent* event) {
	rvn_tree_view_t* tree = (rvn_tree_view_t*)widget;
	float indent = rvn_widget_metric(widget, R_METRIC_INDENT);
	int changed = R_FALSE, row, page;
	rvn_tree_item_t* item;

	if (event->type == R_EVENT_MOUSE_DOWN || event->type == R_EVENT_MOUSE_MOVE || event->type == R_EVENT_MOUSE_UP) {
		if (rvn_scrollbar_event(&tree->bar, widget, event, rvn_tree_track(widget), R_TRUE, &changed)) {
			rvn_widget_invalidate_paint(widget);
			return R_TRUE;
		}
	}
	switch (event->type) {
		case R_EVENT_MOUSE_MOVE:
			row = rvn_tree_row_at(widget, event->local_y);
			if (row != tree->hot) {
				tree->hot = row;
				rvn_widget_invalidate_paint(widget);
			}
			return R_FALSE;
		case R_EVENT_MOUSE_LEAVE:
			tree->hot = -1;
			rvn_widget_invalidate_paint(widget);
			return R_FALSE;
		case R_EVENT_MOUSE_DOWN: {
			float arrow_x;
			if (event->button != R_MOUSE_LEFT) return R_FALSE;
			row = rvn_tree_row_at(widget, event->local_y);
			if (row < 0) return R_TRUE;
			item = tree->rows[row];
			arrow_x = 5.0f + indent * (float)item->depth;
			if (item->child_count > 0 && event->local_x >= arrow_x && event->local_x < arrow_x + indent) {
				/* the arrow only opens and closes the branch */
				rvn_tree_set_expanded(item, !item->expanded);
				return R_TRUE;
			}
			rvn_tree_select(widget, item, R_TRUE);
			if (event->clicks == 2 && rvn_widget_alive(widget) && tree->selected == item) {
				if (item->child_count > 0) rvn_tree_set_expanded(item, !item->expanded);
				rvn_tree_activate(widget);
			}
			return R_TRUE;
		}
		case R_EVENT_MOUSE_WHEEL:
			if (!rvn_scrollbar_set(&tree->bar, tree->bar.offset - event->wheel_y * rvn_tree_row_height(widget) * 3.0f)) return R_FALSE;
			tree->hot = rvn_tree_row_at(widget, event->local_y);
			rvn_widget_invalidate_paint(widget);
			return R_TRUE;
		case R_EVENT_FOCUS_GAINED:
		case R_EVENT_FOCUS_LOST:
			rvn_widget_invalidate_paint(widget);
			return R_FALSE;
		case R_EVENT_KEY_DOWN:
			rvn_tree_update_rows(tree);
			if (tree->row_count == 0) return R_FALSE;
			row = tree->selected ? rvn_tree_row_of(tree, tree->selected) : -1;
			page = rvn_maxi(1, (int)(tree->bar.viewport / rvn_tree_row_height(widget)) - 1);
			item = tree->selected;
			switch (event->key) {
				case R_KEY_UP:
					row = row < 0 ? (int)tree->row_count - 1 : row - 1;
					break;
				case R_KEY_DOWN:
					row = row + 1;
					break;
				case R_KEY_PAGE_UP:
					row -= page;
					break;
				case R_KEY_PAGE_DOWN:
					row += page;
					break;
				case R_KEY_HOME:
					row = 0;
					break;
				case R_KEY_END:
					row = (int)tree->row_count - 1;
					break;
				case R_KEY_LEFT:
					/* close the branch, or go up to the parent */
					if (!item) return R_FALSE;
					if (item->expanded && item->child_count > 0) rvn_tree_set_expanded(item, R_FALSE);
					else if (item->parent && item->parent != &tree->root) rvn_tree_select(widget, item->parent, R_TRUE);
					return R_TRUE;
				case R_KEY_RIGHT:
					/* open the branch, or go down to its first item */
					if (!item || item->child_count == 0) return R_FALSE;
					if (!item->expanded) rvn_tree_set_expanded(item, R_TRUE);
					else rvn_tree_select(widget, item->children[0], R_TRUE);
					return R_TRUE;
				case R_KEY_ENTER:
				case R_KEY_KP_ENTER:
					rvn_tree_activate(widget);
					return R_TRUE;
				default:
					return R_FALSE;
			}
			rvn_tree_select(widget, tree->rows[rvn_clampi(row, 0, (int)tree->row_count - 1)], R_TRUE);
			return R_TRUE;
		default:
			return R_FALSE;
	}
}

/* <Item Text="..." Expanded="true"> ... nested <Item> ... </Item> */
static int rvn_tree_xml_item(rvn_tree_view_t* tree, rvn_tree_item_t* parent, const rvn_xml_node_t* node) {
	const rvn_xml_node_t* child;
	const char* text;
	const char* expanded;
	rvn_tree_item_t* item;
	int flag = R_FALSE;

	if (rvn_stricmp(node->name, "Item") != 0) return R_FALSE;
	text = rvn_xml_attribute(node, "Text");
	item = rvn_tree_item_add(tree, parent, text ? text : node->text);
	if (!item) return R_FALSE;
	expanded = rvn_xml_attribute(node, "Expanded");
	if (expanded && rvn_parse_bool(expanded, &flag)) item->expanded = flag;
	for (child = node->first_child; child; child = child->next) {
		if (!rvn_tree_xml_item(tree, item, child)) return R_FALSE;
	}
	return R_TRUE;
}

static int rvn_tree_view_xml_child(rvn_widget_t* widget, const rvn_xml_node_t* node) {
	rvn_tree_view_t* tree = (rvn_tree_view_t*)widget;
	return rvn_tree_xml_item(tree, &tree->root, node);
}

const rvn_widget_class_t rvn_class_tree_view = {
	.type = R_WIDGET_TREE_VIEW,
	.name = "TreeView",
	.size = sizeof(rvn_tree_view_t),
	.flags = RVN_CF_NO_CHILDREN | RVN_CF_FOCUSABLE,
	.init = rvn_tree_view_init,
	.destroy = rvn_tree_view_destroy,
	.measure = rvn_tree_view_measure,
	.arrange = rvn_tree_view_arrange,
	.paint = rvn_tree_view_paint,
	.event = rvn_tree_view_event,
	.xml_child = rvn_tree_view_xml_child
};

/* ---------------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------------- */

Rid Rtree_view_add_item(Rid id, Rid parent_item, const char* text) {
	rvn_tree_view_t* tree;
	rvn_tree_item_t* parent;
	rvn_tree_item_t* item;
	RVN_WIDGET(widget, id, R_WIDGET_TREE_VIEW, R_INVALID_ID);
	tree = (rvn_tree_view_t*)widget;
	parent = &tree->root;
	if (parent_item != R_INVALID_ID) {
		parent = rvn_tree_item_check(parent_item, __func__);
		if (!parent) return R_INVALID_ID;
		if (parent->tree != tree) {
			rvn_error(R_ERROR_INVALID_OPERATION, R_SEVERITY_ERROR, "Rtree_view_add_item: the parent item belongs to another tree");
			return R_INVALID_ID;
		}
	}
	item = rvn_tree_item_add(tree, parent, text);
	return item ? item->id : R_INVALID_ID;
}

void Rtree_item_destroy(Rid id) {
	rvn_tree_item_t* item = rvn_tree_item_check(id, __func__);
	if (item) rvn_tree_item_delete(item);
}

void Rtree_view_clear_items(Rid id) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_TREE_VIEW);
	rvn_tree_view_t* tree = (rvn_tree_view_t*)widget;
	rvn_tree_item_free(&tree->root);
	tree->selected = NULL;
	tree->hot = -1;
	tree->bar.offset = 0.0f;
	rvn_tree_changed(tree);
}

int Rtree_item_is_valid(Rid item) {
	return g_rvn.initialized && rvn_tree_item_get(item) != NULL;
}

void Rtree_item_set_text(Rid id, const char* text) {
	rvn_tree_item_t* item = rvn_tree_item_check(id, __func__);
	if (item && rvn_str_set(&item->text, text ? text : "")) rvn_widget_invalidate_paint(&item->tree->base);
}

const char* Rtree_item_get_text(Rid id) {
	rvn_tree_item_t* item = rvn_tree_item_check(id, __func__);
	return item ? item->text : "";
}

void Rtree_item_set_expanded(Rid id, int expanded) {
	rvn_tree_item_t* item = rvn_tree_item_check(id, __func__);
	if (item) rvn_tree_set_expanded(item, expanded);
}

int Rtree_item_is_expanded(Rid id) {
	rvn_tree_item_t* item = rvn_tree_item_check(id, __func__);
	return item ? item->expanded : R_FALSE;
}

void Rtree_item_set_user_handle(Rid id, void* handle) {
	rvn_tree_item_t* item = rvn_tree_item_check(id, __func__);
	if (item) item->user_handle = handle;
}

void* Rtree_item_get_user_handle(Rid id) {
	rvn_tree_item_t* item = rvn_tree_item_check(id, __func__);
	return item ? item->user_handle : NULL;
}

int Rtree_view_get_item_count(Rid id) {
	RVN_WIDGET(widget, id, R_WIDGET_TREE_VIEW, 0);
	return (int)((rvn_tree_view_t*)widget)->root.child_count;
}

Rid Rtree_view_get_item(Rid id, int index) {
	RVN_WIDGET(widget, id, R_WIDGET_TREE_VIEW, R_INVALID_ID);
	rvn_tree_view_t* tree = (rvn_tree_view_t*)widget;
	if (index < 0 || index >= (int)tree->root.child_count) return R_INVALID_ID;
	return tree->root.children[index]->id;
}

Rid Rtree_item_get_parent(Rid id) {
	rvn_tree_item_t* item = rvn_tree_item_check(id, __func__);
	if (!item || !item->parent || item->parent == &item->tree->root) return R_INVALID_ID;
	return item->parent->id;
}

int Rtree_item_get_child_count(Rid id) {
	rvn_tree_item_t* item = rvn_tree_item_check(id, __func__);
	return item ? (int)item->child_count : 0;
}

Rid Rtree_item_get_child(Rid id, int index) {
	rvn_tree_item_t* item = rvn_tree_item_check(id, __func__);
	if (!item || index < 0 || index >= (int)item->child_count) return R_INVALID_ID;
	return item->children[index]->id;
}

void Rtree_view_set_selected_item(Rid id, Rid item_id) {
	rvn_tree_item_t* item = NULL;
	RVN_WIDGET_VOID(widget, id, R_WIDGET_TREE_VIEW);
	if (item_id != R_INVALID_ID) {
		item = rvn_tree_item_check(item_id, __func__);
		if (!item) return;
		if (item->tree != (rvn_tree_view_t*)widget) {
			rvn_error(R_ERROR_INVALID_OPERATION, R_SEVERITY_ERROR, "Rtree_view_set_selected_item: the item belongs to another tree");
			return;
		}
	}
	rvn_tree_select(widget, item, R_FALSE);
}

Rid Rtree_view_get_selected_item(Rid id) {
	RVN_WIDGET(widget, id, R_WIDGET_TREE_VIEW, R_INVALID_ID);
	rvn_tree_view_t* tree = (rvn_tree_view_t*)widget;
	return tree->selected ? tree->selected->id : R_INVALID_ID;
}

void Rtree_view_set_selection_changed_callback(Rid id, RTreeEventCallback callback, void* user_data) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_TREE_VIEW);
	((rvn_tree_view_t*)widget)->selection_callback = callback;
	((rvn_tree_view_t*)widget)->selection_user = user_data;
}

void Rtree_view_set_activated_callback(Rid id, RTreeEventCallback callback, void* user_data) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_TREE_VIEW);
	((rvn_tree_view_t*)widget)->activated_callback = callback;
	((rvn_tree_view_t*)widget)->activated_user = user_data;
}
