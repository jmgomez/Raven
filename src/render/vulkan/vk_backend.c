/**
 * HUI - render/vulkan/vk_backend.c
 * The Vulkan implementation of HUI_BackendVTable.
 */

#include "vk_backend.h"

static void hui_vk_shutdown(void) {
	if (g_vk.device && hui_vk.DeviceWaitIdle) {
		hui_vk_queue_lock();
		hui_vk.DeviceWaitIdle(g_vk.device);
		hui_vk_queue_unlock();
		hui_vk_surfaces_shutdown();
		hui_vk_textures_shutdown();
		hui_vk_pipeline_shutdown();
	}
	hui_vk_device_shutdown();
	memset(&g_vk, 0, sizeof(g_vk));
}

static int hui_vk_init(const HUI_BackendInitInfo* info) {
	memset(&g_vk, 0, sizeof(g_vk));
	g_vk.info = *info;
	if (!hui_vk_device_init() || !hui_vk_pipeline_init() || !hui_vk_textures_init()) {
		hui_vk_shutdown();
		return HUI_FALSE;
	}
	g_vk.initialized = HUI_TRUE;
	return HUI_TRUE;
}

static void hui_vk_wait_idle(void) {
	hui_vk_uploads_flush();
	hui_vk_wait_frames();
}

const HUI_BackendVTable* hui_backend_vulkan(void) {
	static const HUI_BackendVTable vtable = {
		"Vulkan",
		hui_vk_init,
		hui_vk_shutdown,
		hui_vk_surface_create,
		hui_vk_surface_destroy,
		hui_vk_texture_create,
		hui_vk_texture_update,
		hui_vk_texture_set_filter,
		hui_vk_texture_destroy,
		hui_vk_render_frame,
		hui_vk_wait_idle
	};
	return &vtable;
}
