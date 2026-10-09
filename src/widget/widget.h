/**
 * Raven - widget/widget.h
 * Internal widget and window model shared by the widget core and by every
 * widget implementation in src/widgets/.
 *
 * A widget type is a rvn_widget_class_t (a table of functions) plus a
 * structure whose first member is rvn_widget_t. To add a widget type:
 *   1. add its value to RWidgetType,
 *   2. write src/widgets/<name>.c defining its structure and class,
 *   3. list the class in widget/registry.c.
 */

#ifndef RVN_WIDGET_INTERNAL_H
#define RVN_WIDGET_INTERNAL_H

#include "../core/context.h"
#include "../render/draw.h"
#include "../render/font.h"

typedef struct rvn_widget_t rvn_widget_t;
typedef struct rvn_window_t rvn_window_t;
struct rvn_xml_node_t;

/* ---------------------------------------------------------------------------
 *  Flags
 * ------------------------------------------------------------------------- */

/* widget state (rvn_widget_t::flags) */
#define RVN_WF_MEASURE_DIRTY (1u << 0)  /* desired size must be recomputed */
#define RVN_WF_ARRANGE_DIRTY (1u << 1)  /* children must be placed again */
#define RVN_WF_DISABLED      (1u << 2)
#define RVN_WF_NO_HIT        (1u << 3)  /* the widget itself ignores the mouse (children do not) */
#define RVN_WF_CLIP          (1u << 4)  /* children are clipped to the bounds */
#define RVN_WF_FOCUSABLE     (1u << 5)
#define RVN_WF_HOVERED       (1u << 6)  /* the cursor is over the widget or a descendant */
#define RVN_WF_PRESSED       (1u << 7)  /* set by widgets while a click is in progress */
#define RVN_WF_FOCUSED       (1u << 8)
#define RVN_WF_DEAD          (1u << 9)  /* deleted, memory released at the end of the dispatch */
#define RVN_WF_POPUP         (1u << 10) /* shown as a popup: rect is in window coordinates */
#define RVN_WF_INTERNAL      (1u << 11) /* owned by another widget without being its child */
#define RVN_WF_LAYOUT_DIRTY  (RVN_WF_MEASURE_DIRTY | RVN_WF_ARRANGE_DIRTY)

/* class traits (rvn_widget_class_t::flags) */
#define RVN_CF_SINGLE_CHILD (1u << 0)   /* adding a child replaces the previous one */
#define RVN_CF_NO_CHILDREN  (1u << 1)
#define RVN_CF_OVERFLOW     (1u << 2)   /* children may lie outside the bounds (no culling by bounds) */
#define RVN_CF_UI_PAINT     (1u << 3)   /* paint calls application code: UI thread only */
#define RVN_CF_NO_HIT       (1u << 4)   /* instances start with RVN_WF_NO_HIT */
#define RVN_CF_FOCUSABLE    (1u << 5)   /* instances start with RVN_WF_FOCUSABLE */
#define RVN_CF_CLIP         (1u << 6)   /* instances start with RVN_WF_CLIP */

/* ---------------------------------------------------------------------------
 *  Widget
 * ------------------------------------------------------------------------- */

/** How a child is placed by its parent (see Rslot_set_... in raven_widget.h). */
typedef struct rvn_slot_t {
	rvn_margin_t padding;
	RHAlign halign;
	RVAlign valign;
	/** R_TRUE = fill with coefficient `fill`, R_FALSE = auto. */
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
} rvn_slot_t;

/** Per-widget overrides of the theme, allocated on first use. */
typedef struct rvn_style_t {
	uint32_t color_mask;
	uint32_t metric_mask;
	rvn_color_t colors[R_COLOR_COUNT];
	float metrics[R_METRIC_COUNT];
} rvn_style_t;

typedef struct rvn_widget_class_t {
	RWidgetType type;
	/** Tag of the widget in XML. */
	const char* name;
	/** Size of the instance structure (rvn_widget_t is its first member). */
	size_t size;
	uint32_t flags;

	void (*init)(rvn_widget_t* widget);
	void (*destroy)(rvn_widget_t* widget);

	/**
	 * Returns the desired size of the content, measuring children with
	 * rvn_widget_measure. NULL: the largest child (slot padding included).
	 */
	rvn_vec2_t (*measure)(rvn_widget_t* widget);
	/**
	 * Places the children inside widget->rect.w x widget->rect.h with
	 * rvn_widget_arrange. NULL: every child fills the widget (slot applied).
	 */
	void (*arrange)(rvn_widget_t* widget);
	/** Draws the widget itself, below its children. */
	void (*paint)(rvn_widget_t* widget, RPainter* painter);
	/** Draws above the children (scrollbars, focus outline). */
	void (*paint_overlay)(rvn_widget_t* widget, RPainter* painter);
	/** Handles an input event. Returns R_TRUE to stop the bubbling. */
	int  (*event)(rvn_widget_t* widget, const REvent* event);
	/** Sets a property from text (XML attribute). Returns R_FALSE when the name is unknown. */
	int  (*set_attribute)(rvn_widget_t* widget, const char* name, const char* value);
	/** Tells whether a child takes part in painting and hit testing. NULL: all do. */
	int  (*child_active)(rvn_widget_t* widget, rvn_widget_t* child);
	/** Called after a child was added or removed. */
	void (*children_changed)(rvn_widget_t* widget);
	/** Handles an XML child element that is not a widget (<Item>, ...). Returns R_FALSE when unknown. */
	int  (*xml_child)(rvn_widget_t* widget, const struct rvn_xml_node_t* node);
	/** Called when the widget, shown as a popup, has been closed. */
	void (*popup_closed)(rvn_widget_t* widget);
} rvn_widget_class_t;

struct rvn_widget_t {
	Rid id;
	const rvn_widget_class_t* cls;
	rvn_widget_t* parent;
	/** Window showing the widget, NULL while detached. */
	rvn_window_t* window;
	rvn_widget_t** children;
	uint32_t child_count;
	uint32_t child_cap;

	uint32_t flags;
	RVisibility visibility;
	/** Bounds relative to the parent (to the window for popups and roots), logical units. */
	rvn_rect_t rect;
	/** Desired size computed by the last measure pass. */
	rvn_vec2_t desired;
	/** Constraints on the desired size; a negative value means "none". */
	rvn_vec2_t fixed_size;
	rvn_vec2_t min_size;
	rvn_vec2_t max_size;
	rvn_slot_t slot;
	float opacity;
	rvn_style_t* style;

	char* name;
	char* tooltip;
	char* action;
	void* user_handle;
	RWidgetEventCallback event_callback;
	void* event_user;
	/** 0 = inherit from the parent. */
	RCursor cursor;
	Rid context_menu;

	/** State shown by the glass themes, eased while painting: [0] hover (or the on / off state), [1] pressed. */
	float anim[2];

	/** Number of widgets in the subtree, this one included. */
	uint32_t subtree_count;
	/** Number of widgets in the subtree whose paint must run on the UI thread. */
	uint32_t ui_paint_count;
};

/* ---------------------------------------------------------------------------
 *  Registry (widget/registry.c)
 * ------------------------------------------------------------------------- */

const rvn_widget_class_t* rvn_widget_class_by_type(RWidgetType type);
const rvn_widget_class_t* rvn_widget_class_by_name(const char* name);

/* ---------------------------------------------------------------------------
 *  Tree (widget/widget.c)
 * ------------------------------------------------------------------------- */

/** Resolves a handle without reporting errors. */
rvn_widget_t* rvn_widget_get(Rid id);
/**
 * Resolves a handle for a public function: checks the library state, the
 * calling thread, the handle and, when `type` is not 0, the widget type.
 * Reports an error naming `function` and returns NULL on failure.
 */
rvn_widget_t* rvn_widget_check(Rid id, RWidgetType type, const char* function);

#define RVN_WIDGET(variable, id, type, result) \
	rvn_widget_t* variable = rvn_widget_check((id), (type), __func__); \
	if (!variable) return result

#define RVN_WIDGET_VOID(variable, id, type) \
	rvn_widget_t* variable = rvn_widget_check((id), (type), __func__); \
	if (!variable) return

rvn_widget_t* rvn_widget_create(RWidgetType type);
/** Creates a widget of a class that is not in the registry (internal popups). */
rvn_widget_t* rvn_widget_create_class(const rvn_widget_class_t* cls);
/** Detaches and destroys a widget and its subtree. */
void rvn_widget_destroy(rvn_widget_t* widget);
/** @param index Position among the children, or -1 to append. */
void rvn_widget_add_child(rvn_widget_t* parent, rvn_widget_t* child, int index);
/** Removes the widget from its parent without destroying it. */
void rvn_widget_detach(rvn_widget_t* widget);
int  rvn_widget_child_index(const rvn_widget_t* parent, const rvn_widget_t* child);
/** Sets the window of a whole subtree (used for roots and popups). */
void rvn_widget_set_window(rvn_widget_t* widget, rvn_window_t* window);
/** R_TRUE when the widget and all its ancestors are visible and active in their parent. */
int  rvn_widget_is_shown(const rvn_widget_t* widget);
/** R_TRUE when the widget and all its ancestors are enabled. */
int  rvn_widget_is_enabled(const rvn_widget_t* widget);
/** R_TRUE when `ancestor` is `widget` or one of its ancestors. */
int  rvn_widget_is_inside(const rvn_widget_t* widget, const rvn_widget_t* ancestor);
/** Top-left corner of the widget in window coordinates. */
rvn_vec2_t rvn_widget_origin(const rvn_widget_t* widget);
/** Releases the memory of the widgets deleted during the event dispatch. */
void rvn_widgets_collect(void);

/* ---------------------------------------------------------------------------
 *  Layout (widget/layout.c)
 * ------------------------------------------------------------------------- */

/** The widget needs a new measure + arrange + paint. */
void rvn_widget_invalidate_layout(rvn_widget_t* widget);
/** The widget only needs to be painted again. */
void rvn_widget_invalidate_paint(rvn_widget_t* widget);

/** Desired size of a widget (cached). (0, 0) when collapsed. */
rvn_vec2_t rvn_widget_measure(rvn_widget_t* widget);
/** Desired size of a child plus its slot padding. */
rvn_vec2_t rvn_widget_measure_slot(rvn_widget_t* child);
/** Gives a widget its final bounds, relative to its parent, and arranges its children. */
void rvn_widget_arrange(rvn_widget_t* widget, float x, float y, float width, float height);
/** Places a child inside `area` according to its slot padding and alignment. */
void rvn_widget_arrange_slot(rvn_widget_t* child, rvn_rect_t area);
RVN_INLINE int rvn_widget_collapsed(const rvn_widget_t* widget) { return widget->visibility == R_COLLAPSED; }

/* ---------------------------------------------------------------------------
 *  Style (widget/style.c)
 * ------------------------------------------------------------------------- */

rvn_color_t rvn_widget_color(const rvn_widget_t* widget, RColorRole role);
float rvn_widget_metric(const rvn_widget_t* widget, RMetric metric);
int   rvn_widget_has_color(const rvn_widget_t* widget, RColorRole role);
void  rvn_widget_set_color(rvn_widget_t* widget, RColorRole role, rvn_color_t color);
void  rvn_widget_set_metric(rvn_widget_t* widget, RMetric metric, float value);
/** Color of the text of a widget: its override, else the inherited color, dimmed when disabled. */
rvn_color_t rvn_widget_text_color(const rvn_widget_t* widget, const RPainter* painter);
/** Sets the colors, the metrics and the material of a built-in theme. */
void  rvn_theme_apply(RTheme theme);

/* ---------------------------------------------------------------------------
 *  Painting and hit testing (widget/paint.c)
 * ------------------------------------------------------------------------- */

/** Paints a widget and its subtree. `parent` describes the space of the parent. */
void rvn_widget_paint(rvn_widget_t* widget, const RPainter* parent);
/** Deepest widget under a point given in the coordinate space of the parent of `widget`. */
rvn_widget_t* rvn_widget_hit_test(rvn_widget_t* widget, float x, float y);
void rvn_paint_init(void);
void rvn_paint_shutdown(void);
/** Number of paint jobs started since the last call (statistics). */
int  rvn_paint_take_job_count(void);

/* ---------------------------------------------------------------------------
 *  Events (widget/events.c)
 * ------------------------------------------------------------------------- */

/** Sends an event to a widget and bubbles it to the ancestors. Returns R_TRUE when handled. */
int  rvn_widget_dispatch(rvn_widget_t* target, REvent* event);
/** Sends an event to one widget only. */
int  rvn_widget_send(rvn_widget_t* target, REvent* event);
/** Fires the action of the widget (Rwidget_set_action). */
void rvn_widget_fire_action(rvn_widget_t* widget);
/** Sends all further mouse events to the widget until the capture is released. */
void rvn_widget_capture_mouse(rvn_widget_t* widget);
void rvn_widget_release_mouse(rvn_widget_t* widget);
void rvn_widget_focus(rvn_widget_t* widget);

/* ---------------------------------------------------------------------------
 *  Window (widget/window.c)
 * ------------------------------------------------------------------------- */

typedef enum rvn_popup_placement_t {
	/** Below the anchor, left edges aligned; above when there is no room. */
	RVN_POPUP_BELOW,
	/** Right of the anchor, top edges aligned; left when there is no room. */
	RVN_POPUP_RIGHT,
	/** Top-left corner on the anchor position. */
	RVN_POPUP_AT
} rvn_popup_placement_t;

typedef struct rvn_popup_t {
	rvn_widget_t* root;
	/** Widget that opened the popup; receives the focus back. May be NULL. */
	rvn_widget_t* owner;
	rvn_rect_t anchor;
	rvn_popup_placement_t placement;
	/** Minimum width (combo box lists match their button). */
	float min_width;
} rvn_popup_t;

typedef struct rvn_shortcut_t {
	RKey key;
	uint32_t modifiers;
	char* action;
} rvn_shortcut_t;

struct rvn_window_t {
	Rid id;
	rvn_pwindow_t* platform;
	rvn_widget_t* root;
	char* title;

	/** Size of the client area in pixels and pixels per logical unit. */
	int width_px, height_px;
	float scale;
	int visible;
	int surface_ready;

	int needs_layout;
	int needs_paint;
	int continuous;
	/** Set from any thread by Rwindow_request_redraw. */
	rvn_atomic_t redraw_requested;
	/** Frames queued or being rendered. */
	rvn_atomic_t frames_in_flight;
	rvn_atomic_t render_us;
	/** Realtime viewports seen during the last paint. */
	rvn_atomic_t realtime_viewports;
	/** Set while painting by widgets whose animation is not finished: another frame is needed. */
	rvn_atomic_t animating;
	/** When the window was last painted, for the pace of the animations. */
	double paint_time;
	/** Image drawn behind the content (Rwindow_set_background), or R_INVALID_ID. */
	Rid background;

	int has_clear_color;
	float clear_color[4];

	/* input state */
	rvn_widget_t* hover;
	rvn_widget_t* focus;
	rvn_widget_t* capture;
	/** R_TRUE when the focus was moved with the keyboard: widgets then draw a focus outline. */
	int focus_visible;
	float mouse_x, mouse_y;
	int mouse_inside;
	uint32_t modifiers;
	uint32_t buttons;
	double click_time;
	float click_x, click_y;
	int click_count;
	int click_button;
	RCursor cursor;

	/* popups, bottom to top */
	rvn_popup_t* popups;
	uint32_t popup_count;
	uint32_t popup_cap;

	/* tooltip */
	rvn_widget_t* tooltip_widget;
	Rid tooltip_timer;
	int tooltip_visible;
	float tooltip_x, tooltip_y;

	rvn_shortcut_t* shortcuts;
	uint32_t shortcut_count;
	uint32_t shortcut_cap;

	/* viewports of this window that own a native child window */
	rvn_widget_t** native_viewports;
	uint32_t native_viewport_count;
	uint32_t native_viewport_cap;

	RWindowCloseCallback close_callback;
	void* close_user;
	RWindowResizeCallback resize_callback;
	void* resize_user;
	void* user_handle;

	RFrameStats stats;
	uint64_t frame_index;
	int dead;
};

rvn_window_t* rvn_window_get(Rid id);
rvn_window_t* rvn_window_check(Rid id, const char* function);
/** Entry point of the platform events. */
void rvn_window_on_platform_event(const rvn_pevent_t* event);
/** Lays out and paints the window when something changed. */
void rvn_window_update(rvn_window_t* window);
/** Runs the layout passes now if the window needs them. */
void rvn_window_layout(rvn_window_t* window);
/** R_TRUE when the window will produce a frame at the next update. */
int  rvn_window_wants_frame(rvn_window_t* window);
/** Clears every reference the window holds to a widget about to be destroyed. */
void rvn_window_forget_widget(rvn_window_t* window, rvn_widget_t* widget);
void rvn_window_set_focus(rvn_window_t* window, rvn_widget_t* widget);
void rvn_window_destroy(rvn_window_t* window);
void rvn_windows_destroy_all(void);
void rvn_windows_collect(void);
/** Logical size of the client area. */
rvn_vec2_t rvn_window_size(const rvn_window_t* window);
void rvn_window_add_shortcut(rvn_window_t* window, RKey key, uint32_t modifiers, const char* action);
void rvn_window_remove_shortcut(rvn_window_t* window, RKey key, uint32_t modifiers);

/** Shows a widget tree above the window content. The popup does not own `root`. */
void rvn_window_open_popup(rvn_window_t* window, rvn_widget_t* root, rvn_widget_t* owner,
	rvn_rect_t anchor, rvn_popup_placement_t placement, float min_width);
/** Closes a popup and all the popups opened above it. */
void rvn_window_close_popup(rvn_window_t* window, rvn_widget_t* root);
void rvn_window_close_popups(rvn_window_t* window);
int  rvn_window_popup_index(const rvn_window_t* window, const rvn_widget_t* root);

/** Triggers a menu item (shortcuts). Implemented in widgets/menu.c. */
void rvn_menu_item_trigger(rvn_widget_t* item);
/** Destroys the native child windows of the viewports inside a subtree leaving the window. */
void rvn_window_release_natives(rvn_window_t* window, rvn_widget_t* subtree);
/** Native viewports (widgets/viewport.c): release the child window / apply its state after a paint. */
void rvn_viewport_native_release(rvn_widget_t* widget, rvn_window_t* window);
void rvn_viewport_native_sync(rvn_widget_t* widget, uint64_t frame);

/** Enabled menu item bound to a shortcut anywhere below `root`, or NULL. Implemented in widgets/menu.c. */
rvn_widget_t* rvn_menu_find_shortcut(rvn_widget_t* root, RKey key, uint32_t modifiers);
/** Parses "Ctrl+Shift+S". Implemented in widgets/menu.c. */
int rvn_parse_shortcut(const char* text, RKey* key, uint32_t* modifiers);
/** Opens a menu as a popup at a window position. Implemented in widgets/menu.c. */
void rvn_menu_popup(rvn_widget_t* menu, rvn_window_t* window, float x, float y);

/* ---------------------------------------------------------------------------
 *  Attribute parsing (xml/xml_attributes.c), shared by all set_attribute functions
 * ------------------------------------------------------------------------- */

int rvn_parse_float(const char* text, float* value);
int rvn_parse_int(const char* text, int* value);
int rvn_parse_bool(const char* text, int* value);
/** "#RGB", "#RRGGBB", "#RRGGBBAA" or a few color names. */
int rvn_parse_color(const char* text, rvn_color_t* color);
/** "all", "horizontal,vertical" or "left,top,right,bottom". */
int rvn_parse_margin(const char* text, rvn_margin_t* margin);
/** Up to `max` comma-separated numbers; returns how many were read. */
int rvn_parse_floats(const char* text, float* values, int max);
int rvn_parse_orientation(const char* text, ROrientation* orientation);
/** Sets a base property (name, visibility, slot, size, colors...). */
int rvn_widget_set_base_attribute(rvn_widget_t* widget, const char* name, const char* value);
/** Sets any attribute: the ones of the widget class first, then the base ones. */
int rvn_widget_set_attribute(rvn_widget_t* widget, const char* name, const char* value);

#endif /* RVN_WIDGET_INTERNAL_H */
