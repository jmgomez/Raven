/**
 * HUI - render/vulkan/vk_backend.h
 * State shared by the files of the Vulkan backend.
 *
 *   vk_loader.c    run-time loading of the Vulkan library
 *   vk_device.c    instance, physical device, device, memory helpers
 *   vk_pipeline.c  shaders, samplers, descriptor sets, render passes, pipelines
 *   vk_texture.c   textures and uploads
 *   vk_surface.c   one surface + swapchain per window
 *   vk_frame.c     recording and presenting a frame, capture
 *   vk_backend.c   the HUI_BackendVTable
 *   vk_public.c    public API of hui_vulkan.h
 *
 * Everything here runs on the render thread, except vk_public.c.
 */

#ifndef HUI_VK_BACKEND_H
#define HUI_VK_BACKEND_H

#include "../renderer.h"
#include "vk_loader.h"

/** Frames recorded ahead of the GPU, per window. */
#define HUI_VK_FRAMES 2
/** A swapchain never has more images than this. */
#define HUI_VK_MAX_IMAGES 16
/** Render pass + pipeline variants, one per swapchain color format. */
#define HUI_VK_MAX_PASSES 4
/** Descriptor sets (= textures) per descriptor pool. */
#define HUI_VK_SETS_PER_POOL 256

/* ---------------------------------------------------------------------------
 *  Objects
 * ------------------------------------------------------------------------- */

typedef struct hui_vk_buffer_t {
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDeviceSize size;
	/** Persistently mapped, or NULL. */
	void* mapped;
} hui_vk_buffer_t;

typedef struct hui_vk_texture_t {
	int used;
	uint32_t width, height;
	HUI_ETextureFormat format;
	HUI_EFilterType filter;
	/** VK_NULL_HANDLE for a wrapped image view. */
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	/** Sampler given by the application, or VK_NULL_HANDLE for the HUI samplers. */
	VkSampler sampler;
	VkDescriptorSet set;
	VkDescriptorPool pool;
	/** The view belongs to the application. */
	int wrapped;
	/** The memory can be exported (HUI_Vulkan_CreateSharedTexture). */
	int shared;
	VkDeviceSize allocation_size;
	/** HUI_FALSE until the first pixels are written: the image has no layout yet. */
	int initialized;
} hui_vk_texture_t;

/** Render pass and pipeline for one color format. */
typedef struct hui_vk_pass_t {
	VkFormat format;
	VkRenderPass render_pass;
	VkPipeline pipeline;
} hui_vk_pass_t;

/** Per-frame resources of a window: a frame is recorded while the previous one is on the GPU. */
typedef struct hui_vk_frame_t {
	VkCommandPool pool;
	VkCommandBuffer commands;
	VkFence fence;
	VkSemaphore acquired;
	/** HUI_TRUE while the fence belongs to a submitted frame. */
	int submitted;
	hui_vk_buffer_t instances;
} hui_vk_frame_t;

typedef struct hui_vk_surface_t {
	int used;
	HUI_id window;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	VkSurfaceFormatKHR format;
	hui_vk_pass_t* pass;
	VkExtent2D extent;
	/** Size HUI asked for when the swapchain was created. */
	uint32_t requested_width, requested_height;
	uint32_t image_count;
	VkImage images[HUI_VK_MAX_IMAGES];
	VkImageView views[HUI_VK_MAX_IMAGES];
	VkFramebuffer framebuffers[HUI_VK_MAX_IMAGES];
	/** Signaled when rendering to the image is finished; waited by the presentation. */
	VkSemaphore rendered[HUI_VK_MAX_IMAGES];
	hui_vk_frame_t frames[HUI_VK_FRAMES];
	uint32_t frame_cursor;
	/** The swapchain no longer matches the window. */
	int out_of_date;
	/** The swapchain images can be read back (HUI_CaptureWindow). */
	int can_capture;
} hui_vk_surface_t;

/* ---------------------------------------------------------------------------
 *  Global state
 * ------------------------------------------------------------------------- */

typedef struct hui_vk_state_t {
	int initialized;
	HUI_BackendInitInfo info;

	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
	VkQueue queue;
	uint32_t queue_family;
	/** The instance and device belong to the application (HUI_Vulkan_SetExternalDevice). */
	int external;
	VkDebugUtilsMessengerEXT messenger;
	VkPhysicalDeviceMemoryProperties memory;
	uint32_t api_version;
	/** Memory of textures can be exported (external memory fd / win32). */
	int can_share;

	/* vk_pipeline.c */
	VkShaderModule vertex_shader;
	VkShaderModule fragment_shader;
	VkDescriptorSetLayout set_layout;
	VkPipelineLayout pipeline_layout;
	VkSampler sampler_linear;
	VkSampler sampler_nearest;
	hui_vk_pass_t passes[HUI_VK_MAX_PASSES];
	uint32_t pass_count;
	VkDescriptorPool* pools;
	uint32_t* pool_used;
	uint32_t pool_count;
	uint32_t pool_cap;

	/* vk_texture.c */
	hui_vk_texture_t* textures;
	uint32_t texture_cap;
	/** 1x1 white texture bound when a command has no texture. */
	hui_vk_texture_t white;
	VkCommandPool upload_pool;
	VkCommandBuffer upload_commands;
	VkFence upload_fence;
	hui_vk_buffer_t staging;
	VkDeviceSize staging_used;
	/** HUI_TRUE while upload commands are recorded and not yet submitted. */
	int upload_open;

	/* vk_surface.c */
	hui_vk_surface_t* surfaces;
	uint32_t surface_cap;
	void* xcb_library;
} hui_vk_state_t;

extern hui_vk_state_t g_vk;

/** Reports a failed Vulkan call and evaluates to HUI_TRUE when `result` is an error. */
int hui_vk_failed(VkResult result, const char* what);

/* ---------------------------------------------------------------------------
 *  vk_device.c
 * ------------------------------------------------------------------------- */

/** Configuration set before HUI_Init through hui_vulkan.h. */
typedef struct hui_vk_config_t {
	int external;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
	VkQueue queue;
	uint32_t queue_family;
	void (*lock)(void* user_data);
	void (*unlock)(void* user_data);
	void* lock_user_data;
} hui_vk_config_t;

extern hui_vk_config_t g_vk_config;

int  hui_vk_device_init(void);
void hui_vk_device_shutdown(void);

/** Names of the extensions HUI needs; used by hui_vulkan.h and by device creation. */
const char* const* hui_vk_instance_extensions(uint32_t* count);
const char* const* hui_vk_device_extensions(uint32_t* count);

/** The queue may be shared with the application: every use is bracketed by these. */
void hui_vk_queue_lock(void);
void hui_vk_queue_unlock(void);

/** Index of a memory type allowed by `type_bits` with all of `flags`, or UINT32_MAX. */
uint32_t hui_vk_find_memory_type(uint32_t type_bits, VkMemoryPropertyFlags flags);

/** Creates a host-visible, persistently mapped buffer. */
int  hui_vk_buffer_create(hui_vk_buffer_t* buffer, VkDeviceSize size, VkBufferUsageFlags usage);
void hui_vk_buffer_destroy(hui_vk_buffer_t* buffer);

/* ---------------------------------------------------------------------------
 *  vk_pipeline.c
 * ------------------------------------------------------------------------- */

int  hui_vk_pipeline_init(void);
void hui_vk_pipeline_shutdown(void);
/** Returns the render pass + pipeline of a color format, creating them on first use. */
hui_vk_pass_t* hui_vk_pass_get(VkFormat format);

/** Allocates the descriptor set of a texture. */
int  hui_vk_set_allocate(VkDescriptorSet* set, VkDescriptorPool* pool);
void hui_vk_set_free(VkDescriptorSet set, VkDescriptorPool pool);
void hui_vk_set_write(VkDescriptorSet set, VkImageView view, VkSampler sampler);

/* ---------------------------------------------------------------------------
 *  vk_texture.c
 * ------------------------------------------------------------------------- */

int  hui_vk_textures_init(void);
void hui_vk_textures_shutdown(void);

int  hui_vk_texture_create(HUI_id texture, const HUI_TextureDesc* desc, const void* pixels);
void hui_vk_texture_update(HUI_id texture, HUI_uint x, HUI_uint y, HUI_uint width, HUI_uint height, const void* pixels);
void hui_vk_texture_set_filter(HUI_id texture, HUI_EFilterType filter);
void hui_vk_texture_destroy(HUI_id texture);

/** Texture of a handle, or NULL. */
hui_vk_texture_t* hui_vk_texture_find(HUI_id texture);
/** Makes a texture out of an image view of the application (replaces the previous view). */
int  hui_vk_texture_wrap(HUI_id texture, VkImageView view, VkSampler sampler, uint32_t width, uint32_t height);
/** Creates an RGBA8 texture whose memory can be exported. */
int  hui_vk_texture_create_shared(HUI_id texture, uint32_t width, uint32_t height);
/** Exports the memory of a shared texture as a new fd / HANDLE. */
int  hui_vk_texture_export(HUI_id texture, intptr_t* handle, uint64_t* allocation_size);

/** Submits the pending texture uploads and waits for them. */
void hui_vk_uploads_flush(void);

/* ---------------------------------------------------------------------------
 *  vk_surface.c
 * ------------------------------------------------------------------------- */

int  hui_vk_surface_create(HUI_id window, const HUI_NativeWindow* native, HUI_uint width, HUI_uint height);
void hui_vk_surface_destroy(HUI_id window);
void hui_vk_surfaces_shutdown(void);
hui_vk_surface_t* hui_vk_surface_find(HUI_id window);
/** (Re)creates the swapchain. Returns HUI_FALSE when the window cannot be rendered now (minimized). */
int  hui_vk_swapchain_create(hui_vk_surface_t* surface, uint32_t width, uint32_t height);
/** Waits until the GPU finished every frame submitted for every window. */
void hui_vk_wait_frames(void);

/* ---------------------------------------------------------------------------
 *  vk_frame.c
 * ------------------------------------------------------------------------- */

void hui_vk_render_frame(const HUI_DrawData* data, HUI_Capture* capture);

#endif /* HUI_VK_BACKEND_H */
