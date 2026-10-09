/**
 * Raven - widget/style.c
 * Global theme and per-widget overrides.
 */

#include "widget.h"
#include "../render/renderer.h"

#include <math.h>

#define RVN_COLOR_INDEX(role) ((int)(role) - (int)R_COLOR_WINDOW)
#define RVN_METRIC_INDEX(metric) ((int)(metric) - (int)R_METRIC_FONT_SIZE)

static int rvn_color_valid(RColorRole role) {
	return role >= R_COLOR_WINDOW && role < R_COLOR_LAST_;
}

static int rvn_metric_valid(RMetric metric) {
	return metric >= R_METRIC_FONT_SIZE && metric < R_METRIC_LAST_;
}

/* ---------------------------------------------------------------------------
 *  Built-in themes
 * ------------------------------------------------------------------------- */

typedef struct rvn_palette_entry_t {
	RColorRole role;
	uint32_t rgb;
	uint32_t alpha;
} rvn_palette_entry_t;

static const rvn_palette_entry_t g_dark_palette[] = {
	{ R_COLOR_WINDOW,           0x1E1E20, 255 },
	{ R_COLOR_PANEL,            0x27272A, 255 },
	{ R_COLOR_BORDER,           0x3F3F46, 255 },
	{ R_COLOR_TEXT,             0xD8D8DC, 255 },
	{ R_COLOR_TEXT_DISABLED,    0x6C6C74, 255 },
	{ R_COLOR_TEXT_PLACEHOLDER, 0x7C7C86, 255 },
	{ R_COLOR_ACCENT,           0x3B82F6, 255 },
	{ R_COLOR_ACCENT_HOVERED,   0x5A9AFA, 255 },
	{ R_COLOR_ACCENT_PRESSED,   0x2B6CD8, 255 },
	{ R_COLOR_ACCENT_TEXT,      0xFFFFFF, 255 },
	{ R_COLOR_CONTROL,          0x35353A, 255 },
	{ R_COLOR_CONTROL_HOVERED,  0x424248, 255 },
	{ R_COLOR_CONTROL_PRESSED,  0x2B2B30, 255 },
	{ R_COLOR_CONTROL_BORDER,   0x4C4C54, 255 },
	{ R_COLOR_INPUT,            0x161618, 255 },
	{ R_COLOR_FOCUS,            0x3B82F6, 255 },
	{ R_COLOR_SELECTION,        0x274B7A, 255 },
	{ R_COLOR_ROW_HOVERED,      0x2E2E33, 255 },
	{ R_COLOR_TRACK,            0x2C2C30, 255 },
	{ R_COLOR_HANDLE,           0x5C5C66, 255 },
	{ R_COLOR_HANDLE_HOVERED,   0x7C7C88, 255 },
	{ R_COLOR_SEPARATOR,        0x3F3F46, 255 },
	{ R_COLOR_HEADER,           0x2D2D31, 255 },
	{ R_COLOR_HEADER_HOVERED,   0x3A3A40, 255 },
	{ R_COLOR_TAB_ACTIVE,       0x1E1E20, 255 },
	{ R_COLOR_POPUP,            0x2B2B2F, 255 },
	{ R_COLOR_POPUP_BORDER,     0x4A4A52, 255 },
	{ R_COLOR_SHADOW,           0x000000, 120 },
	{ R_COLOR_TOOLTIP,          0x3C3C42, 255 },
	{ R_COLOR_TOOLTIP_TEXT,     0xE6E6EA, 255 },
	{ R_COLOR_SWITCH,           0x3B82F6, 255 }
};

static const rvn_palette_entry_t g_light_palette[] = {
	{ R_COLOR_WINDOW,           0xF3F3F5, 255 },
	{ R_COLOR_PANEL,            0xFFFFFF, 255 },
	{ R_COLOR_BORDER,           0xD0D0D6, 255 },
	{ R_COLOR_TEXT,             0x1F1F23, 255 },
	{ R_COLOR_TEXT_DISABLED,    0xA0A0A8, 255 },
	{ R_COLOR_TEXT_PLACEHOLDER, 0x90909A, 255 },
	{ R_COLOR_ACCENT,           0x2563EB, 255 },
	{ R_COLOR_ACCENT_HOVERED,   0x3B7BF5, 255 },
	{ R_COLOR_ACCENT_PRESSED,   0x1D4FC4, 255 },
	{ R_COLOR_ACCENT_TEXT,      0xFFFFFF, 255 },
	{ R_COLOR_CONTROL,          0xE7E7EB, 255 },
	{ R_COLOR_CONTROL_HOVERED,  0xDBDBE1, 255 },
	{ R_COLOR_CONTROL_PRESSED,  0xCBCBD3, 255 },
	{ R_COLOR_CONTROL_BORDER,   0xC2C2CA, 255 },
	{ R_COLOR_INPUT,            0xFFFFFF, 255 },
	{ R_COLOR_FOCUS,            0x2563EB, 255 },
	{ R_COLOR_SELECTION,        0xBBD4FB, 255 },
	{ R_COLOR_ROW_HOVERED,      0xE9EEF6, 255 },
	{ R_COLOR_TRACK,            0xDDDDE2, 255 },
	{ R_COLOR_HANDLE,           0xA9A9B2, 255 },
	{ R_COLOR_HANDLE_HOVERED,   0x8A8A94, 255 },
	{ R_COLOR_SEPARATOR,        0xD0D0D6, 255 },
	{ R_COLOR_HEADER,           0xE9E9ED, 255 },
	{ R_COLOR_HEADER_HOVERED,   0xDDDDE3, 255 },
	{ R_COLOR_TAB_ACTIVE,       0xF3F3F5, 255 },
	{ R_COLOR_POPUP,            0xFFFFFF, 255 },
	{ R_COLOR_POPUP_BORDER,     0xC8C8D0, 255 },
	{ R_COLOR_SHADOW,           0x000000, 60 },
	{ R_COLOR_TOOLTIP,          0x2B2B30, 255 },
	{ R_COLOR_TOOLTIP_TEXT,     0xF2F2F5, 255 },
	{ R_COLOR_SWITCH,           0x2563EB, 255 }
};

/*
 * Glass themes. Most surfaces are translucent: the alpha of a color is how
 * much of it is laid over what shows through. Glass surfaces use these colors
 * as their tint (CONTROL, ACCENT, INPUT, PANEL, POPUP: see widgets/common.c).
 */
static const rvn_palette_entry_t g_glass_dark_palette[] = {
	{ R_COLOR_WINDOW,           0x0D0E14, 255 },
	{ R_COLOR_PANEL,            0x16161C, 96 },
	{ R_COLOR_BORDER,           0xFFFFFF, 34 },
	{ R_COLOR_TEXT,             0xF5F5F7, 255 },
	{ R_COLOR_TEXT_DISABLED,    0xEBEBF5, 80 },
	{ R_COLOR_TEXT_PLACEHOLDER, 0xEBEBF5, 120 },
	{ R_COLOR_ACCENT,           0x0A84FF, 255 },
	{ R_COLOR_ACCENT_HOVERED,   0x409CFF, 255 },
	{ R_COLOR_ACCENT_PRESSED,   0x0A6FDB, 255 },
	{ R_COLOR_ACCENT_TEXT,      0xFFFFFF, 255 },
	{ R_COLOR_CONTROL,          0xFFFFFF, 28 },
	{ R_COLOR_CONTROL_HOVERED,  0xFFFFFF, 46 },
	{ R_COLOR_CONTROL_PRESSED,  0xFFFFFF, 66 },
	{ R_COLOR_CONTROL_BORDER,   0xFFFFFF, 60 },
	{ R_COLOR_INPUT,            0x000000, 62 },
	{ R_COLOR_FOCUS,            0x0A84FF, 210 },
	{ R_COLOR_SELECTION,        0x0A84FF, 130 },
	{ R_COLOR_ROW_HOVERED,      0xFFFFFF, 20 },
	{ R_COLOR_TRACK,            0xFFFFFF, 40 },
	{ R_COLOR_HANDLE,           0xFFFFFF, 90 },
	{ R_COLOR_HANDLE_HOVERED,   0xFFFFFF, 140 },
	{ R_COLOR_SEPARATOR,        0xFFFFFF, 30 },
	{ R_COLOR_HEADER,           0xFFFFFF, 16 },
	{ R_COLOR_HEADER_HOVERED,   0xFFFFFF, 30 },
	{ R_COLOR_TAB_ACTIVE,       0xFFFFFF, 52 },
	{ R_COLOR_POPUP,            0x1C1C22, 150 },
	{ R_COLOR_POPUP_BORDER,     0xFFFFFF, 40 },
	{ R_COLOR_SHADOW,           0x000000, 110 },
	{ R_COLOR_TOOLTIP,          0x1C1C22, 165 },
	{ R_COLOR_TOOLTIP_TEXT,     0xF5F5F7, 255 },
	{ R_COLOR_SWITCH,           0x30D158, 255 }
};

static const rvn_palette_entry_t g_glass_light_palette[] = {
	{ R_COLOR_WINDOW,           0xF2F2F7, 255 },
	{ R_COLOR_PANEL,            0xFFFFFF, 105 },
	{ R_COLOR_BORDER,           0x3C3C43, 40 },
	{ R_COLOR_TEXT,             0x1C1C1E, 255 },
	{ R_COLOR_TEXT_DISABLED,    0x3C3C43, 90 },
	{ R_COLOR_TEXT_PLACEHOLDER, 0x3C3C43, 130 },
	{ R_COLOR_ACCENT,           0x007AFF, 255 },
	{ R_COLOR_ACCENT_HOVERED,   0x2E90FF, 255 },
	{ R_COLOR_ACCENT_PRESSED,   0x0066D6, 255 },
	{ R_COLOR_ACCENT_TEXT,      0xFFFFFF, 255 },
	{ R_COLOR_CONTROL,          0xFFFFFF, 120 },
	{ R_COLOR_CONTROL_HOVERED,  0xFFFFFF, 165 },
	{ R_COLOR_CONTROL_PRESSED,  0xFFFFFF, 205 },
	{ R_COLOR_CONTROL_BORDER,   0x3C3C43, 46 },
	{ R_COLOR_INPUT,            0xFFFFFF, 110 },
	{ R_COLOR_FOCUS,            0x007AFF, 210 },
	{ R_COLOR_SELECTION,        0x007AFF, 80 },
	{ R_COLOR_ROW_HOVERED,      0x3C3C43, 16 },
	{ R_COLOR_TRACK,            0x787880, 52 },
	{ R_COLOR_HANDLE,           0x3C3C43, 80 },
	{ R_COLOR_HANDLE_HOVERED,   0x3C3C43, 130 },
	{ R_COLOR_SEPARATOR,        0x3C3C43, 46 },
	{ R_COLOR_HEADER,           0x3C3C43, 12 },
	{ R_COLOR_HEADER_HOVERED,   0x3C3C43, 26 },
	{ R_COLOR_TAB_ACTIVE,       0xFFFFFF, 215 },
	{ R_COLOR_POPUP,            0xFAFAFC, 160 },
	{ R_COLOR_POPUP_BORDER,     0x3C3C43, 30 },
	{ R_COLOR_SHADOW,           0x000000, 46 },
	{ R_COLOR_TOOLTIP,          0xFAFAFC, 185 },
	{ R_COLOR_TOOLTIP_TEXT,     0x1C1C1E, 255 },
	{ R_COLOR_SWITCH,           0x34C759, 255 }
};

/* Same order as RMetric. */
static const float g_flat_metrics[R_METRIC_COUNT] = {
	13.0f, /* font size */
	4.0f,  /* corner radius */
	1.0f,  /* border width */
	26.0f, /* control height */
	10.0f, /* padding x */
	4.0f,  /* padding y */
	6.0f,  /* spacing */
	10.0f, /* scrollbar size */
	22.0f, /* row height */
	16.0f, /* indent */
	5.0f,  /* splitter size */
	16.0f, /* check size */
	14.0f, /* slider handle size */
	6.0f   /* popup radius */
};

/* Larger, rounder, with more air: controls are capsules. */
static const float g_glass_metrics[R_METRIC_COUNT] = {
	14.0f, /* font size */
	12.0f, /* corner radius */
	1.0f,  /* border width */
	32.0f, /* control height */
	15.0f, /* padding x */
	6.0f,  /* padding y */
	8.0f,  /* spacing */
	10.0f, /* scrollbar size */
	30.0f, /* row height */
	18.0f, /* indent */
	7.0f,  /* splitter size */
	22.0f, /* check size */
	22.0f, /* slider handle size */
	16.0f  /* popup radius */
};

/* ---------------------------------------------------------------------------
 *  Wallpaper of the glass themes
 * ------------------------------------------------------------------------- */

#define RVN_WALLPAPER_SIZE 256

typedef struct rvn_blob_t {
	float x, y, radius;
	float r, g, b;
} rvn_blob_t;

/*
 * Glass needs something to look through. The glass themes give windows a
 * soft gradient of large colored blobs, small and smooth enough to be
 * stretched over any window.
 */
static Rid rvn_wallpaper_create(int dark) {
	static const rvn_blob_t dark_blobs[] = {
		{ 0.15f, 0.20f, 0.55f, 0.20f, 0.16f, 0.72f },
		{ 0.88f, 0.12f, 0.50f, 0.74f, 0.13f, 0.52f },
		{ 0.80f, 0.86f, 0.58f, 0.00f, 0.52f, 0.62f },
		{ 0.12f, 0.92f, 0.46f, 0.86f, 0.38f, 0.10f },
		{ 0.50f, 0.50f, 0.34f, 0.10f, 0.20f, 0.46f }
	};
	static const rvn_blob_t light_blobs[] = {
		{ 0.15f, 0.20f, 0.55f, 0.56f, 0.74f, 1.00f },
		{ 0.88f, 0.12f, 0.50f, 1.00f, 0.66f, 0.82f },
		{ 0.80f, 0.86f, 0.58f, 0.62f, 0.92f, 0.82f },
		{ 0.12f, 0.92f, 0.46f, 1.00f, 0.82f, 0.60f },
		{ 0.50f, 0.50f, 0.34f, 0.86f, 0.84f, 1.00f }
	};
	const rvn_blob_t* blobs = dark ? dark_blobs : light_blobs;
	const float base[3] = { dark ? 0.035f : 0.93f, dark ? 0.035f : 0.94f, dark ? 0.07f : 0.98f };
	uint8_t* pixels = (uint8_t*)rvn_alloc((size_t)RVN_WALLPAPER_SIZE * RVN_WALLPAPER_SIZE * 4);
	int x, y, i;

	for (y = 0; y < RVN_WALLPAPER_SIZE; y++) {
		for (x = 0; x < RVN_WALLPAPER_SIZE; x++) {
			float u = ((float)x + 0.5f) / RVN_WALLPAPER_SIZE, v = ((float)y + 0.5f) / RVN_WALLPAPER_SIZE;
			float color[3];
			uint8_t* out = pixels + ((size_t)y * RVN_WALLPAPER_SIZE + (size_t)x) * 4;
			color[0] = base[0];
			color[1] = base[1];
			color[2] = base[2];
			for (i = 0; i < 5; i++) {
				float dx = u - blobs[i].x, dy = v - blobs[i].y;
				float weight = expf(-(dx * dx + dy * dy) / (blobs[i].radius * blobs[i].radius * 0.5f)) * 0.9f;
				color[0] += (blobs[i].r - color[0]) * weight;
				color[1] += (blobs[i].g - color[1]) * weight;
				color[2] += (blobs[i].b - color[2]) * weight;
			}
			out[0] = (uint8_t)(rvn_clampf(color[0], 0.0f, 1.0f) * 255.0f + 0.5f);
			out[1] = (uint8_t)(rvn_clampf(color[1], 0.0f, 1.0f) * 255.0f + 0.5f);
			out[2] = (uint8_t)(rvn_clampf(color[2], 0.0f, 1.0f) * 255.0f + 0.5f);
			out[3] = 255;
		}
	}
	/* the texture takes ownership of the pixels */
	return rvn_texture_create_raw(RVN_WALLPAPER_SIZE, RVN_WALLPAPER_SIZE, R_TEXTURE_RGBA8, R_FILTER_LINEAR, pixels);
}

void rvn_theme_apply(RTheme theme) {
	const rvn_palette_entry_t* palette;
	size_t count, i;
	int glass = theme == R_THEME_GLASS_DARK || theme == R_THEME_GLASS_LIGHT;
	int dark = theme == R_THEME_DARK || theme == R_THEME_GLASS_DARK;

	switch (theme) {
		case R_THEME_LIGHT:
			palette = g_light_palette;
			count = RVN_COUNT_OF(g_light_palette);
			break;
		case R_THEME_GLASS_DARK:
			palette = g_glass_dark_palette;
			count = RVN_COUNT_OF(g_glass_dark_palette);
			break;
		case R_THEME_GLASS_LIGHT:
			palette = g_glass_light_palette;
			count = RVN_COUNT_OF(g_glass_light_palette);
			break;
		case R_THEME_DARK:
		default:
			palette = g_dark_palette;
			count = RVN_COUNT_OF(g_dark_palette);
			break;
	}
	for (i = 0; i < count; i++) RVN_THEME_COLOR(palette[i].role) = rvn_hex(palette[i].rgb, palette[i].alpha);
	memcpy(g_rvn.theme.metrics, glass ? g_glass_metrics : g_flat_metrics, sizeof(g_rvn.theme.metrics));

	g_rvn.theme.glass = glass;
	g_rvn.theme.dark = dark;
	g_rvn.theme.wallpaper = R_INVALID_ID;
	if (glass) {
		Rid* wallpaper = &g_rvn.theme.wallpapers[dark ? 0 : 1];
		if (!rvn_texture_get(*wallpaper)) *wallpaper = rvn_wallpaper_create(dark);
		g_rvn.theme.wallpaper = *wallpaper;
	}
}

/* ---------------------------------------------------------------------------
 *  Lookup
 * ------------------------------------------------------------------------- */

rvn_color_t rvn_widget_color(const rvn_widget_t* widget, RColorRole role) {
	int index = RVN_COLOR_INDEX(role);
	if (widget->style && (widget->style->color_mask & (1u << index))) return widget->style->colors[index];
	return g_rvn.theme.colors[index];
}

float rvn_widget_metric(const rvn_widget_t* widget, RMetric metric) {
	int index = RVN_METRIC_INDEX(metric);
	if (widget->style && (widget->style->metric_mask & (1u << index))) return widget->style->metrics[index];
	return g_rvn.theme.metrics[index];
}

int rvn_widget_has_color(const rvn_widget_t* widget, RColorRole role) {
	return widget->style && (widget->style->color_mask & (1u << RVN_COLOR_INDEX(role)));
}

static rvn_style_t* rvn_widget_style(rvn_widget_t* widget) {
	if (!widget->style) widget->style = (rvn_style_t*)rvn_calloc(sizeof(rvn_style_t));
	return widget->style;
}

void rvn_widget_set_color(rvn_widget_t* widget, RColorRole role, rvn_color_t color) {
	rvn_style_t* style = rvn_widget_style(widget);
	int index = RVN_COLOR_INDEX(role);
	style->colors[index] = color;
	style->color_mask |= 1u << index;
	rvn_widget_invalidate_paint(widget);
}

void rvn_widget_set_metric(rvn_widget_t* widget, RMetric metric, float value) {
	rvn_style_t* style = rvn_widget_style(widget);
	int index = RVN_METRIC_INDEX(metric);
	style->metrics[index] = value;
	style->metric_mask |= 1u << index;
	rvn_widget_invalidate_layout(widget);
}

rvn_color_t rvn_widget_text_color(const rvn_widget_t* widget, const RPainter* painter) {
	if (!painter->enabled) return rvn_widget_color(widget, R_COLOR_TEXT_DISABLED);
	/* the painter already carries the override of this widget or of an ancestor */
	return painter->text_color;
}

/* ---------------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------------- */

void Rtheme_apply(RTheme theme) {
	RVN_API_GUARD_VOID();
	if (theme < R_THEME_DARK || theme > R_THEME_GLASS_LIGHT) {
		rvn_error(R_ERROR_INVALID_ENUM, R_SEVERITY_ERROR, "Rtheme_apply: invalid theme");
		return;
	}
	rvn_theme_apply(theme);
	rvn_invalidate_all_windows();
}

void Rtheme_set_color(RColorRole role, float r, float g, float b, float a) {
	RVN_API_GUARD_VOID();
	if (!rvn_color_valid(role)) {
		rvn_error(R_ERROR_INVALID_ENUM, R_SEVERITY_ERROR, "Rtheme_set_color: invalid color role");
		return;
	}
	RVN_THEME_COLOR(role) = rvn_rgba(r, g, b, a);
	rvn_invalidate_all_windows();
}

void Rtheme_get_color(RColorRole role, float* r, float* g, float* b, float* a) {
	if (!g_rvn.initialized || !rvn_color_valid(role)) {
		if (r) *r = 0.0f;
		if (g) *g = 0.0f;
		if (b) *b = 0.0f;
		if (a) *a = 0.0f;
		return;
	}
	rvn_color_to_floats(RVN_THEME_COLOR(role), r, g, b, a);
}

void Rtheme_set_metric(RMetric metric, float value) {
	RVN_API_GUARD_VOID();
	if (!rvn_metric_valid(metric)) {
		rvn_error(R_ERROR_INVALID_ENUM, R_SEVERITY_ERROR, "Rtheme_set_metric: invalid metric");
		return;
	}
	RVN_THEME_METRIC(metric) = value;
	rvn_invalidate_all_windows();
}

float Rtheme_get_metric(RMetric metric) {
	if (!g_rvn.initialized || !rvn_metric_valid(metric)) return 0.0f;
	return RVN_THEME_METRIC(metric);
}

void Rtheme_set_font(Rid font) {
	RVN_API_GUARD_VOID();
	if (font != R_INVALID_ID && !rvn_pool_get(&g_rvn.fonts, font)) {
		rvn_error(R_ERROR_INVALID_ID, R_SEVERITY_ERROR, "Rtheme_set_font: invalid font");
		return;
	}
	g_rvn.theme.font = font == R_INVALID_ID ? g_rvn.default_font : font;
	rvn_invalidate_all_windows();
}

Rid Rtheme_get_font(void) {
	return g_rvn.initialized ? g_rvn.theme.font : R_INVALID_ID;
}

void Rwidget_set_color(Rid id, RColorRole role, float r, float g, float b, float a) {
	RVN_WIDGET_VOID(widget, id, 0);
	if (!rvn_color_valid(role)) {
		rvn_error(R_ERROR_INVALID_ENUM, R_SEVERITY_ERROR, "Rwidget_set_color: invalid color role");
		return;
	}
	rvn_widget_set_color(widget, role, rvn_rgba(r, g, b, a));
}

void Rwidget_clear_color(Rid id, RColorRole role) {
	RVN_WIDGET_VOID(widget, id, 0);
	if (!rvn_color_valid(role)) {
		rvn_error(R_ERROR_INVALID_ENUM, R_SEVERITY_ERROR, "Rwidget_clear_color: invalid color role");
		return;
	}
	if (widget->style) widget->style->color_mask &= ~(1u << RVN_COLOR_INDEX(role));
	rvn_widget_invalidate_paint(widget);
}

void Rwidget_set_metric(Rid id, RMetric metric, float value) {
	RVN_WIDGET_VOID(widget, id, 0);
	if (!rvn_metric_valid(metric)) {
		rvn_error(R_ERROR_INVALID_ENUM, R_SEVERITY_ERROR, "Rwidget_set_metric: invalid metric");
		return;
	}
	rvn_widget_set_metric(widget, metric, value);
}

void Rwidget_clear_metric(Rid id, RMetric metric) {
	RVN_WIDGET_VOID(widget, id, 0);
	if (!rvn_metric_valid(metric)) {
		rvn_error(R_ERROR_INVALID_ENUM, R_SEVERITY_ERROR, "Rwidget_clear_metric: invalid metric");
		return;
	}
	if (widget->style) widget->style->metric_mask &= ~(1u << RVN_METRIC_INDEX(metric));
	rvn_widget_invalidate_layout(widget);
}
