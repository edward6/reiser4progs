/*
  nodeptr40_repair.c -- repair default node pointer item plugin methods.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_ALONE

#include "nodeptr40.h"

int32_t nodeptr40_layout_check(item_entity_t *item, region_func_t func, 
    void *data) 
{
    nodeptr40_t *nodeptr;
    blk_t blk;
    int res;
	
    aal_assert("vpf-721", item != NULL);

    nodeptr = nodeptr40_body(item);

    blk = np40_get_ptr(nodeptr);    
    res = func(item, blk, 1, data);
    
    if (res > 0) 
	return item->len;
    else if (res < 0)
	return res;

    return 0;
}

errno_t nodeptr40_check(item_entity_t *item, uint8_t mode) {
    aal_assert("vpf-751", item != NULL);
    return item->len != sizeof(nodeptr40_t);
}

#endif
