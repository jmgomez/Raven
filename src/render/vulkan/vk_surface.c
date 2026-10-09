/**
 * HUI - render/vulkan/vk_surface.c
 * One presentation surface and one swapchain per window.
 *
 * The window-system headers are not included: the create-info structures of
 * the surface extensions are declared here with their exact layout, which
 * keeps <windows.h> and <X11/Xlib.h> out of the renderer.
 */

#include "vk_backend.h"

/* VkWin32SurfaceCreateInfoKHR */
typedef struct hui_vk_win32_surface_info_t {
	VkStructureType sType;
	const void* pNext;
	VkFlags flags;
	void* hinstance;
	void* hwnd;
} hui_vk_win32_surface_info_t;

/* VkXlibSurfaceCreateInfoKHR */
typedef struct hui_vk_xlib_surface_info_t {
	VkStructureType sType;
	const void* pNext;
	VkFlags flags;
	void* dpy;
	/* Window is an XID, which Xlib defines as unsigned long */
	unsigned long window;
} hui_vk_xlib_surface_info_t;

/* VkXcbSurfaceCreateInfoKHR */
typedef struct hui_vk_xcb_surface_info_t {
	VkStructureType sType;
	const void* pNext;
	VkFlags flags;
	void* connection;
	uint32_t window;
} hui_vk_xcb_surface_info_t;

/* ---------------------------------------------------------------------------
 *  Native surface
 * ------------------------------------------------------------------------- */

static int hui_vk_native_surface_create(const HUI_NativeWindow* native, VkSurfaceKHR* out) {
	VkResult result = VK_ERROR_EXTENSION_NOT_PRESENT;

	if (native->system == HUI_WINDOW_SYSTEM_WIN32 && hui_vk.CreateWin32SurfaceKHR) {
		hui_vk_win32_surface_info_t info;
		memset(&info, 0, sizeof(info));
		info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		info.hinstance = native->display;
		info.hwnd = (void*)native->window;
		result = hui_vk.CreateWin32SurfaceKHR(g_vk.instance, &info, NULL, out);
	} else if (native->system == HUI_WINDOW_SYSTEM_X11) {
		if (hui_vk.CreateXlibSurfaceKHR) {
			hui_vk_xlib_surface_info_t info;
			memset(&info, 0, sizeof(info));
			info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
			info.dpy = native->display;
			info.window = (unsigned long)native->window;
			result = hui_vk.CreateXlibSurfaceKHR(g_vk.instance, &info, NULL, out);
		}
		if (result < 0 && hui_vk.CreateXcbSurfaceKHR) {
			/*
			 * The driver only speaks XCB (some software and embedded drivers).
			 * Every Xlib connection sits on an XCB connection, which
			 * libX11-xcb returns; it is opened on demand so that HUI does
			 * not depend on it.
			 */
			void* (*get_connection)(void* display) = NULL;
			void* symbol;
			if (!g_vk.xcb_library) g_vk.xcb_library = hui_vk_library_open("libX11-xcb.so.1");
			symbol = g_vk.xcb_library ? hui_vk_library_symbol(g_vk.xcb_library, "XGetXCBConnection") : NULL;
			memcpy(&get_connection, &symbol, sizeof(void*));
			if (get_connection) {
				hui_vk_xcb_surface_info_t info;
				memset(&info, 0, sizeof(info));
				info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
				info.connection = get_connection(native->display);
				info.window = (uint32_t)native->window;
				result = hui_vk.CreateXcbSurfaceKHR(g_vk.instance, &info, NULL, out);
			} else {
				hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR,
					"Vulkan: this driver needs libX11-xcb.so.1 to present to an X11 window");
				return HUI_FALSE;
			}
		}
	} else if (native->system == HUI_WINDOW_SYSTEM_NONE && hui_vk.CreateHeadlessSurfaceEXT) {
		/* no window system (headless runs): frames are rendered and can be captured */
		VkHeadlessSurfaceCreateInfoEXT info;
		memset(&info, 0, sizeof(info));
		info.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
		result = hui_vk.CreateHeadlessSurfaceEXT(g_vk.instance, &info, NULL, out);
	}

	if (result == VK_ERROR_EXTENSION_NOT_PRESENT) {
		hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR,
			"Vulkan: the surface extension of this window system is not enabled on the instance");
		return HUI_FALSE;
	}
	return !hui_vk_failed(result, "creating the window surface");
}

/* ---------------------------------------------------------------------------
 *  Table of surfaces, indexed by the index part of the window handle
 * ------------------------------------------------------------------------- */

hui_vk_surface_t* hui_vk_surface_find(HUI_id window) {
	uint32_t index = HUI_ID_INDEX(window);
	if (index >= g_vk.surface_cap || !g_vk.surfaces[index].used || g_vk.surfaces[index].window != window) return NULL;
	return &g_vk.surfaces[index];
}

void hui_vk_wait_frames(void) {
	uint32_t i, f;
	for (i = 0; i < g_vk.surface_cap; i++) {
		hui_vk_surface_t* surface = &g_vk.surfaces[i];
		if (!surface->used) continue;
		for (f = 0; f < HUI_VK_FRAMES; f++) {
			hui_vk_frame_t* frame = &surface->frames[f];
			if (!frame->submitted) continue;
			hui_vk.WaitForFences(g_vk.device, 1, &frame->fence, VK_TRUE, UINT64_MAX);
			hui_vk.ResetFences(g_vk.device, 1, &frame->fence);
			frame->submitted = HUI_FALSE;
		}
	}
}

/* ---------------------------------------------------------------------------
 *  Swapchain
 * ------------------------------------------------------------------------- */

static uint32_t hui_vk_clamp(uint32_t value, uint32_t low, uint32_t high) {
	return value < low ? low : (value > high ? high : value);
}

static void hui_vk_swapchain_release_images(hui_vk_surface_t* surface) {
	uint32_t i;
	for (i = 0; i < surface->image_count; i++) {
		if (surface->framebuffers[i]) hui_vk.DestroyFramebuffer(g_vk.device, surface->framebuffers[i], NULL);
		if (surface->views[i]) hui_vk.DestroyImageView(g_vk.device, surface->views[i], NULL);
		surface->framebuffers[i] = VK_NULL_HANDLE;
		surface->views[i] = VK_NULL_HANDLE;
	}
	surface->image_count = 0;
}

/* UI colors are sRGB values blended as they are, so a UNORM format is wanted. */
static VkSurfaceFormatKHR hui_vk_choose_format(hui_vk_surface_t* surface) {
	VkSurfaceFormatKHR formats[64];
	VkSurfaceFormatKHR chosen;
	uint32_t count = (uint32_t)HUI_COUNT_OF(formats), i;

	chosen.format = VK_FORMAT_B8G8R8A8_UNORM;
	chosen.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	if (hui_vk.GetPhysicalDeviceSurfaceFormatsKHR(g_vk.physical_device, surface->surface, &count, formats) < 0 || count == 0) return chosen;
	/* a single UNDEFINED entry means that any format is accepted */
	if (count == 1 && formats[0].format == VK_FORMAT_UNDEFINED) return chosen;
	for (i = 0; i < count; i++) {
		if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM || formats[i].format == VK_FORMAT_R8G8B8A8_UNORM) return formats[i];
	}
	return formats[0];
}

static VkPresentModeKHR hui_vk_choose_present_mode(hui_vk_surface_t* surface) {
	VkPresentModeKHR modes[16];
	uint32_t count = (uint32_t)HUI_COUNT_OF(modes), i;
	int mailbox = HUI_FALSE, immediate = HUI_FALSE;

	/* FIFO is always available and is the vertical synchronization */
	if (g_vk.info.vsync) return VK_PRESENT_MODE_FIFO_KHR;
	if (hui_vk.GetPhysicalDeviceSurfacePresentModesKHR(g_vk.physical_device, surface->surface, &count, modes) < 0) {
		return VK_PRESENT_MODE_FIFO_KHR;
	}
	for (i = 0; i < count; i++) {
		if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) mailbox = HUI_TRUE;
		if (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) immediate = HUI_TRUE;
	}
	if (mailbox) return VK_PRESENT_MODE_MAILBOX_KHR;
	if (immediate) return VK_PRESENT_MODE_IMMEDIATE_KHR;
	return VK_PRESENT_MODE_FIFO_KHR;
}

int hui_vk_swapchain_create(hui_vk_surface_t* surface, uint32_t width, uint32_t height) {
	VkSurfaceCapabilitiesKHR caps;
	VkSwapchainCreateInfoKHR info;
	VkSwapchainKHR created = VK_NULL_HANDLE;
	VkExtent2D extent;
	uint32_t image_count, i;
	static const VkCompositeAlphaFlagBitsKHR alpha_modes[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
	};

	surface->requested_width = width;
	surface->requested_height = height;
	surface->out_of_date = HUI_TRUE;

	if (hui_vk.GetPhysicalDeviceSurfaceCapabilitiesKHR(g_vk.physical_device, surface->surface, &caps) < 0) return HUI_FALSE;
	if (caps.currentExtent.width != UINT32_MAX) {
		/* the window system dictates the size */
		extent = caps.currentExtent;
	} else {
		extent.width = hui_vk_clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
		extent.height = hui_vk_clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
	}
	/* minimized: nothing can be created, the frame is skipped */
	if (extent.width == 0 || extent.height == 0) return HUI_FALSE;

	/* the previous images may still be in use by submitted frames */
	hui_vk_wait_frames();

	surface->format = hui_vk_choose_format(surface);
	surface->pass = hui_vk_pass_get(surface->format.format);
	if (!surface->pass) return HUI_FALSE;

	image_count = caps.minImageCount + 1;
	if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) image_count = caps.maxImageCount;

	memset(&info, 0, sizeof(info));
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = surface->surface;
	info.minImageCount = image_count;
	info.imageFormat = surface->format.format;
	info.imageColorSpace = surface->format.colorSpace;
	info.imageExtent = extent;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	/* reading the images back is optional: it is only needed by HUI_CaptureWindow */
	surface->can_capture = (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
	if (surface->can_capture) info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.preTransform = (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ?
		VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : caps.currentTransform;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	for (i = 0; i < HUI_COUNT_OF(alpha_modes); i++) {
		if (caps.supportedCompositeAlpha & alpha_modes[i]) {
			info.compositeAlpha = alpha_modes[i];
			break;
		}
	}
	info.presentMode = hui_vk_choose_present_mode(surface);
	info.clipped = VK_TRUE;
	info.oldSwapchain = surface->swapchain;

	if (hui_vk_failed(hui_vk.CreateSwapchainKHR(g_vk.device, &info, NULL, &created), "vkCreateSwapchainKHR")) return HUI_FALSE;

	hui_vk_swapchain_release_images(surface);
	if (surface->swapchain) hui_vk.DestroySwapchainKHR(g_vk.device, surface->swapchain, NULL);
	surface->swapchain = created;
	surface->extent = extent;

	image_count = 0;
	hui_vk.GetSwapchainImagesKHR(g_vk.device, surface->swapchain, &image_count, NULL);
	if (image_count == 0 || image_count > HUI_VK_MAX_IMAGES) {
		hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR, "Vulkan: unsupported number of swapchain images (%u)", image_count);
		return HUI_FALSE;
	}
	if (hui_vk_failed(hui_vk.GetSwapchainImagesKHR(g_vk.device, surface->swapchain, &image_count, surface->images),
		"vkGetSwapchainImagesKHR")) return HUI_FALSE;
	surface->image_count = image_count;

	for (i = 0; i < image_count; i++) {
		VkImageViewCreateInfo view;
		VkFramebufferCreateInfo framebuffer;

		memset(&view, 0, sizeof(view));
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.image = surface->images[i];
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = surface->format.format;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.levelCount = 1;
		view.subresourceRange.layerCount = 1;
		if (hui_vk_failed(hui_vk.CreateImageView(g_vk.device, &view, NULL, &surface->views[i]), "vkCreateImageView")) return HUI_FALSE;

		memset(&framebuffer, 0, sizeof(framebuffer));
		framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer.renderPass = surface->pass->render_pass;
		framebuffer.attachmentCount = 1;
		framebuffer.pAttachments = &surface->views[i];
		framebuffer.width = extent.width;
		framebuffer.height = extent.height;
		framebuffer.layers = 1;
		if (hui_vk_failed(hui_vk.CreateFramebuffer(g_vk.device, &framebuffer, NULL, &surface->framebuffers[i]),
			"vkCreateFramebuffer")) return HUI_FALSE;

		if (!surface->rendered[i]) {
			VkSemaphoreCreateInfo semaphore;
			memset(&semaphore, 0, sizeof(semaphore));
			semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			if (hui_vk_failed(hui_vk.CreateSemaphore(g_vk.device, &semaphore, NULL, &surface->rendered[i]),
				"vkCreateSemaphore")) return HUI_FALSE;
		}
	}
	surface->out_of_date = HUI_FALSE;
	return HUI_TRUE;
}

/* ---------------------------------------------------------------------------
 *  Surface
 * ------------------------------------------------------------------------- */

static int hui_vk_frame_create(hui_vk_frame_t* frame) {
	VkCommandPoolCreateInfo pool;
	VkCommandBufferAllocateInfo allocation;
	VkFenceCreateInfo fence;
	VkSemaphoreCreateInfo semaphore;

	memset(&pool, 0, sizeof(pool));
	pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool.queueFamilyIndex = g_vk.queue_family;
	if (hui_vk_failed(hui_vk.CreateCommandPool(g_vk.device, &pool, NULL, &frame->pool), "vkCreateCommandPool")) return HUI_FALSE;

	memset(&allocation, 0, sizeof(allocation));
	allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocation.commandPool = frame->pool;
	allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocation.commandBufferCount = 1;
	if (hui_vk_failed(hui_vk.AllocateCommandBuffers(g_vk.device, &allocation, &frame->commands), "vkAllocateCommandBuffers")) return HUI_FALSE;

	memset(&fence, 0, sizeof(fence));
	fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	if (hui_vk_failed(hui_vk.CreateFence(g_vk.device, &fence, NULL, &frame->fence), "vkCreateFence")) return HUI_FALSE;

	memset(&semaphore, 0, sizeof(semaphore));
	semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	if (hui_vk_failed(hui_vk.CreateSemaphore(g_vk.device, &semaphore, NULL, &frame->acquired), "vkCreateSemaphore")) return HUI_FALSE;
	return HUI_TRUE;
}

static void hui_vk_surface_release(hui_vk_surface_t* surface) {
	uint32_t i;
	hui_vk_wait_frames();
	for (i = 0; i < HUI_VK_FRAMES; i++) {
		hui_vk_frame_t* frame = &surface->frames[i];
		hui_vk_buffer_destroy(&frame->instances);
		if (frame->acquired) hui_vk.DestroySemaphore(g_vk.device, frame->acquired, NULL);
		if (frame->fence) hui_vk.DestroyFence(g_vk.device, frame->fence, NULL);
		/* destroying the pool frees its command buffer */
		if (frame->pool) hui_vk.DestroyCommandPool(g_vk.device, frame->pool, NULL);
	}
	hui_vk_swapchain_release_images(surface);
	for (i = 0; i < HUI_VK_MAX_IMAGES; i++) {
		if (surface->rendered[i]) hui_vk.DestroySemaphore(g_vk.device, surface->rendered[i], NULL);
	}
	if (surface->swapchain) hui_vk.DestroySwapchainKHR(g_vk.device, surface->swapchain, NULL);
	if (surface->surface) hui_vk.DestroySurfaceKHR(g_vk.instance, surface->surface, NULL);
	memset(surface, 0, sizeof(*surface));
}

int hui_vk_surface_create(HUI_id window, const HUI_NativeWindow* native, HUI_uint width, HUI_uint height) {
	uint32_t index = HUI_ID_INDEX(window), i;
	hui_vk_surface_t* surface;
	VkBool32 supported = VK_FALSE;

	if (index >= g_vk.surface_cap) {
		uint32_t cap = g_vk.surface_cap ? g_vk.surface_cap : 8;
		while (cap <= index) cap *= 2;
		g_vk.surfaces = (hui_vk_surface_t*)hui_realloc(g_vk.surfaces, sizeof(hui_vk_surface_t) * cap);
		memset(g_vk.surfaces + g_vk.surface_cap, 0, sizeof(hui_vk_surface_t) * (cap - g_vk.surface_cap));
		g_vk.surface_cap = cap;
	}
	surface = &g_vk.surfaces[index];
	if (surface->used) hui_vk_surface_release(surface);

	memset(surface, 0, sizeof(*surface));
	if (!hui_vk_native_surface_create(native, &surface->surface)) return HUI_FALSE;
	surface->used = HUI_TRUE;
	surface->window = window;

	hui_vk.GetPhysicalDeviceSurfaceSupportKHR(g_vk.physical_device, g_vk.queue_family, surface->surface, &supported);
	if (!supported) {
		hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR, "Vulkan: the graphics queue cannot present to this window");
		hui_vk_surface_release(surface);
		return HUI_FALSE;
	}
	for (i = 0; i < HUI_VK_FRAMES; i++) {
		if (!hui_vk_frame_create(&surface->frames[i])) {
			hui_vk_surface_release(surface);
			return HUI_FALSE;
		}
	}
	/* a window that is still hidden or minimized gets its swapchain on its first frame */
	hui_vk_swapchain_create(surface, width, height);
	return HUI_TRUE;
}

void hui_vk_surface_destroy(HUI_id window) {
	hui_vk_surface_t* surface = hui_vk_surface_find(window);
	if (surface) hui_vk_surface_release(surface);
}

void hui_vk_surfaces_shutdown(void) {
	uint32_t i;
	for (i = 0; i < g_vk.surface_cap; i++) {
		if (g_vk.surfaces[i].used) hui_vk_surface_release(&g_vk.surfaces[i]);
	}
	hui_free(g_vk.surfaces);
	g_vk.surfaces = NULL;
	g_vk.surface_cap = 0;
	hui_vk_library_close(g_vk.xcb_library);
	g_vk.xcb_library = NULL;
}
