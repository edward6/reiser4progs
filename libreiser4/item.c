/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   item.c -- common reiser4 item functions. */

#include <reiser4/reiser4.h>

/* Returns count of units in item. If count method is not implemented, it
   returns 1. */
uint32_t reiser4_item_units(reiser4_place_t *place) {
	aal_assert("umka-1030", place != NULL);
	aal_assert("umka-1448", place->plug != NULL);

	if (place->plug->o.item_ops->units) {
		return plug_call(place->plug->o.item_ops, units,
				 (place_t *)place);
	}
	
	return 1;
}

#ifndef ENABLE_STAND_ALONE
/* Estimating insert operation. */
errno_t reiser4_item_estimate(
	reiser4_place_t *place,	   /* item we will work with */
	insert_hint_t *hint)       /* item hint to be estimated */
{
	aal_assert("vpf-106", place != NULL);
	aal_assert("umka-541", hint != NULL);
	aal_assert("umka-2230", hint->plug != NULL);

	hint->len = 0;
	hint->ohd = 0;

	return plug_call(hint->plug->o.item_ops,
			 estimate_insert, (place_t *)place, hint);
}

/* Prints passed @place into passed @buff */
errno_t reiser4_item_print(
	reiser4_place_t *place,    /* item to be printed */
	aal_stream_t *stream)      /* stream for printing in */
{
	aal_assert("umka-1297", place != NULL);
	aal_assert("umka-1550", stream != NULL);
	aal_assert("umka-1449", place->plug != NULL);

	if (!place->plug->o.item_ops->print)
		return -EINVAL;
	
	return place->plug->o.item_ops->print((place_t *)place,
					      stream, 0);
}

bool_t reiser4_item_statdata(reiser4_place_t *place) {
	aal_assert("umka-1831", place != NULL);
	aal_assert("umka-2382", place->plug != NULL);

	return place->plug->id.group == STATDATA_ITEM;
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
#endif

/* Returns TRUE if @place points to an internal item */
bool_t reiser4_item_branch(reiser4_plug_t *plug) {
	aal_assert("umka-1829", plug != NULL);

	if (!plug->o.item_ops->branch)
		return FALSE;

	return plug->o.item_ops->branch();
}

/* Returns maximal possible key may exist in item at @place. If item's "get_key"
   method is not implemented, it returns item key. */
errno_t reiser4_item_maxposs_key(reiser4_place_t *place,
				 reiser4_key_t *key)
{
	aal_assert("umka-1270", key != NULL);
	aal_assert("umka-1269", place != NULL);
	aal_assert("umka-1456", place->plug != NULL);
	
	aal_memcpy(key, &place->key, sizeof(*key));

	if (place->plug->o.item_ops->maxposs_key == NULL)
		return 0;
	
	return plug_call(place->plug->o.item_ops,
			 maxposs_key, (place_t *)place, key);
}

#ifndef ENABLE_STAND_ALONE
/* Returns real maximal item key */
errno_t reiser4_item_maxreal_key(reiser4_place_t *place,
				 reiser4_key_t *key)
{
	aal_assert("vpf-352", key != NULL);
	aal_assert("vpf-351", place != NULL);
	aal_assert("umka-1457", place->plug != NULL);

	aal_memcpy(key, &place->key, sizeof(*key));

	if (place->plug->o.item_ops->maxreal_key == NULL)
		return 0;

	return plug_call(place->plug->o.item_ops,
			 maxreal_key, (place_t *)place, key);
}

errno_t reiser4_item_ukey(reiser4_place_t *place, reiser4_key_t *key) {
	aal_assert("vpf-1205", key != NULL);
	aal_assert("vpf-1205", place != NULL);
	
	reiser4_key_assign(&place->key, key);
	
	return reiser4_node_ukey(place->node, &place->pos, &place->key);
}

errno_t reiser4_item_key(reiser4_place_t *place, reiser4_key_t *key) {
	aal_assert("vpf-1290", place != NULL);
	aal_assert("vpf-1291", key != NULL);

	aal_memcpy(key, &place->key, sizeof(*key));
	
	if (!place->pos.unit || !place->plug->o.item_ops->get_key)
		return 0;
	
	return plug_call(place->plug->o.item_ops, get_key,
			 (place_t *)place, key);
}
#endif
