#include "allocator.h"

typedef arena_extent **hashtable;
hashtable create_hashtable();
void hashtable_insert(hashtable table, arena_extent *extent);
arena_extent *hashtable_pop(hashtable table, unsigned long pages_number);
int hashtable_find_by_key(hashtable table, unsigned long pages_number);
int hashtable_find_by_value(hashtable table, arena_extent *extent);
void delete_extent_from_hashtable(hashtable table, arena_extent *extent);
void delete_hashtable(hashtable table);
void print_hashtable(hashtable table);

