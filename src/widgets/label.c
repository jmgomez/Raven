/**
 * HUI - widgets/label.c
 * Label (Slate STextBlock): one or several lines of text, optionally
 * wrapped on the width it receives.
 */

#include "common.h"

typedef struct hui_label_t {
	hui_widget_t base;
	char* text;
	HUI_id font;
	float size;
	HUI_ETextAlign align;
	int wrap;
	/** Width received at the last arrange, used to wrap; 0 before the first one. */
	float wrap_width;
} hui_label_t;

static void hui_label_init(hui_widget_t* widget) {
	hui_label_t* label = (hui_label_t*)widget;
	label->font = HUI_INVALID_ID;
	label->align = HUI_TEXT_ALIGN_LEFT;
}

static void hui_label_destroy(hui_widget_t* widget) {
	hui_free(((hui_label_t*)widget)->text);
}

typedef void (*hui_label_line_fn)(const char* line, int length, float width, int index, void* context);

/**
 * Splits the text into lines (explicit '\n' and, when `wrap_width` is
 * positive, word wrapping) and calls `visit` for each. Returns the number of
 * lines and the width of the widest one.
 */
static int hui_label_lines(const hui_label_t* label, hui_font_t* font, float size, float wrap_width,
	float* max_width, hui_label_line_fn visit, void* context) {
	const char* text = label->text ? label->text : "";
	int remaining = (int)strlen(text);
	int count = 0;
	float widest = 0.0f;

	do {
		float width = 0.0f;
		int consumed = hui_text_wrap(font, size, text, remaining, wrap_width, &width);
		int length = consumed;
		/* the line itself excludes the newline and the spaces it was cut on */
		while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == ' ' || text[length - 1] == '\t')) length--;
		if (visit) visit(text, length, width, count, context);
		widest = hui_maxf(widest, width);
		count++;
		/* a text ending with '\n' has one more, empty, line */
		if (consumed == remaining && consumed > 0 && text[consumed - 1] == '\n') {
			if (visit) visit(text + consumed, 0, 0.0f, count, context);
			count++;
		}
		text += consumed;
		remaining -= consumed;
	} while (remaining > 0);

	if (max_width) *max_width = widest;
	return count;
}

static hui_vec2_t hui_label_measure(hui_widget_t* widget) {
	hui_label_t* label = (hui_label_t*)widget;
	hui_font_t* font = hui_font_resolve(label->font);
	float size = hui_widget_font_size(widget, label->size);
	hui_font_metrics_t metrics;
	float width = 0.0f;
	int lines;

	if (!font) return hui_vec2(0.0f, 0.0f);
	hui_font_metrics(font, size, &metrics);
	lines = hui_label_lines(label, font, size, label->wrap ? label->wrap_width : 0.0f, &width, NULL, NULL);
	/* a wrapped label keeps the width it was given, or it would be re-wrapped
	 * narrower and narrower by a parent that sizes it to its content */
	if (label->wrap && label->wrap_width > 0.0f && lines > 1) width = hui_maxf(width, label->wrap_width);
	return hui_vec2(ceilf(width), ceilf(metrics.line_height * (float)lines));
}

static void hui_label_arrange(hui_widget_t* widget) {
	hui_label_t* label = (hui_label_t*)widget;
	if (label->wrap && fabsf(label->wrap_width - widget->rect.w) > 0.01f) {
		label->wrap_width = widget->rect.w;
		/* the height we announced was computed for another width */
		hui_widget_invalidate_layout(widget);
	}
}

typedef struct hui_label_paint_t {
	hui_label_t* label;
	HUI_Painter* painter;
	float size;
	float line_height;
	float align;
	hui_color_t color;
} hui_label_paint_t;

static void hui_label_paint_line(const char* line, int length, float width, int index, void* context) {
	hui_label_paint_t* paint = (hui_label_paint_t*)context;
	float x = (paint->label->base.rect.w - width) * paint->align;
	if (length <= 0) return;
	hui_paint_text(paint->painter, paint->label->font, paint->size, x, paint->line_height * (float)index,
		line, length, paint->color);
}

static void hui_label_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_label_t* label = (hui_label_t*)widget;
	hui_font_t* font = hui_font_resolve(label->font);
	hui_font_metrics_t metrics;
	hui_label_paint_t paint;
	int clipped;

	if (!font || !label->text || !label->text[0]) return;
	paint.label = label;
	paint.painter = painter;
	paint.size = hui_widget_font_size(widget, label->size);
	hui_font_metrics(font, paint.size, &metrics);
	paint.line_height = metrics.line_height;
	paint.align = label->align == HUI_TEXT_ALIGN_CENTER ? 0.5f : (label->align == HUI_TEXT_ALIGN_RIGHT ? 1.0f : 0.0f);
	paint.color = hui_widget_text_color(widget, painter);

	/* text larger than the space it received is cut at the bounds of the label */
	clipped = widget->desired.x > widget->rect.w + 0.5f || widget->desired.y > widget->rect.h + 0.5f;
	if (clipped) hui_paint_push_clip(painter, hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h));
	hui_label_lines(label, font, paint.size, label->wrap ? widget->rect.w : 0.0f, NULL, hui_label_paint_line, &paint);
	if (clipped) hui_paint_pop_clip(painter);
}

static int hui_label_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_label_t* label = (hui_label_t*)widget;
	hui_color_t color;
	if (HUI_ATTR("Text")) {
		hui_str_set(&label->text, value);
	} else if (HUI_ATTR("Font")) {
		HUI_id font = hui_xml_font(value);
		if (font == HUI_INVALID_ID) return HUI_FALSE;
		label->font = font;
	} else if (HUI_ATTR("FontSize") || HUI_ATTR("TextSize")) {
		if (!hui_parse_float(value, &label->size)) return HUI_FALSE;
	} else if (HUI_ATTR("Align") || HUI_ATTR("TextAlign")) {
		if (hui_stricmp(value, "Left") == 0) label->align = HUI_TEXT_ALIGN_LEFT;
		else if (hui_stricmp(value, "Center") == 0) label->align = HUI_TEXT_ALIGN_CENTER;
		else if (hui_stricmp(value, "Right") == 0) label->align = HUI_TEXT_ALIGN_RIGHT;
		else return HUI_FALSE;
	} else if (HUI_ATTR("Wrap")) {
		if (!hui_parse_bool(value, &label->wrap)) return HUI_FALSE;
	} else if (HUI_ATTR("Color")) {
		if (!hui_parse_color(value, &color)) return HUI_FALSE;
		hui_widget_set_color(widget, HUI_COLOR_TEXT, color);
	} else {
		return HUI_FALSE;
	}
	hui_widget_invalidate_layout(widget);
	return HUI_TRUE;
}

const hui_widget_class_t hui_class_label = {
	.type = HUI_WIDGET_LABEL,
	.name = "Label",
	.size = sizeof(hui_label_t),
	.flags = HUI_CF_NO_HIT | HUI_CF_NO_CHILDREN,
	.init = hui_label_init,
	.destroy = hui_label_destroy,
	.measure = hui_label_measure,
	.arrange = hui_label_arrange,
	.paint = hui_label_paint,
	.set_attribute = hui_label_set_attribute
};

/* ---------------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------------- */

HUI_id HUI_CreateLabel(const char* text) {
	hui_widget_t* widget;
	HUI_API_GUARD(HUI_INVALID_ID);
	widget = hui_widget_create(HUI_WIDGET_LABEL);
	if (!widget) return HUI_INVALID_ID;
	((hui_label_t*)widget)->text = hui_strdup(text);
	return widget->id;
}

void HUI_SetLabelText(HUI_id id, const char* text) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_LABEL);
	if (hui_str_set(&((hui_label_t*)widget)->text, text)) hui_widget_invalidate_layout(widget);
}

const char* HUI_GetLabelText(HUI_id id) {
	HUI_WIDGET(widget, id, HUI_WIDGET_LABEL, "");
	return ((hui_label_t*)widget)->text ? ((hui_label_t*)widget)->text : "";
}

void HUI_SetLabelFont(HUI_id id, HUI_id font) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_LABEL);
	if (font != HUI_INVALID_ID && !HUI_IsValidFont(font)) {
		hui_error(HUI_ERROR_INVALID_ID, HUI_SEVERITY_ERROR, "HUI_SetLabelFont: invalid font");
		return;
	}
	((hui_label_t*)widget)->font = font;
	hui_widget_invalidate_layout(widget);
}

void HUI_SetLabelTextSize(HUI_id id, float size) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_LABEL);
	((hui_label_t*)widget)->size = size > 0.0f ? size : 0.0f;
	hui_widget_invalidate_layout(widget);
}

void HUI_SetLabelTextAlign(HUI_id id, HUI_ETextAlign align) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_LABEL);
	if (align < HUI_TEXT_ALIGN_LEFT || align > HUI_TEXT_ALIGN_RIGHT) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetLabelTextAlign: invalid alignment");
		return;
	}
	((hui_label_t*)widget)->align = align;
	hui_widget_invalidate_paint(widget);
}

void HUI_SetLabelWrap(HUI_id id, int wrap) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_LABEL);
	((hui_label_t*)widget)->wrap = wrap != 0;
	hui_widget_invalidate_layout(widget);
}

void HUI_SetLabelColor(HUI_id id, float r, float g, float b, float a) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_LABEL);
	hui_widget_set_color(widget, HUI_COLOR_TEXT, hui_rgba(r, g, b, a));
}
