/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   alloc.h -- reiser4 block allocator functions. */

#ifndef REISER4_ALLOC_H
#define REISER4_ALLOC_H

#ifndef ENABLE_MINIMAL
#include <reiser4/types.h>

extern reiser4_alloc_t *reiser4_alloc_open(reiser4_fs_t *fs, 
					   count_t blocks);

extern reiser4_alloc_t *reiser4_alloc_create(reiser4_fs_t *fs, 
					     count_t blocks);

extern errno_t reiser4_alloc_extract(reiser4_alloc_t *alloc,
				     reiser4_bitmap_t *bitmap);

extern errno_t reiser4_alloc_assign(reiser4_alloc_t *alloc, 
				    reiser4_bitmap_t *bitmap);

extern errno_t reiser4_alloc_sync(reiser4_alloc_t *alloc);

extern errno_t reiser4_alloc_occupy(reiser4_alloc_t *alloc,
				    blk_t start, count_t count);

extern errno_t reiser4_alloc_release(reiser4_alloc_t *alloc,
				     blk_t start, count_t count);

extern count_t reiser4_alloc_allocate(reiser4_alloc_t *alloc,
				      blk_t *start, count_t count);

extern void reiser4_alloc_close(reiser4_alloc_t *alloc);
extern errno_t reiser4_alloc_valid(reiser4_alloc_t *alloc);

extern bool_t reiser4_alloc_isdirty(reiser4_alloc_t *alloc);
extern void reiser4_alloc_mkdirty(reiser4_alloc_t *alloc);
extern void reiser4_alloc_mkclean(reiser4_alloc_t *alloc);

extern count_t reiser4_alloc_used(reiser4_alloc_t *alloc);
extern count_t reiser4_alloc_free(reiser4_alloc_t *alloc);

extern bool_t reiser4_alloc_occupied(reiser4_alloc_t *alloc,
				     blk_t start, count_t count);

extern bool_t reiser4_alloc_available(reiser4_alloc_t *alloc,
				      blk_t start, count_t count);

extern errno_t reiser4_alloc_layout(reiser4_alloc_t *alloc,
				    region_func_t region_func,
				    void *data);

extern errno_t reiser4_alloc_region(reiser4_alloc_t *alloc, blk_t blk,
				    region_func_t func, void *data);

#endif

#endif
