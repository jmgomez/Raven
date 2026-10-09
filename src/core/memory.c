/**
 * Raven - core/memory.c
 * Allocator hooks, dynamic arrays, arena, strings and hashing.
 */

#include "base.h"

/* ---------------------------------------------------------------------------
 *  Allocator
 * ------------------------------------------------------------------------- */

static RAllocator g_allocator;

void Rset_allocator(const RAllocator* allocator) {
	if (Ris_initialized()) {
		rvn_error(R_ERROR_INVALID_OPERATION, R_SEVERITY_ERROR, "Rset_allocator must be called before Rinit");
		return;
	}
	if (allocator && allocator->alloc && allocator->realloc && allocator->free) {
		g_allocator = *allocator;
	} else {
		memset(&g_allocator, 0, sizeof(g_allocator));
	}
}

void* rvn_alloc(size_t size) {
	void* ptr;
	if (size == 0) size = 1;
	ptr = g_allocator.alloc ? g_allocator.alloc(size, g_allocator.user_data) : malloc(size);
	if (!ptr) {
		rvn_error(R_ERROR_OUT_OF_MEMORY, R_SEVERITY_FATAL, "allocation of %zu bytes failed", size);
		abort();
	}
	return ptr;
}

void* rvn_calloc(size_t size) {
	void* ptr = rvn_alloc(size);
	memset(ptr, 0, size);
	return ptr;
}

void* rvn_realloc(void* ptr, size_t size) {
	void* out;
	if (size == 0) size = 1;
	out = g_allocator.realloc ? g_allocator.realloc(ptr, size, g_allocator.user_data) : realloc(ptr, size);
	if (!out) {
		rvn_error(R_ERROR_OUT_OF_MEMORY, R_SEVERITY_FATAL, "reallocation to %zu bytes failed", size);
		abort();
	}
	return out;
}

void rvn_free(void* ptr) {
	if (!ptr) return;
	if (g_allocator.free) g_allocator.free(ptr, g_allocator.user_data);
	else free(ptr);
}

uint8_t* Rpixels_alloc(size_t size) {
	return (uint8_t*)rvn_alloc(size);
}

void Rpixels_free(uint8_t* pixels) {
	rvn_free(pixels);
}

/* ---------------------------------------------------------------------------
 *  Dynamic arrays
 * ------------------------------------------------------------------------- */

void* rvn_array_grow_(void* data, uint32_t* cap, uint32_t needed, size_t item_size) {
	uint32_t new_cap = *cap ? *cap : 8;
	while (new_cap < needed) {
		new_cap = new_cap < 0x40000000u ? new_cap * 2 : needed;
	}
	data = rvn_realloc(data, (size_t)new_cap * item_size);
	*cap = new_cap;
	return data;
}

/* ---------------------------------------------------------------------------
 *  Arena
 * ------------------------------------------------------------------------- */

void rvn_arena_init(rvn_arena_t* arena, size_t block_size) {
	arena->head = NULL;
	arena->current = NULL;
	arena->block_size = block_size ? block_size : 16384;
}

void* rvn_arena_alloc(rvn_arena_t* arena, size_t size) {
	rvn_arena_block_t* block = arena->current;
	void* ptr;
	size = (size + 15u) & ~(size_t)15u;
	if (!block || block->used + size > block->cap) {
		size_t cap = size > arena->block_size ? size : arena->block_size;
		block = (rvn_arena_block_t*)rvn_alloc(sizeof(rvn_arena_block_t) + 16 + cap);
		block->next = NULL;
		block->cap = cap;
		block->used = 0;
		if (arena->current) arena->current->next = block;
		else arena->head = block;
		arena->current = block;
	}
	/* the payload starts on a 16-byte boundary after the header */
	ptr = (char*)block + ((sizeof(rvn_arena_block_t) + 15u) & ~(size_t)15u) + block->used;
	block->used += size;
	return ptr;
}

char* rvn_arena_strndup(rvn_arena_t* arena, const char* text, size_t length) {
	char* out = (char*)rvn_arena_alloc(arena, length + 1);
	if (length) memcpy(out, text, length);
	out[length] = '\0';
	return out;
}

void rvn_arena_free(rvn_arena_t* arena) {
	rvn_arena_block_t* block = arena->head;
	while (block) {
		rvn_arena_block_t* next = block->next;
		rvn_free(block);
		block = next;
	}
	arena->head = NULL;
	arena->current = NULL;
}

/* ---------------------------------------------------------------------------
 *  Strings
 * ------------------------------------------------------------------------- */

char* rvn_strndup(const char* text, size_t length) {
	char* out = (char*)rvn_alloc(length + 1);
	if (length) memcpy(out, text, length);
	out[length] = '\0';
	return out;
}

char* rvn_strdup(const char* text) {
	return text ? rvn_strndup(text, strlen(text)) : NULL;
}

int rvn_str_set(char** dst, const char* src) {
	if (*dst == src) return R_FALSE;
	if (*dst && src && strcmp(*dst, src) == 0) return R_FALSE;
	if (!*dst && !src) return R_FALSE;
	{
		char* copy = rvn_strdup(src);
		rvn_free(*dst);
		*dst = copy;
	}
	return R_TRUE;
}

static int rvn_lower(int c) {
	return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

int rvn_stricmp(const char* a, const char* b) {
	if (!a) a = "";
	if (!b) b = "";
	for (;; a++, b++) {
		int ca = rvn_lower((unsigned char)*a), cb = rvn_lower((unsigned char)*b);
		if (ca != cb) return ca - cb;
		if (!ca) return 0;
	}
}

int rvn_strnicmp(const char* a, const char* b, size_t n) {
	size_t i;
	for (i = 0; i < n; i++) {
		int ca = rvn_lower((unsigned char)a[i]), cb = rvn_lower((unsigned char)b[i]);
		if (ca != cb) return ca - cb;
		if (!ca) return 0;
	}
	return 0;
}

uint32_t rvn_hash_bytes(const void* data, size_t size) {
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

uint32_t rvn_hash_str(const char* text) {
	return rvn_hash_bytes(text, text ? strlen(text) : 0);
}

/* ---------------------------------------------------------------------------
 *  Paths
 * ------------------------------------------------------------------------- */

static int rvn_is_separator(char c) {
	return c == '/' || c == '\\';
}

static int rvn_path_is_absolute(const char* path) {
	if (rvn_is_separator(path[0])) return R_TRUE;
	/* drive letter: "C:" */
	return path[0] && path[1] == ':';
}

char* rvn_path_join(const char* base, const char* path) {
	size_t base_len, path_len;
	char* out;
	if (!path) return NULL;
	if (!base || !base[0] || rvn_path_is_absolute(path)) return rvn_strdup(path);
	base_len = strlen(base);
	path_len = strlen(path);
	out = (char*)rvn_alloc(base_len + path_len + 2);
	memcpy(out, base, base_len);
	if (!rvn_is_separator(base[base_len - 1])) out[base_len++] = '/';
	memcpy(out + base_len, path, path_len + 1);
	return out;
}

char* rvn_path_dir(const char* path) {
	size_t length = path ? strlen(path) : 0;
	while (length > 0 && !rvn_is_separator(path[length - 1])) length--;
	return rvn_strndup(path ? path : "", length);
}
