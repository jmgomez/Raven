/**
 * HUI - widget/style.c
 * Global theme and per-widget overrides.
 */

#include "widget.h"

#define HUI_COLOR_INDEX(role) ((int)(role) - (int)HUI_COLOR_WINDOW)
#define HUI_METRIC_INDEX(metric) ((int)(metric) - (int)HUI_METRIC_FONT_SIZE)

static int hui_color_valid(HUI_EColor role) {
	return role >= HUI_COLOR_WINDOW && role < HUI_COLOR_LAST_;
}

static int hui_metric_valid(HUI_EMetric metric) {
	return metric >= HUI_METRIC_FONT_SIZE && metric < HUI_METRIC_LAST_;
}

/* ---------------------------------------------------------------------------
 *  Built-in themes
 * ------------------------------------------------------------------------- */

typedef struct hui_palette_entry_t {
	HUI_EColor role;
	uint32_t rgb;
	uint32_t alpha;
} hui_palette_entry_t;

static const hui_palette_entry_t g_dark_palette[] = {
	{ HUI_COLOR_WINDOW,           0x1E1E20, 255 },
	{ HUI_COLOR_PANEL,            0x27272A, 255 },
	{ HUI_COLOR_BORDER,           0x3F3F46, 255 },
	{ HUI_COLOR_TEXT,             0xD8D8DC, 255 },
	{ HUI_COLOR_TEXT_DISABLED,    0x6C6C74, 255 },
	{ HUI_COLOR_TEXT_PLACEHOLDER, 0x7C7C86, 255 },
	{ HUI_COLOR_ACCENT,           0x3B82F6, 255 },
	{ HUI_COLOR_ACCENT_HOVERED,   0x5A9AFA, 255 },
	{ HUI_COLOR_ACCENT_PRESSED,   0x2B6CD8, 255 },
	{ HUI_COLOR_ACCENT_TEXT,      0xFFFFFF, 255 },
	{ HUI_COLOR_CONTROL,          0x35353A, 255 },
	{ HUI_COLOR_CONTROL_HOVERED,  0x424248, 255 },
	{ HUI_COLOR_CONTROL_PRESSED,  0x2B2B30, 255 },
	{ HUI_COLOR_CONTROL_BORDER,   0x4C4C54, 255 },
	{ HUI_COLOR_INPUT,            0x161618, 255 },
	{ HUI_COLOR_FOCUS,            0x3B82F6, 255 },
	{ HUI_COLOR_SELECTION,        0x274B7A, 255 },
	{ HUI_COLOR_ROW_HOVERED,      0x2E2E33, 255 },
	{ HUI_COLOR_TRACK,            0x2C2C30, 255 },
	{ HUI_COLOR_HANDLE,           0x5C5C66, 255 },
	{ HUI_COLOR_HANDLE_HOVERED,   0x7C7C88, 255 },
	{ HUI_COLOR_SEPARATOR,        0x3F3F46, 255 },
	{ HUI_COLOR_HEADER,           0x2D2D31, 255 },
	{ HUI_COLOR_HEADER_HOVERED,   0x3A3A40, 255 },
	{ HUI_COLOR_TAB_ACTIVE,       0x1E1E20, 255 },
	{ HUI_COLOR_POPUP,            0x2B2B2F, 255 },
	{ HUI_COLOR_POPUP_BORDER,     0x4A4A52, 255 },
	{ HUI_COLOR_SHADOW,           0x000000, 120 },
	{ HUI_COLOR_TOOLTIP,          0x3C3C42, 255 },
	{ HUI_COLOR_TOOLTIP_TEXT,     0xE6E6EA, 255 }
};

static const hui_palette_entry_t g_light_palette[] = {
	{ HUI_COLOR_WINDOW,           0xF3F3F5, 255 },
	{ HUI_COLOR_PANEL,            0xFFFFFF, 255 },
	{ HUI_COLOR_BORDER,           0xD0D0D6, 255 },
	{ HUI_COLOR_TEXT,             0x1F1F23, 255 },
	{ HUI_COLOR_TEXT_DISABLED,    0xA0A0A8, 255 },
	{ HUI_COLOR_TEXT_PLACEHOLDER, 0x90909A, 255 },
	{ HUI_COLOR_ACCENT,           0x2563EB, 255 },
	{ HUI_COLOR_ACCENT_HOVERED,   0x3B7BF5, 255 },
	{ HUI_COLOR_ACCENT_PRESSED,   0x1D4FC4, 255 },
	{ HUI_COLOR_ACCENT_TEXT,      0xFFFFFF, 255 },
	{ HUI_COLOR_CONTROL,          0xE7E7EB, 255 },
	{ HUI_COLOR_CONTROL_HOVERED,  0xDBDBE1, 255 },
	{ HUI_COLOR_CONTROL_PRESSED,  0xCBCBD3, 255 },
	{ HUI_COLOR_CONTROL_BORDER,   0xC2C2CA, 255 },
	{ HUI_COLOR_INPUT,            0xFFFFFF, 255 },
	{ HUI_COLOR_FOCUS,            0x2563EB, 255 },
	{ HUI_COLOR_SELECTION,        0xBBD4FB, 255 },
	{ HUI_COLOR_ROW_HOVERED,      0xE9EEF6, 255 },
	{ HUI_COLOR_TRACK,            0xDDDDE2, 255 },
	{ HUI_COLOR_HANDLE,           0xA9A9B2, 255 },
	{ HUI_COLOR_HANDLE_HOVERED,   0x8A8A94, 255 },
	{ HUI_COLOR_SEPARATOR,        0xD0D0D6, 255 },
	{ HUI_COLOR_HEADER,           0xE9E9ED, 255 },
	{ HUI_COLOR_HEADER_HOVERED,   0xDDDDE3, 255 },
	{ HUI_COLOR_TAB_ACTIVE,       0xF3F3F5, 255 },
	{ HUI_COLOR_POPUP,            0xFFFFFF, 255 },
	{ HUI_COLOR_POPUP_BORDER,     0xC8C8D0, 255 },
	{ HUI_COLOR_SHADOW,           0x000000, 60 },
	{ HUI_COLOR_TOOLTIP,          0x2B2B30, 255 },
	{ HUI_COLOR_TOOLTIP_TEXT,     0xF2F2F5, 255 }
};

void hui_theme_apply(HUI_ETheme theme) {
	const hui_palette_entry_t* palette = theme == HUI_THEME_LIGHT ? g_light_palette : g_dark_palette;
	size_t count = theme == HUI_THEME_LIGHT ? HUI_COUNT_OF(g_light_palette) : HUI_COUNT_OF(g_dark_palette);
	size_t i;
	for (i = 0; i < count; i++) {
		HUI_THEME_COLOR(palette[i].role) = hui_hex(palette[i].rgb, palette[i].alpha);
	}
}

/** Metrics are the same for both palettes; called once at start-up. */
void hui_theme_reset_metrics(void) {
	HUI_THEME_METRIC(HUI_METRIC_FONT_SIZE) = 13.0f;
	HUI_THEME_METRIC(HUI_METRIC_CORNER_RADIUS) = 4.0f;
	HUI_THEME_METRIC(HUI_METRIC_BORDER_WIDTH) = 1.0f;
	HUI_THEME_METRIC(HUI_METRIC_CONTROL_HEIGHT) = 26.0f;
	HUI_THEME_METRIC(HUI_METRIC_PADDING_X) = 10.0f;
	HUI_THEME_METRIC(HUI_METRIC_PADDING_Y) = 4.0f;
	HUI_THEME_METRIC(HUI_METRIC_SPACING) = 6.0f;
	HUI_THEME_METRIC(HUI_METRIC_SCROLLBAR_SIZE) = 10.0f;
	HUI_THEME_METRIC(HUI_METRIC_ROW_HEIGHT) = 22.0f;
	HUI_THEME_METRIC(HUI_METRIC_INDENT) = 16.0f;
	HUI_THEME_METRIC(HUI_METRIC_SPLITTER_SIZE) = 5.0f;
	HUI_THEME_METRIC(HUI_METRIC_CHECK_SIZE) = 16.0f;
	HUI_THEME_METRIC(HUI_METRIC_SLIDER_HANDLE_SIZE) = 14.0f;
}

/* ---------------------------------------------------------------------------
 *  Lookup
 * ------------------------------------------------------------------------- */

hui_color_t hui_widget_color(const hui_widget_t* widget, HUI_EColor role) {
	int index = HUI_COLOR_INDEX(role);
	if (widget->style && (widget->style->color_mask & (1u << index))) return widget->style->colors[index];
	return g_hui.theme.colors[index];
}

float hui_widget_metric(const hui_widget_t* widget, HUI_EMetric metric) {
	int index = HUI_METRIC_INDEX(metric);
	if (widget->style && (widget->style->metric_mask & (1u << index))) return widget->style->metrics[index];
	return g_hui.theme.metrics[index];
}

int hui_widget_has_color(const hui_widget_t* widget, HUI_EColor role) {
	return widget->style && (widget->style->color_mask & (1u << HUI_COLOR_INDEX(role)));
}

static hui_style_t* hui_widget_style(hui_widget_t* widget) {
	if (!widget->style) widget->style = (hui_style_t*)hui_calloc(sizeof(hui_style_t));
	return widget->style;
}

void hui_widget_set_color(hui_widget_t* widget, HUI_EColor role, hui_color_t color) {
	hui_style_t* style = hui_widget_style(widget);
	int index = HUI_COLOR_INDEX(role);
	style->colors[index] = color;
	style->color_mask |= 1u << index;
	hui_widget_invalidate_paint(widget);
}

void hui_widget_set_metric(hui_widget_t* widget, HUI_EMetric metric, float value) {
	hui_style_t* style = hui_widget_style(widget);
	int index = HUI_METRIC_INDEX(metric);
	style->metrics[index] = value;
	style->metric_mask |= 1u << index;
	hui_widget_invalidate_layout(widget);
}

hui_color_t hui_widget_text_color(const hui_widget_t* widget, const HUI_Painter* painter) {
	if (!painter->enabled) return hui_widget_color(widget, HUI_COLOR_TEXT_DISABLED);
	/* the painter already carries the override of this widget or of an ancestor */
	return painter->text_color;
}

/* ---------------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------------- */

void HUI_ApplyTheme(HUI_ETheme theme) {
	HUI_API_GUARD_VOID();
	if (theme != HUI_THEME_DARK && theme != HUI_THEME_LIGHT) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_ApplyTheme: invalid theme");
		return;
	}
	hui_theme_apply(theme);
	hui_invalidate_all_windows();
}

void HUI_SetThemeColor(HUI_EColor role, float r, float g, float b, float a) {
	HUI_API_GUARD_VOID();
	if (!hui_color_valid(role)) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetThemeColor: invalid color role");
		return;
	}
	HUI_THEME_COLOR(role) = hui_rgba(r, g, b, a);
	hui_invalidate_all_windows();
}

void HUI_GetThemeColor(HUI_EColor role, float* r, float* g, float* b, float* a) {
	if (!g_hui.initialized || !hui_color_valid(role)) {
		if (r) *r = 0.0f;
		if (g) *g = 0.0f;
		if (b) *b = 0.0f;
		if (a) *a = 0.0f;
		return;
	}
	hui_color_to_floats(HUI_THEME_COLOR(role), r, g, b, a);
}

void HUI_SetThemeMetric(HUI_EMetric metric, float value) {
	HUI_API_GUARD_VOID();
	if (!hui_metric_valid(metric)) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetThemeMetric: invalid metric");
		return;
	}
	HUI_THEME_METRIC(metric) = value;
	hui_invalidate_all_windows();
}

float HUI_GetThemeMetric(HUI_EMetric metric) {
	if (!g_hui.initialized || !hui_metric_valid(metric)) return 0.0f;
	return HUI_THEME_METRIC(metric);
}

void HUI_SetThemeFont(HUI_id font) {
	HUI_API_GUARD_VOID();
	if (font != HUI_INVALID_ID && !hui_pool_get(&g_hui.fonts, font)) {
		hui_error(HUI_ERROR_INVALID_ID, HUI_SEVERITY_ERROR, "HUI_SetThemeFont: invalid font");
		return;
	}
	g_hui.theme.font = font == HUI_INVALID_ID ? g_hui.default_font : font;
	hui_invalidate_all_windows();
}

HUI_id HUI_GetThemeFont(void) {
	return g_hui.initialized ? g_hui.theme.font : HUI_INVALID_ID;
}

void HUI_SetWidgetColor(HUI_id id, HUI_EColor role, float r, float g, float b, float a) {
	HUI_WIDGET_VOID(widget, id, 0);
	if (!hui_color_valid(role)) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetWidgetColor: invalid color role");
		return;
	}
	hui_widget_set_color(widget, role, hui_rgba(r, g, b, a));
}

void HUI_ClearWidgetColor(HUI_id id, HUI_EColor role) {
	HUI_WIDGET_VOID(widget, id, 0);
	if (!hui_color_valid(role)) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_ClearWidgetColor: invalid color role");
		return;
	}
	if (widget->style) widget->style->color_mask &= ~(1u << HUI_COLOR_INDEX(role));
	hui_widget_invalidate_paint(widget);
}

void HUI_SetWidgetMetric(HUI_id id, HUI_EMetric metric, float value) {
	HUI_WIDGET_VOID(widget, id, 0);
	if (!hui_metric_valid(metric)) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetWidgetMetric: invalid metric");
		return;
	}
	hui_widget_set_metric(widget, metric, value);
}

void HUI_ClearWidgetMetric(HUI_id id, HUI_EMetric metric) {
	HUI_WIDGET_VOID(widget, id, 0);
	if (!hui_metric_valid(metric)) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_ClearWidgetMetric: invalid metric");
		return;
	}
	if (widget->style) widget->style->metric_mask &= ~(1u << HUI_METRIC_INDEX(metric));
	hui_widget_invalidate_layout(widget);
}
