/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   item.c -- common reiser4 item functions. */

#include <reiser4/reiser4.h>

/* Returns count of units in item. If count method is not implemented, it
   returns 1. */
uint32_t reiser4_item_units(place_t *place) {
	aal_assert("umka-1030", place != NULL);
	aal_assert("umka-1448", place->plug != NULL);

	if (place->plug->o.item_ops->balance->units) {
		return plug_call(place->plug->o.item_ops->balance,
				 units, place);
	}
	
	return 1;
}

#ifndef ENABLE_STAND_ALONE
/* Prints passed @place into passed @buff */
errno_t reiser4_item_print(place_t *place,
			   aal_stream_t *stream)
{
	aal_assert("umka-1297", place != NULL);
	aal_assert("umka-1550", stream != NULL);
	aal_assert("umka-1449", place->plug != NULL);

	if (!place->plug->o.item_ops->debug->print)
		return -EINVAL;
	
	return plug_call(place->plug->o.item_ops->debug,
			 print, place, stream, 0);
}

bool_t reiser4_item_statdata(place_t *place) {
	aal_assert("umka-1831", place != NULL);
	aal_assert("umka-2382", place->plug != NULL);

	return place->plug->id.group == STATDATA_ITEM;
}

/* Returns item type from its plugin */
rid_t reiser4_item_type(place_t *place) {
	aal_assert("vpf-424", place != NULL);
	aal_assert("vpf-425", place->plug != NULL);
	
	if (place->plug->id.type != ITEM_PLUG_TYPE)
		return LAST_ITEM;
		
	return (place->plug->id.group < LAST_ITEM ?
		place->plug->id.group : LAST_ITEM);
}
#endif

/* Returns 1 if @place points to an nodeptr item. */
bool_t reiser4_item_branch(reiser4_plug_t *plug) {
	aal_assert("umka-1829", plug != NULL);
	return (plug->o.item_ops->tree->down_link != NULL);
}

/* Returns maximal possible key may exist in item at @place. */
errno_t reiser4_item_maxposs_key(place_t *place,
				 reiser4_key_t *key)
{
	aal_assert("umka-1270", key != NULL);
	aal_assert("umka-1269", place != NULL);
	aal_assert("umka-1456", place->plug != NULL);
	
	aal_memcpy(key, &place->key, sizeof(*key));

	if (!place->plug->o.item_ops->balance->maxposs_key)
		return 0;
	
	return plug_call(place->plug->o.item_ops->balance,
			 maxposs_key, place, key);
}

#ifndef ENABLE_STAND_ALONE
/* Returns real maximal item key */
errno_t reiser4_item_maxreal_key(place_t *place,
				 reiser4_key_t *key)
{
	aal_assert("vpf-352", key != NULL);
	aal_assert("vpf-351", place != NULL);
	aal_assert("umka-1457", place->plug != NULL);

	aal_memcpy(key, &place->key, sizeof(*key));

	if (!place->plug->o.item_ops->balance->maxreal_key)
		return 0;

	return plug_call(place->plug->o.item_ops->balance,
			 maxreal_key, place, key);
}

errno_t reiser4_item_update_key(place_t *place,
				reiser4_key_t *key)
{
	aal_assert("vpf-1205", key != NULL);
	aal_assert("vpf-1205", place != NULL);
	
	reiser4_key_assign(&place->key, key);
	
	return reiser4_node_update_key(place->node,
				       &place->pos,
				       &place->key);
}

errno_t reiser4_item_get_key(place_t *place,
			     reiser4_key_t *key)
{
	aal_assert("vpf-1290", place != NULL);
	aal_assert("vpf-1291", key != NULL);

	aal_memcpy(key, &place->key, sizeof(*key));
	
	if (!place->plug->o.item_ops->balance->fetch_key ||
	    !place->pos.unit)
	{
		return 0;
	}
	
	return plug_call(place->plug->o.item_ops->balance,
			 fetch_key, place, key);
}

errno_t reiser4_item_update_link(place_t *place,
				 blk_t blk)
{
	aal_assert("umka-2668", place != NULL);
	
	return plug_call(place->plug->o.item_ops->tree,
			 update_link, place, blk);
}
#endif

/* Return block number nodeptr item contains. */
blk_t reiser4_item_down_link(place_t *place) {
	aal_assert("umka-2666", place != NULL);
	
	return plug_call(place->plug->o.item_ops->tree,
			 down_link, place);
}
