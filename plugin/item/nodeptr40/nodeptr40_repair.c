/*
  nodeptr40_repair.c -- repair default node pointer item plugin methods.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "nodeptr40.h"
#include <repair/repair_plugin.h>

errno_t nodeptr40_layout_check(item_entity_t *item, region_func_t region_func, 
    void *data, uint8_t mode) 
{
    nodeptr40_t *nodeptr;
    blk_t blk;
    errno_t res;
    
    aal_assert("vpf-721", item != NULL);

    nodeptr = nodeptr40_body(item);

    blk = np40_get_ptr(nodeptr);

    res = region_func(item, blk, 1, data);
    
    if (res > 0) {
	if (mode == REPAIR_REBUILD) {
	    aal_exception_error("Node (%llu), item (%u): a pointer to the "
		"region [%llu..%llu] is removed.", item->context.blk, blk, blk);
	    item->len = 0;
	    
	    return REPAIR_FIXED;
	}
	
	return REPAIR_FATAL;
    } else if (res < 0)
	return res;

    return REPAIR_OK;
}

errno_t nodeptr40_check(item_entity_t *item, uint8_t mode) {
    aal_assert("vpf-751", item != NULL);
    return item->len != sizeof(nodeptr40_t) ? REPAIR_FATAL : REPAIR_OK;
}

#endif
