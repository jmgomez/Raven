/**
 * Raven - render/font.h
 * Fonts, glyph cache, glyph atlas, text measuring and drawing.
 *
 * Glyphs are rasterized on demand at the exact pixel size requested (logical
 * size x window scale) and packed into R8 atlas pages shared by all fonts.
 *
 * Threading: measuring and drawing may run on several threads at once (the
 * paint jobs). The glyph caches are lock-free for reading (core/lfmap.h); a
 * cache miss takes one mutex to rasterize the glyph. Atlas pages reach the
 * GPU only in rvn_fonts_flush, on the UI thread.
 */

#ifndef RVN_FONT_INTERNAL_H
#define RVN_FONT_INTERNAL_H

#include "draw.h"

typedef struct rvn_font_t rvn_font_t;

/** Vertical metrics of a font at a given size, in the unit of that size. */
typedef struct rvn_font_metrics_t {
	float ascent;
	/** Positive distance below the baseline. */
	float descent;
	float line_height;
} rvn_font_metrics_t;

void rvn_fonts_init(void);
void rvn_fonts_shutdown(void);

/** Resolves a font handle; R_INVALID_ID or a stale handle gives the theme / default font. */
rvn_font_t* rvn_font_resolve(Rid font);
/** Resolves a size; 0 or less gives the theme font size. */
float rvn_font_size_resolve(float size);

void rvn_font_metrics(rvn_font_t* font, float size, rvn_font_metrics_t* metrics);

/** Width of a single line of text. Linear in `size`, so it is independent of the window scale. */
float rvn_text_width(rvn_font_t* font, float size, const char* text, int length);

/**
 * Byte offset of the character boundary closest to the horizontal position
 * `x` (same unit as `size`), for caret placement.
 */
int rvn_text_offset_at(rvn_font_t* font, float size, const char* text, int length, float x);

/**
 * Number of bytes of `text` that fit in `max_width` when breaking at spaces
 * (at least one character). Stops after a '\n'.
 * @param line_width Receives the width of the returned part, without trailing spaces.
 */
int rvn_text_wrap(rvn_font_t* font, float size, const char* text, int length, float max_width, float* line_width);

/**
 * Appends the glyphs of one line to a draw list.
 * @param size_px Font size in pixels.
 * @param x,y     Top-left corner of the line box, in pixels.
 */
void rvn_text_draw(rvn_drawlist_t* list, rvn_font_t* font, float size_px, float x, float y,
	const char* text, int length, rvn_color_t color, rvn_box_t clip);

/** Creates and updates the GPU textures of the atlas pages changed since the last call. UI thread. */
void rvn_fonts_flush(void);

/** Replaces the atlas pseudo-handles of a finished draw list by real texture handles. UI thread. */
void rvn_fonts_resolve_pages(rvn_drawlist_t* list);

/* ---------------------------------------------------------------------------
 *  TrueType glue (render/font_raster.c, wraps stb_truetype)
 * ------------------------------------------------------------------------- */

typedef struct rvn_ttf_t rvn_ttf_t;

/** `data` must stay valid for the lifetime of the object. Returns NULL on invalid data. */
rvn_ttf_t* rvn_ttf_open(const uint8_t* data, size_t size);
void  rvn_ttf_close(rvn_ttf_t* ttf);
/** 0 when the font has no glyph for the code point. */
int   rvn_ttf_glyph_index(rvn_ttf_t* ttf, uint32_t codepoint);
/** Horizontal advance as a fraction of the em size. */
float rvn_ttf_advance(rvn_ttf_t* ttf, int glyph);
/** Ascent, descent (positive) and line gap as fractions of the em size. */
void  rvn_ttf_vmetrics(rvn_ttf_t* ttf, float* ascent, float* descent, float* line_gap);
/** Bitmap box of a glyph relative to the pen position on the baseline (y down). */
void  rvn_ttf_glyph_box(rvn_ttf_t* ttf, int glyph, float size_px, int* x0, int* y0, int* x1, int* y1);
/** Rasterizes a glyph as 8-bit coverage into `out` (width x height, `stride` bytes per row). */
void  rvn_ttf_render(rvn_ttf_t* ttf, int glyph, float size_px, uint8_t* out, int width, int height, int stride);

/** Font embedded in the library (resources/default_font.c). */
extern const unsigned char rvn_default_font_data[];
extern const size_t rvn_default_font_size;

#endif /* RVN_FONT_INTERNAL_H */
