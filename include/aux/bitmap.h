/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   bitmap.h -- bitmap functions. Bitmap is used by block allocator plugin and
   fsck program. See libmisc/bitmap.c for more details. */

#ifndef AUX_BITMAP_H
#define AUX_BITMAP_H

#ifndef ENABLE_STAND_ALONE
#include <aal/aal.h>

#define AUX_BITMAP_MAGIC	"R4BtMp"

/* Bitmap structure. It contains: pointer to device instance bitmap opened on,
   start on device, total blocks bitmap described, used blocks, pointer to
   memory chunk bit array placed in and bit array size. */
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

extern void aux_bitmap_mark_region(aux_bitmap_t *bitmap, 
				   uint64_t start,
				   uint64_t count);

extern void aux_bitmap_clear_region(aux_bitmap_t *bitmap, 
				    uint64_t start,
				    uint64_t count);

extern bool_t aux_bitmap_test_region_marked(aux_bitmap_t *bitmap,
					    uint64_t start,	
					    uint64_t count);

extern bool_t aux_bitmap_test_region(aux_bitmap_t *bitmap,
				     uint64_t start,	
				     uint64_t count,
				     int marked);

extern void aux_bitmap_invert(aux_bitmap_t *bitmap);

extern uint64_t aux_bitmap_find_region(aux_bitmap_t *bitmap,
				       uint64_t *start,
				       uint64_t count,
				       int marked);

extern uint64_t aux_bitmap_find_marked(aux_bitmap_t *bitmap,
				       uint64_t start);

extern uint64_t aux_bitmap_find_cleared(aux_bitmap_t *bitmap,
					uint64_t start);

extern uint64_t aux_bitmap_calc_marked(aux_bitmap_t *bitmap);
extern uint64_t aux_bitmap_calc_cleared(aux_bitmap_t *bitmap);

extern uint64_t aux_bitmap_marked(aux_bitmap_t *bitmap);
extern uint64_t aux_bitmap_cleared(aux_bitmap_t *bitmap);

extern aux_bitmap_t *aux_bitmap_create(uint64_t len);
extern aux_bitmap_t *aux_bitmap_clone(aux_bitmap_t *bitmap);

extern void aux_bitmap_resize(aux_bitmap_t *bitmap, uint64_t len);
extern void aux_bitmap_close(aux_bitmap_t *bitmap);
extern char *aux_bitmap_map(aux_bitmap_t *bitmap);

extern aux_bitmap_t *aux_bitmap_unpack(aal_stream_t *stream);
extern errno_t aux_bitmap_pack(aux_bitmap_t *bitmap, 
			       aal_stream_t *stream);

#endif

#endif

