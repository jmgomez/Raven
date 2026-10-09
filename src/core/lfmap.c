/**
 * HUI - core/lfmap.c
 */

#include "lfmap.h"

static uint32_t hui_lfmap_hash(uint64_t key) {
	key ^= key >> 33;
	key *= 0xff51afd7ed558ccdULL;
	key ^= key >> 33;
	key *= 0xc4ceb9fe1a85ec53ULL;
	key ^= key >> 33;
	return (uint32_t)key;
}

int hui_lfmap_find(hui_lfmap_t* map, uint64_t key, uint64_t value[2]) {
	hui_lfmap_table_t* table = (hui_lfmap_table_t*)hui_atomic_ptr_load(&map->table);
	uint32_t mask, slot, probe;
	if (!table) return HUI_FALSE;
	mask = table->cap - 1;
	slot = hui_lfmap_hash(key) & mask;
	for (probe = 0; probe < table->cap; probe++) {
		hui_lfmap_slot_t* entry = &table->slots[slot];
		/* reading the state first makes the key and value safe to read */
		if (!hui_atomic_load(&entry->state)) return HUI_FALSE;
		if (entry->key == key) {
			value[0] = entry->value[0];
			value[1] = entry->value[1];
			return HUI_TRUE;
		}
		slot = (slot + 1) & mask;
	}
	return HUI_FALSE;
}

static hui_lfmap_table_t* hui_lfmap_table_new(uint32_t cap) {
	size_t size = sizeof(hui_lfmap_table_t) + sizeof(hui_lfmap_slot_t) * (size_t)(cap - 1);
	hui_lfmap_table_t* table = (hui_lfmap_table_t*)hui_calloc(size);
	table->cap = cap;
	return table;
}

static void hui_lfmap_table_put(hui_lfmap_table_t* table, uint64_t key, const uint64_t value[2]) {
	uint32_t mask = table->cap - 1;
	uint32_t slot = hui_lfmap_hash(key) & mask;
	hui_lfmap_slot_t* entry;
	while (hui_atomic_load(&table->slots[slot].state)) slot = (slot + 1) & mask;
	entry = &table->slots[slot];
	entry->key = key;
	entry->value[0] = value[0];
	entry->value[1] = value[1];
	/* publication: from here on, readers may use the slot */
	hui_atomic_store(&entry->state, 1);
	table->count++;
}

void hui_lfmap_insert(hui_lfmap_t* map, uint64_t key, const uint64_t value[2]) {
	hui_lfmap_table_t* table = (hui_lfmap_table_t*)hui_atomic_ptr_load(&map->table);
	if (!table || (table->count + 1) * 4 > table->cap * 3) {
		hui_lfmap_table_t* larger = hui_lfmap_table_new(table ? table->cap * 2 : 256);
		uint32_t i;
		for (i = 0; table && i < table->cap; i++) {
			if (hui_atomic_load(&table->slots[i].state)) {
				hui_lfmap_table_put(larger, table->slots[i].key, table->slots[i].value);
			}
		}
		hui_lfmap_table_put(larger, key, value);
		hui_atomic_ptr_store(&map->table, larger);
		/* readers may still be probing the old table */
		if (table) {
			table->retired_next = map->retired;
			map->retired = table;
		}
		return;
	}
	hui_lfmap_table_put(table, key, value);
}

void hui_lfmap_collect(hui_lfmap_t* map) {
	hui_lfmap_table_t* table = map->retired;
	while (table) {
		hui_lfmap_table_t* next = table->retired_next;
		hui_free(table);
		table = next;
	}
	map->retired = NULL;
}

void hui_lfmap_clear(hui_lfmap_t* map) {
	hui_lfmap_collect(map);
	hui_free(hui_atomic_ptr_load(&map->table));
	hui_atomic_ptr_store(&map->table, NULL);
}
