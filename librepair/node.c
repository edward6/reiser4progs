/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/node.c - methods are needed for node recovery. */

#include <repair/librepair.h>

/* Opens the node if it has correct mkid stamp. */
node_t *repair_node_open(reiser4_tree_t *tree, blk_t blk, bool_t check) {
	node_t *node;
	
	aal_assert("vpf-708", tree != NULL);
	
	if (!(node = reiser4_node_open(tree, blk)))
		return NULL;
	
	/* Extra checks are needed. */
	if (check && reiser4_format_get_stamp(tree->fs->format) != 
	    reiser4_node_get_mstamp(node))
	{
		goto error_node_free;
	}
	
	return node;
	
 error_node_free:
	reiser4_node_close(node);
	return NULL;
}

/* Checks all the items of the node. */
static errno_t repair_node_items_check(node_t *node, uint8_t mode) {
	place_t place;
	errno_t res = 0;
	uint32_t count;
	pos_t *pos;
	
	aal_assert("vpf-229", node != NULL);
	aal_assert("vpf-230", node->entity != NULL);
	aal_assert("vpf-231", node->entity->plug != NULL);
	
	pos = &place.pos;
	place.node = node;
	count = reiser4_node_items(node);
	
	for (pos->item = 0; pos->item < count; pos->item++) {
		errno_t ret;
		
		pos->unit = MAX_UINT32;
		
		/* Open the item, checking its plugin id. */
		if (reiser4_place_fetch(&place)) {
			trans_hint_t hint;
			
			aal_exception_error("Node (%llu): Failed to open the "
					    "item (%u). %s", node_blocknr(node),
					    pos->item, mode == RM_BUILD ? 
					    "Removed." : "");
			
			if (mode != RM_BUILD) {
				res |= RE_FATAL;
				continue;
			}

			hint.count = 1;

			if ((res |= reiser4_node_remove(node, pos, &hint)) < 0)
				return res;

			pos->item--;
			count = reiser4_node_items(node);
		} 
		
		/* Check that the item is legal for this node. If not, it 
		   is not recoverable corruption for now. FIXME-VITALY: 
		   how to recover valuable data from here? */
		if (!repair_tree_legal_level(place.plug->id.group, 
					     reiser4_node_get_level(node)))
		{
			aal_exception_error("Node (%llu): Node level (%u) does "
					    "not match to the item type (%s).",
					    node_blocknr(node), 
					    reiser4_node_get_level(node),
					    place.plug->label);
			
			return RE_FATAL;
		}
		
		/* Check the item structure. */
		if ((ret = repair_item_check_struct(&place, mode)) < 0)
			return ret;

		/* Remove the item if fatal error. */
		if ((ret & RE_FATAL) && (mode == RM_BUILD)) {
			trans_hint_t hint;

			aal_exception_error("Node (%llu), item (%u): broken "
					    "item occured, Remove it.",
					    node_blocknr(node), pos->item);

			hint.count = 1;

			if ((ret = reiser4_node_remove(node, pos, &hint)))
				return ret;
			
			pos->item--;
			count = reiser4_node_items(node);
			ret &= ~RE_FATAL;
		}
		
		res |= ret;
	}
	
	return res;
}

/* Checks the set of keys of the node. */
static errno_t repair_node_keys_check(node_t *node, uint8_t mode) {
	reiser4_key_t key, prev_key;
	errno_t res, result = 0;
	place_t place;
	pos_t *pos = &place.pos;
	uint32_t count;
	
	aal_assert("vpf-258", node != NULL);
	
	aal_memset(&place, 0, sizeof(place));
	
	place.node = node;
	place.pos.unit = MAX_UINT32;
	count = reiser4_node_items(node);
	
	for (pos->item = 0; pos->item < count; pos->item++) {
		if ((res = reiser4_place_fetch(&place)))
			return res;
		
		if ((res = reiser4_key_assign(&key, &place.key))) {
			aal_exception_error("Node (%llu): Failed to get the "
					    "key of the item (%u).",
					    node_blocknr(node), pos->item);
			return res;
		}
		
		if ((res = repair_key_check_struct(&key)) < 0)
			return res;
		
		if (res) {
			/* Key has some corruptions and cannot be recovered. */
			trans_hint_t hint;
			
			aal_exception_error("Node (%llu): The key [%s] of the "
					    "item (%u) is broken.%s", 
					    node_blocknr(node),
					    reiser4_print_key(&place.key, PO_DEFAULT),
					    pos->item, mode == RM_BUILD ?
					    " Removed." : "");
			if (mode != RM_BUILD)
				return RE_FATAL;
			
			if ((res = reiser4_node_remove(node, pos, &hint)))
				return res;

			pos->item--;
			count = reiser4_node_items(node);
			
			continue;
		} else if (reiser4_key_compfull(&key, &place.key)) {
			/* Key has been fixed. */
			aal_exception_error("Node (%llu): The key [%s] of the "
					    "item (%u) is broken. %s [%s].", 
					    node_blocknr(node),
					    reiser4_print_key(&place.key, PO_DEFAULT),
					    pos->item, (res && mode == RM_CHECK)
					    ? "Should be" : "Fixed to", 
					    reiser4_print_key(&key, PO_DEFAULT));
			
			if (mode == RM_CHECK)
				result = RE_FIXABLE;
			else {
				reiser4_node_update_key(node, pos, &key);
				reiser4_node_mkdirty(node);
			}
		}
		
		if ((res = reiser4_item_maxreal_key(&place, &key)))
			return res;
		
		if (pos->item) {
			if (reiser4_key_compfull(&prev_key, &key) >= 0) {
				aal_exception_error("Node (%llu), items (%u) "
						    "and (%u): Wrong order of "
						    "keys.", node_blocknr(node), 
						    pos->item - 1, pos->item);
				
				return RE_FATAL;
			}
		}
		
		prev_key = key;
	}
	
	return result;
}

/*  Checks the node content.
    Returns values according to repair_error_codes_t. */
errno_t repair_node_check_struct(node_t *node, uint8_t mode) {
	uint8_t level;
	errno_t res;
	
	aal_assert("vpf-494", node != NULL);
	aal_assert("vpf-193", node->entity != NULL);    
	aal_assert("vpf-220", node->entity->plug != NULL);
	
	level = reiser4_node_get_level(node);
	
	/* Level of the node must be > 0 */
	if (!level) {
		aal_exception_error("Node (%llu): illegal level found (%u).", 
				    node_blocknr(node), level);
		return RE_FATAL;
	}

	res = plug_call(node->entity->plug->o.node_ops, check_struct, 
			node->entity, mode);
	
	if (repair_error_fatal(res))
		return res;
	
	res |= repair_node_keys_check(node, mode);
	
	if (repair_error_fatal(res))
		return res;
	
	res |= repair_node_items_check(node, mode);
	
	if (repair_error_fatal(res))
		return res;    
	
	return res;
}

/* Traverse through all items of the gived node. */
errno_t repair_node_traverse(node_t *node, node_func_t func, 
			     void *data)
{
	place_t place;
	pos_t *pos = &place.pos;
	errno_t res;
	
	aal_assert("vpf-744", node != NULL);
	
	pos->unit = MAX_UINT32;
	
	for (pos->item = 0; pos->item < reiser4_node_items(node); pos->item++) {
		if ((res = reiser4_place_open(&place, node, pos))) {
			aal_exception_error("Node (%llu), item (%u): failed to "
					    "open the item by its place.", 
					    node_blocknr(node), pos->item);
			return res;
		}
		
		if ((res = func(&place, data)))
			return res;
	}
	
	return 0;
}

errno_t repair_node_clear_flags(node_t *node) {
	uint32_t count;
	place_t place;
	pos_t *pos;
	
	aal_assert("vpf-1401", node != NULL);
	aal_assert("vpf-1402", node->entity != NULL);
	aal_assert("vpf-1403", node->entity->plug != NULL);

	place.node = node;
	count = reiser4_node_items(node);
	
	pos = &place.pos;
	pos->unit = MAX_UINT32;
	
	for (pos->item = 0; pos->item < count; pos->item++) {
		if (reiser4_place_fetch(&place))
			return -EINVAL;
		
		repair_item_clear_flag(&place, MAX_UINT16);
	}

	return 0;
}
