/**
 * HUI - render/renderer.h
 * Bridge between the UI thread and the rendering backend.
 *
 * Every backend call is turned into a command and executed, in submission
 * order, by a dedicated render thread. The UI thread never waits for the GPU
 * except for back-pressure: it will not queue more than a couple of frames
 * per window. When the render thread is disabled, commands run inline.
 *
 *      UI thread                               render thread
 *   paint -> frame packet --+
 *   texture create/update --+--> queue --> backend (Vulkan) --> present
 *   surface create/destroy -+
 */

#ifndef HUI_RENDERER_H
#define HUI_RENDERER_H

#include "draw.h"
#include "../core/thread.h"

/* ---------------------------------------------------------------------------
 *  Textures (render/texture.c): UI-thread description of a GPU texture
 * ------------------------------------------------------------------------- */

typedef struct hui_texture_t {
	HUI_id id;
	int width, height;
	HUI_ETextureFormat format;
	HUI_EFilterType filter;
	/** 1 once the pixels were handed to the renderer, or decoding failed. */
	hui_atomic_t ready;
	/** HUI_TRUE while the backend texture exists. */
	int created;
	/** Created by a backend-specific function (wrapped image view, shared memory). */
	int external;
} hui_texture_t;

hui_texture_t* hui_texture_get(HUI_id id);
/** Allocates a handle with no backend texture yet. */
hui_texture_t* hui_texture_register(void);
/** Creates a texture from raw pixels. Takes ownership of `pixels` (hui_alloc'ed, may be NULL). */
HUI_id hui_texture_create_raw(int width, int height, HUI_ETextureFormat format, HUI_EFilterType filter, void* pixels);
void hui_textures_shutdown(void);

/** Decodes an image file into RGBA8 (render/image_decode.c). Free with hui_free. */
uint8_t* hui_image_decode(const uint8_t* data, size_t size, int* width, int* height);

/* ---------------------------------------------------------------------------
 *  Renderer (render/renderer.c)
 * ------------------------------------------------------------------------- */

int  hui_renderer_init(const HUI_BackendVTable* backend, const HUI_BackendInitInfo* info, int threaded);
void hui_renderer_shutdown(void);
int  hui_renderer_is_threaded(void);
/**
 * Sets a function called every few milliseconds while the UI thread is
 * blocked on the render thread (hui_platform_t::wait_service). NULL = none.
 * Call after hui_renderer_init.
 */
void hui_renderer_set_wait_service(void (*service)(void));

/** Blocks until the surface exists. */
int  hui_renderer_create_surface(HUI_id window, const HUI_NativeWindow* native, uint32_t width, uint32_t height);
/** Blocks until every queued frame of the window is rendered and the surface destroyed. */
void hui_renderer_destroy_surface(HUI_id window);

/** The pixel buffers are hui_alloc'ed; the renderer takes ownership and frees them. */
void hui_renderer_create_texture(HUI_id texture, const HUI_TextureDesc* desc, void* pixels);
void hui_renderer_update_texture(HUI_id texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* pixels);
void hui_renderer_set_texture_filter(HUI_id texture, HUI_EFilterType filter);
void hui_renderer_destroy_texture(HUI_id texture);

typedef void (*hui_render_fn)(void* arg);
/** Runs a function on the render thread, in order with the other commands. */
void hui_renderer_call(hui_render_fn function, void* arg, int wait);
/** Blocks until every command queued so far has been executed. */
void hui_renderer_sync(void);

/** One frame of one window travelling from the UI thread to the render thread. */
typedef struct hui_frame_t {
	hui_drawlist_t list;
	HUI_DrawData data;
	/** Optional: receives the rendered pixels. */
	HUI_Capture* capture;
	/** Optional counters owned by the window. */
	hui_atomic_t* frames_in_flight;
	hui_atomic_t* render_us;
	struct hui_frame_t* next;
} hui_frame_t;

/** Returns an empty frame packet (recycled, so its arrays keep their capacity). */
hui_frame_t* hui_renderer_acquire_frame(void);
/** Blocks while `frames_in_flight` is at least `max_frames`. */
void hui_renderer_throttle(hui_atomic_t* frames_in_flight, int max_frames);
/** Queues the frame. `frame->data` must be filled except for the instance / command arrays. */
void hui_renderer_submit_frame(hui_frame_t* frame);

/* Built-in backends */
const HUI_BackendVTable* hui_backend_null(void);
#if defined(HUI_BACKEND_VULKAN_ENABLED)
const HUI_BackendVTable* hui_backend_vulkan(void);
#endif

#endif /* HUI_RENDERER_H */
