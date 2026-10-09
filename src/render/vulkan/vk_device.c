/**
 * HUI - render/vulkan/vk_device.c
 * Instance, physical device selection, logical device and memory helpers.
 *
 * HUI either creates its own instance and device, or adopts the ones of the
 * application (HUI_Vulkan_SetExternalDevice) so that images can be shared
 * with the renderer of the application without any copy.
 */

#include "vk_backend.h"

#include <stdlib.h>

hui_vk_state_t g_vk;
hui_vk_config_t g_vk_config;

#define HUI_VK_MAX_EXTENSIONS 16

#if defined(_WIN32)
#define HUI_VK_EXTERNAL_MEMORY_EXTENSION "VK_KHR_external_memory_win32"
#else
#define HUI_VK_EXTERNAL_MEMORY_EXTENSION "VK_KHR_external_memory_fd"
#endif

int hui_vk_failed(VkResult result, const char* what) {
	if (result >= 0) return HUI_FALSE;
	hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR, "Vulkan: %s failed (%s)", what, hui_vk_result_name(result));
	return HUI_TRUE;
}

void hui_vk_queue_lock(void) {
	if (g_vk_config.lock) g_vk_config.lock(g_vk_config.lock_user_data);
}

void hui_vk_queue_unlock(void) {
	if (g_vk_config.unlock) g_vk_config.unlock(g_vk_config.lock_user_data);
}

/* ---------------------------------------------------------------------------
 *  Extensions
 * ------------------------------------------------------------------------- */

static int hui_vk_has_extension(const VkExtensionProperties* list, uint32_t count, const char* name) {
	uint32_t i;
	for (i = 0; i < count; i++) {
		if (strcmp(list[i].extensionName, name) == 0) return HUI_TRUE;
	}
	return HUI_FALSE;
}

static VkExtensionProperties* hui_vk_enumerate_instance_extensions(uint32_t* count) {
	VkExtensionProperties* list;
	*count = 0;
	if (hui_vk.EnumerateInstanceExtensionProperties(NULL, count, NULL) < 0 || *count == 0) return NULL;
	list = (VkExtensionProperties*)hui_calloc(sizeof(VkExtensionProperties) * *count);
	if (hui_vk.EnumerateInstanceExtensionProperties(NULL, count, list) < 0) *count = 0;
	return list;
}

/*
 * Surface extensions of the window systems HUI can run on. X11 windows work
 * with either the Xlib or the XCB flavor: drivers do not all expose both.
 */
static const char* const g_vk_surface_extensions[] = {
#if defined(_WIN32)
	"VK_KHR_win32_surface",
#else
	"VK_KHR_xlib_surface",
	"VK_KHR_xcb_surface",
#endif
	"VK_EXT_headless_surface"
};

const char* const* hui_vk_instance_extensions(uint32_t* count) {
	static const char* names[HUI_VK_MAX_EXTENSIONS];
	VkExtensionProperties* available;
	uint32_t available_count = 0, used = 0;
	size_t i;

	*count = 0;
	if (!hui_vk_load_library()) return NULL;
	available = hui_vk_enumerate_instance_extensions(&available_count);

	names[used++] = "VK_KHR_surface";
	for (i = 0; i < HUI_COUNT_OF(g_vk_surface_extensions); i++) {
		if (hui_vk_has_extension(available, available_count, g_vk_surface_extensions[i])) {
			names[used++] = g_vk_surface_extensions[i];
		}
	}
	hui_free(available);
	*count = used;
	return names;
}

const char* const* hui_vk_device_extensions(uint32_t* count) {
	static const char* const names[] = { "VK_KHR_swapchain" };
	*count = (uint32_t)HUI_COUNT_OF(names);
	return names;
}

/* ---------------------------------------------------------------------------
 *  Validation messages
 * ------------------------------------------------------------------------- */

static VKAPI_ATTR VkBool32 VKAPI_CALL hui_vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT types, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user_data) {
	HUI_UNUSED(types);
	HUI_UNUSED(user_data);
	hui_error(HUI_ERROR_BACKEND,
		(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? HUI_SEVERITY_ERROR : HUI_SEVERITY_WARNING,
		"Vulkan validation: %s", data && data->pMessage ? data->pMessage : "");
	return VK_FALSE;
}

/* ---------------------------------------------------------------------------
 *  Instance
 * ------------------------------------------------------------------------- */

static int hui_vk_create_instance(void) {
	VkApplicationInfo application;
	VkInstanceCreateInfo info;
	VkDebugUtilsMessengerCreateInfoEXT messenger;
	VkExtensionProperties* available;
	const char* extensions[HUI_VK_MAX_EXTENSIONS];
	const char* const* required;
	const char* layers[1];
	uint32_t available_count = 0, required_count = 0, extension_count = 0, layer_count = 0, i;
	uint32_t loader_version = VK_API_VERSION_1_0;
	int debug = HUI_FALSE;
	VkResult result;

	required = hui_vk_instance_extensions(&required_count);
	if (required_count < 2) {
		hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR, "Vulkan: the driver cannot present to a window (no surface extension)");
		return HUI_FALSE;
	}
	for (i = 0; i < required_count; i++) extensions[extension_count++] = required[i];

	if (hui_vk.EnumerateInstanceVersion) hui_vk.EnumerateInstanceVersion(&loader_version);

	if (g_vk.info.enable_validation) {
		VkLayerProperties* layer_list;
		uint32_t count = 0;
		hui_vk.EnumerateInstanceLayerProperties(&count, NULL);
		layer_list = (VkLayerProperties*)hui_calloc(sizeof(VkLayerProperties) * (count + 1));
		hui_vk.EnumerateInstanceLayerProperties(&count, layer_list);
		for (i = 0; i < count; i++) {
			if (strcmp(layer_list[i].layerName, "VK_LAYER_KHRONOS_validation") == 0) layers[layer_count++] = "VK_LAYER_KHRONOS_validation";
		}
		hui_free(layer_list);
		if (layer_count == 0) {
			hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_WEAK_WARNING,
				"Vulkan: validation was requested but VK_LAYER_KHRONOS_validation is not installed");
		}
		available = hui_vk_enumerate_instance_extensions(&available_count);
		if (layer_count && hui_vk_has_extension(available, available_count, "VK_EXT_debug_utils")) {
			extensions[extension_count++] = "VK_EXT_debug_utils";
			debug = HUI_TRUE;
		}
		hui_free(available);
	}

	memset(&application, 0, sizeof(application));
	application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	application.pApplicationName = g_vk.info.app_name ? g_vk.info.app_name : "HUI application";
	application.pEngineName = "HUI";
	application.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	/* 1.1 only brings external memory in core; everything else is Vulkan 1.0 */
	application.apiVersion = loader_version >= VK_API_VERSION_1_1 ? VK_API_VERSION_1_1 : VK_API_VERSION_1_0;

	memset(&messenger, 0, sizeof(messenger));
	messenger.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	messenger.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	messenger.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	messenger.pfnUserCallback = hui_vk_debug_callback;

	memset(&info, 0, sizeof(info));
	info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	info.pApplicationInfo = &application;
	info.enabledExtensionCount = extension_count;
	info.ppEnabledExtensionNames = extensions;
	info.enabledLayerCount = layer_count;
	info.ppEnabledLayerNames = layers;
	/* also reports problems of instance creation itself */
	if (debug) info.pNext = &messenger;

	result = hui_vk.CreateInstance(&info, NULL, &g_vk.instance);
	if (hui_vk_failed(result, "vkCreateInstance")) return HUI_FALSE;
	if (!hui_vk_load_instance(g_vk.instance)) return HUI_FALSE;

	if (debug && hui_vk.CreateDebugUtilsMessengerEXT) {
		hui_vk.CreateDebugUtilsMessengerEXT(g_vk.instance, &messenger, NULL, &g_vk.messenger);
	}
	return HUI_TRUE;
}

/* ---------------------------------------------------------------------------
 *  Physical device
 * ------------------------------------------------------------------------- */

static int hui_vk_device_has_extension(VkPhysicalDevice device, const char* name) {
	VkExtensionProperties* list;
	uint32_t count = 0;
	int found;
	if (hui_vk.EnumerateDeviceExtensionProperties(device, NULL, &count, NULL) < 0 || count == 0) return HUI_FALSE;
	list = (VkExtensionProperties*)hui_calloc(sizeof(VkExtensionProperties) * count);
	if (hui_vk.EnumerateDeviceExtensionProperties(device, NULL, &count, list) < 0) count = 0;
	found = hui_vk_has_extension(list, count, name);
	hui_free(list);
	return found;
}

/* First queue family able to draw, or UINT32_MAX. */
static uint32_t hui_vk_graphics_family(VkPhysicalDevice device) {
	VkQueueFamilyProperties families[32];
	uint32_t count = (uint32_t)HUI_COUNT_OF(families), i;
	hui_vk.GetPhysicalDeviceQueueFamilyProperties(device, &count, families);
	for (i = 0; i < count; i++) {
		if (families[i].queueCount > 0 && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) return i;
	}
	return UINT32_MAX;
}

static int hui_vk_device_score(VkPhysicalDeviceType type) {
	switch (type) {
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 5;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 4;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 3;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: return 1;
		default: return 2;
	}
}

/*
 * A UI needs very little from the GPU, so any device that can draw and
 * present is accepted; discrete GPUs are preferred. The environment variable
 * HUI_VK_DEVICE=<index> forces a choice (multi-GPU machines).
 */
static int hui_vk_pick_physical_device(void) {
	VkPhysicalDevice devices[16];
	uint32_t count = (uint32_t)HUI_COUNT_OF(devices), i;
	int best_score = -1;
	const char* forced = getenv("HUI_VK_DEVICE");
	VkResult result = hui_vk.EnumeratePhysicalDevices(g_vk.instance, &count, devices);

	if (result < 0 || count == 0) {
		hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR, "Vulkan: no graphics device found");
		return HUI_FALSE;
	}
	for (i = 0; i < count; i++) {
		VkPhysicalDeviceProperties properties;
		uint32_t family = hui_vk_graphics_family(devices[i]);
		int score;
		if (family == UINT32_MAX || !hui_vk_device_has_extension(devices[i], "VK_KHR_swapchain")) continue;
		hui_vk.GetPhysicalDeviceProperties(devices[i], &properties);
		score = hui_vk_device_score(properties.deviceType);
		if (forced && (uint32_t)atoi(forced) == i) score = 100;
		if (score > best_score) {
			best_score = score;
			g_vk.physical_device = devices[i];
			g_vk.queue_family = family;
		}
	}
	if (best_score < 0) {
		hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR, "Vulkan: no device can render to a window (VK_KHR_swapchain missing)");
		return HUI_FALSE;
	}
	return HUI_TRUE;
}

/* ---------------------------------------------------------------------------
 *  Device
 * ------------------------------------------------------------------------- */

static int hui_vk_create_device(void) {
	VkDeviceQueueCreateInfo queue;
	VkDeviceCreateInfo info;
	VkPhysicalDeviceProperties properties;
	const char* extensions[HUI_VK_MAX_EXTENSIONS];
	const char* const* required;
	uint32_t required_count = 0, extension_count = 0, i;
	float priority = 1.0f;
	int share;
	VkResult result;

	required = hui_vk_device_extensions(&required_count);
	for (i = 0; i < required_count; i++) extensions[extension_count++] = required[i];

	/* optional: textures whose memory can be imported by OpenGL or another device */
	hui_vk.GetPhysicalDeviceProperties(g_vk.physical_device, &properties);
	share = properties.apiVersion >= VK_API_VERSION_1_1 && g_vk.api_version >= VK_API_VERSION_1_1 &&
		hui_vk_device_has_extension(g_vk.physical_device, HUI_VK_EXTERNAL_MEMORY_EXTENSION);
	if (share) extensions[extension_count++] = HUI_VK_EXTERNAL_MEMORY_EXTENSION;

	memset(&queue, 0, sizeof(queue));
	queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue.queueFamilyIndex = g_vk.queue_family;
	queue.queueCount = 1;
	queue.pQueuePriorities = &priority;

	memset(&info, 0, sizeof(info));
	info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	info.queueCreateInfoCount = 1;
	info.pQueueCreateInfos = &queue;
	info.enabledExtensionCount = extension_count;
	info.ppEnabledExtensionNames = extensions;

	result = hui_vk.CreateDevice(g_vk.physical_device, &info, NULL, &g_vk.device);
	if (hui_vk_failed(result, "vkCreateDevice")) return HUI_FALSE;
	if (!hui_vk_load_device(g_vk.device)) return HUI_FALSE;
	hui_vk.GetDeviceQueue(g_vk.device, g_vk.queue_family, 0, &g_vk.queue);
	return HUI_TRUE;
}

int hui_vk_device_init(void) {
	VkPhysicalDeviceProperties properties;

	if (!hui_vk_load_library()) return HUI_FALSE;

	if (g_vk_config.external) {
		/* the application owns everything: only resolve the entry points */
		g_vk.external = HUI_TRUE;
		g_vk.instance = g_vk_config.instance;
		g_vk.physical_device = g_vk_config.physical_device;
		g_vk.device = g_vk_config.device;
		g_vk.queue = g_vk_config.queue;
		g_vk.queue_family = g_vk_config.queue_family;
		if (!g_vk.instance || !g_vk.physical_device || !g_vk.device || !g_vk.queue) {
			hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_Vulkan_SetExternalDevice: NULL handle");
			return HUI_FALSE;
		}
		if (!hui_vk_load_instance(g_vk.instance) || !hui_vk_load_device(g_vk.device)) return HUI_FALSE;
		g_vk.api_version = VK_API_VERSION_1_0;
	} else {
		uint32_t loader_version = VK_API_VERSION_1_0;
		if (hui_vk.EnumerateInstanceVersion) hui_vk.EnumerateInstanceVersion(&loader_version);
		g_vk.api_version = loader_version >= VK_API_VERSION_1_1 ? VK_API_VERSION_1_1 : VK_API_VERSION_1_0;
		if (!hui_vk_create_instance()) return HUI_FALSE;
		if (!hui_vk_pick_physical_device()) return HUI_FALSE;
		if (!hui_vk_create_device()) return HUI_FALSE;
	}

	hui_vk.GetPhysicalDeviceMemoryProperties(g_vk.physical_device, &g_vk.memory);
	hui_vk.GetPhysicalDeviceProperties(g_vk.physical_device, &properties);
	/* the entry point only resolves when the extension is enabled on the device */
#if defined(_WIN32)
	g_vk.can_share = hui_vk.GetMemoryWin32HandleKHR != NULL;
#else
	g_vk.can_share = hui_vk.GetMemoryFdKHR != NULL;
#endif
	if (!g_vk.external && !(properties.apiVersion >= VK_API_VERSION_1_1 && g_vk.api_version >= VK_API_VERSION_1_1)) {
		g_vk.can_share = HUI_FALSE;
	}
	return HUI_TRUE;
}

void hui_vk_device_shutdown(void) {
	if (!g_vk.external) {
		if (g_vk.device && hui_vk.DestroyDevice) hui_vk.DestroyDevice(g_vk.device, NULL);
		if (g_vk.messenger && hui_vk.DestroyDebugUtilsMessengerEXT) {
			hui_vk.DestroyDebugUtilsMessengerEXT(g_vk.instance, g_vk.messenger, NULL);
		}
		if (g_vk.instance && hui_vk.DestroyInstance) hui_vk.DestroyInstance(g_vk.instance, NULL);
	}
	g_vk.device = VK_NULL_HANDLE;
	g_vk.instance = VK_NULL_HANDLE;
	g_vk.physical_device = VK_NULL_HANDLE;
	g_vk.queue = VK_NULL_HANDLE;
	g_vk.messenger = VK_NULL_HANDLE;
	hui_vk_unload_library();
}

/* ---------------------------------------------------------------------------
 *  Memory
 * ------------------------------------------------------------------------- */

uint32_t hui_vk_find_memory_type(uint32_t type_bits, VkMemoryPropertyFlags flags) {
	uint32_t i;
	for (i = 0; i < g_vk.memory.memoryTypeCount; i++) {
		if ((type_bits & (1u << i)) && (g_vk.memory.memoryTypes[i].propertyFlags & flags) == flags) return i;
	}
	return UINT32_MAX;
}

int hui_vk_buffer_create(hui_vk_buffer_t* buffer, VkDeviceSize size, VkBufferUsageFlags usage) {
	VkBufferCreateInfo info;
	VkMemoryRequirements requirements;
	VkMemoryAllocateInfo allocation;

	memset(buffer, 0, sizeof(*buffer));
	memset(&info, 0, sizeof(info));
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size = size;
	info.usage = usage;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (hui_vk_failed(hui_vk.CreateBuffer(g_vk.device, &info, NULL, &buffer->buffer), "vkCreateBuffer")) return HUI_FALSE;

	hui_vk.GetBufferMemoryRequirements(g_vk.device, buffer->buffer, &requirements);
	memset(&allocation, 0, sizeof(allocation));
	allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocation.allocationSize = requirements.size;
	/* this combination is guaranteed to exist by the Vulkan specification */
	allocation.memoryTypeIndex = hui_vk_find_memory_type(requirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (allocation.memoryTypeIndex == UINT32_MAX ||
		hui_vk_failed(hui_vk.AllocateMemory(g_vk.device, &allocation, NULL, &buffer->memory), "vkAllocateMemory") ||
		hui_vk_failed(hui_vk.BindBufferMemory(g_vk.device, buffer->buffer, buffer->memory, 0), "vkBindBufferMemory") ||
		hui_vk_failed(hui_vk.MapMemory(g_vk.device, buffer->memory, 0, VK_WHOLE_SIZE, 0, &buffer->mapped), "vkMapMemory")) {
		hui_vk_buffer_destroy(buffer);
		return HUI_FALSE;
	}
	buffer->size = size;
	return HUI_TRUE;
}

void hui_vk_buffer_destroy(hui_vk_buffer_t* buffer) {
	if (buffer->buffer) hui_vk.DestroyBuffer(g_vk.device, buffer->buffer, NULL);
	/* freeing the memory also unmaps it */
	if (buffer->memory) hui_vk.FreeMemory(g_vk.device, buffer->memory, NULL);
	memset(buffer, 0, sizeof(*buffer));
}
