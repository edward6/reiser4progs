/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   nodeptr40.c -- reiser4 default node pointer item plugin. */

#include "nodeptr40.h"

static reiser4_core_t *core = NULL;

/* Returns the number of units in nodeptr. As nodeptr40 has not units and thus
   cannot be splitted by balancing, it has one unit. */
static uint32_t nodeptr40_units(place_t *place) {
	return 1;
}

/* Fetches nodeptr into passed @hint */
static int64_t nodeptr40_fetch(place_t *place, trans_hint_t *hint) {
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

static int nodeptr40_branch(void) {
	return 1;
}

#ifndef ENABLE_STAND_ALONE
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
static errno_t nodeptr40_estimate_insert(place_t *place,
					 trans_hint_t *hint)
{
	aal_assert("vpf-068", hint != NULL);
	aal_assert("umka-2436", place != NULL);

	hint->len = sizeof(nodeptr40_t);
	return 0;
}

/* Writes of the specified nodeptr into passed @place */
static int64_t nodeptr40_insert(place_t *place,
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

	aal_stream_format(stream, "NODEPTR PLUGIN=%s LEN=%u, "
			  "KEY=[%s] UNITS=1\n[%llu]\n",
			  place->plug->label, place->len, 
			  core->key_ops.print(&place->key, PO_DEF), 
			  np40_get_ptr(nodeptr));
	
	return 0;
}

extern errno_t nodeptr40_check_struct(place_t *place,
				      uint8_t mode);

extern errno_t nodeptr40_check_layout(place_t *place,
				      region_func_t func, 
				      void *data, uint8_t mode);

#endif

static reiser4_item_ops_t nodeptr40_ops = {
	.fetch            = nodeptr40_fetch,
	.units		  = nodeptr40_units,
	.branch           = nodeptr40_branch,
	
#ifndef ENABLE_STAND_ALONE	    
	.print		  = nodeptr40_print,
	.layout           = nodeptr40_layout,
	.insert           = nodeptr40_insert,
	.update           = nodeptr40_insert,
	.check_struct	  = nodeptr40_check_struct,
	.check_layout	  = nodeptr40_check_layout,
	.estimate_insert  = nodeptr40_estimate_insert,

	.estimate_merge	  = NULL,
	.estimate_shift   = NULL,
	.estimate_write   = NULL,

	.init		  = NULL,
	.write            = NULL,
	.remove		  = NULL,
	.merge            = NULL,
	.shift            = NULL,
	.truncate         = NULL,
	.size		  = NULL,
	.bytes		  = NULL,
	.overhead         = NULL,
	.set_key	  = NULL,
	.maxreal_key      = NULL,
#endif
	.read             = NULL,
	.lookup		  = NULL,
	.plugid		  = NULL,
	.mergeable        = NULL,
	.maxposs_key	  = NULL,
	.get_key	  = NULL
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
