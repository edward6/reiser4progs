/*
  journal40.c -- reiser4 default journal plugin.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "journal40.h"

errno_t journal40_check(object_entity_t *entity) {
    aal_assert("vpf-447", entity != NULL, return -1);

    while (journal40_tx_layout((journal40_t *)entity, callback_layout, 
	callback_update) == 0)
		trans_nr++;

	return trans_nr;
}
