/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   librepair/item.c -- common repair item functions. */

#include <repair/librepair.h>

/* Checks if length has been changed, shrink the node if so. */
static errno_t repair_item_check_fini(reiser4_place_t *place, repair_error_t result, 
				      uint32_t old_len, uint8_t mode)
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
			aal_exception_bug("Node (%llu), item (%u), len (%u): Failed "
					  "to shrink the node on (%u) bytes.", 
					  place->node->blk, pos.item, old_len, 
					  old_len - place->item.len);
			return ret;
		}
		
		result = REPAIR_FIXED;
	}
	
	if ((result & REPAIR_FATAL) && mode == REPAIR_REBUILD) {
		aal_exception_error("Node (%llu), item (%u): unrecoverable corruption "
				    "found. Remove item.", place->node->blk, 
				    place->pos.item);
		
		place->pos.unit = ~0ul;
		
		if ((ret = reiser4_node_remove(place->node, &place->pos, 1))) {
			aal_exception_error("Node (%llu), item (%u): failed to remove "
					    "the item.", place->node->blk, 
					    place->pos.item);
			return ret;
		}
		
		result = REPAIR_REMOVED;
	}
	
	return result;
}

/* Calls the item check method to check the item structure and shrink the node if 
   item length has been changed. Returns values are described in repair_error_t, 
   but REPAIR_FIXED. */
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
	aal_assert("vpf-789", mode != REPAIR_CHECK || length == place->item.len);
	aal_assert("vpf-767", length == place->item.len || res != REPAIR_OK);
	
	return repair_item_check_fini(place, res, length, mode);
}

/* Calls the item check_layout method to check the layout of an item and shrink 
   the node if item length has been changed. Returns values are described in 
   repair_error_codes_t, but REPAIR_FIXED. */
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
	aal_assert("vpf-795", mode != REPAIR_CHECK || length == place->item.len);
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

#if 0

errno_t repair_item_handle_ptr(reiser4_place_t *place) {
	reiser4_ptr_hint_t hint;
	rpos_t prev;

	aal_assert("vpf-416", place != NULL, return -1);
	aal_assert("vpf-417", place->node != NULL, return -1);
    
	/* Fetch the pointer from the place. */
	if (plugin_call(place->item.plugin->o.item_ops,
			read, &place->item, &hint, place->pos.unit, 1) != 1)
		return -1;
	if (hint.width == 1 && reiser4_item_extent(place)) {
		/* For one unit extent pointer we can just zero the start block 
		 * number. */
		aal_exception_error("Node (%llu), item (%u), unit (%u): pointer "
				    "(start %llu, count %llu) is zeroed.", 
				    place->node->blk, place->pos.item, 
				    place->pos.unit, hint.ptr, hint.width);

		hint.ptr = 0;

		if (plugin_call(place->item.plugin->o.item_ops,
				write, &place->item, &hint, place->pos.unit, 1))
			return -1;	    
	} else {
		/* For many unit pointers there is no way to figure out what 
		 * is broken - the start block of the width. 
		 * Delete the unit if node pointer. 
		 * Delete the item if extent pointer. */	

		/* Correct position to work with the whole item for extent items. */
		if (reiser4_item_extent(place))
			place->pos.unit = ~0ul;

		aal_exception_error("Node (%llu), item (%u), unit (%u): pointer "
				    "(start %llu, count %llu) is removed.", 
				    place->node->blk, place->pos.item, place->pos.unit,
				    hint.ptr, hint.width);

		repair_place_left_pos_save(place, &prev);
		if (reiser4_node_remove(place->node, &place->pos, 1))
			return -1;		    
		    
		place->pos = prev;
	}

	return 0;
}

/* Blocks pointed by place should not be used in bitmap. 
 * Returns -1 if fatal error; 1 if not used; 0 - ok. */
errno_t repair_item_ptr_unused(reiser4_place_t *place, aux_bitmap_t *bitmap) {
	blk_t next_blk;
	reiser4_ptr_hint_t ptr;
	int res;

	aal_assert("vpf-500", place != NULL, return -1);
	aal_assert("vpf-397", bitmap != NULL, return -1);
	aal_assert("vpf-497", reiser4_item_nodeptr(place) || 
		   reiser4_item_extent(place), return -1);

	if ((res = plugin_call(place->item.plugin->o.item_ops, read, 
			       &place->item, &ptr, place->pos.unit, 1)) != 1)
		return res;

	/* Ptr can be 0 if extent item only. Width cannot be 0. */
	if ((!ptr.ptr && reiser4_item_nodeptr(place)) || !ptr.width) 
		goto error;

	if ((ptr.ptr >= bitmap->total) || (ptr.width >= bitmap->total) || 
	    (ptr.ptr >= bitmap->total - ptr.width)) 
		goto error;
    
	if (!ptr.ptr)
		return 0;

	/* Check that ptr does not point any used block. */
	if (!aux_bitmap_test_region(bitmap, ptr.ptr, ptr.width, 0))
		goto error;
	
	return 0;
    
 error:
	aal_exception_error("Node (%llu), item (%u), unit (%u): %s pointer "
			    "(start %llu, count %llu) points to some already used blocks.", 
			    place->node->blk, place->pos.item, 
			    place->pos.unit, reiser4_item_nodeptr(place) ? "node" : "extent", 
			    ptr.ptr, ptr.width);

	return 1;
}

/* Blocks pointed by place should not be used in bitmap. 
 * Returns -1 if fatal error; 1 if not used; 0 - ok. */
errno_t repair_item_ptr_used_in_format(reiser4_place_t *place, 
				       repair_data_t *data) 
{
	int res;
	blk_t next_blk;
	reiser4_ptr_hint_t ptr;

	aal_assert("vpf-270", place != NULL, return -1);
	aal_assert("vpf-271", data != NULL, return -1);
	aal_assert("vpf-272", data->format != NULL, return -1);
	aal_assert("vpf-496", reiser4_item_nodeptr(place) || reiser4_item_extent(place), 
		   return -1);

	if (plugin_call(place->item.plugin->o.item_ops, read,
			&place->item, place->pos.unit, &ptr, 1))
		return -1;
	
	if ((!ptr.ptr && reiser4_item_nodeptr(place)) || !ptr.width) 
		goto error;
    
	if ((ptr.ptr >= reiser4_format_get_len(data->format)) || 
	    (ptr.width >= reiser4_format_get_len(data->format)) || 
	    (ptr.ptr >= reiser4_format_get_len(data->format) - ptr.width)) 
		goto error;
    
	/* Check if no any formatted block exists after ptr. 
	   FIXME-VITALY: should not depend on filter specific data. */
	if ((next_blk = aux_bitmap_find_marked(
					       repair_filter_data(data)->bm_format, ptr.ptr)) == INVAL_BLK)
		return 0;
    
	if (next_blk >= ptr.ptr && next_blk < ptr.ptr + ptr.width) 
		return 1;

	return 0;
    
 error:
	aal_exception_error("Node (%llu), item (%u), unit(%u): %s pointer "
			    "(start (%llu), count (%llu)) points some reiser4 system blocks.", 
			    place->node->blk, place->pos.item, place->pos.unit,
			    reiser4_item_nodeptr(place) ? "node" : "extent",
			    ptr.ptr, ptr.width);
	
	return 1;
}

/* Blocks pointed by place should not be used in bitmap. 
 * Returns -1 if fatal error; 1 if not used; 0 - ok. */
errno_t repair_item_ptr_unused(reiser4_place_t *place, aux_bitmap_t *bitmap) {
	blk_t next_blk;
	reiser4_ptr_hint_t ptr;
	int res;

	aal_assert("vpf-500", place != NULL, return -1);
	aal_assert("vpf-397", bitmap != NULL, return -1);
	aal_assert("vpf-497", reiser4_item_nodeptr(place) || 
		   reiser4_item_extent(place), return -1);

	if ((res = plugin_call(return -1, place->item.plugin->o.item_ops, read, 
			       &place->item, &ptr, place->pos.unit, 1)) != 1)
		return res;

	/* Ptr can be 0 if extent item only. Width cannot be 0. */
	if ((!ptr.ptr && reiser4_item_nodeptr(place)) || !ptr.width) 
		goto error;

	if ((ptr.ptr >= bitmap->total) || (ptr.width >= bitmap->total) || 
	    (ptr.ptr >= bitmap->total - ptr.width)) 
		goto error;
    
	if (!ptr.ptr)
		return 0;

	/* Check that ptr does not point any used block. */
	if (!aux_bitmap_test_region(bitmap, ptr.ptr, ptr.width, 0))
		goto error;
	
	return 0;
    
 error:
	aal_exception_error("Node (%llu), item (%u), unit (%u): %s pointer "
			    "(start %llu, count %llu) points to some already used blocks.", 
			    place->node->blk, place->pos.item, 
			    place->pos.unit, reiser4_item_nodeptr(place) ? "node" : "extent", 
			    ptr.ptr, ptr.width);

	return 1;
}

errno_t repair_item_handle_ptr(reiser4_place_t *place) {
	reiser4_ptr_hint_t hint;
	reiser4_pos_t prev;

	aal_assert("vpf-416", place != NULL, return -1);
	aal_assert("vpf-417", place->node != NULL, return -1);
    
	/* Fetch the pointer from the place. */
	if (plugin_call(return -1, place->item.plugin->o.item_ops,
			read, &place->item, &hint, place->pos.unit, 1) != 1)
		return -1;
	if (hint.width == 1 && reiser4_item_extent(place)) {
		/* For one unit extent pointer we can just zero the start block 
		 * number. */
		aal_exception_error("Node (%llu), item (%u), unit (%u): pointer "
				    "(start %llu, count %llu) is zeroed.", 
				    place->node->blk, place->pos.item, 
				    place->pos.unit, hint.ptr, hint.width);

		hint.ptr = 0;

		if (plugin_call(return -1, place->item.plugin->o.item_ops,
				update, &place->item, &hint, place->pos.unit, 1))
			return -1;
	} else {
		/* For many unit pointers there is no way to figure out what 
		 * is broken - the start block of the width. 
		 * Delete the unit if node pointer. 
		 * Delete the item if extent pointer. */	

		/* Correct position to work with the whole item for extent items. */
		if (reiser4_item_extent(place))
			place->pos.unit = ~0ul;

		aal_exception_error("Node (%llu), item (%u), unit (%u): pointer "
				    "(start %llu, count %llu) is removed.", 
				    place->node->blk, place->pos.item, place->pos.unit,
				    hint.ptr, hint.width);

		repair_place_left_pos_save(place, &prev);
		if (reiser4_node_remove(place->node, &place->pos))
			return -1;		    
		    
		place->pos = prev;
	}

	return 0;
}

#endif

