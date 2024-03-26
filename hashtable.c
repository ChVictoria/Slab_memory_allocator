#include "kernel.h"
#include "hashtable.h"
#include <stdio.h>

unsigned long hash_function(unsigned long pages_number) {
    return pages_number % HASHTABLE_CAPACITY;
}

hashtable create_hashtable() {
    hashtable table = kernel_alloc(sizeof(hashtable[HASHTABLE_CAPACITY]));
    memset(table, 0, sizeof(hashtable[HASHTABLE_CAPACITY]));
    return table;
}

void hashtable_insert(hashtable table, arena_extent *extent) {
    unsigned long index = hash_function(extent->pages_number);
    if (table[index] == NULL) {
        table[index] = extent;
    } else {
        if (table[index]->pages_number == extent->pages_number) {
            extent->next_same_size_extent = table[index]->next_same_size_extent;
            table[index]->next_same_size_extent = extent;
        } else {
            extent->next_extent_in_basket = table[index]->next_extent_in_basket;
            table[index]->next_extent_in_basket = extent;
        }
    }
}


arena_extent *hashtable_pop(hashtable table, unsigned long pages_number) {
    unsigned long index = hash_function(pages_number);
    arena_extent *extent = table[index];
    if (extent== NULL){
        return NULL;
    }
    arena_extent *prev_extent = NULL;
    while (extent->pages_number != pages_number) {
        if (extent->next_extent_in_basket) {
            prev_extent = extent;
            extent = extent->next_extent_in_basket;
        } else {
            return NULL;
        }
    }

    arena_extent *next_extent;
    if (extent->next_same_size_extent) {
        extent->next_same_size_extent->next_extent_in_basket = extent->next_extent_in_basket;
        next_extent = extent->next_same_size_extent;
        extent->next_same_size_extent = NULL;
    } else {
        next_extent = extent->next_extent_in_basket;
        extent->next_extent_in_basket = NULL;
    }

    if (prev_extent) {
        prev_extent->next_extent_in_basket = next_extent;
    } else {
        table[index] = next_extent;
    }

    return extent;
}

int hashtable_find_by_value(hashtable table, arena_extent *extent) {
    unsigned long index = hash_function(extent->pages_number);
    arena_extent *extent_in_table = table[index];
    while ((extent_in_table->pages_number != extent->pages_number) && extent_in_table != NULL) {
        extent_in_table = extent_in_table->next_extent_in_basket;
    }
    while (extent_in_table != NULL) {
        if (extent_in_table == extent) {
            return 1;
        }
        extent_in_table = extent_in_table->next_same_size_extent;
    }
    return 0;
}

int hashtable_find_by_key(hashtable table, unsigned long pages_number){
    unsigned long index = hash_function(pages_number);
    arena_extent *extent = table[index];
    if(extent==NULL){
        return 0;
    }
    while (extent->pages_number != pages_number) {
        if (extent->next_extent_in_basket) {
            extent = extent->next_extent_in_basket;
        } else {
            return 0;
        }
    }
    return 1;
}

void delete_extent_from_hashtable(hashtable table, arena_extent *extent){
    unsigned long index = hash_function(extent->pages_number);
    arena_extent *extent_in_table = table[index];
    arena_extent *prev_same_size = NULL;
    arena_extent *prev_in_basket = NULL;

    while ((extent_in_table->pages_number != extent->pages_number) && extent_in_table != NULL) {
        prev_in_basket = extent_in_table;
        extent_in_table = extent_in_table->next_extent_in_basket;
    }
    while (extent_in_table != NULL) {
        if (extent_in_table == extent) {
            if (prev_same_size){
                prev_same_size->next_same_size_extent = extent_in_table->next_same_size_extent;
            } else {
                arena_extent **prev_item;
                if (prev_in_basket) {
                    prev_item = &prev_in_basket->next_extent_in_basket;
                } else {
                    prev_item = &table[index];
                }
                if (extent_in_table->next_same_size_extent) {
                    *prev_item = extent_in_table->next_same_size_extent;
                    extent_in_table->next_same_size_extent->next_extent_in_basket = extent_in_table->next_extent_in_basket;
                } else {
                    *prev_item = extent_in_table->next_extent_in_basket;
                }
            }
            break;
        }

        prev_same_size = extent_in_table;
        extent_in_table = extent_in_table->next_same_size_extent;
    }
}

void delete_hashtable(hashtable table) {
    kernel_free(table);
}

void print_hashtable(hashtable table) {
    for(int i=0; i<HASHTABLE_CAPACITY;i++){
        arena_extent *in_basket = table[i];
        arena_extent *same_size = table[i];
        while(in_basket) {
            while (same_size) {
                printf("pages_number:%d; start:0x%p\t", same_size->pages_number, same_size->start);
                same_size = same_size->next_same_size_extent;
            }
            printf("\n");
            in_basket = in_basket->next_extent_in_basket;
        }
    }
}