/*
  bitmap.h -- bitmap functions. Bitmap is used by block allocator plugin and
  fsck program. See libmisc/bitmap.c for more details.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
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
	uint64_t marked;
	uint64_t total;
	uint32_t size;
    
	char *map;
};

typedef struct aux_bitmap aux_bitmap_t;

extern void aux_bitmap_mark(aux_bitmap_t *bitmap,
			    uint64_t bit);

extern void aux_bitmap_clear(aux_bitmap_t *bitmap,
			     uint64_t bit);

extern int aux_bitmap_test(aux_bitmap_t *bitmap,
			   uint64_t bit);

extern void aux_bitmap_mark_all(aux_bitmap_t *bitmap);
extern void aux_bitmap_clear_all(aux_bitmap_t *bitmap);

extern void aux_bitmap_mark_region(aux_bitmap_t *bitmap, 
				   uint64_t start,
				   uint64_t end);

extern void aux_bitmap_clear_region(aux_bitmap_t *bitmap, 
				    uint64_t start,
				    uint64_t end);

extern int aux_bitmap_test_region_marked(aux_bitmap_t *bitmap,
					 uint64_t start,	
					 uint64_t end);

extern int aux_bitmap_test_region_cleared(aux_bitmap_t *bitmap,
					  uint64_t start,	
					  uint64_t end);

extern uint64_t aux_bitmap_find_region_marked(aux_bitmap_t *bitmap,
					      uint64_t *start,
					      uint64_t count);

extern uint64_t aux_bitmap_find_region_cleared(aux_bitmap_t *bitmap,
					       uint64_t *start,
					       uint64_t count);

extern uint64_t aux_bitmap_find_marked(aux_bitmap_t *bitmap,
				       uint64_t start);

extern uint64_t aux_bitmap_find_cleared(aux_bitmap_t *bitmap,
					uint64_t start);

extern uint64_t aux_bitmap_calc_marked(aux_bitmap_t *bitmap);
extern uint64_t aux_bitmap_calc_cleared(aux_bitmap_t *bitmap);

extern uint64_t aux_bitmap_marked(aux_bitmap_t *bitmap);
extern uint64_t aux_bitmap_cleared(aux_bitmap_t *bitmap);

extern uint64_t aux_bitmap_calc_region_marked(aux_bitmap_t *bitmap, 
					      uint64_t start,
					      uint64_t end);

extern uint64_t aux_bitmap_calc_region_cleared(aux_bitmap_t *bitmap, 
					       uint64_t start,
					       uint64_t end);

extern aux_bitmap_t *aux_bitmap_create(uint64_t len);
extern aux_bitmap_t *aux_bitmap_clone(aux_bitmap_t *bitmap);

extern void aux_bitmap_close(aux_bitmap_t *bitmap);
extern char *aux_bitmap_map(aux_bitmap_t *bitmap);

#endif

