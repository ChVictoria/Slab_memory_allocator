#ifndef ALLOCATOR_H
#define ALLOCATOR_H


#include <stddef.h>
#include "config.h"

void init_allocator();
void deinit_allocator();
void *mem_alloc(size_t size);
void mem_free(void *ptr);
void *mem_realloc(void *ptr, size_t size);
void mem_show();

typedef enum {
    SLAB,
    EXTENT_FROM_ARENA,
    EXTENT_BEYOND_ARENA
} alloc_type;

/************ Struct typedefs **********/

/**Headers of allocator's entities, every has the field alloc_type to recognize what the entity belongs to
 * A pointer, returned by mem_free is converted to page number by the Radix tree and the item of the tree will point
 * to the header of an entity, that object belongs to, so we can add it back to cache or return to the kernel
 */
typedef struct {
    alloc_type type;
    size_t size;

} beyond_arena_extent;

struct slab_object {
    int size_index;
    size_t size;
    size_t prev_size;
};

typedef struct arena_extent {
    alloc_type type;
    void *arena;
    int pages_number;
    void *start;
    struct arena_extent *next_same_size_extent;
    struct arena_extent *next_extent_in_basket;
} arena_extent;

typedef struct slab{
    alloc_type type;
    void *arena;
    void *start;
    struct slab *next_slab;
    size_t obj_size;
    int objs_number;
    int bitmap[];
} slab;

/**Allocator can have multiple arenas, so we need a structure of header, that would be used to search for free entities
 * inside arena and for management arenas in linked list(ex. arena was full, so we have excluded it from the list, now
 * some entity inside arena has been freed, we should know, which arena it belongs to return this arena to the list)
 */
#include "hashtable.h"

typedef struct arena {
    struct arena *prev_arena;
    void *start;
    slab *slab_cache[NUMBER_OF_SLAB_SIZES];
    void *root_extent;
    hashtable extent_cache;
    int *bitmap;
} arena;




#endif