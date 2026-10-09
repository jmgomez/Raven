/**
 * HUI - widgets/image.c
 * Image (Slate SImage): a texture, or a part of one, fitted in the widget.
 */

#include "common.h"
#include "../render/renderer.h"

typedef struct hui_image_t {
	hui_widget_t base;
	HUI_id texture;
	hui_color_t tint;
	HUI_EStretch stretch;
	float uv[4];
	float radius;
} hui_image_t;

static void hui_image_init(hui_widget_t* widget) {
	hui_image_t* image = (hui_image_t*)widget;
	image->texture = HUI_INVALID_ID;
	image->tint = 0xFFFFFFFFu;
	image->stretch = HUI_STRETCH_FIT;
	image->uv[2] = 1.0f;
	image->uv[3] = 1.0f;
}

/* Size of the shown part of the texture, in texels. */
static hui_vec2_t hui_image_source_size(const hui_image_t* image) {
	hui_texture_t* texture = hui_texture_get(image->texture);
	if (!texture) return hui_vec2(0.0f, 0.0f);
	return hui_vec2((float)texture->width * fabsf(image->uv[2] - image->uv[0]),
		(float)texture->height * fabsf(image->uv[3] - image->uv[1]));
}

static hui_vec2_t hui_image_measure(hui_widget_t* widget) {
	/* one texel per logical unit unless the widget is given another size */
	return hui_image_source_size((hui_image_t*)widget);
}

/**
 * Computes where a source of `size` texels goes in a `width` x `height` area
 * and which part of it is visible. Shared with the viewport widget.
 */
void hui_image_fit(HUI_EStretch stretch, hui_vec2_t size, float width, float height, const float uv_in[4],
	hui_rect_t* rect, float uv_out[4]) {
	float scale;
	memcpy(uv_out, uv_in, sizeof(float) * 4);
	*rect = hui_rect(0.0f, 0.0f, width, height);
	if (size.x <= 0.0f || size.y <= 0.0f || width <= 0.0f || height <= 0.0f) return;

	switch (stretch) {
		case HUI_STRETCH_FIT:
			scale = hui_minf(width / size.x, height / size.y);
			rect->w = size.x * scale;
			rect->h = size.y * scale;
			rect->x = (width - rect->w) * 0.5f;
			rect->y = (height - rect->h) * 0.5f;
			break;
		case HUI_STRETCH_COVER: {
			/* keep the whole area, show the centered part of the source that has its ratio */
			float visible_u, visible_v, du = uv_in[2] - uv_in[0], dv = uv_in[3] - uv_in[1];
			scale = hui_maxf(width / size.x, height / size.y);
			visible_u = width / (size.x * scale);
			visible_v = height / (size.y * scale);
			uv_out[0] = uv_in[0] + du * (1.0f - visible_u) * 0.5f;
			uv_out[2] = uv_in[2] - du * (1.0f - visible_u) * 0.5f;
			uv_out[1] = uv_in[1] + dv * (1.0f - visible_v) * 0.5f;
			uv_out[3] = uv_in[3] - dv * (1.0f - visible_v) * 0.5f;
			break;
		}
		case HUI_STRETCH_NONE:
			rect->w = size.x;
			rect->h = size.y;
			rect->x = floorf((width - size.x) * 0.5f);
			rect->y = floorf((height - size.y) * 0.5f);
			break;
		case HUI_STRETCH_FILL:
		default:
			break;
	}
}

static void hui_image_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_image_t* image = (hui_image_t*)widget;
	hui_rect_t rect;
	float uv[4];
	hui_color_t tint = image->tint;
	int clip;

	if (image->texture == HUI_INVALID_ID) return;
	hui_image_fit(image->stretch, hui_image_source_size(image), widget->rect.w, widget->rect.h, image->uv, &rect, uv);
	if (!painter->enabled) tint = hui_color_fade(tint, 0.4f);
	/* an unscaled image larger than the widget is cut at its bounds */
	clip = image->stretch == HUI_STRETCH_NONE && (rect.w > widget->rect.w || rect.h > widget->rect.h);
	if (clip) hui_paint_push_clip(painter, hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h));
	hui_paint_image(painter, rect, image->texture, uv, tint, image->radius);
	if (clip) hui_paint_pop_clip(painter);
}

int hui_parse_stretch(const char* value, HUI_EStretch* stretch) {
	if (hui_stricmp(value, "Fill") == 0) *stretch = HUI_STRETCH_FILL;
	else if (hui_stricmp(value, "Fit") == 0) *stretch = HUI_STRETCH_FIT;
	else if (hui_stricmp(value, "Cover") == 0) *stretch = HUI_STRETCH_COVER;
	else if (hui_stricmp(value, "None") == 0) *stretch = HUI_STRETCH_NONE;
	else return HUI_FALSE;
	return HUI_TRUE;
}

static int hui_image_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_image_t* image = (hui_image_t*)widget;
	if (HUI_ATTR("Source")) {
		image->texture = hui_xml_texture(value);
		if (image->texture == HUI_INVALID_ID) return HUI_FALSE;
	} else if (HUI_ATTR("Tint")) {
		if (!hui_parse_color(value, &image->tint)) return HUI_FALSE;
	} else if (HUI_ATTR("Stretch")) {
		if (!hui_parse_stretch(value, &image->stretch)) return HUI_FALSE;
	} else if (HUI_ATTR("Region")) {
		if (hui_parse_floats(value, image->uv, 4) != 4) return HUI_FALSE;
	} else if (HUI_ATTR("CornerRadius")) {
		if (!hui_parse_float(value, &image->radius)) return HUI_FALSE;
	} else {
		return HUI_FALSE;
	}
	hui_widget_invalidate_layout(widget);
	return HUI_TRUE;
}

const hui_widget_class_t hui_class_image = {
	.type = HUI_WIDGET_IMAGE,
	.name = "Image",
	.size = sizeof(hui_image_t),
	.flags = HUI_CF_NO_CHILDREN | HUI_CF_NO_HIT,
	.init = hui_image_init,
	.measure = hui_image_measure,
	.paint = hui_image_paint,
	.set_attribute = hui_image_set_attribute
};

void HUI_SetImageTexture(HUI_id id, HUI_id texture) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_IMAGE);
	if (texture != HUI_INVALID_ID && !hui_texture_get(texture)) {
		hui_error(HUI_ERROR_INVALID_ID, HUI_SEVERITY_ERROR, "HUI_SetImageTexture: invalid texture");
		return;
	}
	((hui_image_t*)widget)->texture = texture;
	hui_widget_invalidate_layout(widget);
}

void HUI_SetImageTintColor(HUI_id id, float r, float g, float b, float a) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_IMAGE);
	((hui_image_t*)widget)->tint = hui_rgba(r, g, b, a);
	hui_widget_invalidate_paint(widget);
}

void HUI_SetImageStretch(HUI_id id, HUI_EStretch stretch) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_IMAGE);
	if (stretch < HUI_STRETCH_FILL || stretch > HUI_STRETCH_NONE) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetImageStretch: invalid stretch mode");
		return;
	}
	((hui_image_t*)widget)->stretch = stretch;
	hui_widget_invalidate_paint(widget);
}

void HUI_SetImageRegion(HUI_id id, float min_u, float min_v, float max_u, float max_v) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_IMAGE);
	hui_image_t* image = (hui_image_t*)widget;
	image->uv[0] = min_u;
	image->uv[1] = min_v;
	image->uv[2] = max_u;
	image->uv[3] = max_v;
	hui_widget_invalidate_layout(widget);
}

void HUI_SetImageCornerRadius(HUI_id id, float radius) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_IMAGE);
	((hui_image_t*)widget)->radius = radius > 0.0f ? radius : 0.0f;
	hui_widget_invalidate_paint(widget);
}
