/*
  alloc.h -- reiser4 block allocator functions.
    
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef ALLOC_H
#define ALLOC_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>
#include <reiser4/filesystem.h>

extern reiser4_alloc_t *reiser4_alloc_open(reiser4_format_t *format, 
					   count_t len);

#ifndef ENABLE_COMPACT

extern reiser4_alloc_t *reiser4_alloc_create(reiser4_format_t *format, 
					     count_t len);

extern errno_t reiser4_alloc_assign(reiser4_alloc_t *alloc, 
				    aux_bitmap_t *bitmap);

extern errno_t reiser4_alloc_sync(reiser4_alloc_t *alloc);

extern errno_t reiser4_alloc_mark(reiser4_alloc_t *alloc, blk_t blk);

extern errno_t reiser4_alloc_release(reiser4_alloc_t *alloc, blk_t blk);

extern blk_t reiser4_alloc_allocate(reiser4_alloc_t *alloc);

extern errno_t reiser4_alloc_print(reiser4_alloc_t *alloc,
				   aal_stream_t *stream);

extern errno_t reiser4_alloc_forbid(reiser4_alloc_t *alloc, blk_t blk);

extern errno_t reiser4_alloc_permit(reiser4_alloc_t *alloc, blk_t blk);

extern errno_t reiser4_alloc_assign_forb(reiser4_alloc_t *alloc, 
					 aux_bitmap_t *bitmap);

extern errno_t reiser4_alloc_assign_perm(reiser4_alloc_t *alloc, 
					 aux_bitmap_t *bitmap);

#endif

extern errno_t reiser4_alloc_valid(reiser4_alloc_t *alloc);
extern void reiser4_alloc_close(reiser4_alloc_t *alloc);

extern count_t reiser4_alloc_free(reiser4_alloc_t *alloc);
extern count_t reiser4_alloc_used(reiser4_alloc_t *alloc);

extern int reiser4_alloc_test(reiser4_alloc_t *alloc, blk_t blk);

extern errno_t reiser4_alloc_region_layout(reiser4_alloc_t *alloc, blk_t blk, 
	alloc_layout_func_t func, void *data);
#endif

