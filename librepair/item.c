/* Copyright 2001-2005 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/item.c -- common repair item functions. */

#include <repair/librepair.h>

/* Checks if length has been changed, shrink the node if so. */
static errno_t repair_item_check_fini(reiser4_place_t *place,
				      repair_hint_t *hint)
{
	pos_t pos;
	
	aal_assert("vpf-768", place->len >= hint->len);
	
	if (place->len == hint->len)
		return RE_FATAL;

	pos = place->pos;
	pos.unit = 0;

	return reiser4_node_shrink(place->node, &pos, hint->len, 1);
}

/* Calls the item check method to check the item structure and shrink the 
   node if item length has been changed. Returns values are described in 
   repair_error_t. */
errno_t repair_item_check_struct(reiser4_place_t *place, uint8_t mode) {
	repair_hint_t hint;
	errno_t res;
	
	aal_assert("vpf-791", place != NULL);
	aal_assert("vpf-792", place->node != NULL);
	
	if (!place->plug->o.item_ops->repair->check_struct)
		return 0;
	
	aal_memset(&hint, 0, sizeof(hint));
	hint.mode = mode;
	
	if ((res = plug_call(place->plug->o.item_ops->repair,
			     check_struct, place, &hint)))
		return res;
	
	if (!hint.len)
		return 0;
	
	return repair_item_check_fini(place, &hint);
}

/* Calls the item check_layout method to check the layout of an item and 
   shrink the node if item length has been changed. Returns values are 
   described in repair_error_codes_t. */
errno_t repair_item_check_layout(reiser4_place_t *place, region_func_t func, 
				 void *data, uint8_t mode) 
{
	repair_hint_t hint;
	errno_t res;
	
	aal_assert("vpf-793", place != NULL);
	aal_assert("vpf-794", place->node != NULL);
	
	if (!place->plug->o.item_ops->repair->check_layout)
		return 0;
	
	aal_memset(&hint, 0, sizeof(hint));
	hint.mode = mode;
	
	if ((res = plug_call(place->plug->o.item_ops->repair, check_layout, 
			     place, &hint, func, data)) < 0)
		return res;
	
	if (!hint.len)
		return 0;
	
	return repair_item_check_fini(place, &hint);
}

/* Prints passed @place into passed @buff */
void repair_item_print(reiser4_place_t *place, aal_stream_t *stream) {
	aal_assert("umka-1297", place != NULL);
	aal_assert("umka-1550", stream != NULL);
	aal_assert("umka-1449", place->plug != NULL);

	if (!place->plug->o.item_ops->debug->print)
		return;
	
	plug_call(place->plug->o.item_ops->debug, print,
		  place, stream, 0);
}

