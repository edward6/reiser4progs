/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   bbox40.c -- black box, reiser4 safe link items plugin implementation. */

#include <reiser4/plugin.h>
#include "bbox40_repair.h"

reiser4_core_t *bbox40_core = NULL;

static uint32_t bbox40_units(reiser4_place_t *place) {
	return 1;
}

static errno_t bbox40_prep_insert(reiser4_place_t *place,
				  trans_hint_t *hint)
{
	slink_hint_t *link;
	
	aal_assert("vpf-1569", hint != NULL);

	link = (slink_hint_t *)hint->specific;
	
	aal_assert("vpf-1570", link->key.plug != NULL);
	
	hint->overhead = 0;
	
	hint->len = plug_call(link->key.plug->o.key_ops, bodysize);
	hint->len *= sizeof(uint64_t);

	if (link->type == SL_TRUNCATE)
		hint->len += sizeof(uint64_t);
	
	return 0;
}

static errno_t bbox40_insert_units(reiser4_place_t *place,
				   trans_hint_t *hint) 
{
	slink_hint_t *link;
	uint8_t size;

	aal_assert("vpf-1571", place != NULL);
	aal_assert("vpf-1572", hint != NULL);
	
	link = (slink_hint_t *)hint->specific;
	
	aal_assert("vpf-1573", link->key.plug != NULL);

	size = plug_call(link->key.plug->o.key_ops, bodysize) * 
		sizeof(uint64_t);
	
	aal_memcpy(place->body, &link->key.body, size);
	
	if (link->type == SL_TRUNCATE)
		aal_memcpy(place->body + size, &link->size, sizeof(uint64_t));
	
	return 0;
}

static errno_t bbox40_remove_units(reiser4_place_t *place,
				   trans_hint_t *hint)
{
	slink_hint_t *link;
	
	aal_assert("vpf-1574", hint != NULL);

	link = (slink_hint_t *)hint->specific;
	
	aal_assert("vpf-1575", link->key.plug != NULL);
	
	hint->overhead = 0;
	hint->len = plug_call(link->key.plug->o.key_ops, bodysize);
	hint->len *= sizeof(uint64_t);

	if (link->type == SL_TRUNCATE)
		hint->len += sizeof(uint64_t);

	return 0;
}

static errno_t bbox40_fetch_units(reiser4_place_t *place,
				  trans_hint_t *hint)
{
	slink_hint_t *link;
	uint64_t type;
	uint8_t size;
	
	aal_assert("vpf-1576", hint != NULL);
	aal_assert("vpf-1577", place != NULL);
	aal_assert("vpf-1578", place->key.plug != NULL);

	link = (slink_hint_t *)hint->specific;
	
	size = plug_call(place->key.plug->o.key_ops, bodysize) * 
		sizeof(uint64_t);

	link->key.plug = place->key.plug;
	aal_memcpy(&link->key.body, place->body, size);
	
	/* FIXME: this is hardcoded, type should be obtained in another way. */
	type = plug_call(place->key.plug->o.key_ops, get_offset, &place->key);
	
	if (type == SL_TRUNCATE)
		aal_memcpy(&link->size, place->body + size, sizeof(uint64_t));

	return 0;
}


static item_balance_ops_t balance_ops = {
#ifndef ENABLE_STAND_ALONE
	.fuse		  = NULL,
	.update_key	  = NULL,
	.mergeable	  = NULL,
	.maxreal_key	  = NULL,
	.prep_shift	  = NULL,
	.shift_units	  = NULL,
	.collision	  = NULL,
#endif
	.lookup		  = NULL,
	.fetch_key	  = NULL,
	.maxposs_key	  = NULL,
	
	.units            = bbox40_units
};

static item_object_ops_t object_ops = {
#ifndef ENABLE_STAND_ALONE
	.size		  = NULL,
	.bytes		  = NULL,
	.overhead	  = NULL,
	
	.prep_write	  = NULL,
	.write_units	  = NULL,
	.trunc_units	  = NULL,
	
	.prep_insert	  = bbox40_prep_insert,
	.insert_units	  = bbox40_insert_units,
	.remove_units	  = bbox40_remove_units,
	.update_units	  = NULL,
	.layout		  = NULL,
#endif
	.fetch_units	  = bbox40_fetch_units,
	.object_plug	  = NULL,
	.read_units	  = NULL
};

static item_repair_ops_t repair_ops = {
#ifndef ENABLE_STAND_ALONE
	.check_struct	  = bbox40_check_struct,
	.check_layout	  = NULL,

	.prep_merge	  = NULL,
	.merge		  = NULL,

	.pack		  = NULL,
	.unpack		  = NULL
#endif
};

static item_debug_ops_t debug_ops = {
#ifndef ENABLE_STAND_ALONE
	.print		  = bbox40_print
#endif
};

static item_tree_ops_t tree_ops = {
	.down_link	  = NULL,
#ifndef ENABLE_STAND_ALONE
	.update_link	  = NULL
#endif
};

static reiser4_item_ops_t bbox40_ops = {
	.tree		  = &tree_ops,
	.debug		  = &debug_ops,
	.object		  = &object_ops,
	.repair		  = &repair_ops,
	.balance	  = &balance_ops
};

static reiser4_plug_t bbox40_plug = {
	.cl    = class_init,
	.id    = {ITEM_BLACKBOX40_ID, SAFE_LINK_ITEM, ITEM_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "bbox40",
	.desc  = "Safe link item plugin for reiser4, ver. " VERSION,
#endif
	.o = {
		.item_ops = &bbox40_ops
	}
};

static reiser4_plug_t *bbox40_start(reiser4_core_t *c) {
	bbox40_core = c;
	return &bbox40_plug;
}

plug_register(bbox40, bbox40_start, NULL);
