/**
 * HUI - core/memory.c
 * Allocator hooks, dynamic arrays, arena, strings and hashing.
 */

#include "base.h"

/* ---------------------------------------------------------------------------
 *  Allocator
 * ------------------------------------------------------------------------- */

static HUI_Allocator g_allocator;

void HUI_SetAllocator(const HUI_Allocator* allocator) {
	if (HUI_IsInitialized()) {
		hui_error(HUI_ERROR_INVALID_OPERATION, HUI_SEVERITY_ERROR, "HUI_SetAllocator must be called before HUI_Init");
		return;
	}
	if (allocator && allocator->alloc && allocator->realloc && allocator->free) {
		g_allocator = *allocator;
	} else {
		memset(&g_allocator, 0, sizeof(g_allocator));
	}
}

void* hui_alloc(size_t size) {
	void* ptr;
	if (size == 0) size = 1;
	ptr = g_allocator.alloc ? g_allocator.alloc(size, g_allocator.user_data) : malloc(size);
	if (!ptr) {
		hui_error(HUI_ERROR_OUT_OF_MEMORY, HUI_SEVERITY_FATAL, "allocation of %zu bytes failed", size);
		abort();
	}
	return ptr;
}

void* hui_calloc(size_t size) {
	void* ptr = hui_alloc(size);
	memset(ptr, 0, size);
	return ptr;
}

void* hui_realloc(void* ptr, size_t size) {
	void* out;
	if (size == 0) size = 1;
	out = g_allocator.realloc ? g_allocator.realloc(ptr, size, g_allocator.user_data) : realloc(ptr, size);
	if (!out) {
		hui_error(HUI_ERROR_OUT_OF_MEMORY, HUI_SEVERITY_FATAL, "reallocation to %zu bytes failed", size);
		abort();
	}
	return out;
}

void hui_free(void* ptr) {
	if (!ptr) return;
	if (g_allocator.free) g_allocator.free(ptr, g_allocator.user_data);
	else free(ptr);
}

uint8_t* HUI_AllocPixels(size_t size) {
	return (uint8_t*)hui_alloc(size);
}

void HUI_FreePixels(uint8_t* pixels) {
	hui_free(pixels);
}

/* ---------------------------------------------------------------------------
 *  Dynamic arrays
 * ------------------------------------------------------------------------- */

void* hui_array_grow_(void* data, uint32_t* cap, uint32_t needed, size_t item_size) {
	uint32_t new_cap = *cap ? *cap : 8;
	while (new_cap < needed) {
		new_cap = new_cap < 0x40000000u ? new_cap * 2 : needed;
	}
	data = hui_realloc(data, (size_t)new_cap * item_size);
	*cap = new_cap;
	return data;
}

/* ---------------------------------------------------------------------------
 *  Arena
 * ------------------------------------------------------------------------- */

void hui_arena_init(hui_arena_t* arena, size_t block_size) {
	arena->head = NULL;
	arena->current = NULL;
	arena->block_size = block_size ? block_size : 16384;
}

void* hui_arena_alloc(hui_arena_t* arena, size_t size) {
	hui_arena_block_t* block = arena->current;
	void* ptr;
	size = (size + 15u) & ~(size_t)15u;
	if (!block || block->used + size > block->cap) {
		size_t cap = size > arena->block_size ? size : arena->block_size;
		block = (hui_arena_block_t*)hui_alloc(sizeof(hui_arena_block_t) + 16 + cap);
		block->next = NULL;
		block->cap = cap;
		block->used = 0;
		if (arena->current) arena->current->next = block;
		else arena->head = block;
		arena->current = block;
	}
	/* the payload starts on a 16-byte boundary after the header */
	ptr = (char*)block + ((sizeof(hui_arena_block_t) + 15u) & ~(size_t)15u) + block->used;
	block->used += size;
	return ptr;
}

char* hui_arena_strndup(hui_arena_t* arena, const char* text, size_t length) {
	char* out = (char*)hui_arena_alloc(arena, length + 1);
	if (length) memcpy(out, text, length);
	out[length] = '\0';
	return out;
}

void hui_arena_free(hui_arena_t* arena) {
	hui_arena_block_t* block = arena->head;
	while (block) {
		hui_arena_block_t* next = block->next;
		hui_free(block);
		block = next;
	}
	arena->head = NULL;
	arena->current = NULL;
}

/* ---------------------------------------------------------------------------
 *  Strings
 * ------------------------------------------------------------------------- */

char* hui_strndup(const char* text, size_t length) {
	char* out = (char*)hui_alloc(length + 1);
	if (length) memcpy(out, text, length);
	out[length] = '\0';
	return out;
}

char* hui_strdup(const char* text) {
	return text ? hui_strndup(text, strlen(text)) : NULL;
}

int hui_str_set(char** dst, const char* src) {
	if (*dst == src) return HUI_FALSE;
	if (*dst && src && strcmp(*dst, src) == 0) return HUI_FALSE;
	if (!*dst && !src) return HUI_FALSE;
	{
		char* copy = hui_strdup(src);
		hui_free(*dst);
		*dst = copy;
	}
	return HUI_TRUE;
}

static int hui_lower(int c) {
	return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

int hui_stricmp(const char* a, const char* b) {
	if (!a) a = "";
	if (!b) b = "";
	for (;; a++, b++) {
		int ca = hui_lower((unsigned char)*a), cb = hui_lower((unsigned char)*b);
		if (ca != cb) return ca - cb;
		if (!ca) return 0;
	}
}

int hui_strnicmp(const char* a, const char* b, size_t n) {
	size_t i;
	for (i = 0; i < n; i++) {
		int ca = hui_lower((unsigned char)a[i]), cb = hui_lower((unsigned char)b[i]);
		if (ca != cb) return ca - cb;
		if (!ca) return 0;
	}
	return 0;
}

uint32_t hui_hash_bytes(const void* data, size_t size) {
	/* FNV-1a */
	const uint8_t* bytes = (const uint8_t*)data;
	uint32_t hash = 2166136261u;
	size_t i;
	for (i = 0; i < size; i++) {
		hash ^= bytes[i];
		hash *= 16777619u;
	}
	return hash;
}

uint32_t hui_hash_str(const char* text) {
	return hui_hash_bytes(text, text ? strlen(text) : 0);
}

/* ---------------------------------------------------------------------------
 *  Paths
 * ------------------------------------------------------------------------- */

static int hui_is_separator(char c) {
	return c == '/' || c == '\\';
}

static int hui_path_is_absolute(const char* path) {
	if (hui_is_separator(path[0])) return HUI_TRUE;
	/* drive letter: "C:" */
	return path[0] && path[1] == ':';
}

char* hui_path_join(const char* base, const char* path) {
	size_t base_len, path_len;
	char* out;
	if (!path) return NULL;
	if (!base || !base[0] || hui_path_is_absolute(path)) return hui_strdup(path);
	base_len = strlen(base);
	path_len = strlen(path);
	out = (char*)hui_alloc(base_len + path_len + 2);
	memcpy(out, base, base_len);
	if (!hui_is_separator(base[base_len - 1])) out[base_len++] = '/';
	memcpy(out + base_len, path, path_len + 1);
	return out;
}

char* hui_path_dir(const char* path) {
	size_t length = path ? strlen(path) : 0;
	while (length > 0 && !hui_is_separator(path[length - 1])) length--;
	return hui_strndup(path ? path : "", length);
}
