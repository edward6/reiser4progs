/*
    bitmap.h -- bitmap functions. Bitmap is used by block allocator plugin
    and fsck program. See libmisc/bitmap.c for more details.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#ifndef BITMAP_H
#define BITMAP_H

#include <aal/aal.h>

/* 
    Bitmap structure. It contains: pointer to device instance bitmap opened on,
    start on device, total blocks bitmap described, used blocks, pointer to memory
    chunk bit array placed in and bit array size.
*/
struct reiser4_bitmap {
    count_t used_blocks;
    count_t total_blocks;
    
    uint32_t size;
    char *map;
};

typedef struct reiser4_bitmap aux_bitmap_t;

extern void aux_bitmap_use(aux_bitmap_t *bitmap, blk_t blk);
extern void aux_bitmap_unuse(aux_bitmap_t *bitmap, blk_t blk);
extern int aux_bitmap_test(aux_bitmap_t *bitmap, blk_t blk);

extern blk_t aux_bitmap_find(aux_bitmap_t *bitmap, blk_t start);

extern count_t aux_bitmap_calc_used(aux_bitmap_t *bitmap);
extern count_t aux_bitmap_calc_unused(aux_bitmap_t *bitmap);

extern count_t aux_bitmap_used(aux_bitmap_t *bitmap);
extern count_t aux_bitmap_unused(aux_bitmap_t *bitmap);

extern count_t aux_bitmap_calc_used_in_area(aux_bitmap_t *bitmap, 
    blk_t start, blk_t end);

extern count_t aux_bitmap_calc_unused_in_area(aux_bitmap_t *bitmap, 
    blk_t start, blk_t end);

extern aux_bitmap_t *aux_bitmap_create(count_t len);
extern aux_bitmap_t *aux_bitmap_clone(aux_bitmap_t *bitmap);

extern void aux_bitmap_close(aux_bitmap_t *bitmap);
extern char *aux_bitmap_map(aux_bitmap_t *bitmap);

#endif

