/*
  alloc.c -- repair block allocator code.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/librepair.h>

errno_t repair_alloc_related_region(reiser4_alloc_t *alloc, blk_t blk, 
    region_func_t func, void *data)
{
    aal_assert("vpf-557", alloc != NULL);
    aal_assert("umka-1685", func != NULL);

    return plugin_call(alloc->entity->plugin->o.alloc_ops, related, 
	alloc->entity, blk, func, data);
}

errno_t repair_alloc_check(reiser4_alloc_t *alloc, uint8_t mode) {
    aal_assert("vpf-867", alloc != NULL);

    return plugin_call(alloc->entity->plugin->o.alloc_ops, check, alloc->entity, 
	mode);
}

