/*
  extent40_repare.c -- repair dafault extent plugin methods.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "extent40.h"
#include <repair/repair_plugin.h>

extern uint32_t extent40_units(item_entity_t *item);

errno_t extent40_layout_check(item_entity_t *item, region_func_t func, 
    void *data, uint8_t mode) 
{
    uint32_t i, units;
    extent40_t *extent;
    errno_t res, result = REPAIR_OK;
	
    aal_assert("vpf-724", item != NULL);
    aal_assert("vpf-725", func != NULL);

    extent = extent40_body(item);
    units = extent40_units(item);
			
    for (i = 0; i < units; i++, extent++) {
	uint64_t start, width;

	start = et40_get_start(extent);
	width = et40_get_width(extent);

	if (start) {
	    res = func(item, start, width, data);

	    if (res > 0) {
		if (mode == REPAIR_CHECK)
		    result = REPAIR_FIXABLE;
		else {
		    /* Zero the problem region. */
		    aal_exception_error("Node (%llu), item (%u): pointed "
			"region [%llu..%llu] is zeroed.", item->context.blk, 
			item->pos.item, start, start + width - 1);
		    et40_set_start(extent, 0);
		    result = REPAIR_FIXED;
		}
	    } else if (res < 0) 
		return res;
	}
    }

    return result;
}

errno_t extent40_check(item_entity_t *item, uint8_t mode) {
    aal_assert("vpf-750", item != NULL);
    return item->len % sizeof(extent40_t) ? REPAIR_FATAL : REPAIR_OK;
}

#endif
