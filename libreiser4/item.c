/*
  item.c -- common reiser4 item functions.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/reiser4.h>

/*
  Returns count of units in item. If count method is not implemented,
  it returns 1.
*/
uint32_t reiser4_item_units(reiser4_place_t *place) {
	uint32_t units;
	item_entity_t *item;
	
	aal_assert("umka-1030", place != NULL);

	item = &place->item;
	aal_assert("umka-1448", item->plugin != NULL);

	if (item->plugin->o.item_ops->units) {
		units = item->plugin->o.item_ops->units(item);

		aal_assert("umka-1883", units > 0);
		return units;
	}
	
	return 1;
}

#ifndef ENABLE_STAND_ALONE

/*
  Estimating insert operation. Below is the possible cases.

  1. Insertion of data: 
  a) pos->unit == ~0ul 
  b) hint->data != NULL
  c) get hint->plugin on the base of pos.
    
  2. Insertion of info: 
  a) pos->unit == ~0ul 
  b) hint->hint != NULL
  c) hint->plugin != NULL
    
  3. Pasting of data: 
  a) pos->unit != ~0ul 
  b) hint->data != NULL
  c) get hint->plugin on the base of pos.
    
  4. Pasting of info: 
  a) pos->unit_pos != ~0ul 
  b) hint->hint != NULL
  c) get hint->plugin on the base of pos.
*/
errno_t reiser4_item_estimate(
	reiser4_place_t *place,	   /* item we will work with */
	create_hint_t *hint)       /* item hint to be estimated */
{
	errno_t res;
	
	aal_assert("vpf-106", place != NULL);
	aal_assert("umka-541", hint != NULL);
   
	if (hint->flags == HF_RAWDATA)
		return 0;

	aal_assert("umka-2230", hint->plugin != NULL);

	/* Check if we're going to insert an unit or an item instead */
	if (place->pos.unit != ~0ul) {
		/*
		  Unit component is set, so, we assume this is an attempt insert
		  new unit and item_entity should be passed to item's estimate
		  method carefully. Method place_realise() will take care about
		  @place->item.
		*/
		if ((res = reiser4_place_realize(place)))
			return res;
	}

	return plugin_call(hint->plugin->o.item_ops, estimate_insert,
			   &place->item, place->pos.unit, hint->count, hint);
}

/* Prints passed @place into passed @buff */
errno_t reiser4_item_print(
	reiser4_place_t *place,    /* item to be printed */
	aal_stream_t *stream)      /* stream for printing in */
{
	item_entity_t *item;
	
	aal_assert("umka-1297", place != NULL);
	aal_assert("umka-1550", stream != NULL);

	item = &place->item;
	aal_assert("umka-1449", item->plugin != NULL);

	if (!item->plugin->o.item_ops->print)
		return -EINVAL;
	
	return item->plugin->o.item_ops->print(item, stream, 0);
}

bool_t reiser4_item_filebody(reiser4_place_t *place) {
	aal_assert("umka-1098", place != NULL);

	return reiser4_key_get_type(&place->item.key) == KEY_FILEBODY_TYPE;
}

bool_t reiser4_item_statdata(reiser4_place_t *place) {
	aal_assert("umka-1831", place != NULL);

	return reiser4_key_get_type(&place->item.key) == KEY_STATDATA_TYPE;
}

bool_t reiser4_item_filename(reiser4_place_t *place) {
	aal_assert("umka-1830", place != NULL);

	return reiser4_key_get_type(&place->item.key) == KEY_FILENAME_TYPE;
}

/* Returns item type from its plugin */
rid_t reiser4_item_type(reiser4_place_t *place) {
	item_entity_t *item;
	
	aal_assert("vpf-424", place != NULL);

	item = &place->item;
	aal_assert("vpf-425", item->plugin != NULL);
	
	if (item->plugin->h.type != ITEM_PLUGIN_TYPE)
		return LAST_ITEM;
		
	return (item->plugin->h.group < LAST_ITEM ?
		item->plugin->h.group : LAST_ITEM);
}

/* Retuns item body pointer */
body_t *reiser4_item_body(reiser4_place_t *place) {
	item_entity_t *item;
	
	aal_assert("umka-554", place != NULL);
    
	item = &place->item;
	aal_assert("umka-1461", item->plugin != NULL);
	
	return item->body;
}

/* Returns item plugin in use */
reiser4_plugin_t *reiser4_item_plugin(reiser4_place_t *place) {
	aal_assert("umka-755", place != NULL);
	return place->item.plugin;
}

#endif

/* Returns TRUE if @place points to an internal item */
bool_t reiser4_item_branch(reiser4_place_t *place) {
	item_entity_t *item;
	
	aal_assert("umka-1828", place != NULL);

	item = &place->item;
	aal_assert("umka-1829", item->plugin != NULL);

	if (!item->plugin->o.item_ops->branch)
		return FALSE;

	return item->plugin->o.item_ops->branch();
}

/* Returns item len */
uint32_t reiser4_item_len(reiser4_place_t *place) {
	item_entity_t *item;
	
	aal_assert("umka-760", place != NULL);

	item = &place->item;
	aal_assert("umka-1460", item->plugin != NULL);
	
	return item->len;
}

#ifndef ENABLE_STAND_ALONE
/* Updates item key in node and in place->item.key field */
errno_t reiser4_item_set_key(reiser4_place_t *place,
			     reiser4_key_t *key)
{
	item_entity_t *item;
	object_entity_t *entity;
	
	aal_assert("umka-1404", key != NULL);
	aal_assert("umka-1403", place != NULL);

	item = &place->item;
	
	if (!(entity = place->node->entity))
		return -EINVAL;

	aal_memcpy(&item->key, key, sizeof(*key));
	
	return plugin_call(entity->plugin->o.node_ops, set_key, 
			   entity, &place->pos, key);
}
#endif

/*
  Returns maximal possible key may exist in item at @place. If item's "get_key"
  method is not implemented, it returns item key.
*/
errno_t reiser4_item_maxposs_key(reiser4_place_t *place,
				 reiser4_key_t *key)
{
	errno_t res;
	item_entity_t *item;
	
	aal_assert("umka-1269", place != NULL);
	aal_assert("umka-1270", key != NULL);

	item = &place->item;
	aal_assert("umka-1456", item->plugin != NULL);
	
	aal_memcpy(key, &item->key, sizeof(*key));

	if (item->plugin->o.item_ops->maxposs_key)
		return item->plugin->o.item_ops->maxposs_key(item, key);

	return 0;
}

#ifndef ENABLE_STAND_ALONE
bool_t reiser4_item_mergeable(reiser4_place_t *place1,
			      reiser4_place_t *place2)
{
	item_entity_t *item1;
	item_entity_t *item2;
	
	aal_assert("umka-2006", place1 != NULL);
	aal_assert("umka-2007", place2 != NULL);
	
	item1 = &place1->item;
	item2 = &place2->item;
	
	if (!item1->plugin->o.item_ops->mergeable)
		return FALSE;
	
	return item1->plugin->o.item_ops->mergeable(item1, item2) ?
		TRUE : FALSE;
}

/* Returns real maximal item key */
errno_t reiser4_item_maxreal_key(reiser4_place_t *place,
				 reiser4_key_t *key)
{
	errno_t res;
	item_entity_t *item;
	
	aal_assert("vpf-351", place != NULL);
	aal_assert("vpf-352", key != NULL);
    
	item = &place->item;
	aal_assert("umka-1457", item->plugin != NULL);

	aal_memcpy(key, &item->key, sizeof(*key));

	if (item->plugin->o.item_ops->maxreal_key) 
		return item->plugin->o.item_ops->maxreal_key(item, key);	
		
	return 0;
}

bool_t reiser4_item_data(reiser4_plugin_t *plugin) {
	aal_assert("vpf-747", plugin != NULL);

	return (plugin->o.item_ops->data &&
		plugin->o.item_ops->data());
}

errno_t reiser4_item_insert(reiser4_place_t *place,
			    create_hint_t *hint)
{
	errno_t res;
	
	aal_assert("umka-2257", place != NULL);
	aal_assert("umka-2258", hint != NULL);

	if ((res = plugin_call(place->item.plugin->o.item_ops,
			       insert, &place->item, hint,
			       place->pos.unit)))
	{
		return res;
	}

	reiser4_node_mkdirty(place->node);
	
	return 0;
}
#endif
