/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   ctail40.c -- reiser4 compressed tail item plugin repair functions. */

#ifndef ENABLE_MINIMAL
#include "ctail40.h"
#include "ctail40_repair.h"

#include <plugin/item/body40/body40.h>
#include <plugin/item/tail40/tail40.h>
#include <plugin/item/tail40/tail40_repair.h>

reiser4_core_t *ctail40_core;

static void ctail40_init(reiser4_place_t *place) {
	aal_assert("vpf-1763", place != NULL);
	place->off = sizeof(ctail40_t);
}

/* Return 1 if two ctail items are mergeable while balancing. 0 otherwise. */
static int ctail40_mergeable(reiser4_place_t *place1, reiser4_place_t *place2) {
	uint64_t off1;
	uint64_t off2;
	uint8_t shift;

	aal_assert("vpf-1765", place1 != NULL);
	aal_assert("vpf-1807", place2 != NULL);
	aal_assert("vpf-1808", place1->plug == place2->plug);
	aal_assert("vpf-1809", ct40_get_shift(place1->body) == 
			       ct40_get_shift(place2->body));
	
	shift = ct40_get_shift(place1->body);
	off1 = plug_call(place1->key.plug->pl.key, get_offset, &place1->key);
	off2 = plug_call(place2->key.plug->pl.key, get_offset, &place2->key);
	
	/* If they are of different clusters, not mergeable. */
	if (off1 + (1 << shift) >= off2)
		return 0;
	
	return body40_mergeable(place1, place2);
}

static errno_t ctail40_prep_shift(reiser4_place_t *src_place,
				  reiser4_place_t *dst_place,
				  shift_hint_t *hint)
{
	if (dst_place)
		dst_place->off = sizeof(ctail40_t);

	return tail40_prep_shift(src_place, dst_place, hint);
}

static errno_t ctail40_prep_write(reiser4_place_t *place, trans_hint_t *hint) {
	place->off = sizeof(ctail40_t);
	return tail40_prep_write(place, hint);
}

static item_balance_ops_t balance_ops = {
	.merge		  = NULL,
	.update_key	  = NULL,
	.mergeable	  = ctail40_mergeable,
	.maxreal_key	  = tail40_maxreal_key,
	.prep_shift	  = ctail40_prep_shift,
	.shift_units	  = tail40_shift_units,
	.collision	  = NULL,
	.units            = tail40_units,
	.lookup		  = tail40_lookup,
	.fetch_key	  = tail40_fetch_key,
	.maxposs_key	  = tail40_maxposs_key
};

static item_object_ops_t object_ops = {
	.size		  = tail40_size,
	.bytes		  = tail40_size,
	.overhead	  = NULL,

	.prep_write	  = ctail40_prep_write,
	.write_units	  = tail40_write_units,
	.trunc_units	  = tail40_trunc_units,

	.prep_insert	  = NULL,
	.insert_units	  = NULL,
	.remove_units	  = NULL,
	.update_units	  = NULL,
	.layout		  = NULL,
	.fetch_units	  = NULL,
	.read_units	  = tail40_read_units
};

static item_repair_ops_t repair_ops = {
	.check_struct	  = tail40_check_struct,
	.check_layout	  = NULL,

	.prep_insert_raw  = ctail40_prep_insert_raw,
	.insert_raw	  = tail40_insert_raw,

	.pack		  = tail40_pack,
	.unpack		  = tail40_unpack
};

static item_debug_ops_t debug_ops = {
	.print		  = NULL
};

static item_tree_ops_t tree_ops = {
	.init             = ctail40_init,
	.down_link	  = NULL,
	.update_link	  = NULL
};

static reiser4_item_plug_t ctail40 = {
	.tree		  = &tree_ops,
	.object		  = &object_ops,
	.balance	  = &balance_ops,
	.repair		  = &repair_ops,
	.debug		  = &debug_ops,
};

static reiser4_plug_t ctail40_plug = {
	.cl    = class_init,
	.id    = {ITEM_CTAIL40_ID, CTAIL_ITEM, ITEM_PLUG_TYPE},
	.label = "ctail40",
	.desc  = "Compressed file body item plugin.",
	.pl = {
		.item = &ctail40
	}
};

static reiser4_plug_t *ctail40_start(reiser4_core_t *c) {
	ctail40_core = c;
	return &ctail40_plug;
}

plug_register(ctail40, ctail40_start, NULL);
#endif
