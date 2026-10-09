/**
 * HUI - widget/widget.h
 * Internal widget and window model shared by the widget core and by every
 * widget implementation in src/widgets/.
 *
 * A widget type is a hui_widget_class_t (a table of functions) plus a
 * structure whose first member is hui_widget_t. To add a widget type:
 *   1. add its value to HUI_EWidgetType,
 *   2. write src/widgets/<name>.c defining its structure and class,
 *   3. list the class in widget/registry.c.
 */

#ifndef HUI_WIDGET_INTERNAL_H
#define HUI_WIDGET_INTERNAL_H

#include "../core/context.h"
#include "../render/draw.h"
#include "../render/font.h"

typedef struct hui_widget_t hui_widget_t;
typedef struct hui_window_t hui_window_t;
struct hui_xml_node_t;

/* ---------------------------------------------------------------------------
 *  Flags
 * ------------------------------------------------------------------------- */

/* widget state (hui_widget_t::flags) */
#define HUI_WF_MEASURE_DIRTY (1u << 0)  /* desired size must be recomputed */
#define HUI_WF_ARRANGE_DIRTY (1u << 1)  /* children must be placed again */
#define HUI_WF_DISABLED      (1u << 2)
#define HUI_WF_NO_HIT        (1u << 3)  /* the widget itself ignores the mouse (children do not) */
#define HUI_WF_CLIP          (1u << 4)  /* children are clipped to the bounds */
#define HUI_WF_FOCUSABLE     (1u << 5)
#define HUI_WF_HOVERED       (1u << 6)  /* the cursor is over the widget or a descendant */
#define HUI_WF_PRESSED       (1u << 7)  /* set by widgets while a click is in progress */
#define HUI_WF_FOCUSED       (1u << 8)
#define HUI_WF_DEAD          (1u << 9)  /* deleted, memory released at the end of the dispatch */
#define HUI_WF_POPUP         (1u << 10) /* shown as a popup: rect is in window coordinates */
#define HUI_WF_INTERNAL      (1u << 11) /* owned by another widget without being its child */
#define HUI_WF_LAYOUT_DIRTY  (HUI_WF_MEASURE_DIRTY | HUI_WF_ARRANGE_DIRTY)

/* class traits (hui_widget_class_t::flags) */
#define HUI_CF_SINGLE_CHILD (1u << 0)   /* adding a child replaces the previous one */
#define HUI_CF_NO_CHILDREN  (1u << 1)
#define HUI_CF_OVERFLOW     (1u << 2)   /* children may lie outside the bounds (no culling by bounds) */
#define HUI_CF_UI_PAINT     (1u << 3)   /* paint calls application code: UI thread only */
#define HUI_CF_NO_HIT       (1u << 4)   /* instances start with HUI_WF_NO_HIT */
#define HUI_CF_FOCUSABLE    (1u << 5)   /* instances start with HUI_WF_FOCUSABLE */
#define HUI_CF_CLIP         (1u << 6)   /* instances start with HUI_WF_CLIP */

/* ---------------------------------------------------------------------------
 *  Widget
 * ------------------------------------------------------------------------- */

/** How a child is placed by its parent (see HUI_SetSlot... in hui_widget.h). */
typedef struct hui_slot_t {
	hui_margin_t padding;
	HUI_EHAlign halign;
	HUI_EVAlign valign;
	/** HUI_TRUE = fill with coefficient `fill`, HUI_FALSE = auto. */
	int is_fill;
	float fill;
	float max_size;
	int column, row;
	int column_span, row_span;
	/** Canvas: min x, min y, max x, max y. */
	float anchors[4];
	float position[2];
	float size[2];
	float alignment[2];
	char* title;
} hui_slot_t;

/** Per-widget overrides of the theme, allocated on first use. */
typedef struct hui_style_t {
	uint32_t color_mask;
	uint32_t metric_mask;
	hui_color_t colors[HUI_COLOR_COUNT];
	float metrics[HUI_METRIC_COUNT];
} hui_style_t;

typedef struct hui_widget_class_t {
	HUI_EWidgetType type;
	/** Tag of the widget in XML. */
	const char* name;
	/** Size of the instance structure (hui_widget_t is its first member). */
	size_t size;
	uint32_t flags;

	void (*init)(hui_widget_t* widget);
	void (*destroy)(hui_widget_t* widget);

	/**
	 * Returns the desired size of the content, measuring children with
	 * hui_widget_measure. NULL: the largest child (slot padding included).
	 */
	hui_vec2_t (*measure)(hui_widget_t* widget);
	/**
	 * Places the children inside widget->rect.w x widget->rect.h with
	 * hui_widget_arrange. NULL: every child fills the widget (slot applied).
	 */
	void (*arrange)(hui_widget_t* widget);
	/** Draws the widget itself, below its children. */
	void (*paint)(hui_widget_t* widget, HUI_Painter* painter);
	/** Draws above the children (scrollbars, focus outline). */
	void (*paint_overlay)(hui_widget_t* widget, HUI_Painter* painter);
	/** Handles an input event. Returns HUI_TRUE to stop the bubbling. */
	int  (*event)(hui_widget_t* widget, const HUI_Event* event);
	/** Sets a property from text (XML attribute). Returns HUI_FALSE when the name is unknown. */
	int  (*set_attribute)(hui_widget_t* widget, const char* name, const char* value);
	/** Tells whether a child takes part in painting and hit testing. NULL: all do. */
	int  (*child_active)(hui_widget_t* widget, hui_widget_t* child);
	/** Called after a child was added or removed. */
	void (*children_changed)(hui_widget_t* widget);
	/** Handles an XML child element that is not a widget (<Item>, ...). Returns HUI_FALSE when unknown. */
	int  (*xml_child)(hui_widget_t* widget, const struct hui_xml_node_t* node);
	/** Called when the widget, shown as a popup, has been closed. */
	void (*popup_closed)(hui_widget_t* widget);
} hui_widget_class_t;

struct hui_widget_t {
	HUI_id id;
	const hui_widget_class_t* cls;
	hui_widget_t* parent;
	/** Window showing the widget, NULL while detached. */
	hui_window_t* window;
	hui_widget_t** children;
	uint32_t child_count;
	uint32_t child_cap;

	uint32_t flags;
	HUI_EVisibility visibility;
	/** Bounds relative to the parent (to the window for popups and roots), logical units. */
	hui_rect_t rect;
	/** Desired size computed by the last measure pass. */
	hui_vec2_t desired;
	/** Constraints on the desired size; a negative value means "none". */
	hui_vec2_t fixed_size;
	hui_vec2_t min_size;
	hui_vec2_t max_size;
	hui_slot_t slot;
	float opacity;
	hui_style_t* style;

	char* name;
	char* tooltip;
	char* action;
	void* user_handle;
	HUI_CWidgetEvent event_callback;
	void* event_user;
	/** 0 = inherit from the parent. */
	HUI_ECursor cursor;
	HUI_id context_menu;

	/** Number of widgets in the subtree, this one included. */
	uint32_t subtree_count;
	/** Number of widgets in the subtree whose paint must run on the UI thread. */
	uint32_t ui_paint_count;
};

/* ---------------------------------------------------------------------------
 *  Registry (widget/registry.c)
 * ------------------------------------------------------------------------- */

const hui_widget_class_t* hui_widget_class_by_type(HUI_EWidgetType type);
const hui_widget_class_t* hui_widget_class_by_name(const char* name);

/* ---------------------------------------------------------------------------
 *  Tree (widget/widget.c)
 * ------------------------------------------------------------------------- */

/** Resolves a handle without reporting errors. */
hui_widget_t* hui_widget_get(HUI_id id);
/**
 * Resolves a handle for a public function: checks the library state, the
 * calling thread, the handle and, when `type` is not 0, the widget type.
 * Reports an error naming `function` and returns NULL on failure.
 */
hui_widget_t* hui_widget_check(HUI_id id, HUI_EWidgetType type, const char* function);

#define HUI_WIDGET(variable, id, type, result) \
	hui_widget_t* variable = hui_widget_check((id), (type), __func__); \
	if (!variable) return result

#define HUI_WIDGET_VOID(variable, id, type) \
	hui_widget_t* variable = hui_widget_check((id), (type), __func__); \
	if (!variable) return

hui_widget_t* hui_widget_create(HUI_EWidgetType type);
/** Creates a widget of a class that is not in the registry (internal popups). */
hui_widget_t* hui_widget_create_class(const hui_widget_class_t* cls);
/** Detaches and destroys a widget and its subtree. */
void hui_widget_destroy(hui_widget_t* widget);
/** @param index Position among the children, or -1 to append. */
void hui_widget_add_child(hui_widget_t* parent, hui_widget_t* child, int index);
/** Removes the widget from its parent without destroying it. */
void hui_widget_detach(hui_widget_t* widget);
int  hui_widget_child_index(const hui_widget_t* parent, const hui_widget_t* child);
/** Sets the window of a whole subtree (used for roots and popups). */
void hui_widget_set_window(hui_widget_t* widget, hui_window_t* window);
/** HUI_TRUE when the widget and all its ancestors are visible and active in their parent. */
int  hui_widget_is_shown(const hui_widget_t* widget);
/** HUI_TRUE when the widget and all its ancestors are enabled. */
int  hui_widget_is_enabled(const hui_widget_t* widget);
/** HUI_TRUE when `ancestor` is `widget` or one of its ancestors. */
int  hui_widget_is_inside(const hui_widget_t* widget, const hui_widget_t* ancestor);
/** Top-left corner of the widget in window coordinates. */
hui_vec2_t hui_widget_origin(const hui_widget_t* widget);
/** Releases the memory of the widgets deleted during the event dispatch. */
void hui_widgets_collect(void);

/* ---------------------------------------------------------------------------
 *  Layout (widget/layout.c)
 * ------------------------------------------------------------------------- */

/** The widget needs a new measure + arrange + paint. */
void hui_widget_invalidate_layout(hui_widget_t* widget);
/** The widget only needs to be painted again. */
void hui_widget_invalidate_paint(hui_widget_t* widget);

/** Desired size of a widget (cached). (0, 0) when collapsed. */
hui_vec2_t hui_widget_measure(hui_widget_t* widget);
/** Desired size of a child plus its slot padding. */
hui_vec2_t hui_widget_measure_slot(hui_widget_t* child);
/** Gives a widget its final bounds, relative to its parent, and arranges its children. */
void hui_widget_arrange(hui_widget_t* widget, float x, float y, float width, float height);
/** Places a child inside `area` according to its slot padding and alignment. */
void hui_widget_arrange_slot(hui_widget_t* child, hui_rect_t area);
HUI_INLINE int hui_widget_collapsed(const hui_widget_t* widget) { return widget->visibility == HUI_COLLAPSED; }

/* ---------------------------------------------------------------------------
 *  Style (widget/style.c)
 * ------------------------------------------------------------------------- */

hui_color_t hui_widget_color(const hui_widget_t* widget, HUI_EColor role);
float hui_widget_metric(const hui_widget_t* widget, HUI_EMetric metric);
int   hui_widget_has_color(const hui_widget_t* widget, HUI_EColor role);
void  hui_widget_set_color(hui_widget_t* widget, HUI_EColor role, hui_color_t color);
void  hui_widget_set_metric(hui_widget_t* widget, HUI_EMetric metric, float value);
/** Color of the text of a widget: its override, else the inherited color, dimmed when disabled. */
hui_color_t hui_widget_text_color(const hui_widget_t* widget, const HUI_Painter* painter);
void  hui_theme_apply(HUI_ETheme theme);
void  hui_theme_reset_metrics(void);

/* ---------------------------------------------------------------------------
 *  Painting and hit testing (widget/paint.c)
 * ------------------------------------------------------------------------- */

/** Paints a widget and its subtree. `parent` describes the space of the parent. */
void hui_widget_paint(hui_widget_t* widget, const HUI_Painter* parent);
/** Deepest widget under a point given in the coordinate space of the parent of `widget`. */
hui_widget_t* hui_widget_hit_test(hui_widget_t* widget, float x, float y);
void hui_paint_init(void);
void hui_paint_shutdown(void);
/** Number of paint jobs started since the last call (statistics). */
int  hui_paint_take_job_count(void);

/* ---------------------------------------------------------------------------
 *  Events (widget/events.c)
 * ------------------------------------------------------------------------- */

/** Sends an event to a widget and bubbles it to the ancestors. Returns HUI_TRUE when handled. */
int  hui_widget_dispatch(hui_widget_t* target, HUI_Event* event);
/** Sends an event to one widget only. */
int  hui_widget_send(hui_widget_t* target, HUI_Event* event);
/** Fires the action of the widget (HUI_SetWidgetAction). */
void hui_widget_fire_action(hui_widget_t* widget);
/** Sends all further mouse events to the widget until the capture is released. */
void hui_widget_capture_mouse(hui_widget_t* widget);
void hui_widget_release_mouse(hui_widget_t* widget);
void hui_widget_focus(hui_widget_t* widget);

/* ---------------------------------------------------------------------------
 *  Window (widget/window.c)
 * ------------------------------------------------------------------------- */

typedef enum hui_popup_placement_t {
	/** Below the anchor, left edges aligned; above when there is no room. */
	HUI_POPUP_BELOW,
	/** Right of the anchor, top edges aligned; left when there is no room. */
	HUI_POPUP_RIGHT,
	/** Top-left corner on the anchor position. */
	HUI_POPUP_AT
} hui_popup_placement_t;

typedef struct hui_popup_t {
	hui_widget_t* root;
	/** Widget that opened the popup; receives the focus back. May be NULL. */
	hui_widget_t* owner;
	hui_rect_t anchor;
	hui_popup_placement_t placement;
	/** Minimum width (combo box lists match their button). */
	float min_width;
} hui_popup_t;

typedef struct hui_shortcut_t {
	HUI_EKey key;
	uint32_t modifiers;
	char* action;
} hui_shortcut_t;

struct hui_window_t {
	HUI_id id;
	hui_pwindow_t* platform;
	hui_widget_t* root;
	char* title;

	/** Size of the client area in pixels and pixels per logical unit. */
	int width_px, height_px;
	float scale;
	int visible;
	int surface_ready;

	int needs_layout;
	int needs_paint;
	int continuous;
	/** Set from any thread by HUI_RequestWindowRedraw. */
	hui_atomic_t redraw_requested;
	/** Frames queued or being rendered. */
	hui_atomic_t frames_in_flight;
	hui_atomic_t render_us;
	/** Realtime viewports seen during the last paint. */
	hui_atomic_t realtime_viewports;

	int has_clear_color;
	float clear_color[4];

	/* input state */
	hui_widget_t* hover;
	hui_widget_t* focus;
	hui_widget_t* capture;
	/** HUI_TRUE when the focus was moved with the keyboard: widgets then draw a focus outline. */
	int focus_visible;
	float mouse_x, mouse_y;
	int mouse_inside;
	uint32_t modifiers;
	uint32_t buttons;
	double click_time;
	float click_x, click_y;
	int click_count;
	int click_button;
	HUI_ECursor cursor;

	/* popups, bottom to top */
	hui_popup_t* popups;
	uint32_t popup_count;
	uint32_t popup_cap;

	/* tooltip */
	hui_widget_t* tooltip_widget;
	HUI_id tooltip_timer;
	int tooltip_visible;
	float tooltip_x, tooltip_y;

	hui_shortcut_t* shortcuts;
	uint32_t shortcut_count;
	uint32_t shortcut_cap;

	/* viewports of this window that own a native child window */
	hui_widget_t** native_viewports;
	uint32_t native_viewport_count;
	uint32_t native_viewport_cap;

	HUI_CWindowClose close_callback;
	void* close_user;
	HUI_CWindowResize resize_callback;
	void* resize_user;
	void* user_handle;

	HUI_FrameStats stats;
	uint64_t frame_index;
	int dead;
};

hui_window_t* hui_window_get(HUI_id id);
hui_window_t* hui_window_check(HUI_id id, const char* function);
/** Entry point of the platform events. */
void hui_window_on_platform_event(const hui_pevent_t* event);
/** Lays out and paints the window when something changed. */
void hui_window_update(hui_window_t* window);
/** Runs the layout passes now if the window needs them. */
void hui_window_layout(hui_window_t* window);
/** HUI_TRUE when the window will produce a frame at the next update. */
int  hui_window_wants_frame(hui_window_t* window);
/** Clears every reference the window holds to a widget about to be destroyed. */
void hui_window_forget_widget(hui_window_t* window, hui_widget_t* widget);
void hui_window_set_focus(hui_window_t* window, hui_widget_t* widget);
void hui_window_destroy(hui_window_t* window);
void hui_windows_destroy_all(void);
void hui_windows_collect(void);
/** Logical size of the client area. */
hui_vec2_t hui_window_size(const hui_window_t* window);
void hui_window_add_shortcut(hui_window_t* window, HUI_EKey key, uint32_t modifiers, const char* action);
void hui_window_remove_shortcut(hui_window_t* window, HUI_EKey key, uint32_t modifiers);

/** Shows a widget tree above the window content. The popup does not own `root`. */
void hui_window_open_popup(hui_window_t* window, hui_widget_t* root, hui_widget_t* owner,
	hui_rect_t anchor, hui_popup_placement_t placement, float min_width);
/** Closes a popup and all the popups opened above it. */
void hui_window_close_popup(hui_window_t* window, hui_widget_t* root);
void hui_window_close_popups(hui_window_t* window);
int  hui_window_popup_index(const hui_window_t* window, const hui_widget_t* root);

/** Triggers a menu item (shortcuts). Implemented in widgets/menu.c. */
void hui_menu_item_trigger(hui_widget_t* item);
/** Destroys the native child windows of the viewports inside a subtree leaving the window. */
void hui_window_release_natives(hui_window_t* window, hui_widget_t* subtree);
/** Native viewports (widgets/viewport.c): release the child window / apply its state after a paint. */
void hui_viewport_native_release(hui_widget_t* widget, hui_window_t* window);
void hui_viewport_native_sync(hui_widget_t* widget, uint64_t frame);

/** Enabled menu item bound to a shortcut anywhere below `root`, or NULL. Implemented in widgets/menu.c. */
hui_widget_t* hui_menu_find_shortcut(hui_widget_t* root, HUI_EKey key, uint32_t modifiers);
/** Parses "Ctrl+Shift+S". Implemented in widgets/menu.c. */
int hui_parse_shortcut(const char* text, HUI_EKey* key, uint32_t* modifiers);
/** Opens a menu as a popup at a window position. Implemented in widgets/menu.c. */
void hui_menu_popup(hui_widget_t* menu, hui_window_t* window, float x, float y);

/* ---------------------------------------------------------------------------
 *  Attribute parsing (xml/xml_attributes.c), shared by all set_attribute functions
 * ------------------------------------------------------------------------- */

int hui_parse_float(const char* text, float* value);
int hui_parse_int(const char* text, int* value);
int hui_parse_bool(const char* text, int* value);
/** "#RGB", "#RRGGBB", "#RRGGBBAA" or a few color names. */
int hui_parse_color(const char* text, hui_color_t* color);
/** "all", "horizontal,vertical" or "left,top,right,bottom". */
int hui_parse_margin(const char* text, hui_margin_t* margin);
/** Up to `max` comma-separated numbers; returns how many were read. */
int hui_parse_floats(const char* text, float* values, int max);
int hui_parse_orientation(const char* text, HUI_EOrientation* orientation);
/** Sets a base property (name, visibility, slot, size, colors...). */
int hui_widget_set_base_attribute(hui_widget_t* widget, const char* name, const char* value);
/** Sets any attribute: the ones of the widget class first, then the base ones. */
int hui_widget_set_attribute(hui_widget_t* widget, const char* name, const char* value);

#endif /* HUI_WIDGET_INTERNAL_H */
