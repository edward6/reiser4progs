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
   start on device, total blocks bitmap described, used blocks, pointer to
   memory chunk bit array placed in and bit array size.
*/
struct aux_bitmap {
	uint64_t used;
	uint64_t total;
	uint32_t size;
    
	char *map;
};

typedef struct aux_bitmap aux_bitmap_t;

extern void aux_bitmap_mark(aux_bitmap_t *bitmap, uint64_t bit);
extern void aux_bitmap_clear(aux_bitmap_t *bitmap, uint64_t bit);
extern int aux_bitmap_test(aux_bitmap_t *bitmap, uint64_t bit);

extern uint64_t aux_bitmap_find(aux_bitmap_t *bitmap, uint64_t start);

extern uint64_t aux_bitmap_calc_used(aux_bitmap_t *bitmap);
extern uint64_t aux_bitmap_calc_free(aux_bitmap_t *bitmap);

extern uint64_t aux_bitmap_used(aux_bitmap_t *bitmap);
extern uint64_t aux_bitmap_free(aux_bitmap_t *bitmap);

extern uint64_t aux_bitmap_calc_used_in_area(aux_bitmap_t *bitmap, 
					     uint64_t start, uint64_t end);

extern uint64_t aux_bitmap_calc_free_in_area(aux_bitmap_t *bitmap, 
					     uint64_t start, uint64_t end);

extern aux_bitmap_t *aux_bitmap_create(uint64_t len);
extern aux_bitmap_t *aux_bitmap_clone(aux_bitmap_t *bitmap);

extern void aux_bitmap_close(aux_bitmap_t *bitmap);
extern char *aux_bitmap_map(aux_bitmap_t *bitmap);

#endif

