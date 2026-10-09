/**
 * HUI - render/draw.c
 * Draw list storage and painter primitives.
 */

#include "draw.h"
#include "font.h"
#include "renderer.h"
#include "../core/context.h"

/* ---------------------------------------------------------------------------
 *  Draw list
 * ------------------------------------------------------------------------- */

void hui_drawlist_init(hui_drawlist_t* list) {
	memset(list, 0, sizeof(*list));
}

void hui_drawlist_free(hui_drawlist_t* list) {
	hui_free(list->instances);
	hui_free(list->commands);
	memset(list, 0, sizeof(*list));
}

void hui_drawlist_reset(hui_drawlist_t* list) {
	list->instance_count = 0;
	list->command_count = 0;
}

static HUI_DrawCommand* hui_drawlist_new_command(hui_drawlist_t* list, HUI_id texture) {
	HUI_DrawCommand* command;
	HUI_ARRAY_RESERVE(list->commands, list->command_cap, list->command_count + 1);
	command = &list->commands[list->command_count++];
	memset(command, 0, sizeof(*command));
	command->type = HUI_DRAW_INSTANCES;
	command->texture = texture;
	command->viewport = HUI_INVALID_ID;
	command->first_instance = list->instance_count;
	return command;
}

HUI_DrawInstance* hui_drawlist_add(hui_drawlist_t* list, HUI_id texture) {
	HUI_DrawCommand* command = list->command_count ? &list->commands[list->command_count - 1] : NULL;

	if (!command || command->type != HUI_DRAW_INSTANCES) {
		command = hui_drawlist_new_command(list, texture);
	} else if (texture != HUI_INVALID_ID && command->texture != texture) {
		/* a batch that only holds untextured instances can adopt the texture */
		if (command->texture == HUI_INVALID_ID) command->texture = texture;
		else command = hui_drawlist_new_command(list, texture);
	}
	HUI_ARRAY_RESERVE(list->instances, list->instance_cap, list->instance_count + 1);
	command->instance_count++;
	return &list->instances[list->instance_count++];
}

void hui_drawlist_add_callback(hui_drawlist_t* list, const HUI_DrawCommand* callback) {
	HUI_DrawCommand* command;
	HUI_ARRAY_RESERVE(list->commands, list->command_cap, list->command_count + 1);
	command = &list->commands[list->command_count++];
	*command = *callback;
	command->type = HUI_DRAW_CALLBACK;
	command->first_instance = list->instance_count;
	command->instance_count = 0;
}

void hui_drawlist_append(hui_drawlist_t* list, const hui_drawlist_t* source) {
	uint32_t base = list->instance_count;
	if (source->instance_count) {
		HUI_ARRAY_RESERVE(list->instances, list->instance_cap, list->instance_count + source->instance_count);
		memcpy(list->instances + list->instance_count, source->instances,
			sizeof(HUI_DrawInstance) * source->instance_count);
		list->instance_count += source->instance_count;
	}
	hui_drawlist_append_commands(list, source, base);
}

void hui_drawlist_append_commands(hui_drawlist_t* list, const hui_drawlist_t* source, uint32_t base) {
	uint32_t i;
	for (i = 0; i < source->command_count; i++) {
		const HUI_DrawCommand* in = &source->commands[i];
		HUI_DrawCommand* last = list->command_count ? &list->commands[list->command_count - 1] : NULL;
		if (in->type == HUI_DRAW_INSTANCES && in->instance_count == 0) continue;
		if (in->type == HUI_DRAW_INSTANCES && last && last->type == HUI_DRAW_INSTANCES &&
			last->first_instance + last->instance_count == base + in->first_instance &&
			(last->texture == in->texture || last->texture == HUI_INVALID_ID || in->texture == HUI_INVALID_ID)) {
			if (last->texture == HUI_INVALID_ID) last->texture = in->texture;
			last->instance_count += in->instance_count;
			continue;
		}
		HUI_ARRAY_RESERVE(list->commands, list->command_cap, list->command_count + 1);
		list->commands[list->command_count] = *in;
		list->commands[list->command_count].first_instance = base + in->first_instance;
		list->command_count++;
	}
}

/* ---------------------------------------------------------------------------
 *  Painter helpers
 * ------------------------------------------------------------------------- */

hui_box_t hui_paint_snap(const HUI_Painter* painter, hui_rect_t rect) {
	hui_box_t box;
	box.x0 = floorf((painter->ox + rect.x) * painter->scale + 0.5f);
	box.y0 = floorf((painter->oy + rect.y) * painter->scale + 0.5f);
	box.x1 = floorf((painter->ox + rect.x + rect.w) * painter->scale + 0.5f);
	box.y1 = floorf((painter->oy + rect.y + rect.h) * painter->scale + 0.5f);
	return box;
}

/** Thickness in pixels of a logical line width: never thinner than one pixel. */
static float hui_paint_stroke(const HUI_Painter* painter, float width) {
	float pixels;
	if (width <= 0.0f) return 0.0f;
	pixels = floorf(width * painter->scale + 0.5f);
	return pixels < 1.0f ? 1.0f : pixels;
}

static hui_color_t hui_paint_color(const HUI_Painter* painter, hui_color_t color) {
	return painter->opacity < 0.999f ? hui_color_fade(color, painter->opacity) : color;
}

static void hui_instance_clip(HUI_DrawInstance* instance, const HUI_Painter* painter) {
	instance->clip[0] = painter->clip.x0;
	instance->clip[1] = painter->clip.y0;
	instance->clip[2] = painter->clip.x1;
	instance->clip[3] = painter->clip.y1;
}

/* Appends an instance unless `bounds` is entirely clipped. */
static HUI_DrawInstance* hui_paint_emit(HUI_Painter* painter, hui_box_t bounds, HUI_id texture) {
	HUI_DrawInstance* instance;
	if (!hui_box_overlaps(bounds, painter->clip)) return NULL;
	instance = hui_drawlist_add(painter->list, texture);
	memset(instance, 0, sizeof(*instance));
	hui_instance_clip(instance, painter);
	return instance;
}

/* ---------------------------------------------------------------------------
 *  Primitives
 * ------------------------------------------------------------------------- */

void hui_paint_box4(HUI_Painter* painter, hui_rect_t rect, const float radii[4], hui_color_t fill,
	float border_width, hui_color_t border_color) {

	HUI_DrawInstance* instance;
	hui_box_t box;
	float limit;
	int i;

	fill = hui_paint_color(painter, fill);
	border_color = hui_paint_color(painter, border_color);
	if (border_width <= 0.0f || hui_color_a(border_color) == 0) {
		border_width = 0.0f;
		border_color = 0;
	}
	if (hui_color_a(fill) == 0 && border_width <= 0.0f) return;

	box = hui_paint_snap(painter, rect);
	if (box.x1 <= box.x0 || box.y1 <= box.y0) return;
	instance = hui_paint_emit(painter, box, HUI_INVALID_ID);
	if (!instance) return;

	limit = hui_minf(box.x1 - box.x0, box.y1 - box.y0) * 0.5f;
	instance->rect[0] = box.x0;
	instance->rect[1] = box.y0;
	instance->rect[2] = box.x1;
	instance->rect[3] = box.y1;
	for (i = 0; i < 4; i++) {
		instance->radii[i] = radii ? hui_clampf(radii[i] * painter->scale, 0.0f, limit) : 0.0f;
	}
	instance->color = fill;
	instance->border_color = border_color;
	instance->border_width = hui_paint_stroke(painter, border_width);
	instance->flags = HUI_PRIM_RECT;
}

void hui_paint_box(HUI_Painter* painter, hui_rect_t rect, float radius, hui_color_t fill,
	float border_width, hui_color_t border_color) {
	float radii[4];
	radii[0] = radii[1] = radii[2] = radii[3] = radius;
	hui_paint_box4(painter, rect, radii, fill, border_width, border_color);
}

void hui_paint_rect(HUI_Painter* painter, hui_rect_t rect, hui_color_t color) {
	hui_paint_box4(painter, rect, NULL, color, 0.0f, 0);
}

void hui_paint_shadow(HUI_Painter* painter, hui_rect_t rect, float radius, float blur, hui_color_t color) {
	HUI_DrawInstance* instance;
	hui_box_t box, bounds;
	float blur_px = blur * painter->scale;
	float limit;

	color = hui_paint_color(painter, color);
	if (hui_color_a(color) == 0 || blur_px <= 0.0f) return;
	box = hui_paint_snap(painter, rect);
	bounds = hui_box(box.x0 - blur_px, box.y0 - blur_px, box.x1 + blur_px, box.y1 + blur_px);
	instance = hui_paint_emit(painter, bounds, HUI_INVALID_ID);
	if (!instance) return;
	limit = hui_minf(box.x1 - box.x0, box.y1 - box.y0) * 0.5f;
	instance->rect[0] = box.x0;
	instance->rect[1] = box.y0;
	instance->rect[2] = box.x1;
	instance->rect[3] = box.y1;
	instance->radii[0] = instance->radii[1] = instance->radii[2] = instance->radii[3] =
		hui_clampf(radius * painter->scale, 0.0f, limit);
	instance->color = color;
	instance->border_width = blur_px;
	instance->flags = HUI_PRIM_SHADOW;
}

void hui_paint_line(HUI_Painter* painter, float x0, float y0, float x1, float y1, float thickness, hui_color_t color) {
	HUI_DrawInstance* instance;
	hui_box_t bounds;
	float ax = (painter->ox + x0) * painter->scale, ay = (painter->oy + y0) * painter->scale;
	float bx = (painter->ox + x1) * painter->scale, by = (painter->oy + y1) * painter->scale;
	float width = hui_maxf(1.0f, thickness * painter->scale);
	float extent = width * 0.5f + 1.0f;

	color = hui_paint_color(painter, color);
	if (hui_color_a(color) == 0) return;
	bounds = hui_box(hui_minf(ax, bx) - extent, hui_minf(ay, by) - extent,
		hui_maxf(ax, bx) + extent, hui_maxf(ay, by) + extent);
	instance = hui_paint_emit(painter, bounds, HUI_INVALID_ID);
	if (!instance) return;
	instance->rect[0] = ax;
	instance->rect[1] = ay;
	instance->rect[2] = bx;
	instance->rect[3] = by;
	instance->radii[0] = width;
	instance->color = color;
	instance->flags = HUI_PRIM_LINE;
}

void hui_paint_triangle(HUI_Painter* painter, float x0, float y0, float x1, float y1, float x2, float y2, hui_color_t color) {
	HUI_DrawInstance* instance;
	hui_box_t bounds;
	float ax = (painter->ox + x0) * painter->scale, ay = (painter->oy + y0) * painter->scale;
	float bx = (painter->ox + x1) * painter->scale, by = (painter->oy + y1) * painter->scale;
	float cx = (painter->ox + x2) * painter->scale, cy = (painter->oy + y2) * painter->scale;

	color = hui_paint_color(painter, color);
	if (hui_color_a(color) == 0) return;
	bounds = hui_box(hui_minf(ax, hui_minf(bx, cx)) - 1.0f, hui_minf(ay, hui_minf(by, cy)) - 1.0f,
		hui_maxf(ax, hui_maxf(bx, cx)) + 1.0f, hui_maxf(ay, hui_maxf(by, cy)) + 1.0f);
	instance = hui_paint_emit(painter, bounds, HUI_INVALID_ID);
	if (!instance) return;
	instance->rect[0] = ax;
	instance->rect[1] = ay;
	instance->rect[2] = bx;
	instance->rect[3] = by;
	instance->radii[0] = cx;
	instance->radii[1] = cy;
	instance->color = color;
	instance->flags = HUI_PRIM_TRIANGLE;
}

void hui_paint_image(HUI_Painter* painter, hui_rect_t rect, HUI_id texture, const float uv[4],
	hui_color_t tint, float radius) {

	HUI_DrawInstance* instance;
	hui_texture_t* object = hui_texture_get(texture);
	hui_box_t box;
	float limit, r;

	/* a texture still being decoded draws nothing */
	if (!object || !object->created) return;
	tint = hui_paint_color(painter, tint);
	if (hui_color_a(tint) == 0) return;
	box = hui_paint_snap(painter, rect);
	if (box.x1 <= box.x0 || box.y1 <= box.y0) return;
	instance = hui_paint_emit(painter, box, texture);
	if (!instance) return;
	limit = hui_minf(box.x1 - box.x0, box.y1 - box.y0) * 0.5f;
	r = hui_clampf(radius * painter->scale, 0.0f, limit);
	instance->rect[0] = box.x0;
	instance->rect[1] = box.y0;
	instance->rect[2] = box.x1;
	instance->rect[3] = box.y1;
	instance->uv[0] = uv ? uv[0] : 0.0f;
	instance->uv[1] = uv ? uv[1] : 0.0f;
	instance->uv[2] = uv ? uv[2] : 1.0f;
	instance->uv[3] = uv ? uv[3] : 1.0f;
	instance->radii[0] = instance->radii[1] = instance->radii[2] = instance->radii[3] = r;
	instance->color = tint;
	instance->flags = HUI_PRIM_RECT | HUI_INSTANCE_TEXTURED;
}

void hui_paint_text(HUI_Painter* painter, HUI_id font_id, float size, float x, float y,
	const char* text, int length, hui_color_t color) {
	hui_font_t* font = hui_font_resolve(font_id);
	if (!font || !text) return;
	size = hui_font_size_resolve(size);
	color = hui_paint_color(painter, color);
	hui_text_draw(painter->list, font, size * painter->scale,
		floorf((painter->ox + x) * painter->scale + 0.5f),
		floorf((painter->oy + y) * painter->scale + 0.5f),
		text, length, color, painter->clip);
}

void hui_paint_text_in(HUI_Painter* painter, HUI_id font_id, float size, hui_rect_t rect, float align,
	const char* text, int length, hui_color_t color) {
	hui_font_t* font = hui_font_resolve(font_id);
	hui_font_metrics_t metrics;
	float width;
	if (!font || !text) return;
	size = hui_font_size_resolve(size);
	hui_font_metrics(font, size, &metrics);
	width = align > 0.0f ? hui_text_width(font, size, text, length) : 0.0f;
	hui_paint_text(painter, font_id, size, rect.x + (rect.w - width) * align,
		rect.y + (rect.h - metrics.line_height) * 0.5f, text, length, color);
}

void hui_paint_icon(HUI_Painter* painter, hui_icon_t icon, float cx, float cy, float size, hui_color_t color) {
	float h = size * 0.5f, q = size * 0.25f;
	float t = hui_maxf(1.25f, size * 0.14f);
	switch (icon) {
		case HUI_ICON_CHEVRON_RIGHT:
			hui_paint_line(painter, cx - q, cy - h, cx + q, cy, t, color);
			hui_paint_line(painter, cx + q, cy, cx - q, cy + h, t, color);
			break;
		case HUI_ICON_CHEVRON_LEFT:
			hui_paint_line(painter, cx + q, cy - h, cx - q, cy, t, color);
			hui_paint_line(painter, cx - q, cy, cx + q, cy + h, t, color);
			break;
		case HUI_ICON_CHEVRON_DOWN:
			hui_paint_line(painter, cx - h, cy - q, cx, cy + q, t, color);
			hui_paint_line(painter, cx, cy + q, cx + h, cy - q, t, color);
			break;
		case HUI_ICON_CHEVRON_UP:
			hui_paint_line(painter, cx - h, cy + q, cx, cy - q, t, color);
			hui_paint_line(painter, cx, cy - q, cx + h, cy + q, t, color);
			break;
		case HUI_ICON_CHECK:
			hui_paint_line(painter, cx - h, cy, cx - h * 0.25f, cy + h * 0.7f, t, color);
			hui_paint_line(painter, cx - h * 0.25f, cy + h * 0.7f, cx + h, cy - h * 0.6f, t, color);
			break;
		case HUI_ICON_CROSS:
			hui_paint_line(painter, cx - h, cy - h, cx + h, cy + h, t, color);
			hui_paint_line(painter, cx - h, cy + h, cx + h, cy - h, t, color);
			break;
	}
}

void hui_paint_push_clip(HUI_Painter* painter, hui_rect_t rect) {
	if (painter->clip_depth < HUI_PAINTER_CLIP_STACK) {
		painter->clip_stack[painter->clip_depth] = painter->clip;
	}
	painter->clip_depth++;
	painter->clip = hui_box_intersect(painter->clip, hui_paint_snap(painter, rect));
}

void hui_paint_pop_clip(HUI_Painter* painter) {
	if (painter->clip_depth <= 0) return;
	painter->clip_depth--;
	if (painter->clip_depth < HUI_PAINTER_CLIP_STACK) {
		painter->clip = painter->clip_stack[painter->clip_depth];
	}
}

/* ---------------------------------------------------------------------------
 *  Public drawing API (custom widgets)
 * ------------------------------------------------------------------------- */

void HUI_DrawRect(HUI_Painter* painter, float x, float y, float width, float height,
	float radius, float r, float g, float b, float a) {
	if (!painter) return;
	hui_paint_box(painter, hui_rect(x, y, width, height), radius, hui_rgba(r, g, b, a), 0.0f, 0);
}

void HUI_DrawRectOutline(HUI_Painter* painter, float x, float y, float width, float height,
	float radius, float thickness, float r, float g, float b, float a) {
	if (!painter) return;
	hui_paint_box(painter, hui_rect(x, y, width, height), radius, 0, thickness, hui_rgba(r, g, b, a));
}

void HUI_DrawCircle(HUI_Painter* painter, float center_x, float center_y, float radius,
	float r, float g, float b, float a) {
	if (!painter) return;
	hui_paint_box(painter, hui_rect(center_x - radius, center_y - radius, radius * 2.0f, radius * 2.0f),
		radius, hui_rgba(r, g, b, a), 0.0f, 0);
}

void HUI_DrawLine(HUI_Painter* painter, float x0, float y0, float x1, float y1,
	float thickness, float r, float g, float b, float a) {
	if (!painter) return;
	hui_paint_line(painter, x0, y0, x1, y1, thickness, hui_rgba(r, g, b, a));
}

void HUI_DrawTriangle(HUI_Painter* painter, float x0, float y0, float x1, float y1,
	float x2, float y2, float r, float g, float b, float a) {
	if (!painter) return;
	hui_paint_triangle(painter, x0, y0, x1, y1, x2, y2, hui_rgba(r, g, b, a));
}

void HUI_DrawImage(HUI_Painter* painter, HUI_id texture, float x, float y, float width, float height,
	float r, float g, float b, float a) {
	if (!painter) return;
	hui_paint_image(painter, hui_rect(x, y, width, height), texture, NULL, hui_rgba(r, g, b, a), 0.0f);
}

void HUI_DrawText(HUI_Painter* painter, HUI_id font, float size, float x, float y,
	const char* text, float r, float g, float b, float a) {
	hui_font_t* resolved;
	hui_font_metrics_t metrics;
	const char* line = text;
	if (!painter || !text) return;
	resolved = hui_font_resolve(font);
	if (!resolved) return;
	size = hui_font_size_resolve(size);
	hui_font_metrics(resolved, size, &metrics);
	for (;;) {
		const char* newline = strchr(line, '\n');
		int length = newline ? (int)(newline - line) : (int)strlen(line);
		hui_paint_text(painter, font, size, x, y, line, length, hui_rgba(r, g, b, a));
		if (!newline) break;
		line = newline + 1;
		y += metrics.line_height;
	}
}

void HUI_PushClipRect(HUI_Painter* painter, float x, float y, float width, float height) {
	if (!painter) return;
	hui_paint_push_clip(painter, hui_rect(x, y, width, height));
}

void HUI_PopClipRect(HUI_Painter* painter) {
	if (!painter) return;
	hui_paint_pop_clip(painter);
}
