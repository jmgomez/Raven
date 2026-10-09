/**
 * HUI - widgets/menu.c
 * MenuBar, Menu and MenuItem.
 *
 *   MenuBar   draws the titles of its Menu children and opens them
 *   Menu      a popup listing its children: MenuItems and sub-Menus
 *   MenuItem  one entry: text, shortcut, optional check mark, or a separator
 *
 * Menus and items are widgets so that they have handles, names, actions and
 * an enabled state like everything else, but they are not laid out by the
 * generic passes: a Menu measures and draws its entries itself and is shown
 * through the popup layer of the window.
 */

#include "common.h"
#include "../xml/xml.h"

typedef struct hui_menu_item_t {
	hui_widget_t base;
	char* text;
	char* shortcut_text;
	HUI_EKey key;
	uint32_t modifiers;
	int separator;
	int checkable;
	int checked;
	HUI_CMenuItemClicked callback;
	void* user_data;
} hui_menu_item_t;

typedef struct hui_menu_t {
	hui_widget_t base;
	char* title;
	/** Index of the highlighted child, or -1. */
	int hot;
	/** Sub-menu currently open, or NULL. */
	hui_widget_t* open_sub;
} hui_menu_t;

typedef struct hui_menu_bar_t {
	hui_widget_t base;
	int hot;
	/** Index of the child menu currently open, or -1. */
	int open;
} hui_menu_bar_t;

#define HUI_MENU_PADDING 4.0f
#define HUI_MENU_GUTTER 24.0f
#define HUI_MENU_SEPARATOR_HEIGHT 7.0f

/* ===========================================================================
 *  Shortcut names
 * ========================================================================= */

static const struct { const char* name; HUI_EKey key; } g_key_names[] = {
	{ "Space", HUI_KEY_SPACE }, { "Escape", HUI_KEY_ESCAPE }, { "Esc", HUI_KEY_ESCAPE }, { "Enter", HUI_KEY_ENTER },
	{ "Return", HUI_KEY_ENTER }, { "Tab", HUI_KEY_TAB }, { "Backspace", HUI_KEY_BACKSPACE }, { "Insert", HUI_KEY_INSERT },
	{ "Delete", HUI_KEY_DELETE }, { "Del", HUI_KEY_DELETE }, { "Right", HUI_KEY_RIGHT }, { "Left", HUI_KEY_LEFT },
	{ "Down", HUI_KEY_DOWN }, { "Up", HUI_KEY_UP }, { "PageUp", HUI_KEY_PAGE_UP }, { "PageDown", HUI_KEY_PAGE_DOWN },
	{ "Home", HUI_KEY_HOME }, { "End", HUI_KEY_END }, { "F1", HUI_KEY_F1 }, { "F2", HUI_KEY_F2 }, { "F3", HUI_KEY_F3 },
	{ "F4", HUI_KEY_F4 }, { "F5", HUI_KEY_F5 }, { "F6", HUI_KEY_F6 }, { "F7", HUI_KEY_F7 }, { "F8", HUI_KEY_F8 },
	{ "F9", HUI_KEY_F9 }, { "F10", HUI_KEY_F10 }, { "F11", HUI_KEY_F11 }, { "F12", HUI_KEY_F12 }
};

static const char* hui_key_name(HUI_EKey key, char buffer[2]) {
	size_t i;
	for (i = 0; i < HUI_COUNT_OF(g_key_names); i++) {
		if (g_key_names[i].key == key) return g_key_names[i].name;
	}
	if ((int)key > 32 && (int)key < 127) {
		buffer[0] = (char)key;
		buffer[1] = '\0';
		return buffer;
	}
	return "";
}

static void hui_shortcut_format(HUI_EKey key, uint32_t modifiers, char* out, size_t size) {
	char single[2];
	snprintf(out, size, "%s%s%s%s%s", (modifiers & HUI_MOD_CTRL) ? "Ctrl+" : "", (modifiers & HUI_MOD_SUPER) ? "Super+" : "",
		(modifiers & HUI_MOD_ALT) ? "Alt+" : "", (modifiers & HUI_MOD_SHIFT) ? "Shift+" : "", hui_key_name(key, single));
}

/** Parses "Ctrl+Shift+S". Also used by the <Window> shortcuts of the XML loader. */
int hui_parse_shortcut(const char* text, HUI_EKey* key, uint32_t* modifiers) {
	*key = HUI_KEY_UNKNOWN;
	*modifiers = 0;
	while (*text) {
		char token[16];
		size_t length = strcspn(text, "+"), i;
		/* "Ctrl++" : the last token is the plus key itself */
		if (length == 0 && text[0] == '+') length = 1;
		if (length >= sizeof(token)) return HUI_FALSE;
		memcpy(token, text, length);
		token[length] = '\0';
		text += length;
		if (*text == '+') text++;

		if (hui_stricmp(token, "Ctrl") == 0 || hui_stricmp(token, "Control") == 0) *modifiers |= HUI_MOD_CTRL;
		else if (hui_stricmp(token, "Shift") == 0) *modifiers |= HUI_MOD_SHIFT;
		else if (hui_stricmp(token, "Alt") == 0) *modifiers |= HUI_MOD_ALT;
		else if (hui_stricmp(token, "Super") == 0 || hui_stricmp(token, "Cmd") == 0 || hui_stricmp(token, "Win") == 0) *modifiers |= HUI_MOD_SUPER;
		else {
			if (*key != HUI_KEY_UNKNOWN) return HUI_FALSE;
			for (i = 0; i < HUI_COUNT_OF(g_key_names); i++) {
				if (hui_stricmp(token, g_key_names[i].name) == 0) *key = g_key_names[i].key;
			}
			if (*key == HUI_KEY_UNKNOWN && length == 1) {
				char c = token[0];
				if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
				if (c > 32 && c < 127) *key = (HUI_EKey)c;
			}
			if (*key == HUI_KEY_UNKNOWN) return HUI_FALSE;
		}
	}
	return *key != HUI_KEY_UNKNOWN;
}

/* ===========================================================================
 *  MenuItem
 * ========================================================================= */

static void hui_menu_item_destroy(hui_widget_t* widget) {
	hui_menu_item_t* item = (hui_menu_item_t*)widget;
	hui_free(item->text);
	hui_free(item->shortcut_text);
}

static void hui_menu_item_set_shortcut(hui_widget_t* widget, HUI_EKey key, uint32_t modifiers) {
	hui_menu_item_t* item = (hui_menu_item_t*)widget;
	char buffer[48];
	item->key = key;
	item->modifiers = modifiers & 0xFu;
	if (key == HUI_KEY_UNKNOWN) {
		hui_str_set(&item->shortcut_text, NULL);
	} else {
		hui_shortcut_format(key, item->modifiers, buffer, sizeof(buffer));
		hui_str_set(&item->shortcut_text, buffer);
	}
	if (widget->parent) hui_widget_invalidate_layout(widget->parent);
}

void hui_menu_item_trigger(hui_widget_t* widget) {
	hui_menu_item_t* item = (hui_menu_item_t*)widget;
	hui_widget_t* root;
	hui_window_t* window = widget->window;

	if (item->separator) return;
	if (item->checkable) item->checked = !item->checked;
	/* a context menu is only attached to a window while it is open */
	for (root = widget; root->parent; root = root->parent) {}
	if (!window) window = root->window;
	/* close the menus first: the callback may open a dialog or delete the menu */
	if (window) hui_window_close_popups(window);
	if (item->callback) item->callback(widget->id, item->user_data);
	if (hui_widget_alive(widget)) hui_widget_fire_action(widget);
}

static int hui_menu_item_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_menu_item_t* item = (hui_menu_item_t*)widget;
	HUI_EKey key;
	uint32_t modifiers;
	if (HUI_ATTR("Text")) {
		hui_str_set(&item->text, value);
	} else if (HUI_ATTR("Shortcut")) {
		if (!hui_parse_shortcut(value, &key, &modifiers)) return HUI_FALSE;
		hui_menu_item_set_shortcut(widget, key, modifiers);
	} else if (HUI_ATTR("Checkable")) {
		if (!hui_parse_bool(value, &item->checkable)) return HUI_FALSE;
	} else if (HUI_ATTR("Checked")) {
		if (!hui_parse_bool(value, &item->checked)) return HUI_FALSE;
		if (item->checked) item->checkable = HUI_TRUE;
	} else if (HUI_ATTR("Separator")) {
		if (!hui_parse_bool(value, &item->separator)) return HUI_FALSE;
	} else {
		return HUI_FALSE;
	}
	if (widget->parent) hui_widget_invalidate_layout(widget->parent);
	return HUI_TRUE;
}

const hui_widget_class_t hui_class_menu_item = {
	.type = HUI_WIDGET_MENU_ITEM,
	.name = "MenuItem",
	.size = sizeof(hui_menu_item_t),
	.flags = HUI_CF_NO_CHILDREN | HUI_CF_NO_HIT,
	.destroy = hui_menu_item_destroy,
	.set_attribute = hui_menu_item_set_attribute
};

/* ===========================================================================
 *  Menu
 * ========================================================================= */

static void hui_menu_destroy(hui_widget_t* widget) {
	hui_free(((hui_menu_t*)widget)->title);
}

static void hui_menu_init(hui_widget_t* widget) {
	((hui_menu_t*)widget)->hot = -1;
}

static int hui_menu_is_entry(const hui_widget_t* child) {
	return (child->cls->type == HUI_WIDGET_MENU_ITEM || child->cls->type == HUI_WIDGET_MENU) && !hui_widget_collapsed(child);
}

static int hui_menu_entry_is_separator(const hui_widget_t* child) {
	return child->cls->type == HUI_WIDGET_MENU_ITEM && ((const hui_menu_item_t*)child)->separator;
}

static const char* hui_menu_entry_text(const hui_widget_t* child) {
	const char* text = child->cls->type == HUI_WIDGET_MENU ? ((const hui_menu_t*)child)->title : ((const hui_menu_item_t*)child)->text;
	return text ? text : "";
}

static float hui_menu_entry_height(const hui_widget_t* menu, const hui_widget_t* child) {
	return hui_menu_entry_is_separator(child) ? HUI_MENU_SEPARATOR_HEIGHT : hui_widget_metric(menu, HUI_METRIC_ROW_HEIGHT) + 2.0f;
}

/* An entry can be highlighted and triggered. */
static int hui_menu_entry_selectable(const hui_widget_t* child) {
	return hui_menu_is_entry(child) && !hui_menu_entry_is_separator(child) && !(child->flags & HUI_WF_DISABLED);
}

static hui_vec2_t hui_menu_measure(hui_widget_t* widget) {
	float text_width = 0.0f, shortcut_width = 0.0f, height = HUI_MENU_PADDING * 2.0f;
	uint32_t i;
	for (i = 0; i < widget->child_count; i++) {
		hui_widget_t* child = widget->children[i];
		if (!hui_menu_is_entry(child)) continue;
		height += hui_menu_entry_height(widget, child);
		if (hui_menu_entry_is_separator(child)) continue;
		text_width = hui_maxf(text_width, hui_widget_text_width(widget, HUI_INVALID_ID, 0.0f, hui_menu_entry_text(child)));
		if (child->cls->type == HUI_WIDGET_MENU_ITEM) {
			shortcut_width = hui_maxf(shortcut_width,
				hui_widget_text_width(widget, HUI_INVALID_ID, 0.0f, ((hui_menu_item_t*)child)->shortcut_text));
		}
	}
	/* check gutter | text | gap | shortcut | room for the sub-menu arrow */
	return hui_vec2(ceilf(HUI_MENU_GUTTER + text_width + 28.0f + shortcut_width + 20.0f), ceilf(height));
}

/* Rectangle of the entry `index` in menu coordinates; HUI_FALSE when it is not an entry. */
static int hui_menu_entry_rect(const hui_widget_t* widget, int index, hui_rect_t* rect) {
	float y = HUI_MENU_PADDING;
	uint32_t i;
	for (i = 0; i < widget->child_count; i++) {
		const hui_widget_t* child = widget->children[i];
		float height;
		if (!hui_menu_is_entry(child)) continue;
		height = hui_menu_entry_height(widget, child);
		if ((int)i == index) {
			*rect = hui_rect(HUI_MENU_PADDING, y, widget->rect.w - HUI_MENU_PADDING * 2.0f, height);
			return HUI_TRUE;
		}
		y += height;
	}
	return HUI_FALSE;
}

static int hui_menu_entry_at(const hui_widget_t* widget, float x, float y) {
	uint32_t i;
	if (x < 0.0f || x >= widget->rect.w) return -1;
	for (i = 0; i < widget->child_count; i++) {
		hui_rect_t rect;
		if (hui_menu_entry_rect(widget, (int)i, &rect) && y >= rect.y && y < rect.y + rect.h) return (int)i;
	}
	return -1;
}

static void hui_menu_close_sub(hui_widget_t* widget) {
	hui_menu_t* menu = (hui_menu_t*)widget;
	if (menu->open_sub && widget->window) hui_window_close_popup(widget->window, menu->open_sub);
	menu->open_sub = NULL;
}

static void hui_menu_open_sub(hui_widget_t* widget, int index) {
	hui_menu_t* menu = (hui_menu_t*)widget;
	hui_widget_t* sub = widget->children[index];
	hui_vec2_t origin = hui_widget_origin(widget);
	hui_rect_t rect;
	if (menu->open_sub == sub || !widget->window) return;
	hui_menu_close_sub(widget);
	if (!hui_menu_entry_rect(widget, index, &rect)) return;
	menu->open_sub = sub;
	/* overlap the parent menu by its padding, like native menus */
	hui_window_open_popup(widget->window, sub, widget,
		hui_rect(origin.x, origin.y + rect.y - HUI_MENU_PADDING, widget->rect.w - 2.0f, rect.h), HUI_POPUP_RIGHT, 0.0f);
}

static void hui_menu_set_hot(hui_widget_t* widget, int index, int open_sub) {
	hui_menu_t* menu = (hui_menu_t*)widget;
	if (index >= 0 && !hui_menu_entry_selectable(widget->children[index])) index = -1;
	if (menu->hot != index) {
		menu->hot = index;
		hui_widget_invalidate_paint(widget);
	}
	if (index >= 0 && widget->children[index]->cls->type == HUI_WIDGET_MENU) {
		if (open_sub) hui_menu_open_sub(widget, index);
	} else if (index >= 0) {
		hui_menu_close_sub(widget);
	}
}

/* Moves the highlight to the next selectable entry in a direction. */
static void hui_menu_step(hui_widget_t* widget, int direction) {
	hui_menu_t* menu = (hui_menu_t*)widget;
	int count = (int)widget->child_count, index = menu->hot, i;
	for (i = 0; i < count; i++) {
		index = index < 0 ? (direction > 0 ? 0 : count - 1) : (index + direction + count) % count;
		if (hui_menu_entry_selectable(widget->children[index])) {
			hui_menu_set_hot(widget, index, HUI_FALSE);
			return;
		}
	}
}

static void hui_menu_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_menu_t* menu = (hui_menu_t*)widget;
	hui_rect_t rect = hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h);
	float radius = hui_widget_metric(widget, HUI_METRIC_CORNER_RADIUS) + 2.0f;
	float font_size = hui_widget_font_size(widget, 0.0f);
	hui_color_t text = hui_widget_color(widget, HUI_COLOR_TEXT);
	hui_color_t dim = hui_widget_color(widget, HUI_COLOR_TEXT_DISABLED);
	uint32_t i;

	hui_paint_shadow(painter, hui_rect(0.0f, 4.0f, rect.w, rect.h), radius, 16.0f, hui_widget_color(widget, HUI_COLOR_SHADOW));
	hui_paint_box(painter, rect, radius, hui_widget_color(widget, HUI_COLOR_POPUP), 1.0f, hui_widget_color(widget, HUI_COLOR_POPUP_BORDER));

	for (i = 0; i < widget->child_count; i++) {
		hui_widget_t* child = widget->children[i];
		hui_rect_t row;
		int enabled = !(child->flags & HUI_WF_DISABLED);
		int hot = (int)i == menu->hot || (menu->open_sub == child);
		hui_color_t color = enabled ? text : dim;

		if (!hui_menu_entry_rect(widget, (int)i, &row)) continue;
		if (hui_menu_entry_is_separator(child)) {
			hui_paint_rect(painter, hui_rect(row.x + 4.0f, row.y + floorf(row.h * 0.5f), row.w - 8.0f, 1.0f),
				hui_widget_color(widget, HUI_COLOR_SEPARATOR));
			continue;
		}
		if (hot && enabled) {
			hui_paint_box(painter, row, 3.0f, hui_widget_color(widget, HUI_COLOR_SELECTION), 0.0f, 0);
		}
		hui_paint_text_in(painter, HUI_INVALID_ID, font_size,
			hui_rect(row.x + HUI_MENU_GUTTER - HUI_MENU_PADDING, row.y, row.w, row.h), 0.0f, hui_menu_entry_text(child), -1, color);

		if (child->cls->type == HUI_WIDGET_MENU) {
			hui_paint_icon(painter, HUI_ICON_CHEVRON_RIGHT, row.x + row.w - 10.0f, row.y + row.h * 0.5f, 7.0f, color);
		} else {
			hui_menu_item_t* item = (hui_menu_item_t*)child;
			if (item->checked) {
				hui_paint_icon(painter, HUI_ICON_CHECK, row.x + 10.0f, row.y + row.h * 0.5f, 8.0f, color);
			}
			if (item->shortcut_text) {
				hui_paint_text_in(painter, HUI_INVALID_ID, font_size, hui_rect(row.x, row.y, row.w - 10.0f, row.h), 1.0f,
					item->shortcut_text, -1, hui_color_mix(color, dim, 0.6f));
			}
		}
	}
}

static int hui_menu_event(hui_widget_t* widget, const HUI_Event* event) {
	hui_menu_t* menu = (hui_menu_t*)widget;
	int index;

	/* a menu only reacts while it is shown */
	if (!(widget->flags & HUI_WF_POPUP)) return HUI_FALSE;

	switch (event->type) {
		case HUI_EVENT_MOUSE_MOVE:
			index = hui_menu_entry_at(widget, event->local_x, event->local_y);
			if (index >= 0) hui_menu_set_hot(widget, index, HUI_TRUE);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_DOWN:
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_UP:
			if (event->button != HUI_MOUSE_LEFT) return HUI_TRUE;
			index = hui_menu_entry_at(widget, event->local_x, event->local_y);
			if (index < 0 || !hui_menu_entry_selectable(widget->children[index])) return HUI_TRUE;
			if (widget->children[index]->cls->type == HUI_WIDGET_MENU) hui_menu_set_hot(widget, index, HUI_TRUE);
			else hui_menu_item_trigger(widget->children[index]);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_WHEEL:
			return HUI_TRUE;
		case HUI_EVENT_KEY_DOWN:
			switch (event->key) {
				case HUI_KEY_UP:
					hui_menu_step(widget, -1);
					return HUI_TRUE;
				case HUI_KEY_DOWN:
					hui_menu_step(widget, 1);
					return HUI_TRUE;
				case HUI_KEY_RIGHT:
					if (menu->hot >= 0 && widget->children[menu->hot]->cls->type == HUI_WIDGET_MENU) {
						hui_widget_t* sub = widget->children[menu->hot];
						hui_menu_open_sub(widget, menu->hot);
						hui_menu_step(sub, 1);
					}
					return HUI_TRUE;
				case HUI_KEY_LEFT:
					/* closes a sub-menu and returns to its parent menu */
					if (widget->parent && widget->parent->cls->type == HUI_WIDGET_MENU && widget->window) {
						hui_window_close_popup(widget->window, widget);
					}
					return HUI_TRUE;
				case HUI_KEY_ENTER:
				case HUI_KEY_KP_ENTER:
				case HUI_KEY_SPACE:
					if (menu->hot < 0) return HUI_TRUE;
					if (widget->children[menu->hot]->cls->type == HUI_WIDGET_MENU) {
						hui_widget_t* sub = widget->children[menu->hot];
						hui_menu_open_sub(widget, menu->hot);
						hui_menu_step(sub, 1);
					} else {
						hui_menu_item_trigger(widget->children[menu->hot]);
					}
					return HUI_TRUE;
				default:
					/* Escape goes to the window, which closes the top popup */
					return HUI_FALSE;
			}
		default:
			return HUI_FALSE;
	}
}

static void hui_menu_arrange(hui_widget_t* widget) {
	/* entries are not widgets on screen, and sub-menus are placed by the popup layer */
	HUI_UNUSED(widget);
}

static int hui_menu_child_active(hui_widget_t* widget, hui_widget_t* child) {
	/* entries are drawn by the menu itself and sub-menus live in the popup layer */
	HUI_UNUSED(widget);
	HUI_UNUSED(child);
	return HUI_FALSE;
}

static void hui_menu_bar_menu_closed(hui_widget_t* bar, hui_widget_t* menu);

static void hui_menu_popup_closed(hui_widget_t* widget) {
	hui_menu_t* menu = (hui_menu_t*)widget;
	menu->hot = -1;
	menu->open_sub = NULL;
	if (widget->parent && widget->parent->cls->type == HUI_WIDGET_MENU) {
		hui_menu_t* parent = (hui_menu_t*)widget->parent;
		if (parent->open_sub == widget) parent->open_sub = NULL;
		hui_widget_invalidate_paint(widget->parent);
	} else if (widget->parent && widget->parent->cls->type == HUI_WIDGET_MENU_BAR) {
		hui_menu_bar_menu_closed(widget->parent, widget);
	}
}

void hui_menu_popup(hui_widget_t* menu, hui_window_t* window, float x, float y) {
	if (menu->window && menu->window != window) hui_window_close_popup(menu->window, menu);
	hui_window_open_popup(window, menu, NULL, hui_rect(x, y, 0.0f, 0.0f), HUI_POPUP_AT, 0.0f);
}

static hui_widget_t* hui_menu_add_separator(hui_widget_t* menu) {
	hui_widget_t* item = hui_widget_create(HUI_WIDGET_MENU_ITEM);
	if (!item) return NULL;
	((hui_menu_item_t*)item)->separator = HUI_TRUE;
	hui_widget_add_child(menu, item, -1);
	return item;
}

static int hui_menu_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	if (HUI_ATTR("Text") || HUI_ATTR("Title")) {
		hui_str_set(&((hui_menu_t*)widget)->title, value);
		if (widget->parent) hui_widget_invalidate_layout(widget->parent);
		return HUI_TRUE;
	}
	return HUI_FALSE;
}

static int hui_menu_xml_child(hui_widget_t* widget, const hui_xml_node_t* node) {
	/* inside a menu, <Separator/> is a menu separator, not the Separator widget */
	if (hui_stricmp(node->name, "Separator") == 0 || hui_stricmp(node->name, "MenuSeparator") == 0) {
		return hui_menu_add_separator(widget) != NULL;
	}
	return HUI_FALSE;
}

static void hui_menu_children_changed(hui_widget_t* widget) {
	((hui_menu_t*)widget)->hot = -1;
}

const hui_widget_class_t hui_class_menu = {
	.type = HUI_WIDGET_MENU,
	.name = "Menu",
	.size = sizeof(hui_menu_t),
	.init = hui_menu_init,
	.destroy = hui_menu_destroy,
	.measure = hui_menu_measure,
	.arrange = hui_menu_arrange,
	.paint = hui_menu_paint,
	.event = hui_menu_event,
	.set_attribute = hui_menu_set_attribute,
	.child_active = hui_menu_child_active,
	.children_changed = hui_menu_children_changed,
	.xml_child = hui_menu_xml_child,
	.popup_closed = hui_menu_popup_closed
};

/* Finds the enabled menu item bound to a shortcut anywhere below `root`. */
hui_widget_t* hui_menu_find_shortcut(hui_widget_t* root, HUI_EKey key, uint32_t modifiers) {
	uint32_t i;
	if (!root || (root->flags & HUI_WF_DISABLED)) return NULL;
	if (root->cls->type == HUI_WIDGET_MENU_ITEM) {
		hui_menu_item_t* item = (hui_menu_item_t*)root;
		return (item->key == key && item->key != HUI_KEY_UNKNOWN && item->modifiers == modifiers) ? root : NULL;
	}
	for (i = 0; i < root->child_count; i++) {
		hui_widget_t* found = hui_menu_find_shortcut(root->children[i], key, modifiers);
		if (found) return found;
	}
	return NULL;
}

/* ===========================================================================
 *  MenuBar
 * ========================================================================= */

static void hui_menu_bar_init(hui_widget_t* widget) {
	hui_menu_bar_t* bar = (hui_menu_bar_t*)widget;
	bar->hot = -1;
	bar->open = -1;
}

static float hui_menu_bar_title_width(const hui_widget_t* widget, const hui_widget_t* child) {
	return ceilf(hui_widget_text_width(widget, HUI_INVALID_ID, 0.0f, hui_menu_entry_text(child))) +
		hui_widget_metric(widget, HUI_METRIC_PADDING_X) * 2.0f;
}

static int hui_menu_bar_is_title(const hui_widget_t* child) {
	return child->cls->type == HUI_WIDGET_MENU && !hui_widget_collapsed(child);
}

static hui_vec2_t hui_menu_bar_measure(hui_widget_t* widget) {
	float width = 4.0f;
	uint32_t i;
	for (i = 0; i < widget->child_count; i++) {
		if (hui_menu_bar_is_title(widget->children[i])) width += hui_menu_bar_title_width(widget, widget->children[i]);
	}
	return hui_vec2(width, hui_widget_metric(widget, HUI_METRIC_CONTROL_HEIGHT));
}

static int hui_menu_bar_title_rect(const hui_widget_t* widget, int index, hui_rect_t* rect) {
	float x = 2.0f;
	uint32_t i;
	for (i = 0; i < widget->child_count; i++) {
		float width;
		if (!hui_menu_bar_is_title(widget->children[i])) continue;
		width = hui_menu_bar_title_width(widget, widget->children[i]);
		if ((int)i == index) {
			*rect = hui_rect(x, 2.0f, width, widget->rect.h - 4.0f);
			return HUI_TRUE;
		}
		x += width;
	}
	return HUI_FALSE;
}

static int hui_menu_bar_title_at(const hui_widget_t* widget, float x, float y) {
	uint32_t i;
	if (y < 0.0f || y >= widget->rect.h) return -1;
	for (i = 0; i < widget->child_count; i++) {
		hui_rect_t rect;
		if (hui_menu_bar_title_rect(widget, (int)i, &rect) && x >= rect.x && x < rect.x + rect.w) return (int)i;
	}
	return -1;
}

static void hui_menu_bar_open(hui_widget_t* widget, int index) {
	hui_menu_bar_t* bar = (hui_menu_bar_t*)widget;
	hui_vec2_t origin = hui_widget_origin(widget);
	hui_rect_t rect;
	if (!widget->window || !hui_menu_bar_title_rect(widget, index, &rect)) return;
	if (widget->children[index]->flags & HUI_WF_DISABLED) return;
	hui_window_close_popups(widget->window);
	bar->open = index;
	hui_window_open_popup(widget->window, widget->children[index], widget,
		hui_rect(origin.x + rect.x, origin.y, rect.w, widget->rect.h), HUI_POPUP_BELOW, 0.0f);
	hui_widget_invalidate_paint(widget);
}

static void hui_menu_bar_menu_closed(hui_widget_t* widget, hui_widget_t* menu) {
	hui_menu_bar_t* bar = (hui_menu_bar_t*)widget;
	if (bar->open >= 0 && bar->open < (int)widget->child_count && widget->children[bar->open] == menu) bar->open = -1;
	hui_widget_invalidate_paint(widget);
}

static void hui_menu_bar_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_menu_bar_t* bar = (hui_menu_bar_t*)widget;
	hui_color_t text = hui_widget_text_color(widget, painter);
	uint32_t i;

	hui_paint_rect(painter, hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h), hui_widget_color(widget, HUI_COLOR_HEADER));
	for (i = 0; i < widget->child_count; i++) {
		hui_widget_t* child = widget->children[i];
		hui_rect_t rect;
		int enabled = painter->enabled && !(child->flags & HUI_WF_DISABLED);
		if (!hui_menu_bar_title_rect(widget, (int)i, &rect)) continue;
		if ((int)i == bar->open) {
			hui_paint_box(painter, rect, 3.0f, hui_widget_color(widget, HUI_COLOR_SELECTION), 0.0f, 0);
		} else if ((int)i == bar->hot && (widget->flags & HUI_WF_HOVERED) && enabled) {
			hui_paint_box(painter, rect, 3.0f, hui_widget_color(widget, HUI_COLOR_HEADER_HOVERED), 0.0f, 0);
		}
		hui_paint_text_in(painter, HUI_INVALID_ID, hui_widget_font_size(widget, 0.0f), rect, 0.5f, hui_menu_entry_text(child), -1,
			enabled ? text : hui_widget_color(widget, HUI_COLOR_TEXT_DISABLED));
	}
}

static int hui_menu_bar_event(hui_widget_t* widget, const HUI_Event* event) {
	hui_menu_bar_t* bar = (hui_menu_bar_t*)widget;
	int index;
	switch (event->type) {
		case HUI_EVENT_MOUSE_MOVE:
			index = hui_menu_bar_title_at(widget, event->local_x, event->local_y);
			if (index != bar->hot) {
				bar->hot = index;
				hui_widget_invalidate_paint(widget);
			}
			/* with a menu open, moving over another title switches to it */
			if (bar->open >= 0 && index >= 0 && index != bar->open) hui_menu_bar_open(widget, index);
			return HUI_FALSE;
		case HUI_EVENT_MOUSE_LEAVE:
			bar->hot = -1;
			hui_widget_invalidate_paint(widget);
			return HUI_FALSE;
		case HUI_EVENT_MOUSE_DOWN:
			index = hui_menu_bar_title_at(widget, event->local_x, event->local_y);
			if (event->button != HUI_MOUSE_LEFT || index < 0) return HUI_FALSE;
			hui_menu_bar_open(widget, index);
			return HUI_TRUE;
		default:
			return HUI_FALSE;
	}
}

const hui_widget_class_t hui_class_menu_bar = {
	.type = HUI_WIDGET_MENU_BAR,
	.name = "MenuBar",
	.size = sizeof(hui_menu_bar_t),
	.init = hui_menu_bar_init,
	.measure = hui_menu_bar_measure,
	.arrange = hui_menu_arrange,
	.paint = hui_menu_bar_paint,
	.event = hui_menu_bar_event,
	.child_active = hui_menu_child_active
};

/* ===========================================================================
 *  Public API
 * ========================================================================= */

HUI_id HUI_AddMenu(HUI_id id, const char* title) {
	hui_widget_t* menu;
	HUI_WIDGET(parent, id, 0, HUI_INVALID_ID);
	if (parent->cls->type != HUI_WIDGET_MENU_BAR && parent->cls->type != HUI_WIDGET_MENU) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_AddMenu: expected a MenuBar or a Menu, got a %s", parent->cls->name);
		return HUI_INVALID_ID;
	}
	menu = hui_widget_create(HUI_WIDGET_MENU);
	if (!menu) return HUI_INVALID_ID;
	((hui_menu_t*)menu)->title = hui_strdup(title);
	hui_widget_add_child(parent, menu, -1);
	return menu->id;
}

void HUI_SetMenuTitle(HUI_id id, const char* title) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_MENU);
	if (hui_str_set(&((hui_menu_t*)widget)->title, title) && widget->parent) hui_widget_invalidate_layout(widget->parent);
}

HUI_id HUI_AddMenuItem(HUI_id id, const char* text) {
	hui_widget_t* item;
	HUI_WIDGET(menu, id, HUI_WIDGET_MENU, HUI_INVALID_ID);
	item = hui_widget_create(HUI_WIDGET_MENU_ITEM);
	if (!item) return HUI_INVALID_ID;
	((hui_menu_item_t*)item)->text = hui_strdup(text);
	hui_widget_add_child(menu, item, -1);
	return item->id;
}

HUI_id HUI_AddMenuSeparator(HUI_id id) {
	hui_widget_t* item;
	HUI_WIDGET(menu, id, HUI_WIDGET_MENU, HUI_INVALID_ID);
	item = hui_menu_add_separator(menu);
	return item ? item->id : HUI_INVALID_ID;
}

void HUI_SetMenuItemText(HUI_id id, const char* text) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_MENU_ITEM);
	if (hui_str_set(&((hui_menu_item_t*)widget)->text, text) && widget->parent) hui_widget_invalidate_layout(widget->parent);
}

void HUI_SetMenuItemShortcut(HUI_id id, HUI_EKey key, HUI_uint modifiers) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_MENU_ITEM);
	hui_menu_item_set_shortcut(widget, key, modifiers);
}

void HUI_SetMenuItemCheckable(HUI_id id, int checkable) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_MENU_ITEM);
	((hui_menu_item_t*)widget)->checkable = checkable != 0;
}

void HUI_SetMenuItemChecked(HUI_id id, int checked) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_MENU_ITEM);
	((hui_menu_item_t*)widget)->checked = checked != 0;
	if (widget->parent) hui_widget_invalidate_paint(widget->parent);
}

int HUI_IsMenuItemChecked(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_MENU_ITEM, HUI_FALSE);
	return ((hui_menu_item_t*)widget)->checked;
}

void HUI_SetMenuItemClickedCallback(HUI_id id, HUI_CMenuItemClicked callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_MENU_ITEM);
	((hui_menu_item_t*)widget)->callback = callback;
	((hui_menu_item_t*)widget)->user_data = user_data;
}

void HUI_OpenMenu(HUI_id id, HUI_id window_id, float x, float y) {
	hui_window_t* window;
	HUI_WIDGET_VOID(menu, id, HUI_WIDGET_MENU);
	window = hui_window_check(window_id, __func__);
	if (!window) return;
	hui_window_close_popups(window);
	hui_menu_popup(menu, window, x, y);
}
