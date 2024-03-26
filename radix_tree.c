#include "radix_tree.h"
#include "kernel.h"
#include <memoryapi.h>
#include <stdio.h>

enum if_find_fails {
    ADD_LEVEL,
    EXIT
};

static void *add_level_item(radix_tree *tree) {
    void *(*item)[tree->items_per_level] = kernel_alloc(sizeof(void *) * tree->items_per_level);
    memset(item, 0, sizeof(&item));
    return item;
}
/**
 *
 * @param key_size should be provided in bits
 *
 */
void *create_radix_tree(unsigned long long key_size, int levels_number) {
    unsigned long bits_per_level = key_size / levels_number;
    unsigned long items_per_level = 1 << bits_per_level;
    radix_tree *tree = kernel_alloc(sizeof(radix_tree));
    tree->levels_number = levels_number;
    tree->items_per_level = items_per_level;
    tree->bits_per_level = bits_per_level;
    tree->root = add_level_item(tree);
    return tree;
}

static void failed_find_exit(void) {
    fprintf(stderr, "Can't radix_tree_find the item in Radix tree, not enough levels in tree\n");
    exit(EXIT_FAILURE);
}

void *radix_tree_find(unsigned long long key, radix_tree *tree, enum if_find_fails item_check) {
    void *(*item)[tree->items_per_level] = tree->root;
    unsigned long long end_mask = (tree->items_per_level - 1);
    unsigned long long level_mask;
    unsigned long index;
    for (unsigned long i = tree->levels_number - 1; i > 0; i--) {
        level_mask = end_mask << (i * tree->bits_per_level);
        index = (key & level_mask) >> (i * tree->bits_per_level);
        if ((*item)[index] == NULL) {
            switch (item_check) {
                case ADD_LEVEL:
                    (*item)[index] = add_level_item(tree);
                    break;
                case EXIT:
                    return NULL;
            }
        }
        item = (*item)[index];
    }
    index = key & end_mask;
    return *item + index;
}


void radix_tree_insert(unsigned long long key, void *value, radix_tree *tree) {
    void **item_ptr = radix_tree_find(key, tree, ADD_LEVEL);
    *item_ptr = value;
}

void *radix_tree_get_value(unsigned long long key, radix_tree *tree) {
    void **item_ptr = radix_tree_find(key, tree, EXIT);
    if (!item_ptr){
        return NULL;
    }
    return *item_ptr;
}

void *radix_tree_delete_item(unsigned long long key, radix_tree *tree){
    void **item_ptr = radix_tree_find(key, tree, EXIT);
    *item_ptr = NULL;
}

void delete_level(void *(*item)[], unsigned long item_size, unsigned long level, unsigned long levels_number) {
    if (level != levels_number) {
        for (int j = 0; j < item_size; j++) {
            if ((*item)[j] != NULL) {
                delete_level((*item)[j], item_size, level + 1, levels_number);
            }
        }
    }
    kernel_free(item);
}

void delete_radix_tree(radix_tree *tree) {
    void *(*root_level)[tree->items_per_level] = tree->root;
    delete_level(root_level, tree->items_per_level, 1, tree->levels_number);
    kernel_free(tree);
}
