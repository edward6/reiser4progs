/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   alloc.c -- repair block allocator code. */

#include <repair/librepair.h>

errno_t repair_alloc_layout_bad(reiser4_alloc_t *alloc, region_func_t func, 
				void *data) 
{
	aal_assert("vpf-1322", alloc != NULL);
	
	return plug_call(alloc->entity->plug->o.alloc_ops, layout_bad, 
			 alloc->entity, func, data);
}

