#include <stddef.h>
#include <memoryapi.h>
#include <math.h>
#include "radix_tree.h"
#include "bitmap.h"
#include "kernel.h"
#include "allocator.h"
#include <stdio.h>
/************     Macroses     ***********/
#define ptr_to_radix_key(ptr) (((unsigned long long) (ptr) & (~(PAGE_SIZE - 1))) >> (int) log2(PAGE_SIZE))
/************ Static variables ***********/

static arena *last_arena = NULL;//last node of linked list of arenas, that have free space
static radix_tree *alloc_tree = NULL;

/*************** Func prototypes *********/
void *slab_alloc(size_t size, arena *arena);


/************ Private functions ************/

arena *create_arena() {
    arena *new_arena = kernel_alloc(sizeof(arena));
    new_arena->start = kernel_alloc(ARENA_SIZE);
    new_arena->prev_arena = last_arena;

    size_t slab_cache_size = NUMBER_OF_SLAB_SIZES * sizeof(slab *);
    memset(new_arena->slab_cache, 0, slab_cache_size);

    new_arena->extent_cache = create_hashtable();
    arena_extent *root_extent = kernel_alloc(sizeof(arena_extent));
    root_extent->type = EXTENT_FROM_ARENA;
    root_extent->pages_number = NUMBER_ARENA_PAGES;
    root_extent->start = new_arena->start;
    root_extent->arena = new_arena;
    new_arena->root_extent = root_extent;
    hashtable_insert(new_arena->extent_cache, root_extent);
    unsigned long long radix_key = ptr_to_radix_key(root_extent->start);
    radix_tree_insert(radix_key, root_extent, alloc_tree);
    radix_tree_insert(radix_key + root_extent->pages_number-1, root_extent, alloc_tree);

    int *bitmap = kernel_alloc(NUMBER_ARENA_PAGES*PAGE_SIZE);
    create_bitmap(NUMBER_ARENA_PAGES, bitmap, EXTENT_FROM_ARENA);
    set_bit(NUMBER_ARENA_PAGES - 1, FREE, bitmap, NUMBER_ARENA_PAGES);
    new_arena->bitmap = bitmap;

    last_arena = new_arena;
    return new_arena;
}

void covert_size_to_slab_object(double size, struct slab_object *obj) {
    if (size <= MAX_ALIGN * N) {
        obj->size_index = (int) ceil(size / MAX_ALIGN) - 1;
        obj->size = MAX_ALIGN * (obj->size_index+1);
        obj->prev_size = obj->size - MAX_ALIGN;
    } else {
        size_t size_n = MAX_ALIGN * N;
        unsigned long long group = ceil(log2(floor((size - size_n - 1) / (M * MAX_ALIGN * 2))));
        size_t max_group_size = size_n + M * MAX_ALIGN * 2 * (int) ((1 << group) - 1);
        size_t step = (1 << group) * MAX_ALIGN;
        unsigned long long group_index = M - floor((max_group_size - size) / step);
        obj->size_index = N + (group - 1) * M + group_index - 1;
        obj->size = max_group_size - (M - group_index) * step;
        obj->prev_size = obj->size - MAX_ALIGN * (1 << group);
    }
}

int gcd(int x, int y) {
    int temp;
    while (y != 0) {
        temp = y;
        y = x % y;
        x = temp;
    }
    return abs(x);
}

/** 1.radix_tree_find in bitmap best fit extent, set bit to 0
   *  2.exclude it from hashtable
   *  3.divide it into two extents: needed size, (sizeof initial extent - needed size)
   *                                                                      |->add second to hashtable(change size in struct)
   *  4.set 1 in bitmap for second extent
   *  5.return pointer to first extent's start
   */
void *get_free_extent(int pages_number, arena *a) {
    int bf_index = find_best_fit(pages_number - 1, a->bitmap, NUMBER_ARENA_PAGES);
    if (bf_index == -1) {
        return NULL;
    }
    arena_extent *free_extent = hashtable_pop(a->extent_cache, bf_index+1);
    if (!hashtable_find_by_key(a->extent_cache, free_extent->pages_number)) {
        set_bit(free_extent->pages_number - 1, BUSY, a->bitmap, NUMBER_ARENA_PAGES);
    }
    if (bf_index + 1 == pages_number) {
        return free_extent->start;
    }
    void *needed_extent_ptr = free_extent->start;
    free_extent->start = (void *)((unsigned long long)needed_extent_ptr + pages_number*PAGE_SIZE);
    free_extent->pages_number = free_extent->pages_number - pages_number;
    hashtable_insert(a->extent_cache, free_extent);
    set_bit(free_extent->pages_number - 1, FREE, a->bitmap, NUMBER_ARENA_PAGES);
    radix_tree_insert(ptr_to_radix_key(free_extent->start), free_extent, alloc_tree);
    void * value = radix_tree_get_value(ptr_to_radix_key(free_extent->start), alloc_tree);
    return needed_extent_ptr;

}

void *arena_extent_alloc(size_t size, arena *arena) {
    int pages_number = ceil((double) size / PAGE_SIZE);
    void *extent_start = get_free_extent(pages_number, arena);
    if (extent_start == NULL) {
        return NULL;
    }
    arena_extent *extent_header = mem_alloc(sizeof(arena_extent));

    extent_header->start = extent_start;
    extent_header->pages_number = pages_number;
    extent_header->arena = arena;
    extent_header->type = EXTENT_FROM_ARENA;
    extent_header->next_same_size_extent = NULL;
    extent_header->next_extent_in_basket = NULL;
    unsigned long long radix_key = ptr_to_radix_key(extent_start);
    radix_tree_insert(radix_key, extent_header, alloc_tree);
    radix_tree_insert(radix_key + extent_header->pages_number-1, extent_header, alloc_tree);
    return extent_start;
}

arena_extent *merge_extents(arena_extent *neighbor, arena_extent *extent) {
    delete_extent_from_hashtable(((arena *) neighbor->arena)->extent_cache, neighbor);
    extent->pages_number = extent->pages_number + neighbor->pages_number;
    unsigned long long radix_key;
    if (extent->start < neighbor->start) {
        radix_key = ptr_to_radix_key(neighbor->start);
        radix_tree_delete_item(radix_key, alloc_tree);
        radix_tree_delete_item(radix_key - 1, alloc_tree);
        radix_tree_insert(radix_key + neighbor->pages_number-1, extent, alloc_tree);
    } else {
        radix_key = ptr_to_radix_key(extent->start);
        radix_tree_delete_item(radix_key, alloc_tree);
        radix_tree_delete_item(radix_key - 1, alloc_tree);
        extent->start = neighbor->start;
        radix_tree_insert(radix_key - neighbor->pages_number, extent, alloc_tree);
    }

    if (!hashtable_find_by_key(((arena *) neighbor->arena)->extent_cache, neighbor->pages_number)) {
        set_bit(neighbor->pages_number - 1, BUSY, ((arena *) neighbor->arena)->bitmap, NUMBER_ARENA_PAGES);
    }
    mem_free(neighbor);
    return extent;
}

void arena_extent_free(arena_extent *extent) {

    arena_extent *result = extent;
    arena_extent *neighbor = radix_tree_get_value(ptr_to_radix_key(extent->start) - 1, alloc_tree);
    if (neighbor != NULL) {
        if ((neighbor->type == EXTENT_FROM_ARENA) && (hashtable_find_by_value(((arena *) neighbor->arena)->extent_cache,
                                                                              neighbor))) {
            result = merge_extents(neighbor, extent);
        }
    }
    neighbor = radix_tree_get_value(ptr_to_radix_key(result->start) + result->pages_number, alloc_tree);
    if (neighbor != NULL) {
        if ((neighbor->type == EXTENT_FROM_ARENA) && (hashtable_find_by_value(((arena *) neighbor->arena)->extent_cache,
                                                                              neighbor))) {
            result = merge_extents(neighbor, result);
        }
    }

    hashtable_insert(((arena *) result->arena)->extent_cache, result);
    set_bit(result->pages_number - 1, FREE, ((arena *) result->arena)->bitmap, NUMBER_ARENA_PAGES);

}

void *realloc_arena_extent(arena_extent *extent, size_t size) {
    if (size > extent->pages_number * PAGE_SIZE) {
        arena_extent *neighbor = radix_tree_get_value(ptr_to_radix_key(extent->start) +extent->pages_number, alloc_tree);
        int add_pages = ceil((double) size / PAGE_SIZE) - extent->pages_number;
        if ((neighbor->type == EXTENT_FROM_ARENA) && (neighbor->pages_number >= add_pages) &&
            hashtable_find_by_value(((arena *) neighbor->arena)->extent_cache, neighbor)) {
            if (neighbor->pages_number == add_pages) {
                extent = merge_extents(neighbor, extent);
            } else {
                unsigned long long radix_key_nb = ptr_to_radix_key(neighbor->start);
                radix_tree_delete_item(radix_key_nb, alloc_tree);
                radix_tree_delete_item(radix_key_nb - 1, alloc_tree);
                radix_tree_insert(radix_key_nb + add_pages - 1, extent, alloc_tree);
                radix_tree_insert(radix_key_nb + add_pages, neighbor, alloc_tree);
                delete_extent_from_hashtable(((arena *) neighbor->arena)->extent_cache, neighbor);
                if (!hashtable_find_by_key(((arena *) neighbor->arena)->extent_cache, neighbor->pages_number)) {
                    set_bit(neighbor->pages_number - 1, BUSY, ((arena *) neighbor->arena)->bitmap, NUMBER_ARENA_PAGES);
                }

                neighbor->pages_number = neighbor->pages_number - add_pages;
                neighbor->start = neighbor->start + add_pages * PAGE_SIZE;
                hashtable_insert(((arena *) neighbor->arena)->extent_cache, neighbor);

                set_bit(neighbor->pages_number - 1, FREE, ((arena *) neighbor->arena)->bitmap, NUMBER_ARENA_PAGES);

                extent->pages_number = extent->pages_number + add_pages;

            }

            return extent->start;
        } else {
            void *ptr = extent->start;
            void *new_ptr = arena_extent_alloc(size, extent->arena);
            memcpy(new_ptr, ptr, extent->pages_number * PAGE_SIZE);
            arena_extent_free(extent);
            return new_ptr;
        }
    } else {
        int redundant_pages = extent->pages_number - ceil((double) size / PAGE_SIZE);
        extent->pages_number = extent->pages_number - redundant_pages;
        unsigned long long radix_key_ext = ptr_to_radix_key(extent->start);
        radix_tree_insert(radix_key_ext + extent->pages_number - 1, extent, alloc_tree);
        arena_extent *new_extent = mem_alloc(sizeof(arena_extent));
        new_extent->start = extent->start + extent->pages_number * PAGE_SIZE;
        new_extent->pages_number = redundant_pages;
        new_extent->arena = extent->arena;
        new_extent->type = EXTENT_FROM_ARENA;
        new_extent->next_same_size_extent = NULL;
        new_extent->next_extent_in_basket = NULL;
        radix_tree_insert(radix_key_ext + extent->pages_number, new_extent, alloc_tree);
        radix_tree_insert(radix_key_ext + extent->pages_number + new_extent->pages_number - 1, new_extent, alloc_tree);
        hashtable_insert(((arena *) new_extent->arena)->extent_cache, new_extent);
        set_bit(new_extent->pages_number - 1, FREE, ((arena *) new_extent->arena)->bitmap, NUMBER_ARENA_PAGES);
        return extent->start;
    }

}


int create_slab(arena *arena, struct slab_object obj) {
    int pages_in_extent = (int) obj.size / gcd((int) obj.size, PAGE_SIZE);
    void *extent_start = get_free_extent(pages_in_extent, arena);
    if (extent_start == NULL) {
        return -1;
    }
    int objs_number = pages_in_extent * PAGE_SIZE / (int) obj.size;
    struct bitmap_levels lvls;
    calc_bitmap_levels(objs_number, &lvls);
    int bitmap_len = 1;
    for (int i=0; i<lvls.levels_num-1;i++){
        bitmap_len += lvls.low_levels[i];
    }

    int header_size = (int) (sizeof(slab) + sizeof(int[bitmap_len]));
    slab *header_ptr;
    //header_size > obj.prev_size && header_size <= obj.size
    if (header_size <= MAX_SMALL_SLAB_SIZE && header_size>((double)2/3*obj.size)) {
        header_ptr = (slab *) extent_start;

    } else {
        header_ptr = slab_alloc(header_size, arena);
    }

    header_ptr->type = SLAB;
    header_ptr->obj_size = obj.size;
    header_ptr->objs_number = objs_number;
    header_ptr->arena = arena;
    header_ptr->start = extent_start;
    header_ptr->next_slab = NULL;
    create_bitmap(objs_number, header_ptr->bitmap, SLAB);
    if (header_ptr == extent_start){
        int header_objs = ceil((double)header_size/(double)obj.size);
        for(int i=0;i<header_objs;i++){
            set_bit(i, BUSY, header_ptr->bitmap, objs_number);
        }

    }
    arena->slab_cache[obj.size_index] = header_ptr;

    unsigned long long radix_key = ptr_to_radix_key(extent_start);
    for (int page = 0; page < pages_in_extent; page++) {
        radix_tree_insert(radix_key, header_ptr, alloc_tree);
    }
    return 0;
}


void *slab_alloc(size_t size, arena *arena) {
    struct slab_object obj;
    covert_size_to_slab_object((double) size, &obj);
    if (arena->slab_cache[obj.size_index] == NULL) {
        int status = create_slab(arena, obj);
        if (status == -1) {
            return NULL;
        }
    }
    volatile int obj_index = bitmap_alloc(arena->slab_cache[obj.size_index]->bitmap,
                                          arena->slab_cache[obj.size_index]->objs_number);
    if (obj_index == -1) {
        arena->slab_cache[obj.size_index] = arena->slab_cache[obj.size_index]->next_slab;
        return slab_alloc(size, arena);
    }
    return (void *)((unsigned long long)arena->slab_cache[obj.size_index]->start +
           obj_index * arena->slab_cache[obj.size_index]->obj_size);
}

//2. if slab - add it to slab cache, set a bit in slab bitmap
void slab_obj_free(void *obj_ptr, slab *slab) {
    int obj_index = (int) ((obj_ptr - slab->start) / slab->obj_size);
    struct bitmap_levels lvls;
    calc_bitmap_levels(slab->objs_number, &lvls);
    if (slab->bitmap[lvls.last_level_offset] == 0) {
        struct slab_object obj;
        covert_size_to_slab_object((double) slab->obj_size, &obj);
        int slab_index = obj.size_index;
        slab->next_slab = ((arena *) slab->arena)->slab_cache[slab_index];
        ((arena *) slab->arena)->slab_cache[slab_index] = slab;
    }
    set_bit(obj_index, FREE, slab->bitmap, slab->objs_number);

}

void *realloc_in_slab(void *obj_ptr, size_t size, slab *slab) {
    int objs_num = ceil((double) size / (double) slab->obj_size) - 1;
    if (objs_num == 0) {
        void *new_ptr = slab_alloc(size, last_arena);
        memcpy(new_ptr, obj_ptr, size);
        slab_obj_free(obj_ptr, slab);
        return new_ptr;
    }
    int start_obj_index = (int) ((obj_ptr - slab->start) / slab->obj_size) + 1;
    if (!bitmap_multiple_alloc(slab->bitmap, start_obj_index, objs_num, slab->objs_number)) {
        void *new_ptr = mem_alloc(size);
        memcpy(new_ptr, obj_ptr, slab->obj_size);
        slab_obj_free(obj_ptr, slab);
        return new_ptr;
    }
    for(int i=0; i<objs_num;i++){
        set_bit(start_obj_index+i, BUSY, slab->bitmap, slab->objs_number);
    }
    return obj_ptr;
}

void *alloc_inside_arena(size_t size, arena *a) {
    void *(*handler_func)(size_t, arena *) = (size < MAX_SLAB_OBJECT_SIZE) ? slab_alloc : arena_extent_alloc;
    void *ptr = NULL;
    while (ptr == NULL) {
        if (a == NULL) {
            a = create_arena();
        }
        ptr = handler_func(size, a);
        a = a->prev_arena;
    }
    return ptr;
}


void *alloc_outside_arena(size_t size) {
    int page_rounded_size = (int) ceil((double) size / PAGE_SIZE) * PAGE_SIZE;
    void *extent = kernel_alloc(page_rounded_size);
    beyond_arena_extent *extent_header = mem_alloc(sizeof(beyond_arena_extent));
    extent_header->type = EXTENT_BEYOND_ARENA;
    extent_header->size = page_rounded_size;
    radix_tree_insert(ptr_to_radix_key(extent), extent_header, alloc_tree);
    return extent;

}

void beyond_arena_extent_free(void *extent_ptr, beyond_arena_extent *extent) {
    kernel_free(extent_ptr);
    radix_tree_delete_item(ptr_to_radix_key(extent_ptr), alloc_tree);
    mem_free(extent);
}

void *realloc_beyond_arena(void *ptr, beyond_arena_extent *extent, size_t size) {
    int page_rounded_size = (int) ceil((double) size / PAGE_SIZE) * PAGE_SIZE;
    kernel_realloc(ptr, page_rounded_size, extent->size);
    extent->size += size;
    return ptr;
}

/************        API        ***********/

void init_allocator() {
    alloc_tree = create_radix_tree(sizeof(void *) * 8 - (unsigned long long) log2(PAGE_SIZE), RADIX_TREE_LEVELS_NUMBER);
}

void deinit_allocator() {
    delete_radix_tree(alloc_tree);
    arena *cur = last_arena;
    arena *prev;
    while(cur){
        kernel_free(cur->start);
        kernel_free(cur->root_extent);
        delete_hashtable(cur->extent_cache);
        prev = cur->prev_arena;
        kernel_free(cur);
        cur = prev;
    }
}

void *mem_alloc(size_t size) {
    if (size > 0) {
        return (size < ARENA_SIZE) ? alloc_inside_arena(size, last_arena) : alloc_outside_arena(size);
    }
    return NULL;
}

void mem_free(void *ptr) {
    if (!ptr) {
        return;
    }
    slab *object = radix_tree_get_value(ptr_to_radix_key(ptr), alloc_tree);
    if (!object) {
        return;
    }
    switch (object->type) {
        case SLAB:
            slab_obj_free(ptr, object);
            break;
        case EXTENT_FROM_ARENA:
            arena_extent_free((arena_extent *) object);
            break;
        case EXTENT_BEYOND_ARENA:
            beyond_arena_extent_free(ptr, (beyond_arena_extent *) object);
            break;
    }
}


void *mem_realloc(void *ptr, size_t size) {
    if (!size) {
        return NULL;
    }
    if (!ptr) {
        return mem_alloc(size);
    }
    slab *object = radix_tree_get_value(ptr_to_radix_key(ptr), alloc_tree);
    if (!object) {
        return NULL;
    }
    switch (object->type) {
        case SLAB:
            return realloc_in_slab(ptr, size, object);
        case EXTENT_FROM_ARENA:
            return realloc_arena_extent((arena_extent *) object, size);
        case EXTENT_BEYOND_ARENA:
            return realloc_beyond_arena(ptr, (beyond_arena_extent *) object, size);
    }
    return NULL;
}
void print_slab_cache(slab* cache[NUMBER_OF_SLAB_SIZES]){
    slab* s;
    for(int i=0; i<NUMBER_OF_SLAB_SIZES; i++){
        s = cache[i];
        while(s){
            printf("start:%p; object size:%zu; objects number:%d\nbitmap:\n", s->start, s->obj_size, s->objs_number);
            print_bitmap(s->bitmap, s->objs_number);
            s = s->next_slab;
        }

    }
}
void mem_show() {
    arena *cur = last_arena;
    while(cur){
        printf("Arena (start:%p)\nExtent cache\n", cur->start);
        print_hashtable(cur->extent_cache);
        printf("Arena bitmap:\n");
        print_bitmap(cur->bitmap, NUMBER_ARENA_PAGES);
        printf("Slab cache\n");
        print_slab_cache(cur->slab_cache);
        cur = cur->prev_arena;
        printf("---------------------------------------------------------\n");
    }
}
