/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   alloc.h -- repair block allocator functions. */

#ifndef REPAIR_ALLOC_H
#define REPAIR_ALLOC_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

extern errno_t repair_alloc_related_region(reiser4_alloc_t *alloc, blk_t blk,
					   region_func_t func, void *data);

extern errno_t repair_alloc_check(reiser4_alloc_t *alloc, uint8_t mode);

#endif
