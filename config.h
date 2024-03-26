#ifndef CONFIG_H
#define CONFIG_H
/******* System dependent configuration *****/
#define PAGE_SIZE 4096
#define NUMBER_ARENA_PAGES 4096
#define ARENA_SIZE (PAGE_SIZE * NUMBER_ARENA_PAGES)
#define MAX_ALIGN 16
#define BIT_DEPTH 64

/******* Object sizes configuration *******/
#define N 8
#define M 4
#define K 7

#define NUMBER_OF_SLAB_SIZES (N+M*K)
#define MAX_SLAB_OBJECT_SIZE (N*MAX_ALIGN + M * MAX_ALIGN * 2 * (int) ((1 << K) - 1))
#define MAX_SMALL_SLAB_SIZE ((double)1/(double)8 * PAGE_SIZE)

/******** Radix tree configuration *******/
#define RADIX_TREE_LEVELS_NUMBER 8

/*********** Hashtable configuration *****/
#define HASHTABLE_CAPACITY 128

#endif