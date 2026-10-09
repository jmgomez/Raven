/**
 * HUI - render/texture.c
 * Texture objects and the public texture API, including asynchronous
 * decoding on the job system.
 */

#include "renderer.h"
#include "../core/context.h"

void hui_tasks_run(void); /* core/context.c */

hui_texture_t* hui_texture_get(HUI_id id) {
	return (hui_texture_t*)hui_pool_get(&g_hui.textures, id);
}

hui_texture_t* hui_texture_register(void) {
	hui_texture_t* texture = (hui_texture_t*)hui_calloc(sizeof(hui_texture_t));
	texture->format = HUI_TEXTURE_RGBA8;
	texture->filter = HUI_FILTER_LINEAR;
	texture->id = hui_pool_add(&g_hui.textures, texture);
	if (texture->id == HUI_INVALID_ID) {
		hui_free(texture);
		return NULL;
	}
	return texture;
}

/* Sends the pixels to the renderer. Takes ownership of `pixels`. */
static void hui_texture_upload(hui_texture_t* texture, int width, int height, void* pixels) {
	HUI_TextureDesc desc;
	if (texture->created) hui_renderer_destroy_texture(texture->id);
	texture->width = width;
	texture->height = height;
	desc.width = (HUI_uint)width;
	desc.height = (HUI_uint)height;
	desc.format = texture->format;
	desc.filter = texture->filter;
	hui_renderer_create_texture(texture->id, &desc, pixels);
	texture->created = HUI_TRUE;
	hui_atomic_store(&texture->ready, 1);
}

HUI_id hui_texture_create_raw(int width, int height, HUI_ETextureFormat format, HUI_EFilterType filter, void* pixels) {
	hui_texture_t* texture;
	if (width <= 0 || height <= 0 || width > 16384 || height > 16384) {
		hui_free(pixels);
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "invalid texture size %d x %d", width, height);
		return HUI_INVALID_ID;
	}
	texture = hui_texture_register();
	if (!texture) {
		hui_free(pixels);
		return HUI_INVALID_ID;
	}
	texture->format = format;
	texture->filter = filter;
	hui_texture_upload(texture, width, height, pixels);
	return texture->id;
}

void hui_textures_shutdown(void) {
	uint32_t i;
	for (i = 0; i < g_hui.textures.used; i++) {
		hui_texture_t* texture = (hui_texture_t*)hui_pool_at(&g_hui.textures, i);
		if (!texture) continue;
		if (texture->created) hui_renderer_destroy_texture(texture->id);
		hui_pool_remove(&g_hui.textures, texture->id);
		hui_free(texture);
	}
}

/* ---------------------------------------------------------------------------
 *  Asynchronous decoding
 * ------------------------------------------------------------------------- */

typedef struct hui_texture_job_t {
	HUI_id id;
	uint8_t* data;
	size_t size;
	char* path;
	uint8_t* pixels;
	int width, height;
} hui_texture_job_t;

/* UI thread: the decoded pixels become the texture, unless it was deleted meanwhile. */
static void hui_texture_job_finish(void* arg) {
	hui_texture_job_t* job = (hui_texture_job_t*)arg;
	hui_texture_t* texture = g_hui.initialized ? hui_texture_get(job->id) : NULL;
	if (texture && job->pixels) {
		hui_texture_upload(texture, job->width, job->height, job->pixels);
		job->pixels = NULL;
		hui_invalidate_all_windows();
	} else if (texture) {
		hui_atomic_store(&texture->ready, 1);
		hui_error(HUI_ERROR_INVALID_FILE_FORMAT, HUI_SEVERITY_ERROR, "asynchronous texture: cannot decode %s",
			job->path ? job->path : "the image");
	}
	hui_free(job->pixels);
	hui_free(job->path);
	hui_free(job);
}

/* Worker thread. */
static void hui_texture_job_run(void* arg) {
	hui_texture_job_t* job = (hui_texture_job_t*)arg;
	if (job->path) job->data = (uint8_t*)hui_read_file(job->path, &job->size);
	if (job->data) job->pixels = hui_image_decode(job->data, job->size, &job->width, &job->height);
	hui_free(job->data);
	job->data = NULL;
	HUI_PostTask(hui_texture_job_finish, job);
}

static HUI_id hui_texture_create_async(const char* data, size_t size, const char* path) {
	hui_texture_t* texture = hui_texture_register();
	hui_texture_job_t* job;
	if (!texture) return HUI_INVALID_ID;
	job = (hui_texture_job_t*)hui_calloc(sizeof(hui_texture_job_t));
	job->id = texture->id;
	if (path) {
		job->path = hui_strdup(path);
	} else {
		job->data = (uint8_t*)hui_alloc(size);
		memcpy(job->data, data, size);
		job->size = size;
	}
	hui_jobs_submit(NULL, hui_texture_job_run, job);
	return texture->id;
}

/* ---------------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------------- */

HUI_id HUI_CreateTexture(const char* data, size_t data_size) {
	int width = 0, height = 0;
	uint8_t* pixels;
	HUI_API_GUARD(HUI_INVALID_ID);
	pixels = hui_image_decode((const uint8_t*)data, data_size, &width, &height);
	if (!pixels) {
		hui_error(HUI_ERROR_INVALID_FILE_FORMAT, HUI_SEVERITY_ERROR, "HUI_CreateTexture: cannot decode the image");
		return HUI_INVALID_ID;
	}
	return hui_texture_create_raw(width, height, HUI_TEXTURE_RGBA8, HUI_FILTER_LINEAR, pixels);
}

HUI_id HUI_CreateTextureAsync(const char* data, size_t data_size) {
	HUI_API_GUARD(HUI_INVALID_ID);
	if (!data || data_size == 0) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_CreateTextureAsync: empty buffer");
		return HUI_INVALID_ID;
	}
	return hui_texture_create_async(data, data_size, NULL);
}

HUI_id HUI_CreateTextureFromFile(const char* path) {
	size_t size = 0;
	char* data;
	int width = 0, height = 0;
	uint8_t* pixels;
	HUI_API_GUARD(HUI_INVALID_ID);
	data = hui_read_file(path, &size);
	if (!data) {
		hui_error(HUI_ERROR_FILE_NOT_FOUND, HUI_SEVERITY_ERROR, "HUI_CreateTextureFromFile: cannot read '%s'", path ? path : "");
		return HUI_INVALID_ID;
	}
	pixels = hui_image_decode((const uint8_t*)data, size, &width, &height);
	hui_free(data);
	if (!pixels) {
		hui_error(HUI_ERROR_INVALID_FILE_FORMAT, HUI_SEVERITY_ERROR, "HUI_CreateTextureFromFile: cannot decode '%s'", path);
		return HUI_INVALID_ID;
	}
	return hui_texture_create_raw(width, height, HUI_TEXTURE_RGBA8, HUI_FILTER_LINEAR, pixels);
}

HUI_id HUI_CreateTextureFromFileAsync(const char* path) {
	HUI_API_GUARD(HUI_INVALID_ID);
	if (!path) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_CreateTextureFromFileAsync: NULL path");
		return HUI_INVALID_ID;
	}
	return hui_texture_create_async(NULL, 0, path);
}

HUI_id HUI_CreateTextureFromPixels(const uint8_t* rgba, int width, int height) {
	uint8_t* copy;
	size_t size;
	HUI_API_GUARD(HUI_INVALID_ID);
	if (!rgba || width <= 0 || height <= 0) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_CreateTextureFromPixels: invalid arguments");
		return HUI_INVALID_ID;
	}
	size = (size_t)width * (size_t)height * 4;
	copy = (uint8_t*)hui_alloc(size);
	memcpy(copy, rgba, size);
	return hui_texture_create_raw(width, height, HUI_TEXTURE_RGBA8, HUI_FILTER_LINEAR, copy);
}

void HUI_UpdateTexture(HUI_id id, const uint8_t* rgba, int width, int height) {
	hui_texture_t* texture;
	uint8_t* copy;
	size_t size;
	HUI_API_GUARD_VOID();
	texture = hui_texture_get(id);
	if (!texture) {
		hui_error(HUI_ERROR_INVALID_ID, HUI_SEVERITY_ERROR, "HUI_UpdateTexture: invalid texture");
		return;
	}
	if (texture->external || texture->format != HUI_TEXTURE_RGBA8) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_UpdateTexture: this texture cannot be updated");
		return;
	}
	if (!rgba || width <= 0 || height <= 0 || width > 16384 || height > 16384) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_UpdateTexture: invalid arguments");
		return;
	}
	size = (size_t)width * (size_t)height * 4;
	copy = (uint8_t*)hui_alloc(size);
	memcpy(copy, rgba, size);
	if (texture->created && texture->width == width && texture->height == height) {
		hui_renderer_update_texture(id, 0, 0, (uint32_t)width, (uint32_t)height, copy);
		hui_repaint_all_windows();
	} else {
		/* widgets sized after the texture must be measured again */
		hui_texture_upload(texture, width, height, copy);
		hui_invalidate_all_windows();
	}
}

int HUI_IsTextureReady(HUI_id id) {
	hui_texture_t* texture = g_hui.initialized ? hui_texture_get(id) : NULL;
	return texture ? hui_atomic_load(&texture->ready) != 0 : HUI_FALSE;
}

void HUI_WaitForTexture(HUI_id id) {
	HUI_API_GUARD_VOID();
	for (;;) {
		hui_texture_t* texture = hui_texture_get(id);
		if (!texture || hui_atomic_load(&texture->ready)) return;
		/* the result is delivered by a task posted to this thread */
		hui_tasks_run();
		texture = hui_texture_get(id);
		if (!texture || hui_atomic_load(&texture->ready)) return;
		hui_sleep_ms(1);
	}
}

void HUI_DeleteTexture(HUI_id id) {
	hui_texture_t* texture;
	HUI_API_GUARD_VOID();
	texture = hui_texture_get(id);
	if (!texture) {
		hui_error(HUI_ERROR_INVALID_ID, HUI_SEVERITY_ERROR, "HUI_DeleteTexture: invalid texture");
		return;
	}
	if (texture->created) hui_renderer_destroy_texture(id);
	/* the image behind an external texture belongs to the application, which is
	 * about to destroy it: return only once the backend no longer uses it */
	if (texture->external) hui_renderer_sync();
	hui_pool_remove(&g_hui.textures, id);
	hui_free(texture);
	hui_invalidate_all_windows();
}

int HUI_IsValidTexture(HUI_id id) {
	return g_hui.initialized && hui_texture_get(id) != NULL;
}

void HUI_GetTextureSize(HUI_id id, int* width, int* height) {
	hui_texture_t* texture = g_hui.initialized ? hui_texture_get(id) : NULL;
	if (width) *width = texture ? texture->width : 0;
	if (height) *height = texture ? texture->height : 0;
}

void HUI_SetTextureFilter(HUI_id id, HUI_EFilterType filter) {
	hui_texture_t* texture;
	HUI_API_GUARD_VOID();
	texture = hui_texture_get(id);
	if (!texture) {
		hui_error(HUI_ERROR_INVALID_ID, HUI_SEVERITY_ERROR, "HUI_SetTextureFilter: invalid texture");
		return;
	}
	if (filter != HUI_FILTER_LINEAR && filter != HUI_FILTER_NEAREST) {
		hui_error(HUI_ERROR_INVALID_ENUM, HUI_SEVERITY_ERROR, "HUI_SetTextureFilter: invalid filter");
		return;
	}
	if (texture->filter == filter) return;
	texture->filter = filter;
	if (texture->created) hui_renderer_set_texture_filter(id, filter);
	hui_repaint_all_windows();
}
