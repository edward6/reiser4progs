/*
  place.c -- reiser4 tree place functions. Place contains full information about
  tree item/unit position in the tree. The instance of structure reiser4_place_t
  contains pointer to node item/unit lies in,, item position and unit position
  in specified item.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <reiser4/reiser4.h>

#ifndef ENABLE_STAND_ALONE
/* Returns TRUE if passed @place points to left delimiting item */
bool_t reiser4_place_leftmost(reiser4_place_t *place) {
	aal_assert("umka-1862", place != NULL);
	
	return ((place->pos.unit == 0 || place->pos.unit == ~0ul) &&
		place->pos.item == 0) ? TRUE : FALSE;
}

/*
  Returns TRUE if passed @place points to the last unit of last item in the
  node.
*/
bool_t reiser4_place_rightmost(reiser4_place_t *place) {
	uint32_t items;
	uint32_t units;
	
	aal_assert("umka-1873", place != NULL);

	reiser4_place_realize(place);

	units = reiser4_item_units(place);
	items = reiser4_node_items(place->node);

	if (place->pos.item == items - 1 && place->pos.unit == units)
		return TRUE;
	
	if (place->pos.item == items && place->pos.unit == ~0ul)
		return TRUE;

	return FALSE;
}
#endif

/* Initializes all item-related fields */
errno_t reiser4_place_realize(reiser4_place_t *place) {
	rid_t pid;
	reiser4_key_t *key;
	
	item_entity_t *item;
	object_entity_t *entity;
	
        aal_assert("umka-1459", place != NULL);

	aal_assert("umka-1895", place->pos.item <
		   reiser4_node_items(place->node));

	entity = place->node->entity;
	
	if ((pid = plugin_call(entity->plugin->node_ops, item_pid,
			       entity, &place->pos)) == INVAL_PID)
	{
		aal_exception_error("Invalid item plugin id has been "
				    "detected.");
		return -EINVAL;
	}

	item = &place->item;
	
	if (!(item->plugin = libreiser4_factory_ifind(ITEM_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find item plugin by its id "
				    "0x%x.", pid);
		return -EINVAL;
	}

	if (!(item->body = plugin_call(entity->plugin->node_ops, item_body,
				       entity, &place->pos)))
	{
		aal_exception_error("Can't get item body.");
		return -EINVAL;
	}

	/* Initializing item entity fields */
	item->pos = place->pos;
	
	item->len = plugin_call(entity->plugin->node_ops, item_len,
				entity, &place->pos);

	/* Initializing item context fields */
	item->context.blk = place->node->blk;
	item->context.device = place->node->device;

	return 0;
}

/* This function initializes passed @place by specified params */
errno_t reiser4_place_init(
	reiser4_place_t *place,	 /* place to be initialized */
	reiser4_node_t *node,	 /* the first component of place */
	pos_t *pos)	         /* place pos component */
{
	aal_assert("umka-795", place != NULL);
    
	place->node = node;

	if (pos != NULL)
		place->pos = *pos;

	return 0;
}

errno_t reiser4_place_assign(
	reiser4_place_t *place,	  /* place to be initialized */
	reiser4_node_t *node,     /* node to be assigned to place */
	uint32_t item,            /* item component */
	uint32_t unit)	          /* unit component */
{
	pos_t pos = {item, unit};
	
        aal_assert("umka-1730", place != NULL);
	
	aal_memset(place, 0, sizeof(*place));
	return reiser4_place_init(place, node, &pos);
}

/* Initializes @place and its item related fields */
errno_t reiser4_place_open(
	reiser4_place_t *place,	 /* place to be initialized */
	reiser4_node_t *node,	 /* the first component of place */
	pos_t *pos)	         /* place pos component */
{
	errno_t res;
	
        aal_assert("umka-1435", place != NULL);

	if ((res = reiser4_place_init(place, node, pos)))
		return res;

	return reiser4_place_realize(place);
}
