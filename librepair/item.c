/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/item.c -- common repair item functions. */

#include <repair/librepair.h>

/* Checks if length has been changed, shrink the node if so. */
static errno_t repair_item_check_fini(reiser4_place_t *place,
				      repair_error_t result,
				      uint32_t old_len,
				      uint8_t mode)
{
	errno_t ret;
	pos_t pos;
	
	if (place->item.len == 0)
		result = REPAIR_FATAL;
	
	if (old_len != place->item.len && !repair_error_exists(result)) {
		aal_assert("vpf-768", old_len > place->item.len);
		
		pos = place->pos;
		pos.unit = 0;
		
		ret = reiser4_node_shrink(place->node, &pos, 
					  old_len - place->item.len, 1);
		
		if (ret) {
			aal_exception_bug("Node (%llu), item (%u), len (%u): "
					  "Failed to shrink the node on (%u) "
					  "bytes.", place->node->number, 
					  pos.item, old_len, 
					  old_len - place->item.len);
			return ret;
		}
		
		result = REPAIR_FIXED;
	}
	
	if ((result & REPAIR_FATAL) && mode == REPAIR_REBUILD) {
		aal_exception_error("Node (%llu), item (%u): unrecoverable "
				    "corruption found. Remove item.", 
				    place->node->number, place->pos.item);
		
		place->pos.unit = MAX_UINT32;
		
		if ((ret = reiser4_node_remove(place->node, &place->pos, 1))) {
			aal_exception_error("Node (%llu), item (%u): failed to "
					    "remove the item.",
					    place->node->number, 
					    place->pos.item);
			return ret;
		}
		
		result = REPAIR_REMOVED;
	}
	
	return result;
}

/* Calls the item check method to check the item structure and shrink the 
   node if item length has been changed. Returns values are described in 
   repair_error_t, but REPAIR_FIXED. */
errno_t repair_item_check_struct(reiser4_place_t *place, uint8_t mode) {
	uint32_t length;
	errno_t res;
	
	aal_assert("vpf-791", place != NULL);
	aal_assert("vpf-792", place->node != NULL);
	
	if (!place->item.plugin->o.item_ops->check_struct)
		return 0;
	
	length = place->item.len;
	
	res = place->item.plugin->o.item_ops->check_struct(&place->item, mode);
	
	if (res < 0)
		return res;
	
	repair_error_check(res, mode);
	
	
	aal_assert("vpf-789", mode != REPAIR_CHECK || 
			      length == place->item.len);
	
	aal_assert("vpf-767", length == place->item.len || res != REPAIR_OK);
	
	return repair_item_check_fini(place, res, length, mode);
}

/* Calls the item check_layout method to check the layout of an item and 
   shrink the node if item length has been changed. Returns values are 
   described in repair_error_codes_t, but REPAIR_FIXED. */
errno_t repair_item_check_layout(reiser4_place_t *place, region_func_t func, 
				 void *data, uint8_t mode) 
{
	uint32_t length;
	errno_t res;
	
	aal_assert("vpf-793", place != NULL);
	aal_assert("vpf-794", place->node != NULL);
	
	if (!place->item.plugin->o.item_ops->check_layout)
		return 0;
	
	length = place->item.len;
	
	res = place->item.plugin->o.item_ops->check_layout(&place->item, func,
							   data, mode);
	
	repair_error_check(res, mode);
	aal_assert("vpf-795", mode != REPAIR_CHECK || 
			      length == place->item.len);
	aal_assert("vpf-796", length == place->item.len || res != REPAIR_OK);
	
	return repair_item_check_fini(place, res, length, mode);
}

errno_t repair_item_estimate_copy(reiser4_place_t *dst, reiser4_place_t *src,
				  copy_hint_t *hint)
{
	aal_assert("vpf-952", dst  != NULL);
	aal_assert("vpf-953", src  != NULL);
	aal_assert("vpf-954", hint != NULL);
	aal_assert("vpf-955", dst->item.plugin != NULL);
	aal_assert("vpf-956", src->item.plugin != NULL);
	
	return plugin_call(src->item.plugin->o.item_ops, estimate_copy,
			   &dst->item, dst->pos.unit, &src->item, 
			   src->pos.unit, hint);
}

void repair_item_set_flag(reiser4_place_t *place, uint16_t flag) {
	aal_assert("vpf-1041", place != NULL);
	aal_assert("vpf-1111", place->node != NULL);
	
	plugin_call(place->node->entity->plugin->o.node_ops, set_flag, 
		    place->node->entity, place->pos.item, flag);
}

void repair_item_clear_flag(reiser4_place_t *place, uint16_t flag) {
	aal_assert("vpf-1042", place != NULL);
	aal_assert("vpf-1112", place->node != NULL);
	
	plugin_call(place->node->entity->plugin->o.node_ops, clear_flag, 
		    place->node->entity, place->pos.item, flag);
}

bool_t repair_item_test_flag(reiser4_place_t *place, uint16_t flag) {
	aal_assert("vpf-1043", place != NULL);
	aal_assert("vpf-1113", place->node != NULL);
	
	return plugin_call(place->node->entity->plugin->o.node_ops, test_flag, 
			   place->node->entity, place->pos.item, flag);
}

