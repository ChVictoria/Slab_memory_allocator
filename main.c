#include <stdio.h>
#include <sysinfoapi.h>
#include <stdalign.h>
#include "config.h"
#include "radix_tree.h"
#include "bitmap.h"
#include "hashtable.h"
#include "allocator.h"

void radix_test() {
    radix_tree *tree = create_radix_tree(sizeof(void *) * 8, 8);
    radix_tree_insert((unsigned long) 0x11223344, (void *) 0xAABBCCDD, tree);
    void *value = radix_tree_get_value((unsigned long) 0x11223344, tree);
    printf("value = %x\n", value);
    delete_radix_tree(tree);
}

void bitmap_test() {
    slab my_slab;
    create_bitmap(63, my_slab.bitmap, SLAB);
    int allocated_obj_id = bitmap_alloc(my_slab.bitmap,63);
    printf("object with size_index %d allocated", allocated_obj_id);
    set_bit(allocated_obj_id, FREE, my_slab.bitmap, 63);

}

void hashtable_test() {
    hashtable table = create_hashtable();
    arena_extent extent;
    extent.pages_number = NUMBER_ARENA_PAGES;
    extent.next_extent_in_basket = NULL;
    extent.next_same_size_extent = NULL;
    hashtable_insert(table, &extent);
    arena_extent *poped = hashtable_pop(table, extent.pages_number);
    delete_hashtable(table);
}
int main() {
    init_allocator();

    void * slab_object1 = mem_alloc(12);
    mem_show();
    void * slab_object2 = mem_alloc(5000);
    mem_show();
    void * slab_object3 = mem_alloc(5100);
    mem_show();
    void * arena_extent1 = mem_alloc(200500);
    mem_show();
    void * slab_object4 = mem_alloc(99);
    mem_show();
    printf("REALLOC SLAB\n");
    mem_realloc(slab_object4, 150);
    mem_show();
    void * arena_extent2 = mem_alloc(200500);
    mem_show();
    printf("REALLOC ARENA EXTENT\n");
    mem_realloc(arena_extent2, 250458);
    mem_show();
    mem_free(slab_object1);
    mem_show();
    mem_free(slab_object2);
    mem_show();
    mem_free(slab_object3);
    mem_show();
    mem_free(arena_extent1);
    mem_show();
    mem_free(slab_object4);
    mem_show();
    mem_free(arena_extent2);
    mem_show();

    void *extent_outside_arena = mem_alloc(17777012);
    mem_free(extent_outside_arena);

    deinit_allocator();
    return 0;
}
