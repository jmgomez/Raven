/**
 * HUI - render/null/null_backend.c
 * Backend that draws nothing. It is used for headless runs and unit tests,
 * and is the smallest example of a HUI_BackendVTable: copy it to start a
 * new backend.
 */

#include "../renderer.h"

static int hui_null_init(const HUI_BackendInitInfo* info) {
	HUI_UNUSED(info);
	return HUI_TRUE;
}

static void hui_null_shutdown(void) {
}

static int hui_null_create_surface(HUI_id window, const HUI_NativeWindow* native, HUI_uint width, HUI_uint height) {
	HUI_UNUSED(window);
	HUI_UNUSED(native);
	HUI_UNUSED(width);
	HUI_UNUSED(height);
	return HUI_TRUE;
}

static void hui_null_destroy_surface(HUI_id window) {
	HUI_UNUSED(window);
}

static int hui_null_create_texture(HUI_id texture, const HUI_TextureDesc* desc, const void* pixels) {
	HUI_UNUSED(texture);
	HUI_UNUSED(desc);
	HUI_UNUSED(pixels);
	return HUI_TRUE;
}

static void hui_null_update_texture(HUI_id texture, HUI_uint x, HUI_uint y, HUI_uint width, HUI_uint height, const void* pixels) {
	HUI_UNUSED(texture);
	HUI_UNUSED(x);
	HUI_UNUSED(y);
	HUI_UNUSED(width);
	HUI_UNUSED(height);
	HUI_UNUSED(pixels);
}

static void hui_null_set_texture_filter(HUI_id texture, HUI_EFilterType filter) {
	HUI_UNUSED(texture);
	HUI_UNUSED(filter);
}

static void hui_null_destroy_texture(HUI_id texture) {
	HUI_UNUSED(texture);
}

static void hui_null_render_frame(const HUI_DrawData* data, HUI_Capture* capture) {
	if (capture) {
		/* a capture of the null backend is the clear color */
		size_t count = (size_t)data->framebuffer_width * data->framebuffer_height, i;
		hui_color_t clear = hui_rgba(data->clear_color[0], data->clear_color[1], data->clear_color[2], data->clear_color[3]);
		capture->width = (int)data->framebuffer_width;
		capture->height = (int)data->framebuffer_height;
		capture->pixels = count ? HUI_AllocPixels(count * 4) : NULL;
		for (i = 0; i < count; i++) memcpy(capture->pixels + i * 4, &clear, 4);
	}
}

static void hui_null_wait_idle(void) {
}

const HUI_BackendVTable* hui_backend_null(void) {
	static const HUI_BackendVTable vtable = {
		"null",
		hui_null_init,
		hui_null_shutdown,
		hui_null_create_surface,
		hui_null_destroy_surface,
		hui_null_create_texture,
		hui_null_update_texture,
		hui_null_set_texture_filter,
		hui_null_destroy_texture,
		hui_null_render_frame,
		hui_null_wait_idle
	};
	return &vtable;
}
