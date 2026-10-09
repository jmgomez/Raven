/**
 * HUI - render/vulkan/vk_public.c
 * Public API of hui_vulkan.h.
 *
 * These functions are called from the UI thread. Whatever touches Vulkan
 * objects is forwarded to the render thread, in order with the frames.
 */

#include "vk_backend.h"
#include "../../core/context.h"

#include <hui/hui_vulkan.h>

static int hui_vk_active(const char* function) {
	if (g_hui.initialized && g_hui.backend == HUI_BACKEND_VULKAN) return HUI_TRUE;
	if (function) hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "%s: the Vulkan backend is not running", function);
	return HUI_FALSE;
}

/* ---------------------------------------------------------------------------
 *  Sharing a device
 * ------------------------------------------------------------------------- */

const char* const* HUI_Vulkan_GetRequiredInstanceExtensions(HUI_uint* count) {
	uint32_t found = 0;
	const char* const* names = hui_vk_instance_extensions(&found);
	if (count) *count = found;
	return names;
}

const char* const* HUI_Vulkan_GetRequiredDeviceExtensions(HUI_uint* count) {
	uint32_t found = 0;
	const char* const* names = hui_vk_device_extensions(&found);
	if (count) *count = found;
	return names;
}

void HUI_Vulkan_SetExternalDevice(void* instance, void* physical_device, void* device, void* queue, HUI_uint queue_family) {
	if (g_hui.initialized) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_Vulkan_SetExternalDevice must be called before HUI_Init");
		return;
	}
	/* NULL handles go back to a device created by HUI */
	g_vk_config.external = instance != NULL && device != NULL;
	g_vk_config.instance = (VkInstance)instance;
	g_vk_config.physical_device = (VkPhysicalDevice)physical_device;
	g_vk_config.device = (VkDevice)device;
	g_vk_config.queue = (VkQueue)queue;
	g_vk_config.queue_family = queue_family;
}

void HUI_Vulkan_SetQueueLockCallbacks(HUI_CVulkanQueueLock lock, HUI_CVulkanQueueLock unlock, void* user_data) {
	if (g_hui.initialized) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_Vulkan_SetQueueLockCallbacks must be called before HUI_Init");
		return;
	}
	g_vk_config.lock = lock;
	g_vk_config.unlock = unlock;
	g_vk_config.lock_user_data = user_data;
}

/* ---------------------------------------------------------------------------
 *  Access to the device
 * ------------------------------------------------------------------------- */

void* HUI_Vulkan_GetInstance(void) {
	return hui_vk_active(NULL) ? (void*)g_vk.instance : NULL;
}

void* HUI_Vulkan_GetPhysicalDevice(void) {
	return hui_vk_active(NULL) ? (void*)g_vk.physical_device : NULL;
}

void* HUI_Vulkan_GetDevice(void) {
	return hui_vk_active(NULL) ? (void*)g_vk.device : NULL;
}

void* HUI_Vulkan_GetGraphicsQueue(void) {
	return hui_vk_active(NULL) ? (void*)g_vk.queue : NULL;
}

HUI_uint HUI_Vulkan_GetGraphicsQueueFamily(void) {
	return hui_vk_active(NULL) ? g_vk.queue_family : 0;
}

/* ---------------------------------------------------------------------------
 *  Textures
 * ------------------------------------------------------------------------- */

typedef struct hui_vk_texture_call_t {
	HUI_id id;
	VkImageView view;
	VkSampler sampler;
	uint32_t width, height;
	intptr_t handle;
	uint64_t allocation_size;
	int result;
} hui_vk_texture_call_t;

static void hui_vk_call_wrap(void* arg) {
	hui_vk_texture_call_t* call = (hui_vk_texture_call_t*)arg;
	call->result = hui_vk_texture_wrap(call->id, call->view, call->sampler, call->width, call->height);
}

static void hui_vk_call_create_shared(void* arg) {
	hui_vk_texture_call_t* call = (hui_vk_texture_call_t*)arg;
	call->result = hui_vk_texture_create_shared(call->id, call->width, call->height);
}

static void hui_vk_call_export(void* arg) {
	hui_vk_texture_call_t* call = (hui_vk_texture_call_t*)arg;
	call->result = hui_vk_texture_export(call->id, &call->handle, &call->allocation_size);
}

/* Registers a texture whose backend object is made by `function` on the render thread. */
static HUI_id hui_vk_create_external(hui_render_fn function, hui_vk_texture_call_t* call) {
	hui_texture_t* texture = hui_texture_register();
	if (!texture) return HUI_INVALID_ID;
	call->id = texture->id;
	hui_renderer_call(function, call, HUI_TRUE);
	if (!call->result) {
		hui_pool_remove(&g_hui.textures, texture->id);
		hui_free(texture);
		return HUI_INVALID_ID;
	}
	texture->width = (int)call->width;
	texture->height = (int)call->height;
	texture->external = HUI_TRUE;
	texture->created = HUI_TRUE;
	hui_atomic_store(&texture->ready, 1);
	return texture->id;
}

HUI_id HUI_Vulkan_CreateTextureFromImageView(uint64_t image_view, uint64_t sampler, int width, int height) {
	hui_vk_texture_call_t call;
	HUI_API_GUARD(HUI_INVALID_ID);
	if (!hui_vk_active(__func__)) return HUI_INVALID_ID;
	if (image_view == 0 || width <= 0 || height <= 0) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_Vulkan_CreateTextureFromImageView: invalid arguments");
		return HUI_INVALID_ID;
	}
	memset(&call, 0, sizeof(call));
	call.view = (VkImageView)(uintptr_t)image_view;
	call.sampler = (VkSampler)(uintptr_t)sampler;
	call.width = (uint32_t)width;
	call.height = (uint32_t)height;
	return hui_vk_create_external(hui_vk_call_wrap, &call);
}

void HUI_Vulkan_SetTextureImageView(HUI_id id, uint64_t image_view, uint64_t sampler, int width, int height) {
	hui_vk_texture_call_t call;
	hui_texture_t* texture;
	HUI_API_GUARD_VOID();
	if (!hui_vk_active(__func__)) return;
	texture = hui_texture_get(id);
	if (!texture || !texture->external) {
		hui_error(HUI_ERROR_INVALID_ID, HUI_SEVERITY_ERROR, "HUI_Vulkan_SetTextureImageView: not a texture made from an image view");
		return;
	}
	if (image_view == 0 || width <= 0 || height <= 0) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_Vulkan_SetTextureImageView: invalid arguments");
		return;
	}
	memset(&call, 0, sizeof(call));
	call.id = id;
	call.view = (VkImageView)(uintptr_t)image_view;
	call.sampler = (VkSampler)(uintptr_t)sampler;
	call.width = (uint32_t)width;
	call.height = (uint32_t)height;
	/* waits: once this returns, HUI no longer uses the previous view and it can be destroyed */
	hui_renderer_call(hui_vk_call_wrap, &call, HUI_TRUE);
	texture->width = width;
	texture->height = height;
	hui_invalidate_all_windows();
}

HUI_id HUI_Vulkan_CreateSharedTexture(int width, int height) {
	hui_vk_texture_call_t call;
	HUI_id id;
	HUI_API_GUARD(HUI_INVALID_ID);
	if (!hui_vk_active(__func__)) return HUI_INVALID_ID;
	if (width <= 0 || height <= 0 || width > 16384 || height > 16384) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_Vulkan_CreateSharedTexture: invalid size %d x %d", width, height);
		return HUI_INVALID_ID;
	}
	memset(&call, 0, sizeof(call));
	call.width = (uint32_t)width;
	call.height = (uint32_t)height;
	id = hui_vk_create_external(hui_vk_call_create_shared, &call);
	if (id == HUI_INVALID_ID) {
		hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR,
			"HUI_Vulkan_CreateSharedTexture: the device cannot export memory (external memory extension missing)");
	}
	return id;
}

int HUI_Vulkan_GetSharedTextureInfo(HUI_id id, HUI_SharedTextureInfo* info) {
	hui_vk_texture_call_t call;
	hui_texture_t* texture;
	HUI_API_GUARD(HUI_FALSE);
	if (!hui_vk_active(__func__)) return HUI_FALSE;
	texture = hui_texture_get(id);
	if (!texture || !info) {
		hui_error(HUI_ERROR_INVALID_ID, HUI_SEVERITY_ERROR, "HUI_Vulkan_GetSharedTextureInfo: invalid arguments");
		return HUI_FALSE;
	}
	memset(&call, 0, sizeof(call));
	call.id = id;
	hui_renderer_call(hui_vk_call_export, &call, HUI_TRUE);
	if (!call.result) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_Vulkan_GetSharedTextureInfo: not a shared texture");
		return HUI_FALSE;
	}
	info->handle = call.handle;
	info->allocation_size = call.allocation_size;
	info->width = texture->width;
	info->height = texture->height;
	return HUI_TRUE;
}
