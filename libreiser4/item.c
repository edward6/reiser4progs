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

	if (item->plugin->item_ops.units) {
		units = item->plugin->item_ops.units(item);

		aal_assert("umka-1883", units > 0);
		return units;
	}
	
	return 1;
}

#ifndef ENABLE_ALONE

/*
  We can estimate size for insertion and for pasting of hint->data (to be
  memcpy) or of item_info->info (data to be created on the base of).
    
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
	reiser4_item_hint_t *hint) /* item hint to be estimated */
{
	aal_assert("vpf-106", place != NULL);
	aal_assert("umka-541", hint != NULL);

	/* We must have hint->plugin initialized for the 2nd case */
	aal_assert("vpf-118", place->pos.unit != ~0ul || 
		   hint->plugin != NULL);
   
	/* Here hint has been already set for the 3rd case */
	if (hint->flags == HF_RAWDATA)
		return 0;

	/* Check if we're egoing insert unit or an item instead */
	if (place->pos.unit == ~0ul) {
		return plugin_call(hint->plugin->item_ops, estimate, NULL,
				   hint, place->pos.unit, hint->count);
	} else {
		/*
		  Unit component is set up, so, we assume this is an attempt
		  insert new unit and item_entity should be passed to item's
		  estimate method carefully.
		*/
		
		if (reiser4_place_realize(place))
			return -1;
		
		return plugin_call(hint->plugin->item_ops, estimate,
				   &place->item, hint, place->pos.unit,
				   hint->count);
	}
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

	if (!item->plugin->item_ops.print)
		return -1;
	
	return item->plugin->item_ops.print(item, stream, 0);
}

#endif

bool_t reiser4_item_filebody(reiser4_place_t *place) {
	aal_assert("umka-1098", place != NULL);

	if (reiser4_item_get_key(place, NULL))
		return FALSE;

	return reiser4_key_get_type(&place->item.key) == KEY_FILEBODY_TYPE;
}

bool_t reiser4_item_statdata(reiser4_place_t *place) {
	aal_assert("umka-1831", place != NULL);

	if (reiser4_item_get_key(place, NULL))
		return FALSE;

	return reiser4_key_get_type(&place->item.key) == KEY_STATDATA_TYPE;
}

bool_t reiser4_item_filename(reiser4_place_t *place) {
	aal_assert("umka-1830", place != NULL);

	if (reiser4_item_get_key(place, NULL))
		return FALSE;

	return reiser4_key_get_type(&place->item.key) == KEY_FILENAME_TYPE;
}

bool_t reiser4_item_extent(reiser4_place_t *place) {
	item_entity_t *item;
	
	aal_assert("vpf-238", place != NULL);

	item = &place->item;
	aal_assert("umka-1452", item->plugin != NULL);
	
	return item->plugin->h.type == ITEM_PLUGIN_TYPE &&
		item->plugin->h.group == EXTENT_ITEM;
}

bool_t reiser4_item_nodeptr(reiser4_place_t *place) {
	item_entity_t *item;
	
	aal_assert("vpf-042", place != NULL);

	item = &place->item;
	aal_assert("umka-1455", item->plugin != NULL);
	
	return item->plugin->h.type == ITEM_PLUGIN_TYPE &&
		item->plugin->h.group == NODEPTR_ITEM;
}

/* Returns TRUE if @place points to an internal item */
bool_t reiser4_item_branch(reiser4_place_t *place) {
	item_entity_t *item;
	
	aal_assert("umka-1828", place != NULL);

	item = &place->item;
	aal_assert("umka-1829", item->plugin != NULL);

	if (!item->plugin->item_ops.branch)
		return FALSE;

	return item->plugin->item_ops.branch(item);
}

/* Returns item type from its plugin */
rpid_t reiser4_item_type(reiser4_place_t *place) {
	item_entity_t *item;
	
	aal_assert("vpf-424", place != NULL);

	item = &place->item;
	aal_assert("vpf-425", item->plugin != NULL);
	
	if (item->plugin->h.type != ITEM_PLUGIN_TYPE)
		return LAST_ITEM;
		
	return (item->plugin->h.group < LAST_ITEM ?
		item->plugin->h.group : LAST_ITEM);
}

/* Returns item len */
uint32_t reiser4_item_len(reiser4_place_t *place) {
	item_entity_t *item;
	
	aal_assert("umka-760", place != NULL);

	item = &place->item;
	aal_assert("umka-1460", item->plugin != NULL);
	
	return item->len;
}

/* Retuns item body pointer */
rbody_t *reiser4_item_body(reiser4_place_t *place) {
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

/* Returns item key and updates key fields in place->item.key */
errno_t reiser4_item_get_key(reiser4_place_t *place,
			     reiser4_key_t *key)
{
	item_entity_t *item;
	object_entity_t *entity;
	
	aal_assert("umka-1215", place != NULL);

	item = &place->item;

	if (!(entity = place->node->entity))
		return -1;

	aal_assert("umka-1462", item->plugin != NULL);

	if (plugin_call(entity->plugin->node_ops, get_key, entity,
			&place->pos, &item->key))
	{
		aal_exception_error("Can't get item key.");
		return -1;
	}

	if (reiser4_key_guess(&item->key))
		return -1;
	
	if (place->pos.unit != ~0ul && item->plugin->item_ops.get_key) {
		
		if (item->plugin->item_ops.get_key(item, place->pos.unit, 
						   &item->key))
			return -1;
	}

	if (key != NULL)
		aal_memcpy(key, &item->key, sizeof(*key));

	return 0;
}

errno_t reiser4_item_realize(reiser4_place_t *place) {
	return reiser4_item_get_key(place, NULL);
}

#ifndef ENABLE_ALONE

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
		return -1;

	aal_memcpy(&item->key, key, sizeof(*key));
	
	if (place->pos.unit != ~0ul) {
		return plugin_call(item->plugin->item_ops, set_key,
				   item, place->pos.unit, key);
	} else {
		return plugin_call(entity->plugin->node_ops, 
				   set_key, entity, &place->pos, key);
	}
}

#endif

/*
  Returns maximal possible key may exist in item at @place. If item's "get_key"
  method is not implemented, it returns item key.
*/
errno_t reiser4_item_maxposs_key(reiser4_place_t *place,
				 reiser4_key_t *key)
{
	item_entity_t *item;
	
	aal_assert("umka-1269", place != NULL);
	aal_assert("umka-1270", key != NULL);

	item = &place->item;
	aal_assert("umka-1456", item->plugin != NULL);
		
	if (reiser4_item_get_key(place, key))
		return -1;
	
	if (item->plugin->item_ops.maxposs_key)
		return item->plugin->item_ops.maxposs_key(item, key);

	return 0;
}

/* Returns real maximal item key */
errno_t reiser4_item_utmost_key(reiser4_place_t *place,
				reiser4_key_t *key)
{
	item_entity_t *item;
	
	aal_assert("vpf-351", place != NULL);
	aal_assert("vpf-352", key != NULL);
    
	item = &place->item;
	aal_assert("umka-1457", item->plugin != NULL);

	if (reiser4_item_get_key(place, key))
		return -1;
	    
	if (item->plugin->item_ops.utmost_key) 
		return item->plugin->item_ops.utmost_key(item, key);	
		
	return 0;
}

/* Returns item gap key. It is the key where non-contiguous region starts */
errno_t reiser4_item_gap_key(reiser4_place_t *place,
			     reiser4_key_t *key)
{
	item_entity_t *item;
	
	aal_assert("vpf-691", place != NULL);
	aal_assert("vpf-688", key != NULL);
	
	item = &place->item;
	aal_assert("vpf-692", item->plugin != NULL);
	
	if (reiser4_item_get_key(place, key))
		return -1;

	if (item->plugin->item_ops.gap_key) 
		return item->plugin->item_ops.gap_key(item, key);
	
	return 0;
}

bool_t reiser4_item_data(reiser4_plugin_t *plugin) {
	aal_assert("vpf-747", plugin != NULL);

	return plugin->item_ops.data && plugin->item_ops.data();
}
