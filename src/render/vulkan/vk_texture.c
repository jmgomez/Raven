/**
 * HUI - render/vulkan/vk_texture.c
 * Textures and pixel uploads.
 *
 * A texture is an image, a view and one descriptor set. Uploads are batched:
 * the pixels are copied into a staging buffer and the copy commands are
 * recorded in one command buffer, submitted once before the next frame
 * (hui_vk_uploads_flush). The many small updates of the glyph atlas at
 * start-up therefore cost a single submission.
 *
 * Textures can also wrap an image view owned by the application (zero-copy
 * viewports) or export their memory to another API (shared textures).
 */

#include "vk_backend.h"

/** First size of the staging buffer; it grows to the largest upload. */
#define HUI_VK_STAGING_SIZE ((VkDeviceSize)4 * 1024 * 1024)

/* ---------------------------------------------------------------------------
 *  Upload batch
 * ------------------------------------------------------------------------- */

void hui_vk_uploads_flush(void) {
	VkSubmitInfo submit;
	VkResult result;

	if (!g_vk.upload_open) return;
	g_vk.upload_open = HUI_FALSE;
	g_vk.staging_used = 0;
	if (hui_vk_failed(hui_vk.EndCommandBuffer(g_vk.upload_commands), "vkEndCommandBuffer")) return;

	memset(&submit, 0, sizeof(submit));
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &g_vk.upload_commands;
	hui_vk_queue_lock();
	result = hui_vk.QueueSubmit(g_vk.queue, 1, &submit, g_vk.upload_fence);
	hui_vk_queue_unlock();
	if (hui_vk_failed(result, "vkQueueSubmit")) return;

	/* the staging buffer is reused right away, so the copies must be finished */
	hui_vk.WaitForFences(g_vk.device, 1, &g_vk.upload_fence, VK_TRUE, UINT64_MAX);
	hui_vk.ResetFences(g_vk.device, 1, &g_vk.upload_fence);
}

/* Starts the batch when needed. */
static int hui_vk_upload_begin(void) {
	VkCommandBufferBeginInfo begin;
	if (g_vk.upload_open) return HUI_TRUE;
	memset(&begin, 0, sizeof(begin));
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (hui_vk_failed(hui_vk.BeginCommandBuffer(g_vk.upload_commands, &begin), "vkBeginCommandBuffer")) return HUI_FALSE;
	g_vk.upload_open = HUI_TRUE;
	return HUI_TRUE;
}

/* Returns `size` bytes of the staging buffer, or NULL. */
static void* hui_vk_staging_reserve(VkDeviceSize size, VkDeviceSize* offset) {
	VkDeviceSize start = (g_vk.staging_used + 15) & ~(VkDeviceSize)15;
	if (start + size > g_vk.staging.size) {
		/* full: send what is pending, which frees the whole buffer */
		hui_vk_uploads_flush();
		start = 0;
		if (size > g_vk.staging.size) {
			VkDeviceSize wanted = g_vk.staging.size ? g_vk.staging.size : HUI_VK_STAGING_SIZE;
			while (wanted < size) wanted *= 2;
			hui_vk_buffer_destroy(&g_vk.staging);
			if (!hui_vk_buffer_create(&g_vk.staging, wanted, VK_BUFFER_USAGE_TRANSFER_SRC_BIT)) return NULL;
		}
	}
	*offset = start;
	g_vk.staging_used = start + size;
	return (uint8_t*)g_vk.staging.mapped + start;
}

static void hui_vk_image_barrier(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to,
	VkAccessFlags source_access, VkAccessFlags target_access, VkPipelineStageFlags source_stage, VkPipelineStageFlags target_stage) {
	VkImageMemoryBarrier barrier;
	memset(&barrier, 0, sizeof(barrier));
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = source_access;
	barrier.dstAccessMask = target_access;
	barrier.oldLayout = from;
	barrier.newLayout = to;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	hui_vk.CmdPipelineBarrier(commands, source_stage, target_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
}

/* ---------------------------------------------------------------------------
 *  Texture objects
 * ------------------------------------------------------------------------- */

static uint32_t hui_vk_texel_size(HUI_ETextureFormat format) {
	return format == HUI_TEXTURE_ALPHA8 ? 1u : 4u;
}

static VkSampler hui_vk_texture_sampler(const hui_vk_texture_t* texture) {
	if (texture->sampler) return texture->sampler;
	return texture->filter == HUI_FILTER_NEAREST ? g_vk.sampler_nearest : g_vk.sampler_linear;
}

static void hui_vk_texture_release(hui_vk_texture_t* texture) {
	hui_vk_set_free(texture->set, texture->pool);
	if (!texture->wrapped && texture->view) hui_vk.DestroyImageView(g_vk.device, texture->view, NULL);
	if (texture->image) hui_vk.DestroyImage(g_vk.device, texture->image, NULL);
	if (texture->memory) hui_vk.FreeMemory(g_vk.device, texture->memory, NULL);
	memset(texture, 0, sizeof(*texture));
}

/* Records the copy of `pixels` (tightly packed) into a region of the texture. */
static void hui_vk_texture_write(hui_vk_texture_t* texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
	const void* pixels) {
	VkDeviceSize size = (VkDeviceSize)width * height * hui_vk_texel_size(texture->format);
	VkDeviceSize offset = 0;
	VkBufferImageCopy region;
	void* staging;

	if (width == 0 || height == 0 || x + width > texture->width || y + height > texture->height) return;
	staging = hui_vk_staging_reserve(size, &offset);
	if (!staging || !hui_vk_upload_begin()) return;
	/* no pixels: the texture starts transparent rather than with random memory */
	if (pixels) memcpy(staging, pixels, (size_t)size);
	else memset(staging, 0, (size_t)size);

	hui_vk_image_barrier(g_vk.upload_commands, texture->image,
		texture->initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		texture->initialized ? VK_ACCESS_SHADER_READ_BIT : 0, VK_ACCESS_TRANSFER_WRITE_BIT,
		texture->initialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT);

	memset(&region, 0, sizeof(region));
	region.bufferOffset = offset;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageOffset.x = (int32_t)x;
	region.imageOffset.y = (int32_t)y;
	region.imageExtent.width = width;
	region.imageExtent.height = height;
	region.imageExtent.depth = 1;
	hui_vk.CmdCopyBufferToImage(g_vk.upload_commands, g_vk.staging.buffer, texture->image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	hui_vk_image_barrier(g_vk.upload_commands, texture->image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	texture->initialized = HUI_TRUE;
}

/* Creates the image, its memory, its view and its descriptor set. */
static int hui_vk_texture_build(hui_vk_texture_t* texture, uint32_t width, uint32_t height,
	HUI_ETextureFormat format, HUI_EFilterType filter, int shared) {
	VkImageCreateInfo image;
	VkExternalMemoryImageCreateInfo external_image;
	VkMemoryRequirements requirements;
	VkMemoryAllocateInfo allocation;
	VkExportMemoryAllocateInfo export_allocation;
	VkImageViewCreateInfo view;
#if defined(_WIN32)
	const VkExternalMemoryHandleTypeFlags handle_type = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
	const VkExternalMemoryHandleTypeFlags handle_type = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

	memset(texture, 0, sizeof(*texture));
	texture->used = HUI_TRUE;
	texture->width = width;
	texture->height = height;
	texture->format = format;
	texture->filter = filter;
	texture->shared = shared;

	memset(&image, 0, sizeof(image));
	image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image.imageType = VK_IMAGE_TYPE_2D;
	image.format = format == HUI_TEXTURE_ALPHA8 ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
	image.extent.width = width;
	image.extent.height = height;
	image.extent.depth = 1;
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (shared) {
		/* the other API typically renders into the image or copies from it */
		image.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		memset(&external_image, 0, sizeof(external_image));
		external_image.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
		external_image.handleTypes = handle_type;
		image.pNext = &external_image;
	}
	if (hui_vk_failed(hui_vk.CreateImage(g_vk.device, &image, NULL, &texture->image), "vkCreateImage")) goto failed;

	hui_vk.GetImageMemoryRequirements(g_vk.device, texture->image, &requirements);
	memset(&allocation, 0, sizeof(allocation));
	allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocation.allocationSize = requirements.size;
	allocation.memoryTypeIndex = hui_vk_find_memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (allocation.memoryTypeIndex == UINT32_MAX) allocation.memoryTypeIndex = hui_vk_find_memory_type(requirements.memoryTypeBits, 0);
	if (shared) {
		memset(&export_allocation, 0, sizeof(export_allocation));
		export_allocation.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
		export_allocation.handleTypes = handle_type;
		allocation.pNext = &export_allocation;
	}
	if (allocation.memoryTypeIndex == UINT32_MAX ||
		hui_vk_failed(hui_vk.AllocateMemory(g_vk.device, &allocation, NULL, &texture->memory), "vkAllocateMemory") ||
		hui_vk_failed(hui_vk.BindImageMemory(g_vk.device, texture->image, texture->memory, 0), "vkBindImageMemory")) goto failed;
	texture->allocation_size = requirements.size;

	memset(&view, 0, sizeof(view));
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.image = texture->image;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.format = image.format;
	if (format == HUI_TEXTURE_ALPHA8) {
		/* sampled as (1, 1, 1, value): the same shader path as color textures */
		view.components.r = VK_COMPONENT_SWIZZLE_ONE;
		view.components.g = VK_COMPONENT_SWIZZLE_ONE;
		view.components.b = VK_COMPONENT_SWIZZLE_ONE;
		view.components.a = VK_COMPONENT_SWIZZLE_R;
	}
	view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view.subresourceRange.levelCount = 1;
	view.subresourceRange.layerCount = 1;
	if (hui_vk_failed(hui_vk.CreateImageView(g_vk.device, &view, NULL, &texture->view), "vkCreateImageView")) goto failed;

	if (!hui_vk_set_allocate(&texture->set, &texture->pool)) goto failed;
	hui_vk_set_write(texture->set, texture->view, hui_vk_texture_sampler(texture));
	return HUI_TRUE;

failed:
	hui_vk_texture_release(texture);
	return HUI_FALSE;
}

/* ---------------------------------------------------------------------------
 *  Table of textures, indexed by the index part of the handle
 * ------------------------------------------------------------------------- */

hui_vk_texture_t* hui_vk_texture_find(HUI_id texture) {
	uint32_t index = HUI_ID_INDEX(texture);
	if (texture == HUI_INVALID_ID || index >= g_vk.texture_cap || !g_vk.textures[index].used) return NULL;
	return &g_vk.textures[index];
}

/* Returns the (unused) slot of a handle, growing the table when needed. */
static hui_vk_texture_t* hui_vk_texture_slot(HUI_id texture) {
	uint32_t index = HUI_ID_INDEX(texture);
	if (index >= g_vk.texture_cap) {
		uint32_t cap = g_vk.texture_cap ? g_vk.texture_cap : 64;
		while (cap <= index) cap *= 2;
		g_vk.textures = (hui_vk_texture_t*)hui_realloc(g_vk.textures, sizeof(hui_vk_texture_t) * cap);
		memset(g_vk.textures + g_vk.texture_cap, 0, sizeof(hui_vk_texture_t) * (cap - g_vk.texture_cap));
		g_vk.texture_cap = cap;
	}
	return &g_vk.textures[index];
}

/* The GPU may still be using a texture: nothing of it changes before it is idle. */
static void hui_vk_texture_quiesce(void) {
	hui_vk_uploads_flush();
	hui_vk_wait_frames();
}

int hui_vk_texture_create(HUI_id id, const HUI_TextureDesc* desc, const void* pixels) {
	hui_vk_texture_t* texture = hui_vk_texture_slot(id);
	if (texture->used) {
		hui_vk_texture_quiesce();
		hui_vk_texture_release(texture);
	}
	if (!hui_vk_texture_build(texture, desc->width, desc->height, desc->format, desc->filter, HUI_FALSE)) return HUI_FALSE;
	hui_vk_texture_write(texture, 0, 0, desc->width, desc->height, pixels);
	return HUI_TRUE;
}

void hui_vk_texture_update(HUI_id id, HUI_uint x, HUI_uint y, HUI_uint width, HUI_uint height, const void* pixels) {
	hui_vk_texture_t* texture = hui_vk_texture_find(id);
	if (!texture || texture->wrapped || !pixels) return;
	hui_vk_texture_write(texture, x, y, width, height, pixels);
}

void hui_vk_texture_set_filter(HUI_id id, HUI_EFilterType filter) {
	hui_vk_texture_t* texture = hui_vk_texture_find(id);
	if (!texture || texture->filter == filter) return;
	texture->filter = filter;
	/* a descriptor set must not change while a submitted frame uses it */
	hui_vk_texture_quiesce();
	hui_vk_set_write(texture->set, texture->view, hui_vk_texture_sampler(texture));
}

void hui_vk_texture_destroy(HUI_id id) {
	hui_vk_texture_t* texture = hui_vk_texture_find(id);
	if (!texture) return;
	hui_vk_texture_quiesce();
	hui_vk_texture_release(texture);
}

/* ---------------------------------------------------------------------------
 *  Images of the application
 * ------------------------------------------------------------------------- */

int hui_vk_texture_wrap(HUI_id id, VkImageView view, VkSampler sampler, uint32_t width, uint32_t height) {
	hui_vk_texture_t* texture = hui_vk_texture_slot(id);
	if (texture->used && !texture->wrapped) {
		hui_vk_texture_quiesce();
		hui_vk_texture_release(texture);
	}
	if (!texture->used) {
		memset(texture, 0, sizeof(*texture));
		if (!hui_vk_set_allocate(&texture->set, &texture->pool)) return HUI_FALSE;
		texture->used = HUI_TRUE;
		texture->wrapped = HUI_TRUE;
		texture->format = HUI_TEXTURE_RGBA8;
		texture->filter = HUI_FILTER_LINEAR;
		texture->initialized = HUI_TRUE;
	} else {
		/* replacing the view: frames in flight still read the previous one */
		hui_vk_texture_quiesce();
	}
	texture->view = view;
	texture->sampler = sampler;
	texture->width = width;
	texture->height = height;
	hui_vk_set_write(texture->set, view, hui_vk_texture_sampler(texture));
	return HUI_TRUE;
}

int hui_vk_texture_create_shared(HUI_id id, uint32_t width, uint32_t height) {
	hui_vk_texture_t* texture;
	if (!g_vk.can_share) return HUI_FALSE;
	texture = hui_vk_texture_slot(id);
	if (texture->used) {
		hui_vk_texture_quiesce();
		hui_vk_texture_release(texture);
	}
	if (!hui_vk_texture_build(texture, width, height, HUI_TEXTURE_RGBA8, HUI_FILTER_LINEAR, HUI_TRUE)) return HUI_FALSE;
	hui_vk_texture_write(texture, 0, 0, width, height, NULL);
	return HUI_TRUE;
}

int hui_vk_texture_export(HUI_id id, intptr_t* handle, uint64_t* allocation_size) {
	hui_vk_texture_t* texture = hui_vk_texture_find(id);
	/* VkMemoryGetWin32HandleInfoKHR has the same layout: only the type differs */
	VkMemoryGetFdInfoKHR info;
	VkResult result;

	if (!texture || !texture->shared) return HUI_FALSE;
	memset(&info, 0, sizeof(info));
	info.memory = texture->memory;
#if defined(_WIN32)
	{
		void* win32_handle = NULL;
		if (!hui_vk.GetMemoryWin32HandleKHR) return HUI_FALSE;
		info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
		info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
		result = hui_vk.GetMemoryWin32HandleKHR(g_vk.device, &info, &win32_handle);
		*handle = (intptr_t)win32_handle;
	}
#else
	{
		int fd = -1;
		if (!hui_vk.GetMemoryFdKHR) return HUI_FALSE;
		info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
		info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
		result = hui_vk.GetMemoryFdKHR(g_vk.device, &info, &fd);
		*handle = (intptr_t)fd;
	}
#endif
	if (hui_vk_failed(result, "exporting the texture memory")) return HUI_FALSE;
	*allocation_size = (uint64_t)texture->allocation_size;
	return HUI_TRUE;
}

/* ---------------------------------------------------------------------------
 *  Module
 * ------------------------------------------------------------------------- */

int hui_vk_textures_init(void) {
	VkCommandPoolCreateInfo pool;
	VkCommandBufferAllocateInfo allocation;
	VkFenceCreateInfo fence;
	static const uint8_t white[4] = { 255, 255, 255, 255 };

	memset(&pool, 0, sizeof(pool));
	pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool.queueFamilyIndex = g_vk.queue_family;
	if (hui_vk_failed(hui_vk.CreateCommandPool(g_vk.device, &pool, NULL, &g_vk.upload_pool), "vkCreateCommandPool")) return HUI_FALSE;

	memset(&allocation, 0, sizeof(allocation));
	allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocation.commandPool = g_vk.upload_pool;
	allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocation.commandBufferCount = 1;
	if (hui_vk_failed(hui_vk.AllocateCommandBuffers(g_vk.device, &allocation, &g_vk.upload_commands),
		"vkAllocateCommandBuffers")) return HUI_FALSE;

	memset(&fence, 0, sizeof(fence));
	fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	if (hui_vk_failed(hui_vk.CreateFence(g_vk.device, &fence, NULL, &g_vk.upload_fence), "vkCreateFence")) return HUI_FALSE;

	if (!hui_vk_buffer_create(&g_vk.staging, HUI_VK_STAGING_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT)) return HUI_FALSE;

	if (!hui_vk_texture_build(&g_vk.white, 1, 1, HUI_TEXTURE_RGBA8, HUI_FILTER_NEAREST, HUI_FALSE)) return HUI_FALSE;
	hui_vk_texture_write(&g_vk.white, 0, 0, 1, 1, white);
	return HUI_TRUE;
}

void hui_vk_textures_shutdown(void) {
	uint32_t i;
	if (!g_vk.device) return;
	if (g_vk.upload_commands) hui_vk_uploads_flush();
	for (i = 0; i < g_vk.texture_cap; i++) {
		if (g_vk.textures[i].used) hui_vk_texture_release(&g_vk.textures[i]);
	}
	hui_free(g_vk.textures);
	g_vk.textures = NULL;
	g_vk.texture_cap = 0;
	if (g_vk.white.used) hui_vk_texture_release(&g_vk.white);
	hui_vk_buffer_destroy(&g_vk.staging);
	if (g_vk.upload_fence) hui_vk.DestroyFence(g_vk.device, g_vk.upload_fence, NULL);
	if (g_vk.upload_pool) hui_vk.DestroyCommandPool(g_vk.device, g_vk.upload_pool, NULL);
	g_vk.upload_fence = VK_NULL_HANDLE;
	g_vk.upload_pool = VK_NULL_HANDLE;
	g_vk.upload_commands = VK_NULL_HANDLE;
	g_vk.upload_open = HUI_FALSE;
	g_vk.staging_used = 0;
}
