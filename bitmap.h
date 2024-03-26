#ifndef BITMAP_H
#define BITMAP_H
#include "allocator.h"

typedef enum {
    FREE = 1,
    BUSY = 0
} bit_state;
#define MAX_LEVELS 8
struct bitmap_levels {
    int levels_num;
    int last_level_offset;
    int low_levels[MAX_LEVELS];
};

void calc_bitmap_levels(double objs_in_bitmap, struct bitmap_levels *lvls);
int create_bitmap(int objs_in_bitmap, int bitmap[], alloc_type type);
int bitmap_alloc(int bitmap[], double objs_in_bitmap);
int bitmap_multiple_alloc(int bitmap[], int start_bit_num, int objs_num, double objs_in_bitmap);
void set_bit(int bit_num, bit_state value, int bitmap[], double objs_in_bitmap);
int find_best_fit(int bit_num, int bitmap[], double objs_in_bitmap);
void print_bitmap(int bitmap[], double objs_in_bitmap);
#endif