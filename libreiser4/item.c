/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   item.c -- common reiser4 item functions. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

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
	create_hint_t *hint)       /* item hint to be estimated */
{
	errno_t res;
	
	aal_assert("vpf-106", place != NULL);
	aal_assert("umka-541", hint != NULL);
   
	if (hint->flags == HF_RAWDATA)
		return 0;

	aal_assert("umka-2230", hint->plug != NULL);

	/* Method estimate_insert() may be not implemented as it is not needed
	   in some cases like tail item case. */
	if (!hint->plug->o.item_ops->estimate_insert)
		return 0;

	if (place->pos.unit != MAX_UINT32) {
		if ((res = reiser4_place_realize(place)))
			return res;
	}

	return plug_call(hint->plug->o.item_ops, estimate_insert,
			 (place_t *)place, hint, place->pos.unit);
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

bool_t reiser4_item_filebody(reiser4_place_t *place) {
	int type;
	
	aal_assert("umka-1098", place != NULL);

	type = reiser4_key_get_type(&place->key);
	return type == KEY_FILEBODY_TYPE;
}

bool_t reiser4_item_statdata(reiser4_place_t *place) {
	int type;
	
	aal_assert("umka-1831", place != NULL);

	type = reiser4_key_get_type(&place->key);
	return type == KEY_STATDATA_TYPE;
}

bool_t reiser4_item_filename(reiser4_place_t *place) {
	int type;
	
	aal_assert("umka-1830", place != NULL);

	type = reiser4_key_get_type(&place->key);
	return type == KEY_FILENAME_TYPE;
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

/* Retuns item body pointer */
body_t *reiser4_item_body(reiser4_place_t *place) {
	aal_assert("umka-554", place != NULL);
	aal_assert("umka-1461", place->plug != NULL);
	return place->body;
}

/* Returns item plugin in use */
reiser4_plug_t *reiser4_item_plug(reiser4_place_t *place) {
	aal_assert("umka-755", place != NULL);
	return place->plug;
}
#endif

/* Returns TRUE if @place points to an internal item */
bool_t reiser4_item_branch(reiser4_place_t *place) {
	aal_assert("umka-1828", place != NULL);
	aal_assert("umka-1829", place->plug != NULL);

	if (!place->plug->o.item_ops->branch)
		return FALSE;

	return place->plug->o.item_ops->branch();
}

/* Returns item len */
uint32_t reiser4_item_len(reiser4_place_t *place) {
	aal_assert("umka-760", place != NULL);
	aal_assert("umka-1460", place->plug != NULL);
	return place->len;
}

#ifndef ENABLE_STAND_ALONE
/* Updates item key in node and in place->item.key field */
errno_t reiser4_item_set_key(reiser4_place_t *place,
			     reiser4_key_t *key)
{
	object_entity_t *entity;
	
	aal_assert("umka-1404", key != NULL);
	aal_assert("umka-1403", place != NULL);
	aal_assert("umka-2330", key->plug != NULL);
	
	if (!(entity = place->node->entity))
		return -EINVAL;

	reiser4_key_assign(&place->key, key);

	return plug_call(entity->plug->o.node_ops,
			 set_key, entity, &place->pos,
			 key);
}
#endif

/* Returns maximal possible key may exist in item at @place. If item's "get_key"
   method is not implemented, it returns item key. */
errno_t reiser4_item_maxposs_key(reiser4_place_t *place,
				 reiser4_key_t *key)
{
	errno_t res;
	
	aal_assert("umka-1270", key != NULL);
	aal_assert("umka-1269", place != NULL);
	aal_assert("umka-1456", place->plug != NULL);
	
	aal_memcpy(key, &place->key, sizeof(*key));

	if (place->plug->o.item_ops->maxposs_key) {
		return plug_call(place->plug->o.item_ops, maxposs_key,
				(place_t *)place, key);
	}

	return 0;
}

#ifndef ENABLE_STAND_ALONE
bool_t reiser4_item_mergeable(reiser4_place_t *place1,
			      reiser4_place_t *place2)
{
	aal_assert("umka-2006", place1 != NULL);
	aal_assert("umka-2007", place2 != NULL);
	
	if (!place1->plug->o.item_ops->mergeable)
		return FALSE;

	if (place1->plug->o.item_ops->mergeable((place_t *)place1,
						(place_t *)place2))
	{
		return TRUE;
	}

	return FALSE;
}

/* Returns real maximal item key */
errno_t reiser4_item_maxreal_key(reiser4_place_t *place,
				 reiser4_key_t *key)
{
	errno_t res;
	
	aal_assert("vpf-352", key != NULL);
	aal_assert("vpf-351", place != NULL);
	aal_assert("umka-1457", place->plug != NULL);

	aal_memcpy(key, &place->key, sizeof(*key));

	if (place->plug->o.item_ops->maxreal_key) {
		return plug_call(place->plug->o.item_ops, maxreal_key,
				 (place_t *)place, key);
	}
		
	return 0;
}

bool_t reiser4_item_data(reiser4_plug_t *plug) {
	aal_assert("vpf-747", plug != NULL);

	return (plug->o.item_ops->data &&
		plug->o.item_ops->data());
}

errno_t reiser4_item_insert(reiser4_place_t *place,
			    create_hint_t *hint)
{
	errno_t res;
	
	aal_assert("umka-2257", place != NULL);
	aal_assert("umka-2258", hint != NULL);

	if ((res = plug_call(place->plug->o.item_ops, insert,
			     (place_t *)place, hint, place->pos.unit)))
	{
		return res;
	}

	reiser4_node_mkdirty(place->node);
	return 0;
}
#endif
