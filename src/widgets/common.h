/**
 * HUI - widgets/common.h
 * Shared by the widget implementations: class declarations and helpers for
 * text, scrollbars and single-line text editing.
 */

#ifndef HUI_WIDGETS_COMMON_H
#define HUI_WIDGETS_COMMON_H

#include "../widget/widget.h"

/* ---------------------------------------------------------------------------
 *  Classes (one per file of this folder)
 * ------------------------------------------------------------------------- */

extern const hui_widget_class_t hui_class_vbox;
extern const hui_widget_class_t hui_class_hbox;
extern const hui_widget_class_t hui_class_grid;
extern const hui_widget_class_t hui_class_uniform_grid;
extern const hui_widget_class_t hui_class_overlay;
extern const hui_widget_class_t hui_class_border;
extern const hui_widget_class_t hui_class_size_box;
extern const hui_widget_class_t hui_class_spacer;
extern const hui_widget_class_t hui_class_scroll_box;
extern const hui_widget_class_t hui_class_splitter;
extern const hui_widget_class_t hui_class_wrap_box;
extern const hui_widget_class_t hui_class_canvas;
extern const hui_widget_class_t hui_class_switcher;
extern const hui_widget_class_t hui_class_tab_view;
extern const hui_widget_class_t hui_class_expander;
extern const hui_widget_class_t hui_class_label;
extern const hui_widget_class_t hui_class_button;
extern const hui_widget_class_t hui_class_checkbox;
extern const hui_widget_class_t hui_class_slider;
extern const hui_widget_class_t hui_class_progress_bar;
extern const hui_widget_class_t hui_class_image;
extern const hui_widget_class_t hui_class_text_input;
extern const hui_widget_class_t hui_class_spin_box;
extern const hui_widget_class_t hui_class_combo_box;
extern const hui_widget_class_t hui_class_separator;
extern const hui_widget_class_t hui_class_list_view;
extern const hui_widget_class_t hui_class_tree_view;
extern const hui_widget_class_t hui_class_menu_bar;
extern const hui_widget_class_t hui_class_menu;
extern const hui_widget_class_t hui_class_menu_item;
extern const hui_widget_class_t hui_class_viewport;
extern const hui_widget_class_t hui_class_custom;

/** Internal class types, outside of the public enum. */
#define HUI_WIDGET_INTERNAL_COMBO_LIST ((HUI_EWidgetType)((int)HUI_WIDGET_LAST_ + 1))

/** Panels sharing HUI_SetPanelSpacing start with this layout. */
typedef struct hui_spaced_panel_t {
	hui_widget_t base;
	float spacing;
} hui_spaced_panel_t;

/** Case-insensitive comparison of an attribute name, inside a set_attribute function. */
#define HUI_ATTR(literal) (hui_stricmp(name, (literal)) == 0)

/* ---------------------------------------------------------------------------
 *  Text helpers (widgets/common.c)
 * ------------------------------------------------------------------------- */

/** Font size of a widget: `size` if positive, else its HUI_METRIC_FONT_SIZE. */
float hui_widget_font_size(const hui_widget_t* widget, float size);
/** Width of a single line of text for a widget. */
float hui_widget_text_width(const hui_widget_t* widget, HUI_id font, float size, const char* text);
/** Height of one line of text for a widget. */
float hui_widget_line_height(const hui_widget_t* widget, HUI_id font, float size);
/** Calls the typed callback result and then the action of the widget; HUI_TRUE while the widget is alive. */
int hui_widget_alive(const hui_widget_t* widget);
/** Draws the keyboard focus outline of a widget when it should be visible. */
void hui_paint_focus(const hui_widget_t* widget, HUI_Painter* painter, float radius);

/** Where a source of `size` texels goes in an area, and which part of it shows (widgets/image.c). */
void hui_image_fit(HUI_EStretch stretch, hui_vec2_t size, float width, float height, const float uv_in[4],
	hui_rect_t* rect, float uv_out[4]);
int  hui_parse_stretch(const char* value, HUI_EStretch* stretch);
/** Texture / font of a file named in an attribute, loaded once (xml/xml_loader.c). */
HUI_id hui_xml_texture(const char* path);
HUI_id hui_xml_font(const char* path);
void hui_xml_shutdown(void);

/* ---------------------------------------------------------------------------
 *  Scrollbar (widgets/common.c): state + painting + mouse handling
 * ------------------------------------------------------------------------- */

typedef struct hui_scrollbar_t {
	/** Current scroll position, 0 .. content - viewport. */
	float offset;
	/** Length of the content and of the visible part. */
	float content;
	float viewport;
	int dragging;
	/** Distance between the cursor and the start of the thumb while dragging. */
	float grab;
} hui_scrollbar_t;

float hui_scrollbar_max(const hui_scrollbar_t* bar);
int   hui_scrollbar_needed(const hui_scrollbar_t* bar);
/** Clamps the offset; returns HUI_TRUE when it changed. */
int   hui_scrollbar_clamp(hui_scrollbar_t* bar);
int   hui_scrollbar_set(hui_scrollbar_t* bar, float offset);
void  hui_scrollbar_paint(const hui_scrollbar_t* bar, const hui_widget_t* widget, HUI_Painter* painter,
	hui_rect_t track, int vertical, float mouse_x, float mouse_y);
/**
 * Handles mouse down / move / up on the track (given in widget coordinates).
 * Returns HUI_TRUE when the event was used; *changed is set when the offset moved.
 */
int   hui_scrollbar_event(hui_scrollbar_t* bar, hui_widget_t* widget, const HUI_Event* event,
	hui_rect_t track, int vertical, int* changed);

/* ---------------------------------------------------------------------------
 *  Single-line text editing (widgets/textedit.c), used by TextInput and SpinBox
 * ------------------------------------------------------------------------- */

#define HUI_TEXTEDIT_HANDLED   (1 << 0)
#define HUI_TEXTEDIT_CHANGED   (1 << 1)
#define HUI_TEXTEDIT_COMMITTED (1 << 2)

typedef struct hui_textedit_t {
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
	HUI_id blink_timer;
	double blink_start;
} hui_textedit_t;

void hui_textedit_init(hui_textedit_t* edit);
void hui_textedit_free(hui_textedit_t* edit);
/** Returns HUI_TRUE when the text changed. */
int  hui_textedit_set_text(hui_textedit_t* edit, const char* text);
void hui_textedit_select_all(hui_textedit_t* edit);
/** Starts or stops the caret blinking of the widget owning the editor. */
void hui_textedit_set_focused(hui_textedit_t* edit, hui_widget_t* widget, int focused);
/** Key and text events. Returns a combination of HUI_TEXTEDIT_... flags. */
int  hui_textedit_key(hui_textedit_t* edit, const HUI_Event* event);
int  hui_textedit_input(hui_textedit_t* edit, uint32_t codepoint);
/** Mouse press / drag at a horizontal position relative to the left of the text area. */
void hui_textedit_mouse_down(hui_textedit_t* edit, const hui_widget_t* widget, HUI_id font, float size,
	float x, int clicks, int extend);
void hui_textedit_mouse_drag(hui_textedit_t* edit, const hui_widget_t* widget, HUI_id font, float size, float x);
/** Draws selection, text and caret inside `area` (widget coordinates). */
void hui_textedit_paint(hui_textedit_t* edit, const hui_widget_t* widget, HUI_Painter* painter,
	hui_rect_t area, HUI_id font, float size, hui_color_t color, int focused);

#endif /* HUI_WIDGETS_COMMON_H */
