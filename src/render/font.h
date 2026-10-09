/**
 * HUI - render/font.h
 * Fonts, glyph cache, glyph atlas, text measuring and drawing.
 *
 * Glyphs are rasterized on demand at the exact pixel size requested (logical
 * size x window scale) and packed into R8 atlas pages shared by all fonts.
 *
 * Threading: measuring and drawing may run on several threads at once (the
 * paint jobs). The glyph caches are lock-free for reading (core/lfmap.h); a
 * cache miss takes one mutex to rasterize the glyph. Atlas pages reach the
 * GPU only in hui_fonts_flush, on the UI thread.
 */

#ifndef HUI_FONT_INTERNAL_H
#define HUI_FONT_INTERNAL_H

#include "draw.h"

typedef struct hui_font_t hui_font_t;

/** Vertical metrics of a font at a given size, in the unit of that size. */
typedef struct hui_font_metrics_t {
	float ascent;
	/** Positive distance below the baseline. */
	float descent;
	float line_height;
} hui_font_metrics_t;

void hui_fonts_init(void);
void hui_fonts_shutdown(void);

/** Resolves a font handle; HUI_INVALID_ID or a stale handle gives the theme / default font. */
hui_font_t* hui_font_resolve(HUI_id font);
/** Resolves a size; 0 or less gives the theme font size. */
float hui_font_size_resolve(float size);

void hui_font_metrics(hui_font_t* font, float size, hui_font_metrics_t* metrics);

/** Width of a single line of text. Linear in `size`, so it is independent of the window scale. */
float hui_text_width(hui_font_t* font, float size, const char* text, int length);

/**
 * Byte offset of the character boundary closest to the horizontal position
 * `x` (same unit as `size`), for caret placement.
 */
int hui_text_offset_at(hui_font_t* font, float size, const char* text, int length, float x);

/**
 * Number of bytes of `text` that fit in `max_width` when breaking at spaces
 * (at least one character). Stops after a '\n'.
 * @param line_width Receives the width of the returned part, without trailing spaces.
 */
int hui_text_wrap(hui_font_t* font, float size, const char* text, int length, float max_width, float* line_width);

/**
 * Appends the glyphs of one line to a draw list.
 * @param size_px Font size in pixels.
 * @param x,y     Top-left corner of the line box, in pixels.
 */
void hui_text_draw(hui_drawlist_t* list, hui_font_t* font, float size_px, float x, float y,
	const char* text, int length, hui_color_t color, hui_box_t clip);

/** Creates and updates the GPU textures of the atlas pages changed since the last call. UI thread. */
void hui_fonts_flush(void);

/** Replaces the atlas pseudo-handles of a finished draw list by real texture handles. UI thread. */
void hui_fonts_resolve_pages(hui_drawlist_t* list);

/* ---------------------------------------------------------------------------
 *  TrueType glue (render/font_raster.c, wraps stb_truetype)
 * ------------------------------------------------------------------------- */

typedef struct hui_ttf_t hui_ttf_t;

/** `data` must stay valid for the lifetime of the object. Returns NULL on invalid data. */
hui_ttf_t* hui_ttf_open(const uint8_t* data, size_t size);
void  hui_ttf_close(hui_ttf_t* ttf);
/** 0 when the font has no glyph for the code point. */
int   hui_ttf_glyph_index(hui_ttf_t* ttf, uint32_t codepoint);
/** Horizontal advance as a fraction of the em size. */
float hui_ttf_advance(hui_ttf_t* ttf, int glyph);
/** Ascent, descent (positive) and line gap as fractions of the em size. */
void  hui_ttf_vmetrics(hui_ttf_t* ttf, float* ascent, float* descent, float* line_gap);
/** Bitmap box of a glyph relative to the pen position on the baseline (y down). */
void  hui_ttf_glyph_box(hui_ttf_t* ttf, int glyph, float size_px, int* x0, int* y0, int* x1, int* y1);
/** Rasterizes a glyph as 8-bit coverage into `out` (width x height, `stride` bytes per row). */
void  hui_ttf_render(hui_ttf_t* ttf, int glyph, float size_px, uint8_t* out, int width, int height, int stride);

/** Font embedded in the library (resources/default_font.c). */
extern const unsigned char hui_default_font_data[];
extern const size_t hui_default_font_size;

#endif /* HUI_FONT_INTERNAL_H */
