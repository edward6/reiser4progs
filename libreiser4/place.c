/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   place.c -- reiser4 tree place functions. Place contains full information
   about position in the tree. The instance of structure reiser4_place_t
   contains pointer to node item/unit lies in. */

#include <reiser4/libreiser4.h>

bool_t reiser4_place_valid(reiser4_place_t *place) {
	aal_assert("umka-2367", place != NULL);
	aal_assert("umka-2368", place->node != NULL);
	
	return place->pos.item < reiser4_node_items(place->node);
}

/* Makes passed @place pointing to the first unit of the first item */
errno_t reiser4_place_first(reiser4_place_t *place) {
	place->pos.item = 0;
	
	if (place->pos.unit != MAX_UINT32)
		place->pos.unit = 0;
	
	return 0;
}

/* Makes passed @place pointing to the last unit of the last item */
errno_t reiser4_place_last(reiser4_place_t *place) {
	uint32_t items = reiser4_node_items(place->node);
			
	place->pos.item = items - 1;
	
	if (place->pos.unit != MAX_UINT32) {
		
		if (reiser4_place_fetch(place))
			return -EINVAL;

		place->pos.unit = reiser4_item_units(place) - 1;
	}

	return 0;
}

/* Returns TRUE if passed @place points to unit that lies after the first unit
   of the first item in node. */
bool_t reiser4_place_gtfirst(reiser4_place_t *place) {

	if (place->pos.unit == MAX_UINT32)
		return place->pos.item > 0;
	
	return place->pos.item > 0 || place->pos.unit > 0;
}

/* Returns TRUE if passed @place points to unit that lies before the last units
   of the last item in node. */
bool_t reiser4_place_ltlast(reiser4_place_t *place) {
	uint32_t items = reiser4_node_items(place->node);
			
	if (place->pos.unit == MAX_UINT32) {
		return (place->pos.item < items - 1);
	} else {
		uint32_t units;

		if (reiser4_place_fetch(place))
			return 0;

		units = reiser4_item_units(place);
				
		return (place->pos.item < items - 1 ||
			place->pos.unit < units - 1);
	}
}

#ifndef ENABLE_MINIMAL
/* Returns TRUE if passed @place points to leftmost item/unit */
bool_t reiser4_place_leftmost(reiser4_place_t *place) {
	aal_assert("umka-1862", place != NULL);
	
	return ((place->pos.unit == 0 || place->pos.unit == MAX_UINT32) &&
		place->pos.item == 0);
}
#endif

/* Returns TRUE if @place sits after the last unit of last item in the node. */
bool_t reiser4_place_rightmost(reiser4_place_t *place) {
	uint32_t items;
	uint32_t units;
	
	aal_assert("umka-1873", place != NULL);
	
	items = reiser4_node_items(place->node);
	
	if (place->pos.item == items)
		return 1;
	
	if (reiser4_place_fetch(place))
		return 0;
	
	units = reiser4_item_units(place);
	
	return (place->pos.item == items - 1 && 
		place->pos.unit == units);
}

/* Returns TRUE for non-existent unit of existent item. */
bool_t reiser4_place_right(reiser4_place_t *place) {
	uint32_t items;
	uint32_t units;

	aal_assert("vpf-1292", place != NULL);

	items = reiser4_node_items(place->node);
	
	if (place->pos.item >= items || 
	    place->pos.unit == MAX_UINT32)
	{
		return 0;
	}
	
	if (reiser4_place_fetch(place))
		return 0;
	
	units = reiser4_item_units(place);
	
	return place->pos.unit >= units;
}

void reiser4_place_inc(reiser4_place_t *place, int whole) {
	uint32_t unit;
	uint32_t units;

	aal_assert("umka-2361", place != NULL);
	aal_assert("umka-2364", place->node != NULL);

	unit = whole ? MAX_UINT32 : 0;
		
	if (place->pos.unit == MAX_UINT32)
		place->pos.unit = unit;

	if (reiser4_place_fetch(place))
		return;

	units = reiser4_item_units(place);
	
	if (place->pos.unit >= units - 1) {
		place->pos.item++;
		place->pos.unit = unit;
	} else
		place->pos.unit++;
}

#ifndef ENABLE_MINIMAL
void reiser4_place_dec(reiser4_place_t *place, int whole) {
	aal_assert("umka-2362", place != NULL);
	aal_assert("umka-2365", place->node != NULL);
	
	if (place->pos.unit == 0 || 
	    place->pos.unit == MAX_UINT32) 
	{
		uint32_t units;

		aal_assert("umka-2404",
			   place->pos.item > 0);
		
		place->pos.item--;

		if (reiser4_place_fetch(place))
			return;

		units = reiser4_item_units(place);
		place->pos.unit = units - 1;
	} else
		place->pos.unit--;
	
	if (whole && place->pos.unit == 0)
		place->pos.unit = MAX_UINT32;
}
#endif

/* Initializes all item-related fields */
errno_t reiser4_place_fetch(reiser4_place_t *place) {
	aal_assert("umka-1459", place != NULL);

	return objcall(place->node, fetch, &place->pos, place);
}

/* This function initializes passed @place by specified params */
errno_t reiser4_place_init(reiser4_place_t *place,
			   reiser4_node_t *node, pos_t *pos)
{
	aal_assert("umka-795", place != NULL);
    
	place->node = node;

	if (pos != NULL)
		place->pos = *pos;

	return 0;
}

errno_t reiser4_place_assign(reiser4_place_t *place,
			     reiser4_node_t *node,
			     uint32_t item, uint32_t unit)
{
	pos_t pos = {item, unit};
	
        aal_assert("umka-1730", place != NULL);
	return reiser4_place_init(place, node, &pos);
}

/* Initializes @place and its item related fields */
errno_t reiser4_place_open(reiser4_place_t *place,
			   reiser4_node_t *node, pos_t *pos)
{
	errno_t res;
	
        aal_assert("umka-1435", place != NULL);

	if ((res = reiser4_place_init(place, node, pos)))
		return res;

	return reiser4_place_fetch(place);
}
