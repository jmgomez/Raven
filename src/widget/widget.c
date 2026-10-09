/**
 * HUI - widget/widget.c
 * Widget lifetime, tree structure and the public functions common to all
 * widgets (hui_widget.h).
 */

#include "widget.h"

/* ---------------------------------------------------------------------------
 *  Lookup
 * ------------------------------------------------------------------------- */

hui_widget_t* hui_widget_get(HUI_id id) {
	return (hui_widget_t*)hui_pool_get(&g_hui.widgets, id);
}

hui_widget_t* hui_widget_check(HUI_id id, HUI_EWidgetType type, const char* function) {
	hui_widget_t* widget;
	if (!hui_api_check(function)) return NULL;
	widget = hui_widget_get(id);
	if (!widget) {
		hui_error(HUI_ERROR_INVALID_ID, HUI_SEVERITY_ERROR, "%s: invalid widget handle 0x%08X", function, (unsigned)id);
		return NULL;
	}
	if (type != 0 && widget->cls->type != type) {
		const hui_widget_class_t* expected = hui_widget_class_by_type(type);
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "%s: expected a %s, got a %s",
			function, expected ? expected->name : "?", widget->cls->name);
		return NULL;
	}
	return widget;
}

/* ---------------------------------------------------------------------------
 *  Creation and destruction
 * ------------------------------------------------------------------------- */

hui_widget_t* hui_widget_create(HUI_EWidgetType type) {
	const hui_widget_class_t* cls = hui_widget_class_by_type(type);
	if (!cls) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "unknown widget type 0x%04X", (unsigned)type);
		return NULL;
	}
	return hui_widget_create_class(cls);
}

hui_widget_t* hui_widget_create_class(const hui_widget_class_t* cls) {
	hui_widget_t* widget = (hui_widget_t*)hui_calloc(cls->size);
	widget->cls = cls;
	widget->flags = HUI_WF_LAYOUT_DIRTY;
	if (cls->flags & HUI_CF_NO_HIT) widget->flags |= HUI_WF_NO_HIT;
	if (cls->flags & HUI_CF_FOCUSABLE) widget->flags |= HUI_WF_FOCUSABLE;
	if (cls->flags & HUI_CF_CLIP) widget->flags |= HUI_WF_CLIP;
	widget->visibility = HUI_VISIBLE;
	widget->fixed_size = hui_vec2(-1.0f, -1.0f);
	widget->min_size = hui_vec2(-1.0f, -1.0f);
	widget->max_size = hui_vec2(-1.0f, -1.0f);
	widget->opacity = 1.0f;
	widget->slot.halign = HUI_HALIGN_FILL;
	widget->slot.valign = HUI_VALIGN_FILL;
	widget->slot.fill = 1.0f;
	widget->slot.column_span = 1;
	widget->slot.row_span = 1;
	widget->context_menu = HUI_INVALID_ID;
	widget->subtree_count = 1;
	widget->ui_paint_count = (cls->flags & HUI_CF_UI_PAINT) ? 1u : 0u;
	widget->id = hui_pool_add(&g_hui.widgets, widget);
	if (widget->id == HUI_INVALID_ID) {
		hui_free(widget);
		return NULL;
	}
	if (cls->init) cls->init(widget);
	return widget;
}

/* First phase of a destruction: the subtree loses its handles and is marked dead. */
static void hui_widget_kill(hui_widget_t* widget) {
	uint32_t i;
	widget->flags |= HUI_WF_DEAD;
	hui_pool_remove(&g_hui.widgets, widget->id);
	widget->window = NULL;
	for (i = 0; i < widget->child_count; i++) hui_widget_kill(widget->children[i]);
}

/* Second phase: the memory of the subtree is released. */
static void hui_widget_free(hui_widget_t* widget) {
	uint32_t i;
	if (widget->cls->destroy) widget->cls->destroy(widget);
	for (i = 0; i < widget->child_count; i++) hui_widget_free(widget->children[i]);
	hui_free(widget->children);
	hui_free(widget->style);
	hui_free(widget->name);
	hui_free(widget->tooltip);
	hui_free(widget->action);
	hui_free(widget->slot.title);
	hui_free(widget);
}

void hui_widget_destroy(hui_widget_t* widget) {
	if (!widget || (widget->flags & HUI_WF_DEAD)) return;
	hui_widget_detach(widget);
	hui_widget_kill(widget);
	if (g_hui.dispatch_depth > 0) {
		/* application code up the call stack may still hold this pointer */
		HUI_ARRAY_RESERVE(g_hui.dead_widgets, g_hui.dead_widget_cap, g_hui.dead_widget_count + 1);
		g_hui.dead_widgets[g_hui.dead_widget_count++] = widget;
	} else {
		hui_widget_free(widget);
	}
}

void hui_widgets_collect(void) {
	/* destroy callbacks may delete more widgets: the array can grow while we walk it */
	uint32_t i;
	for (i = 0; i < g_hui.dead_widget_count; i++) hui_widget_free(g_hui.dead_widgets[i]);
	g_hui.dead_widget_count = 0;
}

/* ---------------------------------------------------------------------------
 *  Tree
 * ------------------------------------------------------------------------- */

void hui_widget_set_window(hui_widget_t* widget, hui_window_t* window) {
	uint32_t i;
	widget->window = window;
	for (i = 0; i < widget->child_count; i++) hui_widget_set_window(widget->children[i], window);
}

int hui_widget_child_index(const hui_widget_t* parent, const hui_widget_t* child) {
	uint32_t i;
	for (i = 0; i < parent->child_count; i++) {
		if (parent->children[i] == child) return (int)i;
	}
	return -1;
}

int hui_widget_is_inside(const hui_widget_t* widget, const hui_widget_t* ancestor) {
	for (; widget; widget = widget->parent) {
		if (widget == ancestor) return HUI_TRUE;
	}
	return HUI_FALSE;
}

static void hui_widget_add_counts(hui_widget_t* from, int subtree, int ui_paint) {
	hui_widget_t* walk;
	for (walk = from; walk; walk = walk->parent) {
		walk->subtree_count = (uint32_t)((int)walk->subtree_count + subtree);
		walk->ui_paint_count = (uint32_t)((int)walk->ui_paint_count + ui_paint);
	}
}

void hui_widget_detach(hui_widget_t* widget) {
	hui_widget_t* parent = widget->parent;
	hui_window_t* window = widget->window;

	if (window) {
		hui_window_forget_widget(window, widget);
		hui_window_release_natives(window, widget);
		if (window->root == widget) window->root = NULL;
		window->needs_layout = HUI_TRUE;
		window->needs_paint = HUI_TRUE;
	}
	if (parent) {
		int index = hui_widget_child_index(parent, widget);
		if (index >= 0) {
			memmove(&parent->children[index], &parent->children[index + 1],
				sizeof(hui_widget_t*) * (parent->child_count - (uint32_t)index - 1));
			parent->child_count--;
		}
		hui_widget_add_counts(parent, -(int)widget->subtree_count, -(int)widget->ui_paint_count);
		widget->parent = NULL;
	}
	if (window) hui_widget_set_window(widget, NULL);
	if (parent) {
		if (parent->cls->children_changed) parent->cls->children_changed(parent);
		hui_widget_invalidate_layout(parent);
	}
}

void hui_widget_add_child(hui_widget_t* parent, hui_widget_t* child, int index) {
	if (child->parent || child->window) hui_widget_detach(child);

	if ((parent->cls->flags & HUI_CF_SINGLE_CHILD) && parent->child_count > 0) {
		while (parent->child_count > 0) hui_widget_destroy(parent->children[parent->child_count - 1]);
	}
	if (index < 0 || index > (int)parent->child_count) index = (int)parent->child_count;
	HUI_ARRAY_RESERVE(parent->children, parent->child_cap, parent->child_count + 1);
	memmove(&parent->children[index + 1], &parent->children[index],
		sizeof(hui_widget_t*) * (parent->child_count - (uint32_t)index));
	parent->children[index] = child;
	parent->child_count++;
	child->parent = parent;
	hui_widget_add_counts(parent, (int)child->subtree_count, (int)child->ui_paint_count);
	hui_widget_set_window(child, parent->window);
	if (parent->cls->children_changed) parent->cls->children_changed(parent);
	hui_widget_invalidate_layout(parent);
}

int hui_widget_is_shown(const hui_widget_t* widget) {
	const hui_widget_t* walk;
	if (!widget->window) return HUI_FALSE;
	for (walk = widget; walk; walk = walk->parent) {
		if (walk->visibility != HUI_VISIBLE) return HUI_FALSE;
		/* an open popup is shown whatever its place in the tree */
		if (walk->flags & HUI_WF_POPUP) return HUI_TRUE;
		if (walk->parent && walk->parent->cls->child_active &&
			!walk->parent->cls->child_active(walk->parent, (hui_widget_t*)(uintptr_t)walk)) {
			return HUI_FALSE;
		}
	}
	return HUI_TRUE;
}

int hui_widget_is_enabled(const hui_widget_t* widget) {
	for (; widget; widget = widget->parent) {
		if (widget->flags & HUI_WF_DISABLED) return HUI_FALSE;
	}
	return HUI_TRUE;
}

hui_vec2_t hui_widget_origin(const hui_widget_t* widget) {
	hui_vec2_t origin = hui_vec2(0.0f, 0.0f);
	for (; widget; widget = widget->parent) {
		origin.x += widget->rect.x;
		origin.y += widget->rect.y;
		if (widget->flags & HUI_WF_POPUP) break;
	}
	return origin;
}

/* ---------------------------------------------------------------------------
 *  Public API: creation and tree
 * ------------------------------------------------------------------------- */

HUI_id HUI_CreateWidget(HUI_EWidgetType type) {
	hui_widget_t* widget;
	HUI_API_GUARD(HUI_INVALID_ID);
	widget = hui_widget_create(type);
	return widget ? widget->id : HUI_INVALID_ID;
}

void HUI_DeleteWidget(HUI_id id) {
	HUI_WIDGET_VOID(widget, id, 0);
	hui_widget_destroy(widget);
}

int HUI_IsValidWidget(HUI_id widget) {
	return g_hui.initialized && hui_widget_get(widget) != NULL;
}

HUI_EWidgetType HUI_GetWidgetType(HUI_id id) {
	HUI_WIDGET(widget, id, 0, (HUI_EWidgetType)0);
	return widget->cls->type;
}

HUI_id HUI_InsertChild(HUI_id parent_id, HUI_id child_id, int index) {
	hui_widget_t* child;
	HUI_WIDGET(parent, parent_id, 0, HUI_INVALID_ID);
	child = hui_widget_check(child_id, 0, __func__);
	if (!child) return HUI_INVALID_ID;
	if (parent->cls->flags & HUI_CF_NO_CHILDREN) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_AddChild: a %s cannot have children", parent->cls->name);
		return HUI_INVALID_ID;
	}
	if (hui_widget_is_inside(parent, child)) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_AddChild: a widget cannot be added to itself or to one of its descendants");
		return HUI_INVALID_ID;
	}
	hui_widget_add_child(parent, child, index);
	return child_id;
}

HUI_id HUI_AddChild(HUI_id parent, HUI_id child) {
	return HUI_InsertChild(parent, child, -1);
}

void HUI_RemoveChild(HUI_id parent_id, HUI_id child_id) {
	hui_widget_t* child;
	HUI_WIDGET_VOID(parent, parent_id, 0);
	child = hui_widget_check(child_id, 0, __func__);
	if (!child) return;
	if (child->parent != parent) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_RemoveChild: the widget is not a child of this parent");
		return;
	}
	hui_widget_detach(child);
}

void HUI_ClearChildren(HUI_id parent_id) {
	HUI_WIDGET_VOID(parent, parent_id, 0);
	while (parent->child_count > 0) hui_widget_destroy(parent->children[parent->child_count - 1]);
}

HUI_id HUI_GetParent(HUI_id id) {
	HUI_WIDGET(widget, id, 0, HUI_INVALID_ID);
	return widget->parent ? widget->parent->id : HUI_INVALID_ID;
}

int HUI_GetChildCount(HUI_id id) {
	HUI_WIDGET(widget, id, 0, 0);
	return (int)widget->child_count;
}

HUI_id HUI_GetChild(HUI_id id, int index) {
	HUI_WIDGET(widget, id, 0, HUI_INVALID_ID);
	if (index < 0 || index >= (int)widget->child_count) return HUI_INVALID_ID;
	return widget->children[index]->id;
}

HUI_id HUI_GetWidgetWindow(HUI_id id) {
	HUI_WIDGET(widget, id, 0, HUI_INVALID_ID);
	return widget->window ? widget->window->id : HUI_INVALID_ID;
}

void HUI_SetWidgetName(HUI_id id, const char* name) {
	HUI_WIDGET_VOID(widget, id, 0);
	hui_str_set(&widget->name, name);
}

const char* HUI_GetWidgetName(HUI_id id) {
	HUI_WIDGET(widget, id, 0, "");
	return widget->name ? widget->name : "";
}

static hui_widget_t* hui_widget_find(hui_widget_t* widget, const char* name) {
	uint32_t i;
	if (widget->name && strcmp(widget->name, name) == 0) return widget;
	for (i = 0; i < widget->child_count; i++) {
		hui_widget_t* found = hui_widget_find(widget->children[i], name);
		if (found) return found;
	}
	return NULL;
}

HUI_id HUI_FindWidget(HUI_id scope, const char* name) {
	hui_widget_t* root;
	hui_widget_t* found;
	HUI_API_GUARD(HUI_INVALID_ID);
	if (!name) return HUI_INVALID_ID;
	if (hui_handle_type(scope) == HUI_HANDLE_WINDOW) {
		hui_window_t* window = hui_window_check(scope, __func__);
		root = window ? window->root : NULL;
	} else {
		root = hui_widget_check(scope, 0, __func__);
	}
	found = root ? hui_widget_find(root, name) : NULL;
	return found ? found->id : HUI_INVALID_ID;
}

/* ---------------------------------------------------------------------------
 *  Public API: slot
 * ------------------------------------------------------------------------- */

/* A slot change concerns the layout of the parent. */
static void hui_slot_changed(hui_widget_t* child) {
	hui_widget_invalidate_layout(child->parent ? child->parent : child);
}

void HUI_SetSlotPadding(HUI_id id, float left, float top, float right, float bottom) {
	HUI_WIDGET_VOID(child, id, 0);
	child->slot.padding = hui_margin(left, top, right, bottom);
	hui_slot_changed(child);
}

void HUI_SetSlotHAlign(HUI_id id, HUI_EHAlign align) {
	HUI_WIDGET_VOID(child, id, 0);
	if (align < HUI_HALIGN_FILL || align > HUI_HALIGN_RIGHT) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetSlotHAlign: invalid alignment");
		return;
	}
	child->slot.halign = align;
	hui_slot_changed(child);
}

void HUI_SetSlotVAlign(HUI_id id, HUI_EVAlign align) {
	HUI_WIDGET_VOID(child, id, 0);
	if (align < HUI_VALIGN_FILL || align > HUI_VALIGN_BOTTOM) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetSlotVAlign: invalid alignment");
		return;
	}
	child->slot.valign = align;
	hui_slot_changed(child);
}

void HUI_SetSlotAuto(HUI_id id) {
	HUI_WIDGET_VOID(child, id, 0);
	child->slot.is_fill = HUI_FALSE;
	hui_slot_changed(child);
}

void HUI_SetSlotFill(HUI_id id, float coefficient) {
	HUI_WIDGET_VOID(child, id, 0);
	child->slot.is_fill = HUI_TRUE;
	child->slot.fill = coefficient > 0.0f ? coefficient : 0.0f;
	hui_slot_changed(child);
}

void HUI_SetSlotMaxSize(HUI_id id, float max_size) {
	HUI_WIDGET_VOID(child, id, 0);
	child->slot.max_size = max_size > 0.0f ? max_size : 0.0f;
	hui_slot_changed(child);
}

void HUI_SetSlotGrid(HUI_id id, int column, int row) {
	HUI_WIDGET_VOID(child, id, 0);
	child->slot.column = hui_clampi(column, 0, 4095);
	child->slot.row = hui_clampi(row, 0, 4095);
	hui_slot_changed(child);
}

void HUI_SetSlotGridSpan(HUI_id id, int column_span, int row_span) {
	HUI_WIDGET_VOID(child, id, 0);
	child->slot.column_span = hui_clampi(column_span, 1, 4096);
	child->slot.row_span = hui_clampi(row_span, 1, 4096);
	hui_slot_changed(child);
}

void HUI_SetSlotAnchors(HUI_id id, float min_x, float min_y, float max_x, float max_y) {
	HUI_WIDGET_VOID(child, id, 0);
	child->slot.anchors[0] = min_x;
	child->slot.anchors[1] = min_y;
	child->slot.anchors[2] = hui_maxf(min_x, max_x);
	child->slot.anchors[3] = hui_maxf(min_y, max_y);
	hui_slot_changed(child);
}

void HUI_SetSlotPosition(HUI_id id, float x, float y) {
	HUI_WIDGET_VOID(child, id, 0);
	child->slot.position[0] = x;
	child->slot.position[1] = y;
	hui_slot_changed(child);
}

void HUI_SetSlotSize(HUI_id id, float width, float height) {
	HUI_WIDGET_VOID(child, id, 0);
	child->slot.size[0] = width;
	child->slot.size[1] = height;
	hui_slot_changed(child);
}

void HUI_SetSlotAlignment(HUI_id id, float x, float y) {
	HUI_WIDGET_VOID(child, id, 0);
	child->slot.alignment[0] = x;
	child->slot.alignment[1] = y;
	hui_slot_changed(child);
}

void HUI_SetSlotTitle(HUI_id id, const char* title) {
	HUI_WIDGET_VOID(child, id, 0);
	if (hui_str_set(&child->slot.title, title)) hui_slot_changed(child);
}

/* ---------------------------------------------------------------------------
 *  Public API: common properties
 * ------------------------------------------------------------------------- */

void HUI_SetWidgetVisibility(HUI_id id, HUI_EVisibility visibility) {
	HUI_WIDGET_VOID(widget, id, 0);
	if (visibility < HUI_VISIBLE || visibility > HUI_COLLAPSED) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetWidgetVisibility: invalid visibility");
		return;
	}
	if (widget->visibility == visibility) return;
	widget->visibility = visibility;
	if (visibility != HUI_VISIBLE && widget->window) hui_window_forget_widget(widget->window, widget);
	hui_widget_invalidate_layout(widget->parent ? widget->parent : widget);
}

HUI_EVisibility HUI_GetWidgetVisibility(HUI_id id) {
	HUI_WIDGET(widget, id, 0, HUI_COLLAPSED);
	return widget->visibility;
}

void HUI_SetWidgetEnabled(HUI_id id, int enabled) {
	HUI_WIDGET_VOID(widget, id, 0);
	if (((widget->flags & HUI_WF_DISABLED) == 0) == (enabled != 0)) return;
	if (enabled) {
		widget->flags &= ~HUI_WF_DISABLED;
	} else {
		widget->flags |= HUI_WF_DISABLED;
		if (widget->window && widget->window->focus && hui_widget_is_inside(widget->window->focus, widget)) {
			hui_window_set_focus(widget->window, NULL);
		}
	}
	hui_widget_invalidate_paint(widget);
}

int HUI_IsWidgetEnabled(HUI_id id) {
	HUI_WIDGET(widget, id, 0, HUI_FALSE);
	return (widget->flags & HUI_WF_DISABLED) == 0;
}

void HUI_SetWidgetHitTestable(HUI_id id, int hit_testable) {
	HUI_WIDGET_VOID(widget, id, 0);
	if (hit_testable) widget->flags &= ~HUI_WF_NO_HIT;
	else widget->flags |= HUI_WF_NO_HIT;
}

void HUI_SetWidgetOpacity(HUI_id id, float opacity) {
	HUI_WIDGET_VOID(widget, id, 0);
	widget->opacity = hui_clampf(opacity, 0.0f, 1.0f);
	hui_widget_invalidate_paint(widget);
}

void HUI_SetWidgetClipping(HUI_id id, int clip) {
	HUI_WIDGET_VOID(widget, id, 0);
	if (clip) widget->flags |= HUI_WF_CLIP;
	else widget->flags &= ~HUI_WF_CLIP;
	hui_widget_invalidate_paint(widget);
}

void HUI_SetWidgetTooltip(HUI_id id, const char* text) {
	HUI_WIDGET_VOID(widget, id, 0);
	hui_str_set(&widget->tooltip, (text && text[0]) ? text : NULL);
}

void HUI_SetWidgetCursor(HUI_id id, HUI_ECursor cursor) {
	HUI_WIDGET_VOID(widget, id, 0);
	widget->cursor = cursor;
}

void HUI_SetWidgetSize(HUI_id id, float width, float height) {
	HUI_WIDGET_VOID(widget, id, 0);
	widget->fixed_size = hui_vec2(width, height);
	hui_widget_invalidate_layout(widget);
}

void HUI_SetWidgetMinSize(HUI_id id, float width, float height) {
	HUI_WIDGET_VOID(widget, id, 0);
	widget->min_size = hui_vec2(width, height);
	hui_widget_invalidate_layout(widget);
}

void HUI_SetWidgetMaxSize(HUI_id id, float width, float height) {
	HUI_WIDGET_VOID(widget, id, 0);
	widget->max_size = hui_vec2(width, height);
	hui_widget_invalidate_layout(widget);
}

void HUI_GetWidgetRect(HUI_id id, float* x, float* y, float* width, float* height) {
	hui_widget_t* widget;
	hui_vec2_t origin;
	if (x) *x = 0.0f;
	if (y) *y = 0.0f;
	if (width) *width = 0.0f;
	if (height) *height = 0.0f;
	widget = hui_widget_check(id, 0, __func__);
	if (!widget) return;
	/* answer with an up-to-date layout, even right after the tree was built */
	if (widget->window && widget->window->needs_layout) hui_window_layout(widget->window);
	origin = hui_widget_origin(widget);
	if (x) *x = origin.x;
	if (y) *y = origin.y;
	if (width) *width = widget->rect.w;
	if (height) *height = widget->rect.h;
}

void HUI_GetWidgetDesiredSize(HUI_id id, float* width, float* height) {
	hui_widget_t* widget;
	hui_vec2_t desired;
	if (width) *width = 0.0f;
	if (height) *height = 0.0f;
	widget = hui_widget_check(id, 0, __func__);
	if (!widget) return;
	desired = hui_widget_measure(widget);
	if (width) *width = desired.x;
	if (height) *height = desired.y;
}

int HUI_IsWidgetHovered(HUI_id id) {
	HUI_WIDGET(widget, id, 0, HUI_FALSE);
	return (widget->flags & HUI_WF_HOVERED) != 0;
}

void HUI_SetWidgetFocusable(HUI_id id, int focusable) {
	HUI_WIDGET_VOID(widget, id, 0);
	if (focusable) {
		widget->flags |= HUI_WF_FOCUSABLE;
	} else {
		widget->flags &= ~HUI_WF_FOCUSABLE;
		if (widget->window && widget->window->focus == widget) hui_window_set_focus(widget->window, NULL);
	}
}

void HUI_FocusWidget(HUI_id id) {
	HUI_WIDGET_VOID(widget, id, 0);
	hui_widget_focus(widget);
}

int HUI_IsWidgetFocused(HUI_id id) {
	HUI_WIDGET(widget, id, 0, HUI_FALSE);
	return (widget->flags & HUI_WF_FOCUSED) != 0;
}

void HUI_SetWidgetUserHandle(HUI_id id, void* handle) {
	HUI_WIDGET_VOID(widget, id, 0);
	widget->user_handle = handle;
}

void* HUI_GetWidgetUserHandle(HUI_id id) {
	HUI_WIDGET(widget, id, 0, NULL);
	return widget->user_handle;
}

void HUI_SetWidgetEventCallback(HUI_id id, HUI_CWidgetEvent callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, 0);
	widget->event_callback = callback;
	widget->event_user = user_data;
}

void HUI_SetWidgetAction(HUI_id id, const char* action) {
	HUI_WIDGET_VOID(widget, id, 0);
	hui_str_set(&widget->action, (action && action[0]) ? action : NULL);
}

const char* HUI_GetWidgetAction(HUI_id id) {
	HUI_WIDGET(widget, id, 0, "");
	return widget->action ? widget->action : "";
}

void HUI_SetWidgetContextMenu(HUI_id id, HUI_id menu) {
	HUI_WIDGET_VOID(widget, id, 0);
	if (menu != HUI_INVALID_ID && !hui_widget_check(menu, HUI_WIDGET_MENU, __func__)) return;
	widget->context_menu = menu;
}

void HUI_InvalidateWidget(HUI_id id) {
	HUI_WIDGET_VOID(widget, id, 0);
	hui_widget_invalidate_layout(widget);
}
