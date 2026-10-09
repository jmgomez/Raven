/**
 * Raven - render/font_raster.c
 * The only file that includes stb_truetype. The implementation is compiled
 * as static functions so that it never clashes with another copy of stb
 * linked into the application (HRL embeds one too).
 */

#include "font.h"

#define STBTT_STATIC
#define STBTT_malloc(size, user) ((void)(user), rvn_alloc(size))
#define STBTT_free(ptr, user) ((void)(user), rvn_free(ptr))
#define STB_TRUETYPE_IMPLEMENTATION

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wcast-qual"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#endif

#include "stb/stb_truetype.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

struct rvn_ttf_t {
	stbtt_fontinfo info;
	float units_per_em;
};

rvn_ttf_t* rvn_ttf_open(const uint8_t* data, size_t size) {
	rvn_ttf_t* ttf;
	int offset;
	if (!data || size < 12) return NULL;
	offset = stbtt_GetFontOffsetForIndex(data, 0);
	if (offset < 0) return NULL;
	ttf = (rvn_ttf_t*)rvn_calloc(sizeof(rvn_ttf_t));
	if (!stbtt_InitFont(&ttf->info, data, offset)) {
		rvn_free(ttf);
		return NULL;
	}
	/* stbtt_ScaleForMappingEmToPixels(1) is 1 / unitsPerEm */
	ttf->units_per_em = 1.0f / stbtt_ScaleForMappingEmToPixels(&ttf->info, 1.0f);
	return ttf;
}

void rvn_ttf_close(rvn_ttf_t* ttf) {
	rvn_free(ttf);
}

int rvn_ttf_glyph_index(rvn_ttf_t* ttf, uint32_t codepoint) {
	return stbtt_FindGlyphIndex(&ttf->info, (int)codepoint);
}

float rvn_ttf_advance(rvn_ttf_t* ttf, int glyph) {
	int advance = 0, bearing = 0;
	stbtt_GetGlyphHMetrics(&ttf->info, glyph, &advance, &bearing);
	return (float)advance / ttf->units_per_em;
}

void rvn_ttf_vmetrics(rvn_ttf_t* ttf, float* ascent, float* descent, float* line_gap) {
	int a = 0, d = 0, g = 0;
	stbtt_GetFontVMetrics(&ttf->info, &a, &d, &g);
	*ascent = (float)a / ttf->units_per_em;
	*descent = (float)(-d) / ttf->units_per_em;
	*line_gap = (float)g / ttf->units_per_em;
}

void rvn_ttf_glyph_box(rvn_ttf_t* ttf, int glyph, float size_px, int* x0, int* y0, int* x1, int* y1) {
	float scale = stbtt_ScaleForMappingEmToPixels(&ttf->info, size_px);
	stbtt_GetGlyphBitmapBox(&ttf->info, glyph, scale, scale, x0, y0, x1, y1);
}

void rvn_ttf_render(rvn_ttf_t* ttf, int glyph, float size_px, uint8_t* out, int width, int height, int stride) {
	float scale = stbtt_ScaleForMappingEmToPixels(&ttf->info, size_px);
	stbtt_MakeGlyphBitmap(&ttf->info, out, width, height, stride, scale, scale, glyph);
}
