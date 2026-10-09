/**
 * HUI - core/handle.h
 * Generational handle pools: HUI_id <-> object pointer.
 *
 * Handle layout (32 bits):   [ type : 4 | generation : 8 | index : 20 ]
 *
 * The type tag makes a font handle invalid where a widget is expected, the
 * generation makes a deleted handle invalid even after its slot is reused.
 * Type 0 and type 15 are never used, so 0 and HUI_INVALID_ID never resolve.
 */

#ifndef HUI_HANDLE_H
#define HUI_HANDLE_H

#include "base.h"

typedef enum hui_handle_type_t {
	HUI_HANDLE_WINDOW = 1,
	HUI_HANDLE_WIDGET,
	HUI_HANDLE_FONT,
	HUI_HANDLE_TEXTURE,
	HUI_HANDLE_TIMER,
	HUI_HANDLE_TREE_ITEM,
	/** Pseudo-handle naming a glyph atlas page inside draw lists (never in a pool). */
	HUI_HANDLE_ATLAS_PAGE
} hui_handle_type_t;

#define HUI_HANDLE_INDEX_BITS 20u
#define HUI_HANDLE_INDEX_MASK 0xFFFFFu
#define HUI_HANDLE_MAX_INDEX  HUI_HANDLE_INDEX_MASK

HUI_INLINE uint32_t hui_handle_index(HUI_id id) { return id & HUI_HANDLE_INDEX_MASK; }
HUI_INLINE uint32_t hui_handle_generation(HUI_id id) { return (id >> HUI_HANDLE_INDEX_BITS) & 0xFFu; }
HUI_INLINE uint32_t hui_handle_type(HUI_id id) { return id >> 28; }
HUI_INLINE HUI_id hui_handle_make(uint32_t type, uint32_t generation, uint32_t index) {
	return (type << 28) | ((generation & 0xFFu) << HUI_HANDLE_INDEX_BITS) | (index & HUI_HANDLE_INDEX_MASK);
}

typedef struct hui_pool_t {
	void** items;
	uint8_t* generations;
	uint32_t* free_list;
	uint32_t cap;
	/** Number of slots ever used (high-water mark). */
	uint32_t used;
	uint32_t free_count;
	uint32_t free_cap;
	uint32_t live;
	uint32_t type;
} hui_pool_t;

void   hui_pool_init(hui_pool_t* pool, hui_handle_type_t type);
void   hui_pool_free(hui_pool_t* pool);
/** Registers an object and returns its handle (HUI_INVALID_ID when the pool is full). */
HUI_id hui_pool_add(hui_pool_t* pool, void* object);
/** Returns the object, or NULL when the handle is stale, deleted or of another type. */
void*  hui_pool_get(const hui_pool_t* pool, HUI_id id);
void   hui_pool_remove(hui_pool_t* pool, HUI_id id);
/** Iteration over live objects: returns the object at slot `index` or NULL. */
void*  hui_pool_at(const hui_pool_t* pool, uint32_t index);

#endif /* HUI_HANDLE_H */
