/**
 * HUI - render/draw.h
 * Draw lists (flat arrays of instances + commands) and the painter used by
 * widgets to fill them.
 *
 * Widgets draw in logical units relative to their own top-left corner; the
 * painter converts to pixels, snaps to the pixel grid, applies clipping and
 * opacity, and appends instances.
 */

#ifndef HUI_DRAW_INTERNAL_H
#define HUI_DRAW_INTERNAL_H

#include "../core/base.h"

struct hui_window_t;
struct hui_font_t;

/* ---------------------------------------------------------------------------
 *  Draw list
 * ------------------------------------------------------------------------- */

typedef struct hui_drawlist_t {
	HUI_DrawInstance* instances;
	uint32_t instance_count;
	uint32_t instance_cap;
	HUI_DrawCommand* commands;
	uint32_t command_count;
	uint32_t command_cap;
} hui_drawlist_t;

void hui_drawlist_init(hui_drawlist_t* list);
void hui_drawlist_free(hui_drawlist_t* list);
void hui_drawlist_reset(hui_drawlist_t* list);

/**
 * Appends one instance and returns it for the caller to fill.
 * @param texture Texture sampled by the instance, or HUI_INVALID_ID when it is
 *                not textured. Untextured instances never break a batch.
 */
HUI_DrawInstance* hui_drawlist_add(hui_drawlist_t* list, HUI_id texture);

/** Appends a viewport callback command. */
void hui_drawlist_add_callback(hui_drawlist_t* list, const HUI_DrawCommand* command);

/** Appends the content of `source`, merging compatible commands. */
void hui_drawlist_append(hui_drawlist_t* list, const hui_drawlist_t* source);
/**
 * Appends only the commands of `source`, whose instances were already copied
 * into `list` starting at index `base`.
 */
void hui_drawlist_append_commands(hui_drawlist_t* list, const hui_drawlist_t* source, uint32_t base);

/* ---------------------------------------------------------------------------
 *  Painter
 * ------------------------------------------------------------------------- */

#define HUI_PAINTER_CLIP_STACK 8

struct HUI_Painter {
	hui_drawlist_t* list;
	struct hui_window_t* window;
	/** Pixels per logical unit. */
	float scale;
	/** Origin of the widget being painted, in logical units from the window corner. */
	float ox, oy;
	/** Current clip rectangle, in pixels. */
	hui_box_t clip;
	/** Opacity accumulated from the ancestors. */
	float opacity;
	/** Text color inherited from the ancestors. */
	hui_color_t text_color;
	/** HUI_FALSE when the widget or one of its ancestors is disabled. */
	int enabled;
	hui_box_t clip_stack[HUI_PAINTER_CLIP_STACK];
	int clip_depth;
};

/** Pixel box of a logical rectangle of the current widget, snapped to the pixel grid. */
hui_box_t hui_paint_snap(const HUI_Painter* painter, hui_rect_t rect);

/** Filled rectangle. */
void hui_paint_rect(HUI_Painter* painter, hui_rect_t rect, hui_color_t color);

/** Rounded rectangle with one radius, optional outline (border_width 0 = none). */
void hui_paint_box(HUI_Painter* painter, hui_rect_t rect, float radius, hui_color_t fill,
	float border_width, hui_color_t border_color);

/** Rounded rectangle with one radius per corner: top-left, top-right, bottom-right, bottom-left. */
void hui_paint_box4(HUI_Painter* painter, hui_rect_t rect, const float radii[4], hui_color_t fill,
	float border_width, hui_color_t border_color);

/** Soft shadow of a rounded rectangle. */
void hui_paint_shadow(HUI_Painter* painter, hui_rect_t rect, float radius, float blur, hui_color_t color);

void hui_paint_line(HUI_Painter* painter, float x0, float y0, float x1, float y1, float thickness, hui_color_t color);
void hui_paint_triangle(HUI_Painter* painter, float x0, float y0, float x1, float y1, float x2, float y2, hui_color_t color);

/** Textured rectangle. `uv` is u0, v0, u1, v1 or NULL for the whole texture. */
void hui_paint_image(HUI_Painter* painter, hui_rect_t rect, HUI_id texture, const float uv[4],
	hui_color_t tint, float radius);

/**
 * Single line of text. (x, y) is the top-left corner of the line box.
 * @param font   Font handle; HUI_INVALID_ID uses the theme font.
 * @param size   Font size in logical units; 0 uses the theme size.
 * @param length Byte length, or -1 for a null-terminated string.
 */
void hui_paint_text(HUI_Painter* painter, HUI_id font, float size, float x, float y,
	const char* text, int length, hui_color_t color);

/** Text aligned inside a rectangle: horizontal 0 = left, 0.5 = center, 1 = right; vertically centered. */
void hui_paint_text_in(HUI_Painter* painter, HUI_id font, float size, hui_rect_t rect, float align,
	const char* text, int length, hui_color_t color);

/** Small vector icons drawn with lines, centered on (cx, cy). */
typedef enum hui_icon_t {
	HUI_ICON_CHEVRON_RIGHT,
	HUI_ICON_CHEVRON_DOWN,
	HUI_ICON_CHEVRON_UP,
	HUI_ICON_CHEVRON_LEFT,
	HUI_ICON_CHECK,
	HUI_ICON_CROSS
} hui_icon_t;
void hui_paint_icon(HUI_Painter* painter, hui_icon_t icon, float cx, float cy, float size, hui_color_t color);

/** Intersects the clip rectangle with a logical rectangle of the current widget. */
void hui_paint_push_clip(HUI_Painter* painter, hui_rect_t rect);
void hui_paint_pop_clip(HUI_Painter* painter);

#endif /* HUI_DRAW_INTERNAL_H */
