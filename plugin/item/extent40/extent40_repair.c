/*
  extent40_repair.c -- repair default extent plugin methods.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "extent40.h"

/*extern uint32_t extent40_units(item_entity_t *item);*/

/* Checking extent item and zeroing out the problem region */
/*int32_t extent40_layout_check(item_entity_t *item, region_func_t func, 
    void *data) 
{
    int res;
    uint32_t i, units;
    extent40_t *extent;
	
    aal_assert("vpf-724", item != NULL, return -1);
    aal_assert("vpf-725", func != NULL, return -1);

    extent = extent40_body(item);
    units = extent40_units(item);
			
    for (i = 0; i < units; i++, extent++) {
	uint64_t blk;
	uint64_t start;

	start = et40_get_start(extent);

	if (start) {
	    res = func(item, start, start + et40_get_start(extent), data);

	    if (res > 0) {
		et40_set_start(extent, 0);
	    } else if (res < 0) 
		return res;
	}
    }

    return 0;
}*/

