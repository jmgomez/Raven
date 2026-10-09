/**
 * HUI - widgets/viewport.c
 * Viewport: an area of the layout showing content rendered by the
 * application (see hui_viewport.h for the three modes), and Custom: a widget
 * measured and painted by application callbacks.
 */

#include "common.h"
#include "../render/renderer.h"

/* ===========================================================================
 *  Viewport
 * ========================================================================= */

typedef struct hui_viewport_t {
	hui_widget_t base;
	/* mode 1: texture */
	HUI_id texture;
	HUI_EStretch stretch;
	int flip_y;
	/* mode 2: draw callback */
	HUI_CViewportDraw draw_callback;
	void* draw_user;
	/* mode 3: native child window */
	hui_pwindow_t* native;
	/** Frame at which the viewport was last painted, and where (pixels). */
	uint64_t native_frame;
	int native_rect[4];
	int native_shown;

	int realtime;
	int pixel_width, pixel_height;
	HUI_CViewportResize resize_callback;
	void* resize_user;
} hui_viewport_t;

static void hui_viewport_init(hui_widget_t* widget) {
	hui_viewport_t* viewport = (hui_viewport_t*)widget;
	viewport->texture = HUI_INVALID_ID;
	viewport->stretch = HUI_STRETCH_FILL;
}

/* Removes the viewport from the native list of a window and destroys its child window. */
void hui_viewport_native_release(hui_widget_t* widget, hui_window_t* window) {
	hui_viewport_t* viewport = (hui_viewport_t*)widget;
	uint32_t i;
	for (i = 0; window && i < window->native_viewport_count; i++) {
		if (window->native_viewports[i] != widget) continue;
		window->native_viewports[i] = window->native_viewports[--window->native_viewport_count];
		break;
	}
	if (viewport->native) {
		g_hui.platform->window_destroy(viewport->native);
		viewport->native = NULL;
	}
	viewport->native_shown = HUI_FALSE;
}

/* Called on the UI thread after each paint: moves, shows or hides the child window. */
void hui_viewport_native_sync(hui_widget_t* widget, uint64_t frame) {
	hui_viewport_t* viewport = (hui_viewport_t*)widget;
	int visible = viewport->native_frame == frame && viewport->native_rect[2] > 0 && viewport->native_rect[3] > 0;
	if (!viewport->native) return;
	if (!visible && !viewport->native_shown) return;
	g_hui.platform->child_set_rect(viewport->native, viewport->native_rect[0], viewport->native_rect[1],
		viewport->native_rect[2], viewport->native_rect[3], visible);
	viewport->native_shown = visible;
}

static void hui_viewport_destroy(hui_widget_t* widget) {
	/* the child window was released when the viewport left its window; this is a safety net */
	hui_viewport_t* viewport = (hui_viewport_t*)widget;
	if (viewport->native && g_hui.platform) g_hui.platform->window_destroy(viewport->native);
	viewport->native = NULL;
}

static hui_vec2_t hui_viewport_measure(hui_widget_t* widget) {
	HUI_UNUSED(widget);
	/* a viewport takes the size it is given; this is only a visible minimum */
	return hui_vec2(64.0f, 64.0f);
}

static void hui_viewport_arrange(hui_widget_t* widget) {
	hui_viewport_t* viewport = (hui_viewport_t*)widget;
	float scale = widget->window ? widget->window->scale : 1.0f;
	int width = (int)floorf(widget->rect.w * scale + 0.5f), height = (int)floorf(widget->rect.h * scale + 0.5f);
	if (width == viewport->pixel_width && height == viewport->pixel_height) return;
	viewport->pixel_width = width;
	viewport->pixel_height = height;
	if (viewport->resize_callback && width > 0 && height > 0) {
		hui_dispatch_begin();
		viewport->resize_callback(widget->id, width, height, viewport->resize_user);
		hui_dispatch_end();
	}
}

static void hui_viewport_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_viewport_t* viewport = (hui_viewport_t*)widget;
	hui_rect_t rect = hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h);
	hui_box_t box = hui_paint_snap(painter, rect);
	hui_box_t visible = hui_box_intersect(box, painter->clip);
	hui_texture_t* texture = hui_texture_get(viewport->texture);

	if (viewport->realtime && painter->window) hui_atomic_add(&painter->window->realtime_viewports, 1);

	if (viewport->native) {
		/* the child window covers this area: nothing of ours would be seen */
		viewport->native_frame = painter->window ? painter->window->frame_index : 0;
		viewport->native_rect[0] = (int)box.x0;
		viewport->native_rect[1] = (int)box.y0;
		viewport->native_rect[2] = (int)(box.x1 - box.x0);
		viewport->native_rect[3] = (int)(box.y1 - box.y0);
		return;
	}

	if (texture && texture->created) {
		static const float full[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
		hui_rect_t target;
		float uv[4];
		hui_image_fit(viewport->stretch, hui_vec2((float)texture->width, (float)texture->height),
			rect.w, rect.h, full, &target, uv);
		if (viewport->flip_y) {
			float top = uv[1];
			uv[1] = uv[3];
			uv[3] = top;
		}
		if (viewport->stretch != HUI_STRETCH_FILL && viewport->stretch != HUI_STRETCH_COVER) {
			hui_paint_rect(painter, rect, hui_rgba8(0, 0, 0, 255));
		}
		hui_paint_push_clip(painter, rect);
		hui_paint_image(painter, target, viewport->texture, uv, 0xFFFFFFFFu, 0.0f);
		hui_paint_pop_clip(painter);
	} else {
		hui_paint_rect(painter, rect, hui_rgba8(0, 0, 0, 255));
	}

	if (viewport->draw_callback && !hui_box_empty(visible)) {
		HUI_DrawCommand command;
		memset(&command, 0, sizeof(command));
		command.callback = viewport->draw_callback;
		command.user_data = viewport->draw_user;
		command.viewport = widget->id;
		command.texture = HUI_INVALID_ID;
		command.rect[0] = (int)box.x0;
		command.rect[1] = (int)box.y0;
		command.rect[2] = (int)(box.x1 - box.x0);
		command.rect[3] = (int)(box.y1 - box.y0);
		command.clip[0] = (int)visible.x0;
		command.clip[1] = (int)visible.y0;
		command.clip[2] = (int)(visible.x1 - visible.x0);
		command.clip[3] = (int)(visible.y1 - visible.y0);
		hui_drawlist_add_callback(painter->list, &command);
	}
}

static void hui_viewport_paint_overlay(hui_widget_t* widget, HUI_Painter* painter) {
	hui_paint_focus(widget, painter, 0.0f);
}

static int hui_viewport_event(hui_widget_t* widget, const HUI_Event* event) {
	/* The application receives the events through HUI_SetWidgetEventCallback
	 * before this function. Here the viewport only keeps a drag alive when
	 * the cursor leaves it, and stops clicks from reaching what is behind. */
	switch (event->type) {
		case HUI_EVENT_MOUSE_DOWN:
			hui_widget_capture_mouse(widget);
			return HUI_TRUE;
		case HUI_EVENT_MOUSE_UP:
			return HUI_TRUE;
		default:
			return HUI_FALSE;
	}
}

static int hui_viewport_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_viewport_t* viewport = (hui_viewport_t*)widget;
	if (HUI_ATTR("Stretch")) {
		if (!hui_parse_stretch(value, &viewport->stretch)) return HUI_FALSE;
	} else if (HUI_ATTR("FlipY")) {
		if (!hui_parse_bool(value, &viewport->flip_y)) return HUI_FALSE;
	} else if (HUI_ATTR("Realtime")) {
		if (!hui_parse_bool(value, &viewport->realtime)) return HUI_FALSE;
	} else if (HUI_ATTR("Source")) {
		viewport->texture = hui_xml_texture(value);
		if (viewport->texture == HUI_INVALID_ID) return HUI_FALSE;
	} else {
		return HUI_FALSE;
	}
	hui_widget_invalidate_paint(widget);
	return HUI_TRUE;
}

const hui_widget_class_t hui_class_viewport = {
	.type = HUI_WIDGET_VIEWPORT,
	.name = "Viewport",
	.size = sizeof(hui_viewport_t),
	/* painting updates the state of the native child window: UI thread only */
	.flags = HUI_CF_NO_CHILDREN | HUI_CF_FOCUSABLE | HUI_CF_UI_PAINT,
	.init = hui_viewport_init,
	.destroy = hui_viewport_destroy,
	.measure = hui_viewport_measure,
	.arrange = hui_viewport_arrange,
	.paint = hui_viewport_paint,
	.paint_overlay = hui_viewport_paint_overlay,
	.event = hui_viewport_event,
	.set_attribute = hui_viewport_set_attribute
};

void HUI_SetViewportTexture(HUI_id id, HUI_id texture) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_VIEWPORT);
	if (texture != HUI_INVALID_ID && !hui_texture_get(texture)) {
		hui_error(HUI_ERROR_INVALID_ID, HUI_SEVERITY_ERROR, "HUI_SetViewportTexture: invalid texture");
		return;
	}
	((hui_viewport_t*)widget)->texture = texture;
	hui_widget_invalidate_paint(widget);
}

void HUI_SetViewportStretch(HUI_id id, HUI_EStretch stretch) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_VIEWPORT);
	if (stretch < HUI_STRETCH_FILL || stretch > HUI_STRETCH_NONE) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetViewportStretch: invalid stretch mode");
		return;
	}
	((hui_viewport_t*)widget)->stretch = stretch;
	hui_widget_invalidate_paint(widget);
}

void HUI_SetViewportFlipY(HUI_id id, int flip) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_VIEWPORT);
	((hui_viewport_t*)widget)->flip_y = flip != 0;
	hui_widget_invalidate_paint(widget);
}

void HUI_SetViewportDrawCallback(HUI_id id, HUI_CViewportDraw callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_VIEWPORT);
	((hui_viewport_t*)widget)->draw_callback = callback;
	((hui_viewport_t*)widget)->draw_user = user_data;
	hui_widget_invalidate_paint(widget);
}

int HUI_SetViewportNative(HUI_id id, int enabled) {
	hui_viewport_t* viewport;
	hui_window_t* window;
	hui_vec2_t origin;
	HUI_WIDGET(widget, id, HUI_WIDGET_VIEWPORT, HUI_FALSE);
	viewport = (hui_viewport_t*)widget;
	window = widget->window;

	if (!enabled) {
		hui_viewport_native_release(widget, window);
		hui_widget_invalidate_paint(widget);
		return HUI_TRUE;
	}
	if (viewport->native) return HUI_TRUE;
	if (!window || !window->platform) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_SetViewportNative: the viewport is not in a window yet");
		return HUI_FALSE;
	}
	if (window->needs_layout) hui_window_layout(window);
	origin = hui_widget_origin(widget);
	viewport->native = g_hui.platform->child_create(window->platform,
		(int)(origin.x * window->scale), (int)(origin.y * window->scale),
		hui_maxi(1, (int)(widget->rect.w * window->scale)), hui_maxi(1, (int)(widget->rect.h * window->scale)), window);
	if (!viewport->native) {
		hui_error(HUI_ERROR_PLATFORM, HUI_SEVERITY_ERROR, "HUI_SetViewportNative: the window system cannot create a child window");
		return HUI_FALSE;
	}
	HUI_ARRAY_RESERVE(window->native_viewports, window->native_viewport_cap, window->native_viewport_count + 1);
	window->native_viewports[window->native_viewport_count++] = widget;
	viewport->native_shown = HUI_TRUE;
	hui_widget_invalidate_paint(widget);
	return HUI_TRUE;
}

int HUI_GetViewportNativeHandle(HUI_id id, HUI_NativeWindow* native) {
	HUI_WIDGET(widget, id, HUI_WIDGET_VIEWPORT, HUI_FALSE);
	if (!native) return HUI_FALSE;
	memset(native, 0, sizeof(*native));
	if (!((hui_viewport_t*)widget)->native) return HUI_FALSE;
	g_hui.platform->window_get_native(((hui_viewport_t*)widget)->native, native);
	return native->system != HUI_WINDOW_SYSTEM_NONE;
}

void HUI_GetViewportPixelSize(HUI_id id, int* width, int* height) {
	hui_widget_t* widget;
	if (width) *width = 0;
	if (height) *height = 0;
	widget = hui_widget_check(id, HUI_WIDGET_VIEWPORT, __func__);
	if (!widget) return;
	if (widget->window && widget->window->needs_layout) hui_window_layout(widget->window);
	if (width) *width = ((hui_viewport_t*)widget)->pixel_width;
	if (height) *height = ((hui_viewport_t*)widget)->pixel_height;
}

void HUI_SetViewportResizeCallback(HUI_id id, HUI_CViewportResize callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_VIEWPORT);
	((hui_viewport_t*)widget)->resize_callback = callback;
	((hui_viewport_t*)widget)->resize_user = user_data;
}

void HUI_SetViewportRealtime(HUI_id id, int realtime) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_VIEWPORT);
	((hui_viewport_t*)widget)->realtime = realtime != 0;
	hui_widget_invalidate_paint(widget);
}

/* ===========================================================================
 *  Custom
 * ========================================================================= */

typedef struct hui_custom_t {
	hui_widget_t base;
	HUI_CPaint paint_callback;
	void* paint_user;
	HUI_CMeasure measure_callback;
	void* measure_user;
} hui_custom_t;

static hui_vec2_t hui_custom_measure(hui_widget_t* widget) {
	hui_custom_t* custom = (hui_custom_t*)widget;
	float width = 0.0f, height = 0.0f;
	uint32_t i;
	if (custom->measure_callback) custom->measure_callback(widget->id, &width, &height, custom->measure_user);
	/* a custom widget may also hold children: it is at least as large as they need */
	for (i = 0; i < widget->child_count; i++) {
		hui_vec2_t child = hui_widget_measure_slot(widget->children[i]);
		width = hui_maxf(width, child.x);
		height = hui_maxf(height, child.y);
	}
	return hui_vec2(hui_maxf(width, 0.0f), hui_maxf(height, 0.0f));
}

static void hui_custom_paint(hui_widget_t* widget, HUI_Painter* painter) {
	hui_custom_t* custom = (hui_custom_t*)widget;
	if (custom->paint_callback) {
		custom->paint_callback(widget->id, painter, widget->rect.w, widget->rect.h, custom->paint_user);
	}
}

const hui_widget_class_t hui_class_custom = {
	.type = HUI_WIDGET_CUSTOM,
	.name = "Custom",
	.size = sizeof(hui_custom_t),
	.flags = HUI_CF_UI_PAINT,
	.measure = hui_custom_measure,
	.paint = hui_custom_paint
};

void HUI_SetCustomPaintCallback(HUI_id id, HUI_CPaint callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_CUSTOM);
	((hui_custom_t*)widget)->paint_callback = callback;
	((hui_custom_t*)widget)->paint_user = user_data;
	hui_widget_invalidate_paint(widget);
}

void HUI_SetCustomMeasureCallback(HUI_id id, HUI_CMeasure callback, void* user_data) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_CUSTOM);
	((hui_custom_t*)widget)->measure_callback = callback;
	((hui_custom_t*)widget)->measure_user = user_data;
	hui_widget_invalidate_layout(widget);
}
