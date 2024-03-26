#include "bitmap.h"
#include <math.h>
#include <stdio.h>

int create_bitmap(int objs_in_bitmap, int *bitmap, alloc_type type) {
    int elements_in_level = ceil((double) objs_in_bitmap / (sizeof(int) * 8));
    int rest = objs_in_bitmap % ((int)sizeof(int) * 8);
    int i = 0;
    int max_index = elements_in_level;
    while (elements_in_level > 1) {
        for (; i < max_index; i++) {
            switch (type) {
                case SLAB:
                    bitmap[i] = ((i == max_index - 1) && (rest != 0)) ? ((1 << rest) - 1) : -1;
                    break;
                case EXTENT_FROM_ARENA:
                    bitmap[i] = 0;
                    break;
            }

        }
        elements_in_level = ceil((double) elements_in_level / (sizeof(int) * 8));
        rest = elements_in_level % ((int)sizeof(int) * 8);
        max_index+=elements_in_level;
    }
    switch (type) {
        case SLAB:
            bitmap[i] = (rest != 0) ? (1 << rest) - 1 : -1;
            break;
        case EXTENT_FROM_ARENA:
            bitmap[i] = 0;
            break;
    }
    return i + 1;
}

void calc_bitmap_levels(double objs_in_bitmap, struct bitmap_levels *lvls) {

    lvls->levels_num = ceil((double)1 / (log2((sizeof(int))) + 3) * log2(objs_in_bitmap));
    lvls->last_level_offset = 0;
    for (int i = 0; i < lvls->levels_num - 1; i++) {
        if (i==0){
            lvls->low_levels[i] = ceil((double) objs_in_bitmap / (sizeof(int) * 8));
        } else {
            lvls->low_levels[i] = ceil((double)lvls->low_levels[i - 1] / (sizeof(int) * 8));
        }
        lvls->last_level_offset += lvls->low_levels[i];
    }

}


int bitmap_alloc(int *bitmap, double objs_in_bitmap) {
    struct bitmap_levels lvls;
    calc_bitmap_levels(objs_in_bitmap, &lvls);
    if (bitmap[lvls.last_level_offset] == 0) {
        return -1;
    }

    int offset = lvls.last_level_offset;
    for (int level = lvls.levels_num - 2; level >= 0; level--) {
        int mask = 1;
        for (int i = 0; i < (sizeof(int) * 8); i++) {
            if (bitmap[offset] & mask) {
                offset = offset - lvls.low_levels[level] + i;
                break;
            }
            mask = mask << 1;
        }
    }
    int index = -1;
    int mask = 1;
    for (int i = 0; i < (sizeof(int) * 8); i++) {
        if (bitmap[offset] & mask) {
            index = offset * 32 + i;
            set_bit(index, BUSY, bitmap, objs_in_bitmap);
            break;
        }
        mask = mask << 1;
    }

    return index;
}
typedef struct multiple_walk_params{
    double obj_in_level_item;
    int offset;
    double start_bit_num;
    double objs_num;
    int free_obj_num;
    int*low_levels;
    int level;
    int *bitmap;
}params;

int multiple_bits_walk(params *p) {
    p->obj_in_level_item = p->obj_in_level_item * (sizeof(int) * 8);
    p->offset -= p->low_levels[p->level];
    int start_index = (int)ceil((double)(p->start_bit_num + 1) / p->obj_in_level_item) - 1;
    int mask = 1 << (int) ceil((double)(p->start_bit_num + 1) / p->obj_in_level_item * (sizeof(int) * 8)) % (sizeof(int) * 8);
    int objs_in_lower_level = (int)ceil((double)p->objs_num / p->obj_in_level_item);
    for (int i = start_index; i < start_index + objs_in_lower_level; i++) {
        if (!(p->bitmap[p->offset + start_index] & mask)) {
            return 0;
        }
        if (p->level == 0) {
            p->free_obj_num += 1;
            if (p->free_obj_num == p->objs_num) {
                return 1;
            }
        } else {
            p->level = p->level - 1;
            multiple_bits_walk(p);
        }
    }
}

int bitmap_multiple_alloc(int *bitmap, int start_bit_num, int objs_num, double objs_in_bitmap) {
    int level = ceil((double) 1 / log2((double)sizeof(int) * 8) * log2(objs_num));

    struct bitmap_levels lvls;
    calc_bitmap_levels(objs_in_bitmap, &lvls);
    double obj_in_level_item =  pow(sizeof(int) * 8, level);
    int start_index = (int)ceil((double)(start_bit_num + 1) / obj_in_level_item) - 1;

    int mask = 1 << (int) ceil((double)(start_bit_num + 1) / obj_in_level_item*(sizeof(int) * 8) ) % (sizeof(int) * 8);
    if(!level){
        for(int i=1; i<objs_num;i++){
            if(!(bitmap[start_index+1]&mask)){
                return 0;
            }
            mask = mask << 1;
        }
        return 1;
    }
    int offset;
    if (level < lvls.levels_num) {
        offset = 0;
        for (int i = 0; i < level - 1; i++) {
            offset += lvls.low_levels[i];
        }

    } else {
        offset = lvls.last_level_offset;
    }

    int free_obj_num = 0;
    if (bitmap[offset + start_index] & mask){
        params p;
        p.obj_in_level_item = obj_in_level_item;
        p.level = level-1;
        p.objs_num = objs_num;
        p.free_obj_num=free_obj_num;
        p.start_bit_num = start_bit_num;
        p.offset=offset;
        p.low_levels = lvls.low_levels;
        p.bitmap = bitmap;
        return multiple_bits_walk(&p);
    }
    return 0;
}

void set_bit(int bit_num, bit_state value, int *bitmap, double objs_in_bitmap) {
    double elements_in_level = ceil(objs_in_bitmap / (sizeof(int) * 8));
    int offset = 0;
    int flag;
    int prev_bitmap_item;
    while (elements_in_level > 1) {
        //bitnum starts from 0
        int index = (int)ceil((double) (bit_num + 1) / (sizeof(int) * 8)) - 1;
        int mask = 1 << (bit_num % (sizeof(int) * 8));
        if (value == BUSY) {
            mask = mask ^ (int) -1;
        }
        prev_bitmap_item = bitmap[index + offset];
        bitmap[index + offset] = (value == FREE) ? (bitmap[index + offset] | mask) : (bitmap[index + offset] & mask);
        if (value == BUSY) {
            flag = (bitmap[index + offset] == 0) ? 1 : 0;
        } else {
            flag = (prev_bitmap_item == 0) ? 1 : 0;
        }
        if (!flag) {
            return;
        }
        bit_num = index;
        offset += (int) elements_in_level;
        elements_in_level = ceil(elements_in_level / (sizeof(int) * 8));
    }
    int mask = 1 << (bit_num % (sizeof(int) * 8));
    if (value == BUSY) {
        mask = mask ^ (int) -1;
    }
    bitmap[offset] = (value == FREE) ? (bitmap[offset] | mask) : (bitmap[offset] & mask);
}

int bitmap_walk(int level, int index, int bit_num, struct bitmap_levels *lvls, int *bitmap) {
    int mask = 1 << bit_num;
    int offset = 0;
    for (int i = 0; i < level; i++) {
        offset += lvls->low_levels[i];
    }
    for (int i = bit_num; i < (sizeof(int) * 8); i++) {

        if (bitmap[offset + index] & mask) {
            if (level) {
                level -= 1;
                index = i + index * (int)sizeof(int) * 8;
                bit_num = 0;
                return bitmap_walk(level, index, bit_num, lvls, bitmap);
            } else {
                return (index * (int)sizeof(int) * 8 + i);
            }
        }
        mask = mask << 1;
    }
    if (level == lvls->levels_num-1) {
        return -1;
    }
    level += 1;
    bit_num = index + 1;
    index = (int)ceil((double)(index + 1) / (sizeof(int) * 8)) - 1;
    return bitmap_walk(level, index, bit_num, lvls, bitmap);
}

int find_best_fit(int bit_num, int *bitmap, double objs_in_bitmap) {
    int index = (int)ceil((double)(bit_num + 1) / ((int)sizeof(int) * 8)) - 1;
    if (bitmap[bit_num]) {
        return bit_num;
    }
    bit_num = (bit_num % (int)(sizeof(int) * 8))+1;
    struct bitmap_levels lvls;
    calc_bitmap_levels(objs_in_bitmap, &lvls);
    int best_fit = bitmap_walk(0, index, bit_num, &lvls, bitmap);

    return best_fit;
}

void print_bitmap(int *bitmap, double objs_in_bitmap){
    struct bitmap_levels lvls;
    calc_bitmap_levels(objs_in_bitmap, &lvls);
    int offset = lvls.last_level_offset;
    printf("%8x ", bitmap[offset]);
    printf("\n");
    for (int l=lvls.levels_num-2; l>=0; l--){
        offset -= lvls.low_levels[l];
        for (int i=0; i<lvls.low_levels[l];i++){
            printf("%08x ", bitmap[offset+i]);
        }
        printf("\n");
    }
}

