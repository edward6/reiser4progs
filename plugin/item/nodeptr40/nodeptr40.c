/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   nodeptr40.c -- reiser4 default node pointer item plugin. */

#include "nodeptr40.h"
#include "nodeptr40_repair.h"

reiser4_core_t *nodeptr40_core = NULL;

/* Returns the number of units in nodeptr. As nodeptr40 has not units and thus
   cannot be splitted by balancing, it has one unit. */
static uint32_t nodeptr40_units(reiser4_place_t *place) {
	return 1;
}

/* Fetches nodeptr into passed @hint */
static int64_t nodeptr40_fetch_units(reiser4_place_t *place,
				     trans_hint_t *hint)
{
	nodeptr40_t *nodeptr;
	ptr_hint_t *ptr_hint;
		
	aal_assert("umka-1420", hint != NULL);
	aal_assert("umka-1419", place != NULL);

	nodeptr = nodeptr40_body(place);
	ptr_hint = (ptr_hint_t *)hint->specific;
	
	ptr_hint->width = 1;
	ptr_hint->start = np40_get_ptr(nodeptr);
	
	return 1;
}

#ifndef ENABLE_MINIMAL
/* Update nodeptr block number by passed @blk. */
static errno_t nodeptr40_update_units(reiser4_place_t *place,
				      trans_hint_t *hint)
{
	ptr_hint_t *ptr;
	
	aal_assert("umka-2667", place != NULL);
	aal_assert("vpf-1863", hint != NULL);
	
	ptr = (ptr_hint_t *)hint->specific;
	np40_set_ptr(nodeptr40_body(place), ptr->start);
	place_mkdirty(place);
	
	return 1;
}

/* Layout implementation for nodeptr40. It calls @geion_func for each block
   nodeptr points to. */
static errno_t nodeptr40_layout(reiser4_place_t *place,
				region_func_t region_func,
				void *data)
{
	blk_t blk;
	
	aal_assert("umka-1749", place != NULL);
	aal_assert("umka-2354", place->body != NULL);
	aal_assert("umka-1750", region_func != NULL);

	blk = np40_get_ptr(nodeptr40_body(place));
	return region_func(blk, 1, data);
}

/* Estimates how many bytes is needed for creating new nodeptr */
static errno_t nodeptr40_prep_insert(reiser4_place_t *place,
				     trans_hint_t *hint)
{
	aal_assert("vpf-068", hint != NULL);
	aal_assert("umka-2436", place != NULL);

	hint->overhead = 0;
	hint->len = sizeof(nodeptr40_t);
	
	return 0;
}

/* Writes of the specified nodeptr into passed @place */
static int64_t nodeptr40_insert_units(reiser4_place_t *place,
				      trans_hint_t *hint)
{
	nodeptr40_t *nodeptr;
	ptr_hint_t *ptr_hint;
		
	aal_assert("umka-1424", hint != NULL);
	aal_assert("umka-1423", place != NULL);

	nodeptr = nodeptr40_body(place);
	
	ptr_hint = (ptr_hint_t *)hint->specific;
	np40_set_ptr(nodeptr, ptr_hint->start);

	place_mkdirty(place);
	return 1;
}

/* Removes nodeptr unit. Asit is always one, we only set up passed @hint here by
   sizeof nodeptr40_t struct. */
static errno_t nodeptr40_remove_units(reiser4_place_t *place,
				      trans_hint_t *hint)
{
	aal_assert("umka-3029", hint != NULL);
	aal_assert("umka-3028", place != NULL);

	hint->overhead = 0;
	hint->len = sizeof(nodeptr40_t);
	
	return 0;
}
#endif

static item_balance_ops_t balance_ops = {
#ifndef ENABLE_MINIMAL
	.merge		  = NULL,
	.prep_shift	  = NULL,
	.shift_units	  = NULL,
	.maxreal_key	  = NULL,
	.update_key	  = NULL,
	.mergeable	  = NULL,
	.collision	  = NULL,
	.overhead	  = NULL,
#endif
	.init		  = NULL,
	.lookup		  = NULL,
	.fetch_key	  = NULL,
	.maxposs_key	  = NULL,

	.units		  = nodeptr40_units
};

static item_object_ops_t object_ops = {
#ifndef ENABLE_MINIMAL
	.layout		  = nodeptr40_layout,
	.prep_insert	  = nodeptr40_prep_insert,
	.insert_units	  = nodeptr40_insert_units,
	.remove_units	  = nodeptr40_remove_units,

	.prep_write	  = NULL,
	.write_units	  = NULL,
	.update_units	  = nodeptr40_update_units,
	.trunc_units	  = NULL,
	.size		  = NULL,
	.bytes		  = NULL,
#endif
	.read_units	  = NULL,
	.fetch_units	  = nodeptr40_fetch_units
};

#ifndef ENABLE_MINIMAL	    
static item_repair_ops_t repair_ops = {
	.check_struct	  = nodeptr40_check_struct,
	.check_layout	  = nodeptr40_check_layout,
	
	.prep_insert_raw = NULL,
	.insert_raw	  = NULL,

	.pack		  = NULL,
	.unpack		  = NULL
};

static item_debug_ops_t debug_ops = {
	.print		  = nodeptr40_print,
};
#endif

reiser4_item_plug_t nodeptr40_plug = {
	.p = {
		.id    = {ITEM_NODEPTR40_ID, PTR_ITEM, ITEM_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "nodeptr40",
		.desc  = "Node pointer item plugin.",
#endif
	},

	.object		  = &object_ops,
	.balance	  = &balance_ops,
#ifndef ENABLE_MINIMAL
	.repair		  = &repair_ops,
	.debug		  = &debug_ops
#endif
};
