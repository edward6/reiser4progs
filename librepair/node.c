/* librepair/node.c - methods are needed for node recovery.
   
   Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING. */

#include <repair/librepair.h>

/* Opens the node if it has correct mkid stamp. */
reiser4_node_t *repair_node_open(reiser4_fs_t *fs, blk_t blk) {
	uint32_t blocksize;
	reiser4_node_t *node;
	
	aal_assert("vpf-708", fs != NULL);
	
	blocksize = reiser4_master_blksize(fs->master);
	
	if ((node = reiser4_node_open(fs->device, blocksize, blk)) == NULL)
		return NULL;
	
	if (reiser4_format_get_stamp(fs->format) != reiser4_node_get_mstamp(node))
		goto error_node_free;
	
	return node;
	
 error_node_free:
	reiser4_node_close(node);
	return NULL;
}

/* Checks all the items of the node. */
static errno_t repair_node_items_check(reiser4_node_t *node, uint8_t mode) {
	reiser4_place_t place;
	pos_t *pos = &place.pos;
	uint32_t count;
	int32_t len;
	errno_t ret, res = REPAIR_OK;
	
	aal_assert("vpf-229", node != NULL);
	aal_assert("vpf-230", node->entity != NULL);
	aal_assert("vpf-231", node->entity->plugin != NULL);
	
	place.node = node;
	count = reiser4_node_items(node);
	
	for (pos->item = 0; pos->item < count; pos->item++) {
		pos->unit = ~0ul;
		
		/* Open the item, checking its plugin id. */
		if (reiser4_place_realize(&place)) {
			aal_exception_error("Node (%llu): Failed to open the item "
					    "(%u). %s", node->blk, pos->item, 
					    mode == REPAIR_REBUILD ? "Removed." : "");
			
			if (mode == REPAIR_REBUILD) {
				if ((ret = reiser4_node_remove(node, pos, 1))) {
					aal_exception_bug("Node (%llu): Failed to "
							  "delete the item (%d).", 
							  node->blk, pos->item);
					return ret;
				}
				
				pos->item--;
				count = reiser4_node_items(node);
				res |= REPAIR_FIXED;
			} else {
				res |= REPAIR_FATAL;
			}
			
			continue;
		}
		
		/* Check that the item is legal for this node. If not, it will be 
		   deleted in update traverse callback method. */
		if (!repair_tree_legal_level(place.item.plugin->h.group, 
					     reiser4_node_get_level(node)))
		{
			aal_exception_error("Node (%llu): Node level (%u) does not "
					    "match to the item type (%s).", node->blk,
					    reiser4_node_get_level(node),
					    place.item.plugin->h.label);
			
			/* FIXME-VITALY: smth should be done here later. */
			res |= REPAIR_FATAL;
		}
		
		/* Check the item structure. */
		if ((ret = repair_item_check_struct(&place, mode)) < 0)
			return ret;
		
		res |= ret;
		
		if (res & REPAIR_REMOVED) {
			pos->item--;
			count = reiser4_node_items(node);
			res &= ~REPAIR_REMOVED;
			res |= REPAIR_FIXED;
		}
	}
	
	return res;
}

/* Sets @key to the left delimiting key of the node kept in the parent. */
static errno_t repair_node_ld_key_fetch(reiser4_node_t *node, 
					reiser4_key_t *ld_key) 
{
	errno_t res;
	
	aal_assert("vpf-501", node != NULL);
	aal_assert("vpf-344", ld_key != NULL);
	
	if (node->parent.node != NULL) {
		if ((res = reiser4_place_realize(&node->parent)))
			return res;
		
		if ((res = reiser4_key_assign(ld_key, &node->parent.item.key)))
			return res;
	} else {
		reiser4_key_guess(ld_key);
		reiser4_key_minimal(ld_key);
	}
	
	return 0;
}

/* Updates the left delimiting key of the node kept in the parent.
   
   FIXME-VITALY: This must be recursive method. + there is smth similar in 
   libreiser4 already.*/
static errno_t repair_node_ld_key_update(reiser4_node_t *node, 
					 reiser4_key_t *ld_key) 
{
	aal_assert("vpf-467", node != NULL);
	aal_assert("vpf-468", ld_key != NULL);
	aal_assert("vpf-469", ld_key->plugin != NULL);
	
	if (node->parent.node == NULL)
		return 0;
	
	return reiser4_item_set_key(&node->parent, ld_key);
}

/* Sets to the @key the right delimiting key of the node kept in the parent. */
errno_t repair_node_rd_key(reiser4_node_t *node, reiser4_key_t *rd_key) {
	reiser4_place_t place;
	errno_t ret;
	
	aal_assert("vpf-502", node != NULL);
	aal_assert("vpf-347", rd_key != NULL);
	
	if (node->parent.node != NULL) {
		/* Take the right delimiting key from the parent. */
		
		if ((ret = reiser4_node_pbc(node, NULL)))
			return ret;
		
		place = node->parent;
		
		/* If this is the last position in the parent, call the method 
		   recursevely for the parent. Get the right delimiting key 
		   otherwise. */		
		if ((reiser4_node_items(node->parent.node) == place.pos.item + 1) && 
		    (reiser4_item_units(&place) == place.pos.unit + 1 || 
		     place.pos.unit == ~0ul)) 
		{
			if ((ret = repair_node_rd_key(node->parent.node, rd_key)))
				return ret;
		} else {
			place.pos.item++;
			place.pos.unit = 0;
			
			if ((ret = reiser4_place_realize(&place)))
				return ret;
			
			if ((ret = reiser4_key_assign(rd_key, &place.item.key)))
				return ret;
		}
	} else {
		reiser4_key_guess(rd_key);
		reiser4_key_maximal(rd_key);
	}
	
	return 0;
}

/* Checks the delimiting keys of the node kept in the parent. 
   
   FIXME-VITALY: this is not node code, move it to tree code. */
errno_t repair_node_dkeys_check(reiser4_node_t *node, uint8_t mode) {
	reiser4_place_t place;
	reiser4_key_t key, d_key;
	pos_t *pos = &place.pos;
	int res;
	
	aal_assert("vpf-248", node != NULL);
	aal_assert("vpf-249", node->entity != NULL);
	aal_assert("vpf-250", node->entity->plugin != NULL);
	
	if ((res = repair_node_ld_key_fetch(node, &d_key))) {
		aal_exception_error("Node (%llu): Failed to get the left "
				    "delimiting key.", node->blk);
		return res;
	}
	
	place.pos.item = 0; 
	place.pos.unit = ~0ul;
	place.node = node;
	
	if ((res = reiser4_place_realize(&place)))
		return res;
	
	res = reiser4_key_compare(&d_key, &place.item.key);
	
	/* Left delimiting key should match the left key in the node. */
	if (res > 0) {
		/* The left delimiting key is much then the left key in the node - 
		   not legal */
		aal_exception_error("Node (%llu): The first key %k is not equal "
				    "to the left delimiting key %k.", node->blk,
				    &place.item.key, &d_key);
		return -ESTRUCT;
	} else if (res < 0) {
		/* It is legal to have the left key in the node much then its 
		   left delimiting key - due to removing some items from the 
		   node, for example. Fix the delemiting key if we have parent. */
		if (node->parent.node != NULL) {
			aal_exception_error("Node (%llu): The left delimiting key "
					    "%k in the parent node (%llu), pos "
					    "(%u/%u) mismatch the first key %k in "
					    "the node. %s", node->blk, &place.item.key,
					    node->parent.node->blk, place.pos.item, 
					    place.pos.unit, &d_key, 
					    mode == REPAIR_REBUILD ? 
					    "Left delimiting key is fixed." : "");
			
			if (mode == REPAIR_REBUILD) {
				if ((res = repair_node_ld_key_update(node, &d_key)))
					return res;
			}
		}
	}
	
	if ((res = repair_node_rd_key(node, &d_key))) {
		aal_exception_error("Node (%llu): Failed to get the right delimiting "
				    "key.", node->blk);
		return res;
	}
	
	pos->item = reiser4_node_items(node) - 1;
	
	if ((res = reiser4_place_realize(&place))) {
		aal_exception_error("Node (%llu): Failed to open the item (%llu).",
				    node->blk, pos->item);
		return res;
	}
	
	if ((res = reiser4_item_maxreal_key(&place, &key))) {
		aal_exception_error("Node (%llu): Failed to get the max real key of "
				    "the last item.", node->blk);
		return res;
	}
	
	if (reiser4_key_compare(&key, &d_key) >= 0) {
		aal_exception_error("Node (%llu): The last key %k in the node is "
				    "greater then the right delimiting key %k.", 
				    node->blk, &key, &d_key);
		return -ESTRUCT;
	}
	
	return 0;
}

/* Checks the set of keys of the node. */
static errno_t repair_node_keys_check(reiser4_node_t *node, uint8_t mode) {
	reiser4_place_t place;
	reiser4_key_t key, prev_key;
	pos_t *pos = &place.pos;
	uint32_t count;
	errno_t res;
	
	aal_assert("vpf-258", node != NULL);
	
	aal_memset(&place, 0, sizeof(place));
	
	place.node = node;
	place.pos.unit = ~0ul;
	count = reiser4_node_items(node);
	
	for (pos->item = 0; pos->item < count; pos->item++) {
		if ((res = reiser4_place_realize(&place)))
			return res;
		
		if ((res = reiser4_key_assign(&key, &place.item.key))) {
			aal_exception_error("Node (%llu): Failed to get the key of "
					    "the item (%u).", node->blk, pos->item);
			return res;
		}
		
		if (reiser4_key_valid(&key)) {
			aal_exception_error("Node (%llu): The key %k of the item "
					    "(%u) is not valid. Item removed.", 
					    node->blk, &key, pos->item);
			
			if ((res = reiser4_node_remove(node, pos, 1))) {
				aal_exception_bug("Node (%llu): Failed to delete the "
						  "item (%d).", node->blk, pos->item);
				return res;
			}
			
			pos->item--;
			count = reiser4_node_items(node);
			
			continue;
		}
		
		if (pos->item) {
			res = reiser4_key_compare(&prev_key, &key);
			
			if ((res == 0 && 
			     (reiser4_key_get_type(&key) != KEY_FILENAME_TYPE ||
			      reiser4_key_get_type(&prev_key) != KEY_FILENAME_TYPE)) ||
			    (res > 0)) 
			{
				/* FIXME-VITALY: Which part does put the rule that 
				   neighbour keys could be equal? */
				aal_exception_error("Node (%llu), items (%u) and "
						    "(%u): Wrong order of keys.", 
						    node->blk, pos->item - 1, 
						    pos->item);
				
				return REPAIR_FATAL;
			}
		}
		
		prev_key = key;
	}
	
	return REPAIR_OK;
}

/*  Checks the node content. Returns values according to repair_error_codes_t. */
errno_t repair_node_check(reiser4_node_t *node, uint8_t mode) {
	errno_t res = REPAIR_OK;
	
	aal_assert("vpf-494", node != NULL);
	aal_assert("vpf-193", node->entity != NULL);    
	aal_assert("vpf-220", node->entity->plugin != NULL);
	
	res |= plugin_call(node->entity->plugin->o.node_ops, check, 
			   node->entity, mode);
	
	if (repair_error_fatal(res))
		return res;
	
	res |= repair_node_keys_check(node, mode);
	
	if (repair_error_fatal(res))
		return res;
	
	res |= repair_node_items_check(node, mode);
	
	if (repair_error_fatal(res))
		return res;    
	
	if (res & REPAIR_FIXED) {
		reiser4_node_mkdirty(node);
		res &= ~REPAIR_FIXED;
	}
	
	return res;
}

/* Traverse through all items of the gived node. */
errno_t repair_node_traverse(reiser4_node_t *node, traverse_node_func_t func, 
			     void *data)
{
	reiser4_place_t place;
	pos_t *pos = &place.pos;
	uint32_t items;
	errno_t res;
	
	aal_assert("vpf-744", node != NULL);
	
	pos->unit = ~0ul;
	
	for (pos->item = 0; pos->item < reiser4_node_items(node); pos->item++) {
		if ((res = reiser4_place_open(&place, node, pos))) {
			aal_exception_error("Node (%llu), item (%u): failed to open "
					    "the item by its place.", node->blk, 
					    pos->item);
			return res;
		}
		
		if ((res = func(&place, data)))
			return res;
	}
	
	return 0;
}

void repair_node_print(reiser4_node_t *node, uint32_t start, uint32_t count, 
		       uint16_t options) 
{
	aal_stream_t stream;
	
	if (node == NULL)
		return;
	
	aal_stream_init(&stream);
	
	plugin_call(node->entity->plugin->o.node_ops, print, node->entity, &stream, 
		    start, count, options);
	
	printf(stream.data);
	fflush(stdout);
	aal_stream_fini(&stream);
}

errno_t repair_node_copy(reiser4_node_t *dst, pos_t *dst_pos, 
			 reiser4_node_t *src, pos_t *src_pos, 
			 copy_hint_t *hint) 
{
	aal_assert("vpf-961", dst != NULL);
	aal_assert("vpf-962", src != NULL);
	aal_assert("vpf-964", dst->entity->plugin->h.id == 
		   src->entity->plugin->h.id);
	aal_assert("vpf-967", dst_pos != NULL);
	aal_assert("vpf-968", src_pos != NULL);
    
	return plugin_call(dst->entity->plugin->o.node_ops, copy, dst->entity, 
			   dst_pos, src->entity, src_pos, hint);
}

void repair_node_set_flag(reiser4_node_t *node, uint32_t pos, uint16_t flag) {
	aal_assert("vpf-1041", node != NULL);
	
	plugin_call(node->entity->plugin->o.node_ops, set_flag, 
		    node->entity, pos, flag);
}

void repair_node_clear_flag(reiser4_node_t *node, uint32_t pos, uint16_t flag) {
	aal_assert("vpf-1042", node != NULL);
	
	plugin_call(node->entity->plugin->o.node_ops, clear_flag, 
		    node->entity, pos, flag);
}

bool_t repair_node_test_flag(reiser4_node_t *node, uint32_t pos, uint16_t flag) {
	aal_assert("vpf-1043", node != NULL);
	
	return plugin_call(node->entity->plugin->o.node_ops, test_flag, 
			   node->entity, pos, flag);
}

