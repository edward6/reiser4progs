/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   bitmap.h -- bitmap functions. Bitmap is used by block allocator plugin and
   fsck program. See libmisc/bitmap.c for more details. */

#ifndef AUX_BITMAP_H
#define AUX_BITMAP_H

#ifndef ENABLE_MINIMAL
#include <aal/libaal.h>

#define AUX_BITMAP_MAGIC	"R4BtMp"

/* Bitmap structure. It contains: pointer to device instance bitmap opened on,
   start on device, total blocks bitmap described, used blocks, pointer to
   memory chunk bit array placed in and bit array size. */
struct reiser4_bitmap {
	uint64_t marked;
	uint64_t total;
	uint32_t size;
    
	char *map;
};

typedef struct reiser4_bitmap reiser4_bitmap_t;

extern void reiser4_bitmap_mark(reiser4_bitmap_t *bitmap, uint64_t bit);
extern void reiser4_bitmap_clear(reiser4_bitmap_t *bitmap, uint64_t bit);
extern int reiser4_bitmap_test(reiser4_bitmap_t *bitmap, uint64_t bit);

extern void reiser4_bitmap_mark_region(reiser4_bitmap_t *bitmap, 
				       uint64_t start,  uint64_t count);

extern void reiser4_bitmap_clear_region(reiser4_bitmap_t *bitmap, 
					uint64_t start, uint64_t count);

extern bool_t reiser4_bitmap_test_region_marked(reiser4_bitmap_t *bitmap,
						uint64_t start, 
						uint64_t count);

extern bool_t reiser4_bitmap_test_region(reiser4_bitmap_t *bitmap,
					 uint64_t start,
					 uint64_t count,
					 int marked);

extern void reiser4_bitmap_invert(reiser4_bitmap_t *bitmap);

extern uint64_t reiser4_bitmap_find_region(reiser4_bitmap_t *bitmap,
					   uint64_t *start,
					   uint64_t count,
					   int marked);

extern uint64_t reiser4_bitmap_find_marked(reiser4_bitmap_t *bitmap, 
					   uint64_t start);

extern uint64_t reiser4_bitmap_find_cleared(reiser4_bitmap_t *bitmap, 
					    uint64_t start);

extern uint64_t reiser4_bitmap_calc_marked(reiser4_bitmap_t *bitmap);
extern uint64_t reiser4_bitmap_calc_cleared(reiser4_bitmap_t *bitmap);

extern uint64_t reiser4_bitmap_marked(reiser4_bitmap_t *bitmap);
extern uint64_t reiser4_bitmap_cleared(reiser4_bitmap_t *bitmap);

extern reiser4_bitmap_t *reiser4_bitmap_create(uint64_t len);
extern reiser4_bitmap_t *reiser4_bitmap_clone(reiser4_bitmap_t *bitmap);

extern void reiser4_bitmap_resize(reiser4_bitmap_t *bitmap, uint64_t len);
extern void reiser4_bitmap_close(reiser4_bitmap_t *bitmap);

extern reiser4_bitmap_t *reiser4_bitmap_unpack(aal_stream_t *stream);
extern errno_t reiser4_bitmap_pack(reiser4_bitmap_t *bitmap, 
			       aal_stream_t *stream);

#endif

#endif

