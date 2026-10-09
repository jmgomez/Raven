/**
 * HUI - core/handle.c
 */

#include "handle.h"

void hui_pool_init(hui_pool_t* pool, hui_handle_type_t type) {
	memset(pool, 0, sizeof(*pool));
	pool->type = (uint32_t)type;
}

void hui_pool_free(hui_pool_t* pool) {
	hui_free(pool->items);
	hui_free(pool->generations);
	hui_free(pool->free_list);
	memset(pool, 0, sizeof(*pool));
}

HUI_id hui_pool_add(hui_pool_t* pool, void* object) {
	uint32_t index;
	if (pool->free_count > 0) {
		index = pool->free_list[--pool->free_count];
	} else {
		if (pool->used > HUI_HANDLE_MAX_INDEX) {
			hui_error(HUI_ERROR_OUT_OF_MEMORY, HUI_SEVERITY_ERROR, "too many live objects of one type");
			return HUI_INVALID_ID;
		}
		if (pool->used == pool->cap) {
			uint32_t old_cap = pool->cap;
			uint32_t new_cap = old_cap ? old_cap * 2 : 64;
			pool->items = (void**)hui_realloc(pool->items, sizeof(void*) * new_cap);
			pool->generations = (uint8_t*)hui_realloc(pool->generations, new_cap);
			memset(pool->items + old_cap, 0, sizeof(void*) * (new_cap - old_cap));
			memset(pool->generations + old_cap, 0, new_cap - old_cap);
			pool->cap = new_cap;
		}
		index = pool->used++;
	}
	pool->items[index] = object;
	pool->live++;
	return hui_handle_make(pool->type, pool->generations[index], index);
}

void* hui_pool_get(const hui_pool_t* pool, HUI_id id) {
	uint32_t index = hui_handle_index(id);
	if (hui_handle_type(id) != pool->type || index >= pool->used) return NULL;
	if (pool->generations[index] != hui_handle_generation(id)) return NULL;
	return pool->items[index];
}

void hui_pool_remove(hui_pool_t* pool, HUI_id id) {
	uint32_t index = hui_handle_index(id);
	if (!hui_pool_get(pool, id)) return;
	pool->items[index] = NULL;
	pool->generations[index]++;
	HUI_ARRAY_RESERVE(pool->free_list, pool->free_cap, pool->free_count + 1);
	pool->free_list[pool->free_count++] = index;
	pool->live--;
}

void* hui_pool_at(const hui_pool_t* pool, uint32_t index) {
	return index < pool->used ? pool->items[index] : NULL;
}
