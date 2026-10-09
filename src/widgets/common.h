/**
 * Raven - widgets/common.h
 * Shared by the widget implementations: class declarations and helpers for
 * text, scrollbars and single-line text editing.
 */

#ifndef RVN_WIDGETS_COMMON_H
#define RVN_WIDGETS_COMMON_H

#include "../widget/widget.h"

/* ---------------------------------------------------------------------------
 *  Classes (one per file of this folder)
 * ------------------------------------------------------------------------- */

extern const rvn_widget_class_t rvn_class_vbox;
extern const rvn_widget_class_t rvn_class_hbox;
extern const rvn_widget_class_t rvn_class_grid;
extern const rvn_widget_class_t rvn_class_uniform_grid;
extern const rvn_widget_class_t rvn_class_overlay;
extern const rvn_widget_class_t rvn_class_border;
extern const rvn_widget_class_t rvn_class_size_box;
extern const rvn_widget_class_t rvn_class_spacer;
extern const rvn_widget_class_t rvn_class_scroll_box;
extern const rvn_widget_class_t rvn_class_splitter;
extern const rvn_widget_class_t rvn_class_wrap_box;
extern const rvn_widget_class_t rvn_class_canvas;
extern const rvn_widget_class_t rvn_class_switcher;
extern const rvn_widget_class_t rvn_class_tab_view;
extern const rvn_widget_class_t rvn_class_expander;
extern const rvn_widget_class_t rvn_class_label;
extern const rvn_widget_class_t rvn_class_button;
extern const rvn_widget_class_t rvn_class_checkbox;
extern const rvn_widget_class_t rvn_class_slider;
extern const rvn_widget_class_t rvn_class_progress_bar;
extern const rvn_widget_class_t rvn_class_image;
extern const rvn_widget_class_t rvn_class_text_input;
extern const rvn_widget_class_t rvn_class_spin_box;
extern const rvn_widget_class_t rvn_class_combo_box;
extern const rvn_widget_class_t rvn_class_separator;
extern const rvn_widget_class_t rvn_class_list_view;
extern const rvn_widget_class_t rvn_class_tree_view;
extern const rvn_widget_class_t rvn_class_menu_bar;
extern const rvn_widget_class_t rvn_class_menu;
extern const rvn_widget_class_t rvn_class_menu_item;
extern const rvn_widget_class_t rvn_class_viewport;
extern const rvn_widget_class_t rvn_class_custom;

/** Internal class types, outside of the public enum. */
#define RVN_WIDGET_INTERNAL_COMBO_LIST ((RWidgetType)((int)R_WIDGET_LAST_ + 1))

/** Panels sharing Rpanel_set_spacing start with this layout. */
typedef struct rvn_spaced_panel_t {
	rvn_widget_t base;
	float spacing;
} rvn_spaced_panel_t;

/** Case-insensitive comparison of an attribute name, inside a set_attribute function. */
#define RVN_ATTR(literal) (rvn_stricmp(name, (literal)) == 0)

/* ---------------------------------------------------------------------------
 *  Text helpers (widgets/common.c)
 * ------------------------------------------------------------------------- */

/** Font size of a widget: `size` if positive, else its R_METRIC_FONT_SIZE. */
float rvn_widget_font_size(const rvn_widget_t* widget, float size);
/** Width of a single line of text for a widget. */
float rvn_widget_text_width(const rvn_widget_t* widget, Rid font, float size, const char* text);
/** Height of one line of text for a widget. */
float rvn_widget_line_height(const rvn_widget_t* widget, Rid font, float size);
/** Calls the typed callback result and then the action of the widget; R_TRUE while the widget is alive. */
int rvn_widget_alive(const rvn_widget_t* widget);
/** Draws the keyboard focus outline of a widget when it should be visible. */
void rvn_paint_focus(const rvn_widget_t* widget, RPainter* painter, float radius);

/* ---------------------------------------------------------------------------
 *  Glass themes (widgets/common.c)
 *
 *  With a glass theme (g_rvn.theme.glass) the widgets draw their surfaces
 *  with the helpers below instead of flat boxes. The tint of each kind of
 *  surface comes from a color role, so the palette still drives the look.
 * ------------------------------------------------------------------------- */

typedef enum rvn_surface_t {
	/** Buttons, combo boxes: R_COLOR_CONTROL, _HOVERED, _PRESSED. */
	RVN_SURFACE_CONTROL,
	/** Primary buttons: R_COLOR_ACCENT, _HOVERED, _PRESSED. */
	RVN_SURFACE_ACCENT,
	/** Text fields: R_COLOR_INPUT. */
	RVN_SURFACE_INPUT,
	/** Bars across the window: R_COLOR_PANEL. */
	RVN_SURFACE_BAR,
	/** Panels (Border with the glass material): R_COLOR_PANEL. */
	RVN_SURFACE_PANEL,
	/** Menus, drop-down lists, tooltips: R_COLOR_POPUP. */
	RVN_SURFACE_POPUP,
	/** A knob being dragged: almost clear, strongly refracting. */
	RVN_SURFACE_LENS
} rvn_surface_t;

/** Corner radius of a control of the given height: its metric, or half the height (a capsule) with a glass theme. */
float rvn_control_radius(const rvn_widget_t* widget, float height);
/** Look of a kind of surface in the current theme; `hover` and `press` are 0..1. */
void rvn_glass_style(const rvn_widget_t* widget, rvn_surface_t kind, float hover, float press, rvn_glass_t* glass);
/** Glass surface with one radius, dimmed when the painter is disabled. */
void rvn_paint_surface(RPainter* painter, const rvn_widget_t* widget, rvn_rect_t rect, float radius,
	rvn_surface_t kind, float hover, float press);
/**
 * Background of a hovered or selected row: a plain rectangle, or an inset
 * rounded one with a glass theme.
 */
void rvn_paint_row(RPainter* painter, rvn_rect_t row, rvn_color_t color);
/** Eased hover / pressed state of a widget for the glass themes (widget->anim). */
float rvn_widget_hover(rvn_widget_t* widget, RPainter* painter, int hovered);
float rvn_widget_press(rvn_widget_t* widget, RPainter* painter, int pressed);
/** A solid knob with a soft shadow (slider, switch), or a lens of clear glass while `press` is 1. */
void rvn_paint_knob(RPainter* painter, const rvn_widget_t* widget, rvn_rect_t rect, float press);

/** Where a source of `size` texels goes in an area, and which part of it shows (widgets/image.c). */
void rvn_image_fit(RStretch stretch, rvn_vec2_t size, float width, float height, const float uv_in[4],
	rvn_rect_t* rect, float uv_out[4]);
int  rvn_parse_stretch(const char* value, RStretch* stretch);
/** Texture / font of a file named in an attribute, loaded once (xml/xml_loader.c). */
Rid rvn_xml_texture(const char* path);
Rid rvn_xml_font(const char* path);
void rvn_xml_shutdown(void);

/* ---------------------------------------------------------------------------
 *  Scrollbar (widgets/common.c): state + painting + mouse handling
 * ------------------------------------------------------------------------- */

typedef struct rvn_scrollbar_t {
	/** Current scroll position, 0 .. content - viewport. */
	float offset;
	/** Length of the content and of the visible part. */
	float content;
	float viewport;
	int dragging;
	/** Distance between the cursor and the start of the thumb while dragging. */
	float grab;
} rvn_scrollbar_t;

float rvn_scrollbar_max(const rvn_scrollbar_t* bar);
int   rvn_scrollbar_needed(const rvn_scrollbar_t* bar);
/** Clamps the offset; returns R_TRUE when it changed. */
int   rvn_scrollbar_clamp(rvn_scrollbar_t* bar);
int   rvn_scrollbar_set(rvn_scrollbar_t* bar, float offset);
void  rvn_scrollbar_paint(const rvn_scrollbar_t* bar, const rvn_widget_t* widget, RPainter* painter,
	rvn_rect_t track, int vertical, float mouse_x, float mouse_y);
/**
 * Handles mouse down / move / up on the track (given in widget coordinates).
 * Returns R_TRUE when the event was used; *changed is set when the offset moved.
 */
int   rvn_scrollbar_event(rvn_scrollbar_t* bar, rvn_widget_t* widget, const REvent* event,
	rvn_rect_t track, int vertical, int* changed);

/* ---------------------------------------------------------------------------
 *  Single-line text editing (widgets/textedit.c), used by TextInput and SpinBox
 * ------------------------------------------------------------------------- */

#define RVN_TEXTEDIT_HANDLED   (1 << 0)
#define RVN_TEXTEDIT_CHANGED   (1 << 1)
#define RVN_TEXTEDIT_COMMITTED (1 << 2)

typedef struct rvn_textedit_t {
	char* text;
	int length;
	int cap;
	/** Caret and selection anchor, as byte offsets. The selection is the range between them. */
	int caret;
	int anchor;
	float scroll;
	int max_chars;
	int password;
	int read_only;
	int mouse_selecting;
	Rid blink_timer;
	double blink_start;
} rvn_textedit_t;

void rvn_textedit_init(rvn_textedit_t* edit);
void rvn_textedit_free(rvn_textedit_t* edit);
/** Returns R_TRUE when the text changed. */
int  rvn_textedit_set_text(rvn_textedit_t* edit, const char* text);
void rvn_textedit_select_all(rvn_textedit_t* edit);
/** Starts or stops the caret blinking of the widget owning the editor. */
void rvn_textedit_set_focused(rvn_textedit_t* edit, rvn_widget_t* widget, int focused);
/** Key and text events. Returns a combination of RVN_TEXTEDIT_... flags. */
int  rvn_textedit_key(rvn_textedit_t* edit, const REvent* event);
int  rvn_textedit_input(rvn_textedit_t* edit, uint32_t codepoint);
/** Mouse press / drag at a horizontal position relative to the left of the text area. */
void rvn_textedit_mouse_down(rvn_textedit_t* edit, const rvn_widget_t* widget, Rid font, float size,
	float x, int clicks, int extend);
void rvn_textedit_mouse_drag(rvn_textedit_t* edit, const rvn_widget_t* widget, Rid font, float size, float x);
/** Draws selection, text and caret inside `area` (widget coordinates). */
void rvn_textedit_paint(rvn_textedit_t* edit, const rvn_widget_t* widget, RPainter* painter,
	rvn_rect_t area, Rid font, float size, rvn_color_t color, int focused);

#endif /* RVN_WIDGETS_COMMON_H */
