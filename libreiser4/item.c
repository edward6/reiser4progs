/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   item.c -- common reiser4 item functions. */

#include <reiser4/libreiser4.h>

/* Returns count of units in item. If count method is not implemented, it
   returns 1. */
uint32_t reiser4_item_units(reiser4_place_t *place) {
	aal_assert("umka-1030", place != NULL);
	aal_assert("umka-1448", place->plug != NULL);

	if (place->plug->pl.item->balance->units) {
		return plug_call(place->plug->pl.item->balance,
				 units, place);
	}
	
	return 1;
}

#ifndef ENABLE_MINIMAL
bool_t reiser4_item_statdata(reiser4_place_t *place) {
	aal_assert("umka-1831", place != NULL);
	aal_assert("umka-2382", place->plug != NULL);

	return place->plug->id.group == STAT_ITEM;
}

/* Returns item type from its plugin */
rid_t reiser4_item_type(reiser4_place_t *place) {
	aal_assert("vpf-424", place != NULL);
	aal_assert("vpf-425", place->plug != NULL);
	
	if (place->plug->id.type != ITEM_PLUG_TYPE)
		return LAST_ITEM;
		
	return (place->plug->id.group < LAST_ITEM ?
		place->plug->id.group : LAST_ITEM);
}

int reiser4_item_mergeable(reiser4_place_t *place1, reiser4_place_t *place2) {
	aal_assert("vpf-1428", place1 != NULL);
	aal_assert("vpf-1428", place2 != NULL);

	/* Check if plugins are equal */
	if (!plug_equal(place1->plug, place2->plug))
		return 0;

	/* Check if mergeable is implemented and calling it if it is. */
	return place1->plug->pl.item->balance->mergeable &&
		place1->plug->pl.item->balance->mergeable(place1, place2);
}
#endif

/* Returns 1 if @place points to an nodeptr item. */
bool_t reiser4_item_branch(reiser4_plug_t *plug) {
	aal_assert("umka-1829", plug != NULL);
	return (plug->id.group == PTR_ITEM);
}

/* Returns maximal possible key may exist in item at @place. */
errno_t reiser4_item_maxposs_key(reiser4_place_t *place,
				 reiser4_key_t *key)
{
	aal_assert("umka-1270", key != NULL);
	aal_assert("umka-1269", place != NULL);
	aal_assert("umka-1456", place->plug != NULL);
	
	aal_memcpy(key, &place->key, sizeof(*key));

	if (!place->plug->pl.item->balance->maxposs_key)
		return 0;
	
	return plug_call(place->plug->pl.item->balance,
			 maxposs_key, place, key);
}

#ifndef ENABLE_MINIMAL
/* Returns real maximal item key */
errno_t reiser4_item_maxreal_key(reiser4_place_t *place,
				 reiser4_key_t *key)
{
	aal_assert("vpf-352", key != NULL);
	aal_assert("vpf-351", place != NULL);
	aal_assert("umka-1457", place->plug != NULL);

	aal_memcpy(key, &place->key, sizeof(*key));

	if (!place->plug->pl.item->balance->maxreal_key)
		return 0;

	return plug_call(place->plug->pl.item->balance,
			 maxreal_key, place, key);
}

errno_t reiser4_item_update_key(reiser4_place_t *place,
				reiser4_key_t *key)
{
	aal_assert("vpf-1205", key != NULL);
	aal_assert("vpf-1205", place != NULL);
	
	aal_memcpy(&place->key, key, sizeof(*key));
	
	return reiser4_node_update_key(place->node,
				       &place->pos,
				       &place->key);
}
#endif

errno_t reiser4_item_get_key(reiser4_place_t *place,
			     reiser4_key_t *key)
{
	aal_assert("vpf-1290", place != NULL);
	aal_assert("vpf-1291", key != NULL);

	/* Getting key from item or node and updating it in passed @place. */
	key->plug = place->key.plug;

	if (place->plug->pl.item->balance->fetch_key &&
	    (place->pos.unit && place->pos.unit != MAX_UINT32))
	{
		plug_call(place->plug->pl.item->balance,
			  fetch_key, place, key);
	} else {
		plug_call(place->node->plug->pl.node,
			  get_key, place->node, &place->pos, key);
	}

	return 0;
}

#ifndef ENABLE_MINIMAL
errno_t reiser4_item_update_link(reiser4_place_t *place,
				 blk_t blk)
{
	trans_hint_t hint;
	ptr_hint_t ptr;
	
	aal_assert("umka-2668", place != NULL);
	
	/* Prepare @hint. */
	hint.count = 1;
	hint.specific = &ptr;
	ptr.start = blk;
	ptr.width = 1;
	
	return plug_call(place->plug->pl.item->object,
			 update_units, place, &hint);
}

uint16_t reiser4_item_overhead(reiser4_plug_t *plug) {
	aal_assert("vpf-1514", plug != NULL);
	aal_assert("vpf-1515", plug->id.type == ITEM_PLUG_TYPE);

	if (!plug->pl.item->balance->overhead)
		return 0;
	
	return plug_call(plug->pl.item->balance, overhead);
}

void reiser4_item_set_flag(reiser4_place_t *place, uint16_t flag) {
	uint16_t old;
	
	aal_assert("vpf-1041", place != NULL);
	aal_assert("vpf-1111", place->node != NULL);
	aal_assert("vpf-1530", flag < sizeof(flag) * 8 - 1);
	
	old = plug_call(place->node->plug->pl.node, 
			get_flags, place->node, place->pos.item);
	
	if (old & (1 << flag))
		return;
	
	plug_call(place->node->plug->pl.node, set_flags,
		  place->node, place->pos.item, old | (1 << flag));
}

void reiser4_item_clear_flag(reiser4_place_t *place, uint16_t flag) {
	uint16_t old;
	
	aal_assert("vpf-1531", place != NULL);
	aal_assert("vpf-1532", place->node != NULL);
	aal_assert("vpf-1533", flag < sizeof(flag) * 8 - 1);
	
	old = plug_call(place->node->plug->pl.node, 
			get_flags, place->node, place->pos.item);
	
	if (~old & (1 << flag)) 
		return;
	
	plug_call(place->node->plug->pl.node, set_flags, 
		  place->node, place->pos.item, old & ~(1 << flag));
}

void reiser4_item_clear_flags(reiser4_place_t *place) {
	uint16_t old;
	
	aal_assert("vpf-1042", place != NULL);
	aal_assert("vpf-1112", place->node != NULL);
	
	old = plug_call(place->node->plug->pl.node, 
			get_flags, place->node, place->pos.item);
	
	if (!old) return;
	
	plug_call(place->node->plug->pl.node, set_flags, 
		  place->node, place->pos.item, 0);
}

bool_t reiser4_item_test_flag(reiser4_place_t *place, uint16_t flag) {
	uint16_t old;
	
	aal_assert("vpf-1043", place != NULL);
	aal_assert("vpf-1113", place->node != NULL);
	aal_assert("vpf-1534", flag < sizeof(flag) * 8 - 1);
	
	old = plug_call(place->node->plug->pl.node, 
			get_flags, place->node, place->pos.item);
	
	return old & (1 << flag);
}

void reiser4_item_dup_flags(reiser4_place_t *place, uint16_t flags) {
	aal_assert("vpf-1540", place != NULL);

	plug_call(place->node->plug->pl.node, set_flags, 
		  place->node, place->pos.item, flags);
}

uint16_t reiser4_item_get_flags(reiser4_place_t *place) {
	aal_assert("vpf-1541", place != NULL);

	return plug_call(place->node->plug->pl.node, get_flags, 
			 place->node, place->pos.item);
}

lookup_t reiser4_item_collision(reiser4_place_t *place, coll_hint_t *hint) {
	aal_assert("vpf-1550", place != NULL);
	aal_assert("vpf-1551", place->plug != NULL);
	
	if (!place->plug->pl.item->balance->collision)
		return PRESENT;

	return place->plug->pl.item->balance->collision(place, hint);
}
#endif

/* Return block number nodeptr item contains. */
blk_t reiser4_item_down_link(reiser4_place_t *place) {
	trans_hint_t hint;
	ptr_hint_t ptr;
	
	aal_assert("umka-2666", place != NULL);
	
	/* Prepare @hint. */
	hint.count = 1;
	hint.specific = &ptr;

	if (plug_call(place->plug->pl.item->object, 
		      fetch_units, place, &hint) != 1)
	{
		return MAX_UINT64;
	}

	return ptr.start;
}
