/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/item.c -- common repair item functions. */

#include <repair/librepair.h>

/* Checks if length has been changed, shrink the node if so. */
static errno_t repair_item_check_fini(reiser4_place_t *place,
				      repair_error_t result,
				      uint32_t old_len)
{
	errno_t res;
	pos_t pos;
	
	if (place->len == 0)
		return RE_FATAL;
	
	if (old_len == place->len || repair_error_exists(result))
		return result;
	
	aal_assert("vpf-768", old_len > place->len);

	pos = place->pos;
	pos.unit = 0;

	if ((res = reiser4_node_shrink(place->node, &pos, 
				       old_len - place->len, 1)))
		return res;

	return 0;
}

/* Calls the item check method to check the item structure and shrink the 
   node if item length has been changed. Returns values are described in 
   repair_error_t. */
errno_t repair_item_check_struct(reiser4_place_t *place, uint8_t mode) {
	uint32_t length;
	errno_t res;
	
	aal_assert("vpf-791", place != NULL);
	aal_assert("vpf-792", place->node != NULL);
	
	if (!place->plug->o.item_ops->check_struct)
		return 0;
	
	length = place->len;
	
	res = place->plug->o.item_ops->check_struct((place_t *)place, mode);
	
	if (res < 0)
		return res;
	
	repair_error_check(res, mode);
	
	aal_assert("vpf-789", mode != RM_CHECK || 
			      length == place->len);
	
	aal_assert("vpf-767", length == place->len || res);
	
	return repair_item_check_fini(place, res, length);
}

/* Calls the item check_layout method to check the layout of an item and 
   shrink the node if item length has been changed. Returns values are 
   described in repair_error_codes_t. */
errno_t repair_item_check_layout(reiser4_place_t *place, region_func_t func, 
				 void *data, uint8_t mode) 
{
	uint32_t length;
	errno_t res;
	
	aal_assert("vpf-793", place != NULL);
	aal_assert("vpf-794", place->node != NULL);
	
	if (!place->plug->o.item_ops->check_layout)
		return 0;
	
	length = place->len;
	
	res = place->plug->o.item_ops->check_layout((place_t *)place, func,
						    data, mode);
	
	repair_error_check(res, mode);
	aal_assert("vpf-795", mode != RM_CHECK || 
			      length == place->len);
	aal_assert("vpf-796", length == place->len || res);
	
	return repair_item_check_fini(place, res, length);
}

errno_t repair_item_estimate_merge(reiser4_place_t *dst, 
				   reiser4_place_t *src,
				   merge_hint_t *hint)
{
	aal_assert("vpf-952", dst  != NULL);
	aal_assert("vpf-953", src  != NULL);
	aal_assert("vpf-954", hint != NULL);
	aal_assert("vpf-955", dst->plug != NULL);
	aal_assert("vpf-956", src->plug != NULL);
	
	return plug_call(src->plug->o.item_ops, estimate_merge,
			 (place_t *)dst, (place_t *)src, hint);
}

void repair_item_set_flag(reiser4_place_t *place, uint16_t flag) {
	aal_assert("vpf-1041", place != NULL);
	aal_assert("vpf-1111", place->node != NULL);
	
	plug_call(place->node->entity->plug->o.node_ops, set_flag, 
		  place->node->entity, place->pos.item, flag);
}

void repair_item_clear_flag(reiser4_place_t *place, uint16_t flag) {
	aal_assert("vpf-1042", place != NULL);
	aal_assert("vpf-1112", place->node != NULL);
	
	plug_call(place->node->entity->plug->o.node_ops, clear_flag, 
		  place->node->entity, place->pos.item, flag);
}

bool_t repair_item_test_flag(reiser4_place_t *place, uint16_t flag) {
	aal_assert("vpf-1043", place != NULL);
	aal_assert("vpf-1113", place->node != NULL);
	
	return plug_call(place->node->entity->plug->o.node_ops, test_flag, 
			 place->node->entity, place->pos.item, flag);
}

