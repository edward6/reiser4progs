/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   nodeptr40.c -- reiser4 default node pointer item plugin. */

#include "nodeptr40.h"
#include "nodeptr40_repair.h"

static reiser4_core_t *core = NULL;

/* Returns the number of units in nodeptr. As nodeptr40 has not units and thus
   cannot be splitted by balancing, it has one unit. */
static uint32_t nodeptr40_units(place_t *place) {
	return 1;
}

/* Fetches nodeptr into passed @hint */
static int64_t nodeptr40_fetch_units(place_t *place,
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

static blk_t nodeptr40_down_link(place_t *place) {
	aal_assert("umka-2665", place != NULL);
	return np40_get_ptr(nodeptr40_body(place));
}

#ifndef ENABLE_STAND_ALONE
/* Update nodeptr block number by passed @blk. */
static errno_t nodeptr40_update_link(place_t *place,
				     blk_t blk)
{
	aal_assert("umka-2667", place != NULL);
	np40_set_ptr(nodeptr40_body(place), blk);
	return 0;
}

/* Layout implementation for nodeptr40. It calls @geion_func for each block
   nodeptr points to. */
static errno_t nodeptr40_layout(place_t *place,
				region_func_t region_func,
				void *data)
{
	blk_t blk;
	
	aal_assert("umka-1749", place != NULL);
	aal_assert("umka-2354", place->body != NULL);
	aal_assert("umka-1750", region_func != NULL);

	blk = np40_get_ptr(nodeptr40_body(place));
	return region_func(place, blk, 1, data);
}

/* Estimates how many bytes is needed for creating new nodeptr */
static errno_t nodeptr40_prep_insert(place_t *place,
				     trans_hint_t *hint)
{
	aal_assert("vpf-068", hint != NULL);
	aal_assert("umka-2436", place != NULL);

	hint->ohd = 0;
	hint->len = sizeof(nodeptr40_t);
	
	return 0;
}

/* Writes of the specified nodeptr into passed @place */
static int64_t nodeptr40_insert_units(place_t *place,
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

/* Prints passed nodeptr into @stream */
static errno_t nodeptr40_print(place_t *place,
			       aal_stream_t *stream,
			       uint16_t options) 
{
	nodeptr40_t *nodeptr;
	
	aal_assert("umka-544", place != NULL);
	aal_assert("umka-545", stream != NULL);
    
	nodeptr = nodeptr40_body(place);

	aal_stream_format(stream, "NODEPTR PLUGIN=%s, LEN=%u, "
			  "KEY=[%s], UNITS=1\n[%llu]\n",
			  place->plug->label, place->len, 
			  core->key_ops.print(&place->key, PO_DEFAULT), 
			  np40_get_ptr(nodeptr));
	
	return 0;
}
#endif

static item_balance_ops_t balance_ops = {
#ifndef ENABLE_STAND_ALONE
	.fuse             = NULL,
	.prep_shift       = NULL,
	.shift_units      = NULL,
	.maxreal_key      = NULL,
	.update_key	  = NULL,
	.mergeable        = NULL,
#endif
	.lookup           = NULL,
	.fetch_key	  = NULL,
	.maxposs_key	  = NULL,
	.units            = nodeptr40_units
};

static item_object_ops_t object_ops = {
#ifndef ENABLE_STAND_ALONE
	.layout           = nodeptr40_layout,
	.prep_insert      = nodeptr40_prep_insert,
	.insert_units     = nodeptr40_insert_units,

	.prep_write       = NULL,
	.write_units      = NULL,
	.update_units     = NULL,
	.remove_units     = NULL,
	.trunc_units      = NULL,
	.size		  = NULL,
	.bytes		  = NULL,
#endif
	.object_plug	  = NULL,
	.read_units       = NULL,
	.fetch_units      = nodeptr40_fetch_units
};

static item_repair_ops_t repair_ops = {
#ifndef ENABLE_STAND_ALONE	    
	.check_struct	  = nodeptr40_check_struct,
	.check_layout	  = nodeptr40_check_layout,
	
	.prep_merge	  = NULL,
	.merge_units      = NULL,
#endif
};

static item_debug_ops_t debug_ops = {
#ifndef ENABLE_STAND_ALONE	    
	.print		  = nodeptr40_print,
#endif
};

static item_tree_ops_t tree_ops = {
	.down_link        = nodeptr40_down_link,
#ifndef ENABLE_STAND_ALONE
	.update_link      = nodeptr40_update_link
#endif
};

static reiser4_item_ops_t nodeptr40_ops = {
	.tree             = &tree_ops,
	.debug            = &debug_ops,
	.object           = &object_ops,
	.repair           = &repair_ops,
	.balance          = &balance_ops
};

static reiser4_plug_t nodeptr40_plug = {
	.cl    = CLASS_INIT,
	.id    = {ITEM_NODEPTR40_ID, NODEPTR_ITEM, ITEM_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "nodeptr40",
	.desc  = "Node pointer item for reiser4, ver. " VERSION,
#endif
	.o = {
		.item_ops = &nodeptr40_ops
	}
};

static reiser4_plug_t *nodeptr40_start(reiser4_core_t *c) {
	core = c;
	return &nodeptr40_plug;
}

plug_register(nodeptr40, nodeptr40_start, NULL);
