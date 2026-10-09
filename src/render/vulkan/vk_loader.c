/**
 * HUI - render/vulkan/vk_loader.c
 * Opens the Vulkan library at run time and fills the `hui_vk` table.
 */

#include "vk_backend.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
/* <windows.h> turns these names into macros, which would rename entries of the function table */
#undef CreateSemaphore
#undef CreateEvent
#else
#include <dlfcn.h>
#endif

hui_vk_functions_t hui_vk;

static void* g_vk_library;

/* ---------------------------------------------------------------------------
 *  Shared libraries
 * ------------------------------------------------------------------------- */

void* hui_vk_library_open(const char* name) {
#if defined(_WIN32)
	return (void*)LoadLibraryA(name);
#else
	return dlopen(name, RTLD_NOW | RTLD_LOCAL);
#endif
}

void* hui_vk_library_symbol(void* library, const char* name) {
#if defined(_WIN32)
	/* a function pointer does not convert to void* in ISO C: go through an integer */
	return (void*)(uintptr_t)GetProcAddress((HMODULE)library, name);
#else
	return dlsym(library, name);
#endif
}

void hui_vk_library_close(void* library) {
	if (!library) return;
#if defined(_WIN32)
	FreeLibrary((HMODULE)library);
#else
	dlclose(library);
#endif
}

/* ---------------------------------------------------------------------------
 *  Entry points
 * ------------------------------------------------------------------------- */

/* Converts the untyped result of the loaders to the type of a table entry. */
#define HUI_VK_ASSIGN(target, pointer) \
	do { \
		void* hui_vk_pointer_ = (void*)(uintptr_t)(pointer); \
		memcpy(&(target), &hui_vk_pointer_, sizeof(void*)); \
	} while (0)

int hui_vk_load_library(void) {
	static const char* const names[] = {
#if defined(_WIN32)
		"vulkan-1.dll"
#elif defined(__APPLE__)
		"libvulkan.1.dylib", "libvulkan.dylib", "libMoltenVK.dylib"
#else
		"libvulkan.so.1", "libvulkan.so"
#endif
	};
	size_t i;
	int complete = HUI_TRUE;

	/* may already be open: hui_vulkan.h can be queried before HUI_Init */
	if (g_vk_library) return HUI_TRUE;
	memset(&hui_vk, 0, sizeof(hui_vk));
	for (i = 0; i < HUI_COUNT_OF(names) && !g_vk_library; i++) g_vk_library = hui_vk_library_open(names[i]);
	if (!g_vk_library) {
		hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR,
			"Vulkan: the Vulkan library (%s) was not found; is a Vulkan driver installed?", names[0]);
		return HUI_FALSE;
	}

	HUI_VK_ASSIGN(hui_vk.GetInstanceProcAddr, hui_vk_library_symbol(g_vk_library, "vkGetInstanceProcAddr"));
	if (!hui_vk.GetInstanceProcAddr) {
		hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR, "Vulkan: vkGetInstanceProcAddr is missing from the Vulkan library");
		hui_vk_unload_library();
		return HUI_FALSE;
	}

#define HUI_VK_LOAD(name) \
	HUI_VK_ASSIGN(hui_vk.name, hui_vk.GetInstanceProcAddr(NULL, "vk" #name)); \
	if (!hui_vk.name) complete = HUI_FALSE;
	HUI_VK_GLOBAL_FUNCTIONS(HUI_VK_LOAD)
#undef HUI_VK_LOAD
	HUI_VK_ASSIGN(hui_vk.EnumerateInstanceVersion, hui_vk.GetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion"));

	if (!complete) {
		hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR, "Vulkan: the Vulkan library is incomplete");
		hui_vk_unload_library();
		return HUI_FALSE;
	}
	return HUI_TRUE;
}

void hui_vk_unload_library(void) {
	hui_vk_library_close(g_vk_library);
	g_vk_library = NULL;
	memset(&hui_vk, 0, sizeof(hui_vk));
}

int hui_vk_load_instance(VkInstance instance) {
	const char* missing = NULL;

#define HUI_VK_LOAD(name) \
	HUI_VK_ASSIGN(hui_vk.name, hui_vk.GetInstanceProcAddr(instance, "vk" #name)); \
	if (!hui_vk.name && !missing) missing = "vk" #name;
	HUI_VK_INSTANCE_FUNCTIONS(HUI_VK_LOAD)
#undef HUI_VK_LOAD

	/* optional extensions: NULL when they are not enabled on the instance */
	HUI_VK_ASSIGN(hui_vk.CreateWin32SurfaceKHR, hui_vk.GetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR"));
	HUI_VK_ASSIGN(hui_vk.CreateXlibSurfaceKHR, hui_vk.GetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR"));
	HUI_VK_ASSIGN(hui_vk.CreateXcbSurfaceKHR, hui_vk.GetInstanceProcAddr(instance, "vkCreateXcbSurfaceKHR"));
	HUI_VK_ASSIGN(hui_vk.CreateHeadlessSurfaceEXT, hui_vk.GetInstanceProcAddr(instance, "vkCreateHeadlessSurfaceEXT"));
	HUI_VK_ASSIGN(hui_vk.CreateDebugUtilsMessengerEXT, hui_vk.GetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
	HUI_VK_ASSIGN(hui_vk.DestroyDebugUtilsMessengerEXT, hui_vk.GetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

	if (missing) {
		hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR,
			"Vulkan: %s is not available (is VK_KHR_surface enabled on the instance?)", missing);
		return HUI_FALSE;
	}
	return HUI_TRUE;
}

int hui_vk_load_device(VkDevice device) {
	const char* missing = NULL;

#define HUI_VK_LOAD(name) \
	HUI_VK_ASSIGN(hui_vk.name, hui_vk.GetDeviceProcAddr(device, "vk" #name)); \
	if (!hui_vk.name && !missing) missing = "vk" #name;
	HUI_VK_DEVICE_FUNCTIONS(HUI_VK_LOAD)
#undef HUI_VK_LOAD

	HUI_VK_ASSIGN(hui_vk.GetMemoryFdKHR, hui_vk.GetDeviceProcAddr(device, "vkGetMemoryFdKHR"));
	HUI_VK_ASSIGN(hui_vk.GetMemoryWin32HandleKHR, hui_vk.GetDeviceProcAddr(device, "vkGetMemoryWin32HandleKHR"));

	if (missing) {
		hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR,
			"Vulkan: %s is not available (is VK_KHR_swapchain enabled on the device?)", missing);
		return HUI_FALSE;
	}
	return HUI_TRUE;
}

const char* hui_vk_result_name(VkResult result) {
	switch (result) {
		case VK_SUCCESS: return "VK_SUCCESS";
		case VK_NOT_READY: return "VK_NOT_READY";
		case VK_TIMEOUT: return "VK_TIMEOUT";
		case VK_INCOMPLETE: return "VK_INCOMPLETE";
		case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
		case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
		case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
		case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
		case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
		case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
		case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
		case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
		case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
		case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
		case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
		default: return "VK_ERROR (unknown)";
	}
}
