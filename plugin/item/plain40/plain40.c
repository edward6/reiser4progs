/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   plain40.c -- reiser4 plain file body (aka formatting) item plugin. */

#include "plain40.h"
#include "plain40_repair.h"

#include <plugin/item/body40/body40.h>
#include <plugin/item/tail40/tail40.h>
#include <plugin/item/tail40/tail40_repair.h>

reiser4_core_t *plain40_core;

#ifndef ENABLE_MINIMAL
/* Return 1 if two tail items are mergeable. Otherwise 0 will be returned. This
   method is used in balancing to determine if two border items may be
   merged. */
static int plain40_mergeable(reiser4_place_t *place1, reiser4_place_t *place2) {
	aal_assert("umka-2201", place1 != NULL);
	aal_assert("umka-2202", place2 != NULL);
	return body40_mergeable(place1, place2);
}

/* Estimates how many bytes in tree is needed to write @hint->count bytes of
   data. This function considers, that tail item is not expandable one. That is,
   tail will not be splitted at insert point, but will be rewritten instead. */
errno_t plain40_prep_write(reiser4_place_t *place, trans_hint_t *hint) {
	place->off = 0;
	return tail40_prep_write(place, hint);
}

/* Estimates how many bytes may be shifted from @stc_place to @dst_place. */
errno_t plain40_prep_shift(reiser4_place_t *src_place,
			   reiser4_place_t *dst_place,
			   shift_hint_t *hint)
{
	if (dst_place)
		dst_place->off = 0;
	
	return tail40_prep_shift(src_place, dst_place, hint);
}
#endif

static item_balance_ops_t balance_ops = {
#ifndef ENABLE_MINIMAL
	.merge		  = NULL,
	.update_key	  = NULL,
	.mergeable	  = plain40_mergeable,
	.maxreal_key	  = tail40_maxreal_key,
	.prep_shift	  = plain40_prep_shift,
	.shift_units	  = tail40_shift_units,
	.collision	  = NULL,
#endif
	.units            = tail40_units,
	.lookup		  = tail40_lookup,
	.fetch_key	  = tail40_fetch_key,
	.maxposs_key	  = tail40_maxposs_key
};

static item_object_ops_t object_ops = {
#ifndef ENABLE_MINIMAL
	.size		  = tail40_size,
	.bytes		  = tail40_size,
	.overhead	  = NULL,
	
	.prep_write	  = plain40_prep_write,
	.write_units	  = tail40_write_units,
	.trunc_units	  = tail40_trunc_units,
	
	.prep_insert	  = NULL,
	.insert_units	  = NULL,
	.remove_units	  = NULL,
	.update_units	  = NULL,
	.layout		  = NULL,
#endif
	.fetch_units	  = NULL,
	.read_units	  = tail40_read_units
};

#ifndef ENABLE_MINIMAL
static item_repair_ops_t repair_ops = {
	.check_struct	  = tail40_check_struct,
	.check_layout	  = NULL,

	.prep_insert_raw  = plain40_prep_insert_raw,
	.insert_raw	  = tail40_insert_raw,

	.pack		  = tail40_pack,
	.unpack		  = tail40_unpack
};

static item_debug_ops_t debug_ops = {
	.print		  = NULL
};
#endif

static item_tree_ops_t tree_ops = {
	.init		  = NULL,
	.down_link	  = NULL,
#ifndef ENABLE_MINIMAL
	.update_link	  = NULL
#endif
};

static reiser4_item_ops_t plain40_ops = {
	.tree		  = &tree_ops,
	.object		  = &object_ops,
	.balance	  = &balance_ops,
#ifndef ENABLE_MINIMAL
	.repair		  = &repair_ops,
	.debug		  = &debug_ops,
#endif
};

static reiser4_plug_t plain40_plug = {
	.cl    = class_init,
	.id    = {ITEM_PLAIN40_ID, TAIL_ITEM, ITEM_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
	.label = "plain40",
	.desc  = "Plain file body item plugin.",
#endif
	.o = {
		.item_ops = &plain40_ops
	}
};

static reiser4_plug_t *plain40_start(reiser4_core_t *c) {
	plain40_core = c;
	return &plain40_plug;
}

plug_register(plain40, plain40_start, NULL);
