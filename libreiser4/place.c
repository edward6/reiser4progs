/*
  place.c -- reiser4 tree place functions. Place contains full information about
  tree item/unit position in the tree. The instance of structure reiser4_place_t
  contains pointer to node item/unit lies in,, item position and unit position
  in specified item.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <reiser4/reiser4.h>

/* Makes passed @place pointing to the first unit of the first item */
errno_t reiser4_place_first(reiser4_place_t *place) {
	place->pos.item = 0;
	
	if (place->pos.unit != ~0ul)
		place->pos.unit = 0;
	
	return 0;
}

/* Makes passed @place pointing to the last unit of the last item */
errno_t reiser4_place_last(reiser4_place_t *place) {
	uint32_t items = reiser4_node_items(place->node);
			
	place->pos.item = items - 1;
	
	if (place->pos.unit != ~0ul) {
		
		if (reiser4_place_realize(place))
			return -EINVAL;

		place->pos.unit = reiser4_item_units(place) - 1;
	}

	return 0;
}

/* Returns TRUE is passed @place is greter than 0 */
bool_t reiser4_place_gtfirst(reiser4_place_t *place) {

	if (place->pos.unit == ~0ul)
		return place->pos.item > 0;
	
	return place->pos.item > 0 || place->pos.unit > 0;
}

/* Returns TRUE is passed @place is greter than 0 */
bool_t reiser4_place_ltlast(reiser4_place_t *place) {
	uint32_t items = reiser4_node_items(place->node);
			
	if (place->pos.unit == ~0ul) {
		return (place->pos.item < items - 1);
	} else {
		uint32_t units;

		if (reiser4_place_realize(place))
			return FALSE;

		units = reiser4_item_units(place);
				
		return (place->pos.item < items - 1 ||
			place->pos.unit < units - 1);
	}
}

#ifndef ENABLE_STAND_ALONE

/* Returns TRUE if passed @place points to left delimiting item */
bool_t reiser4_place_leftmost(reiser4_place_t *place) {
	aal_assert("umka-1862", place != NULL);
	
	return (place->pos.unit == 0 ||
		place->pos.unit == ~0ul) &&
		place->pos.item == 0;
}

/*
  Returns TRUE if passed @place points to the last unit of last item in the
  node.
*/
bool_t reiser4_place_rightmost(reiser4_place_t *place) {
	uint32_t items;
	uint32_t units;
	
	aal_assert("umka-1873", place != NULL);
	
	items = reiser4_node_items(place->node);
	
	if (place->pos.item == items)
		return TRUE;
	
	reiser4_place_realize(place);
	units = reiser4_item_units(place);
	
	return (place->pos.item == items - 1 && 
		place->pos.unit == units);
}
#endif

/* Initializes all item-related fields */
errno_t reiser4_place_realize(reiser4_place_t *place) {
	object_entity_t *entity;
	
	aal_assert("umka-1459", place != NULL);

	entity = place->node->entity;

	return plugin_call(entity->plugin->node_ops, get_item,
			   entity, &place->pos, &place->item);
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
