/*
  alloc.h -- reiser4 block allocator functions.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef REISER4_ALLOC_H
#define REISER4_ALLOC_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/types.h>

extern reiser4_alloc_t *reiser4_alloc_open(reiser4_fs_t *fs, 
					   count_t count);

#ifndef ENABLE_ALONE

extern reiser4_alloc_t *reiser4_alloc_create(reiser4_fs_t *fs, 
					     count_t count);

extern errno_t reiser4_alloc_assign(reiser4_alloc_t *alloc, 
				    aux_bitmap_t *bitmap);

extern errno_t reiser4_alloc_sync(reiser4_alloc_t *alloc);

extern errno_t reiser4_alloc_occupy_region(reiser4_alloc_t *alloc,
					   blk_t start, 
					   count_t count);

extern errno_t reiser4_alloc_release_region(reiser4_alloc_t *alloc,
					    blk_t start, 
					    count_t count);

extern count_t reiser4_alloc_allocate_region(reiser4_alloc_t *alloc,
					     blk_t *start,
					     count_t count);

extern errno_t reiser4_alloc_print(reiser4_alloc_t *alloc,
				   aal_stream_t *stream);

extern errno_t reiser4_alloc_forbid(reiser4_alloc_t *alloc,
				    blk_t start, 
				    count_t count);

extern errno_t reiser4_alloc_permit(reiser4_alloc_t *alloc,
				    blk_t start, 
				    count_t count);

extern errno_t reiser4_alloc_assign_forb(reiser4_alloc_t *alloc, 
					 aux_bitmap_t *bitmap);

extern errno_t reiser4_alloc_assign_perm(reiser4_alloc_t *alloc, 
					 aux_bitmap_t *bitmap);

#endif

extern void reiser4_alloc_close(reiser4_alloc_t *alloc);
extern errno_t reiser4_alloc_valid(reiser4_alloc_t *alloc);

extern count_t reiser4_alloc_used(reiser4_alloc_t *alloc);
extern count_t reiser4_alloc_unused(reiser4_alloc_t *alloc);

extern bool_t reiser4_alloc_used_region(reiser4_alloc_t *alloc,
					blk_t start, 
					count_t count);

extern bool_t reiser4_alloc_unused_region(reiser4_alloc_t *alloc,
					  blk_t start, 
					  count_t count);

extern errno_t reiser4_alloc_related_region(reiser4_alloc_t *alloc,
					    blk_t blk, block_func_t func,
					    void *data);

extern errno_t reiser4_alloc_layout(reiser4_alloc_t *alloc,
				    block_func_t func, void *data);
#endif
