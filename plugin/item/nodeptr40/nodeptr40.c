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

/* Reads nodeptr into passed buff */
static int32_t nodeptr40_read(place_t *place, void *buff,
			      uint32_t pos, uint32_t count)
{
	nodeptr40_t *nodeptr;
	ptr_hint_t *ptr_hint;
		
	aal_assert("umka-1419", place != NULL);
	aal_assert("umka-1420", buff != NULL);

	ptr_hint = (ptr_hint_t *)buff;
	nodeptr = nodeptr40_body(place);
	
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
	nodeptr40_t *nodeptr;
	
	aal_assert("umka-1749", place != NULL);
	aal_assert("umka-2354", place->body != NULL);
	aal_assert("umka-1750", region_func != NULL);

	nodeptr = nodeptr40_body(place);
	return region_func(place, np40_get_ptr(nodeptr), 1, data);
}

/* Estimates how many bytes is needed for creating new nodeptr */
static errno_t nodeptr40_estimate_insert(place_t *place, uint32_t pos,
					 insert_hint_t *hint)
{
	aal_assert("vpf-068", hint != NULL);

	hint->len = sizeof(nodeptr40_t);
	return 0;
}

/* Writes of the specified nodeptr into passed @place */
static errno_t nodeptr40_insert(place_t *place, uint32_t pos,
				insert_hint_t *hint)
{
	nodeptr40_t *nodeptr;
	ptr_hint_t *ptr_hint;
		
	aal_assert("umka-1423", place != NULL);
	aal_assert("umka-1424", hint != NULL);

	nodeptr = nodeptr40_body(place);
	
	ptr_hint = (ptr_hint_t *)hint->type_specific;
	np40_set_ptr(nodeptr, ptr_hint->start);

	place_mkdirty(place);
	return 0;
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
	.units		  = nodeptr40_units,
	.read             = nodeptr40_read,
	.branch           = nodeptr40_branch,
	
#ifndef ENABLE_STAND_ALONE	    
	.insert           = nodeptr40_insert,
	.print		  = nodeptr40_print,
	.check_struct	  = nodeptr40_check_struct,
	.layout           = nodeptr40_layout,
	.check_layout	  = nodeptr40_check_layout,
	.estimate_insert  = nodeptr40_estimate_insert,

	.estimate_copy	  = NULL,
	.estimate_shift   = NULL,

	.init		  = NULL,
	.copy             = NULL,
	.rep		  = NULL,
	.expand		  = NULL,
	.shrink           = NULL,
	.remove		  = NULL,
	.shift            = NULL,
	.size		  = NULL,
	.bytes		  = NULL,
	.overhead         = NULL,
	.set_key	  = NULL,
	.maxreal_key      = NULL,
#endif
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
