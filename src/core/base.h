/**
 * HUI - core/base.h
 * Included by every source file of the library: public API, C runtime,
 * small math helpers, memory, strings and UTF-8.
 */

#ifndef HUI_BASE_H
#define HUI_BASE_H

/* glibc hides POSIX declarations (read-write locks, clock_gettime, ...) in
 * strict C11 mode: ask for them before the first system header. */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "hui/hui.h"
#include "hui/hui_backend.h"

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define HUI_INLINE static inline
#define HUI_UNUSED(x) ((void)(x))
#define HUI_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))
#define HUI_VERSION_STRING "0.1.0"

/* ---------------------------------------------------------------------------
 *  Scalars
 * ------------------------------------------------------------------------- */

HUI_INLINE float hui_minf(float a, float b) { return a < b ? a : b; }
HUI_INLINE float hui_maxf(float a, float b) { return a > b ? a : b; }
HUI_INLINE float hui_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
HUI_INLINE int hui_mini(int a, int b) { return a < b ? a : b; }
HUI_INLINE int hui_maxi(int a, int b) { return a > b ? a : b; }
HUI_INLINE int hui_clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
HUI_INLINE uint32_t hui_minu(uint32_t a, uint32_t b) { return a < b ? a : b; }
HUI_INLINE uint32_t hui_maxu(uint32_t a, uint32_t b) { return a > b ? a : b; }

/* ---------------------------------------------------------------------------
 *  Geometry
 * ------------------------------------------------------------------------- */

typedef struct hui_vec2_t { float x, y; } hui_vec2_t;
/** Position + size, in logical units. */
typedef struct hui_rect_t { float x, y, w, h; } hui_rect_t;
/** Left, top, right, bottom thickness. */
typedef struct hui_margin_t { float l, t, r, b; } hui_margin_t;
/** Min / max corners, in pixels. */
typedef struct hui_box_t { float x0, y0, x1, y1; } hui_box_t;

HUI_INLINE hui_vec2_t hui_vec2(float x, float y) { hui_vec2_t v; v.x = x; v.y = y; return v; }
HUI_INLINE hui_rect_t hui_rect(float x, float y, float w, float h) { hui_rect_t r; r.x = x; r.y = y; r.w = w; r.h = h; return r; }
HUI_INLINE hui_margin_t hui_margin(float l, float t, float r, float b) { hui_margin_t m; m.l = l; m.t = t; m.r = r; m.b = b; return m; }
HUI_INLINE hui_box_t hui_box(float x0, float y0, float x1, float y1) { hui_box_t b; b.x0 = x0; b.y0 = y0; b.x1 = x1; b.y1 = y1; return b; }

HUI_INLINE int hui_rect_contains(hui_rect_t r, float x, float y) {
	return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}
HUI_INLINE hui_rect_t hui_rect_inset(hui_rect_t r, hui_margin_t m) {
	hui_rect_t o;
	o.x = r.x + m.l;
	o.y = r.y + m.t;
	o.w = hui_maxf(0.0f, r.w - m.l - m.r);
	o.h = hui_maxf(0.0f, r.h - m.t - m.b);
	return o;
}
HUI_INLINE hui_rect_t hui_rect_intersect(hui_rect_t a, hui_rect_t b) {
	float x0 = hui_maxf(a.x, b.x), y0 = hui_maxf(a.y, b.y);
	float x1 = hui_minf(a.x + a.w, b.x + b.w), y1 = hui_minf(a.y + a.h, b.y + b.h);
	return hui_rect(x0, y0, hui_maxf(0.0f, x1 - x0), hui_maxf(0.0f, y1 - y0));
}
HUI_INLINE hui_box_t hui_box_intersect(hui_box_t a, hui_box_t b) {
	hui_box_t o;
	o.x0 = hui_maxf(a.x0, b.x0);
	o.y0 = hui_maxf(a.y0, b.y0);
	o.x1 = hui_minf(a.x1, b.x1);
	o.y1 = hui_minf(a.y1, b.y1);
	if (o.x1 < o.x0) o.x1 = o.x0;
	if (o.y1 < o.y0) o.y1 = o.y0;
	return o;
}
HUI_INLINE int hui_box_empty(hui_box_t b) { return b.x1 <= b.x0 || b.y1 <= b.y0; }
HUI_INLINE int hui_box_overlaps(hui_box_t a, hui_box_t b) {
	return a.x0 < b.x1 && a.x1 > b.x0 && a.y0 < b.y1 && a.y1 > b.y0;
}

/* ---------------------------------------------------------------------------
 *  Colors: packed RGBA8, R in the lowest byte (bytes R, G, B, A in memory on
 *  little-endian machines, which is what the backend expects).
 * ------------------------------------------------------------------------- */

typedef uint32_t hui_color_t;

HUI_INLINE hui_color_t hui_rgba8(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
	return (r & 0xFFu) | ((g & 0xFFu) << 8) | ((b & 0xFFu) << 16) | ((a & 0xFFu) << 24);
}
HUI_INLINE hui_color_t hui_rgba(float r, float g, float b, float a) {
	return hui_rgba8((uint32_t)(hui_clampf(r, 0.0f, 1.0f) * 255.0f + 0.5f),
		(uint32_t)(hui_clampf(g, 0.0f, 1.0f) * 255.0f + 0.5f),
		(uint32_t)(hui_clampf(b, 0.0f, 1.0f) * 255.0f + 0.5f),
		(uint32_t)(hui_clampf(a, 0.0f, 1.0f) * 255.0f + 0.5f));
}
/** 0xRRGGBB + alpha, convenient for palettes written in hexadecimal. */
HUI_INLINE hui_color_t hui_hex(uint32_t rgb, uint32_t a) {
	return hui_rgba8((rgb >> 16) & 0xFFu, (rgb >> 8) & 0xFFu, rgb & 0xFFu, a);
}
HUI_INLINE uint32_t hui_color_a(hui_color_t c) { return c >> 24; }
HUI_INLINE hui_color_t hui_color_fade(hui_color_t c, float opacity) {
	uint32_t a = (uint32_t)((float)(c >> 24) * hui_clampf(opacity, 0.0f, 1.0f) + 0.5f);
	return (c & 0x00FFFFFFu) | (a << 24);
}
HUI_INLINE hui_color_t hui_color_mix(hui_color_t a, hui_color_t b, float t) {
	uint32_t i, out = 0;
	t = hui_clampf(t, 0.0f, 1.0f);
	for (i = 0; i < 4; i++) {
		float ca = (float)((a >> (i * 8)) & 0xFFu), cb = (float)((b >> (i * 8)) & 0xFFu);
		out |= ((uint32_t)(ca + (cb - ca) * t + 0.5f) & 0xFFu) << (i * 8);
	}
	return out;
}
HUI_INLINE void hui_color_to_floats(hui_color_t c, float* r, float* g, float* b, float* a) {
	if (r) *r = (float)(c & 0xFFu) / 255.0f;
	if (g) *g = (float)((c >> 8) & 0xFFu) / 255.0f;
	if (b) *b = (float)((c >> 16) & 0xFFu) / 255.0f;
	if (a) *a = (float)((c >> 24) & 0xFFu) / 255.0f;
}

/* ---------------------------------------------------------------------------
 *  Memory (core/memory.c)
 * ------------------------------------------------------------------------- */

void* hui_alloc(size_t size);
/** Zero-initialized allocation. */
void* hui_calloc(size_t size);
void* hui_realloc(void* ptr, size_t size);
void  hui_free(void* ptr);

/** Grows a dynamic array to hold at least `needed` items. Returns the new storage. */
void* hui_array_grow_(void* data, uint32_t* cap, uint32_t needed, size_t item_size);
#define HUI_ARRAY_RESERVE(data, cap, needed) \
	do { if ((uint32_t)(needed) > (cap)) (data) = hui_array_grow_((data), &(cap), (uint32_t)(needed), sizeof(*(data))); } while (0)

/** Chunked bump allocator: everything is released at once. */
typedef struct hui_arena_block_t {
	struct hui_arena_block_t* next;
	size_t cap;
	size_t used;
} hui_arena_block_t;

typedef struct hui_arena_t {
	hui_arena_block_t* head;
	hui_arena_block_t* current;
	size_t block_size;
} hui_arena_t;

void  hui_arena_init(hui_arena_t* arena, size_t block_size);
void* hui_arena_alloc(hui_arena_t* arena, size_t size);
char* hui_arena_strndup(hui_arena_t* arena, const char* text, size_t length);
void  hui_arena_free(hui_arena_t* arena);

/* ---------------------------------------------------------------------------
 *  Strings (core/memory.c) and UTF-8 (core/utf8.c)
 * ------------------------------------------------------------------------- */

char* hui_strdup(const char* text);
char* hui_strndup(const char* text, size_t length);
/** Replaces *dst with a copy of src (NULL clears it). Returns HUI_TRUE when the text changed. */
int   hui_str_set(char** dst, const char* src);
int   hui_stricmp(const char* a, const char* b);
int   hui_strnicmp(const char* a, const char* b, size_t n);
uint32_t hui_hash_bytes(const void* data, size_t size);
uint32_t hui_hash_str(const char* text);
/** Reads a whole file. The result has one extra '\0' byte. Free with hui_free. */
char* hui_read_file(const char* path, size_t* size);
/** Joins a base folder and a relative path. Free with hui_free. */
char* hui_path_join(const char* base, const char* path);
/** Folder part of a path, with its trailing separator. Free with hui_free. */
char* hui_path_dir(const char* path);

/** Decodes one code point and advances *text. Invalid bytes give U+FFFD. */
uint32_t hui_utf8_decode(const char** text, const char* end);
/** Writes 1..4 bytes and returns how many. */
int hui_utf8_encode(uint32_t codepoint, char out[4]);
/** Byte offset of the character boundary after / before `offset`. */
int hui_utf8_next(const char* text, int length, int offset);
int hui_utf8_prev(const char* text, int offset);
int hui_utf8_count(const char* text, int length);

/* ---------------------------------------------------------------------------
 *  Errors (core/error.c)
 * ------------------------------------------------------------------------- */

/** Records an error and calls the error callback. Thread-safe, printf-style. */
void hui_error(HUI_EError code, HUI_ESeverity severity, const char* format, ...);

#endif /* HUI_BASE_H */
