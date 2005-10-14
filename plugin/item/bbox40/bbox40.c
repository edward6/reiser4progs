/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   bbox40.c -- black box, reiser4 safe link items plugin implementation. */

#include <reiser4/plugin.h>
#include "bbox40_repair.h"

reiser4_core_t *bbox40_core = NULL;

static uint32_t bbox40_units(reiser4_place_t *place) {
	return 1;
}

#ifndef ENABLE_MINIMAL

static errno_t bbox40_prep_insert(reiser4_place_t *place,
				  trans_hint_t *hint)
{
	slink_hint_t *link;
	
	aal_assert("vpf-1569", hint != NULL);

	link = (slink_hint_t *)hint->specific;
	
	aal_assert("vpf-1570", link->key.plug != NULL);
	
	hint->overhead = 0;

	hint->count = 1;
	hint->len = plug_call(link->key.plug->pl.key, bodysize);
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

	size = plug_call(link->key.plug->pl.key, bodysize) * 
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
	hint->len = place->len;

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
	
	size = plug_call(place->key.plug->pl.key, bodysize) * 
		sizeof(uint64_t);

	link->key.plug = place->key.plug;
	aal_memcpy(&link->key.body, place->body, size);
	
	/* FIXME: this is hardcoded, type should be obtained in another way. */
	type = plug_call(place->key.plug->pl.key, get_offset, &place->key);
	
	if (type == SL_TRUNCATE)
		aal_memcpy(&link->size, place->body + size, sizeof(uint64_t));

	return 0;
}
#endif


static item_balance_ops_t balance_ops = {
#ifndef ENABLE_MINIMAL
	.merge		  = NULL,
	.update_key	  = NULL,
	.mergeable	  = NULL,
	.maxreal_key	  = NULL,
	.prep_shift	  = NULL,
	.shift_units	  = NULL,
	.collision	  = NULL,
	.overhead	  = NULL,
#endif
	.init		  = NULL,
	.lookup		  = NULL,
	.fetch_key	  = NULL,
	.maxposs_key	  = NULL,
	.units            = bbox40_units
};

static item_object_ops_t object_ops = {
#ifndef ENABLE_MINIMAL
	.size		  = NULL,
	.bytes		  = NULL,
	
	.prep_write	  = NULL,
	.write_units	  = NULL,
	.trunc_units	  = NULL,
	
	.prep_insert	  = bbox40_prep_insert,
	.insert_units	  = bbox40_insert_units,
	.remove_units	  = bbox40_remove_units,
	.update_units	  = NULL,
	.fetch_units	  = bbox40_fetch_units,
	.layout		  = NULL,
#else
	.fetch_units	  = NULL,
#endif
	.read_units	  = NULL
};

#ifndef ENABLE_MINIMAL
static item_repair_ops_t repair_ops = {
	.check_struct	  = bbox40_check_struct,
	.check_layout	  = NULL,

	.prep_insert_raw  = bbox40_prep_insert_raw,
	.insert_raw	  = bbox40_insert_raw,

	.pack		  = NULL,
	.unpack		  = NULL
};

static item_debug_ops_t debug_ops = {
	.print		  = bbox40_print
};
#endif

static reiser4_item_plug_t bbox40 = {
	.object		  = &object_ops,
	.balance	  = &balance_ops,
#ifndef ENABLE_MINIMAL
	.repair		  = &repair_ops,
	.debug		  = &debug_ops
#endif
};

static reiser4_plug_t bbox40_plug = {
	.cl    = class_init,
	.id    = {ITEM_BLACKBOX40_ID, BLACK_BOX_ITEM, ITEM_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
	.label = "bbox40",
	.desc  = "Safe link item plugin.",
#endif
	.pl = {
		.item = &bbox40
	}
};

static reiser4_plug_t *bbox40_start(reiser4_core_t *c) {
	bbox40_core = c;
	return &bbox40_plug;
}

plug_register(bbox40, bbox40_start, NULL);
