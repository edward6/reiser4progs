/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/node.c - methods are needed for node recovery. */

#include <repair/librepair.h>

/* Opens the node if it has correct mkid stamp. */
reiser4_node_t *repair_node_open(reiser4_fs_t *fs, blk_t blk) {
	rid_t pid;
	uint32_t blocksize;
	reiser4_node_t *node;
	
	aal_assert("vpf-708", fs != NULL);
	
	blocksize = reiser4_master_blksize(fs->master);

	/* FIXME-UMKA->VITALY: Here should be used real node id. It can be
	 * obtained from root node. */
	pid = NODE_LARGE_ID;

	aal_assert("umka-2355", fs->tree != NULL);
	
	node = reiser4_node_open(fs->device, blocksize, blk, pid,
				 fs->tree->key.plug);
	if (node == NULL)
		return NULL;
	
	if (reiser4_format_get_stamp(fs->format) != 
	    reiser4_node_get_mstamp(node))
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
	errno_t ret, res = RE_OK;
	
	aal_assert("vpf-229", node != NULL);
	aal_assert("vpf-230", node->entity != NULL);
	aal_assert("vpf-231", node->entity->plug != NULL);
	
	place.node = node;
	count = reiser4_node_items(node);
	
	for (pos->item = 0; pos->item < count; pos->item++) {
		pos->unit = MAX_UINT32;
		
		/* Open the item, checking its plugin id. */
		if (reiser4_place_fetch(&place)) {
			aal_exception_error("Node (%llu): Failed to open the "
					    "item (%u). %s", node->number, 
					    pos->item, mode == RM_BUILD ? 
					    "Removed." : "");
			
			if (mode == RM_BUILD) {
				if ((ret = reiser4_node_remove(node, pos, 1))) {
					aal_exception_bug("Node (%llu): Failed to "
							  "delete the item (%d).", 
							  node->number, pos->item);
					return ret;
				}
				
				pos->item--;
				count = reiser4_node_items(node);
				res |= RE_FIXED;
			} else {
				res |= RE_FATAL;
			}
			
			continue;
		}
		
		/* Check that the item is legal for this node. If not, it 
		   will be deleted in update traverse callback method. */
		if (!repair_tree_legal_level(place.plug->id.group, 
					     reiser4_node_get_level(node)))
		{
			aal_exception_error("Node (%llu): Node level (%u) does "
					    "not match to the item type (%s).",
					    node->number, 
					    reiser4_node_get_level(node),
					    place.plug->label);
			
			/* FIXME-VITALY: smth should be done here later. */
			res |= RE_FATAL;
		}
		
		/* Check the item structure. */
		if ((ret = repair_item_check_struct(&place, mode)) < 0)
			return ret;
		
		res |= ret;
		
		if (res & RE_REMOVED) {
			pos->item--;
			count = reiser4_node_items(node);
			res &= ~RE_REMOVED;
			res |= RE_FIXED;
		}
	}
	
	return res;
}

/* Sets @ld_key to the left delimiting key of the node kept in the parent. */
static errno_t repair_node_ld_key_fetch(reiser4_node_t *node, 
					reiser4_key_t *ld_key) 
{
	errno_t res;
	
	aal_assert("vpf-501", node != NULL);
	aal_assert("vpf-344", ld_key != NULL);
	
	if (node->p.node != NULL) {
		if ((res = reiser4_place_fetch(&node->p)))
			return res;
		
		if ((res = reiser4_key_assign(ld_key, &node->p.key)))
			return res;
	} else {
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
	aal_assert("vpf-469", ld_key->plug != NULL);

	return reiser4_node_ukey(node->p.node, &node->p.pos, ld_key);
}

/* Sets to the @key the right delimiting key of the node kept in the parent. */
errno_t repair_node_rd_key(reiser4_node_t *node, reiser4_key_t *rd_key) {
	reiser4_place_t place;
	errno_t ret;
	
	aal_assert("vpf-502", node != NULL);
	aal_assert("vpf-347", rd_key != NULL);
	
	if (node->p.node != NULL) {
		/* Take the right delimiting key from the parent. */
		
		if ((ret = reiser4_node_realize(node)))
			return ret;
		
		place = node->p;
		
		/* If this is the last position in the parent, call the method 
		   recursevely for the parent. Get the right delimiting key 
		   otherwise. */		
		if ((reiser4_node_items(node->p.node) == place.pos.item + 1) &&
		    (reiser4_item_units(&place) == place.pos.unit + 1 || 
		     place.pos.unit == MAX_UINT32)) 
		{
			if ((ret = repair_node_rd_key(node->p.node, rd_key)))
				return ret;
		} else {
			place.pos.item++;
			place.pos.unit = 0;
			
			if ((ret = reiser4_place_fetch(&place)))
				return ret;
			
			if ((ret = reiser4_key_assign(rd_key, &place.key)))
				return ret;
		}
	} else {
		/* FIXME-UMKA->VITALY: This function no longer exists. */
//		reiser4_key_guess(rd_key);
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
	aal_assert("vpf-250", node->entity->plug != NULL);
	
	if ((res = repair_node_ld_key_fetch(node, &d_key))) {
		aal_exception_error("Node (%llu): Failed to get the left "
				    "delimiting key.", node->number);
		return res;
	}
	
	place.pos.item = 0; 
	place.pos.unit = MAX_UINT32;
	place.node = node;
	
	if ((res = reiser4_place_fetch(&place)))
		return res;
	
	res = reiser4_key_compare(&d_key, &place.key);
	
	/* Left delimiting key should match the left key in the node. */
	if (res > 0) {
		/* The left delimiting key is much then the left key in the 
		   node - not legal */
		aal_exception_error("Node (%llu): The first key %k is not "
				    "equal to the left delimiting key %k.",
				    node->number, &place.key, &d_key);
		return RE_FATAL;
	} else if (res < 0) {
		/* It is legal to have the left key in the node much then 
		   its left delimiting key - due to removing some items 
		   from the node, for example. Fix the delemiting key if 
		   we have parent. */
		if (node->p.node != NULL) {
			aal_exception_error("Node (%llu): The left delimiting "
					    "key %k in the parent node (%llu), "
					    "pos (%u/%u) mismatch the first key "
					    "%k in the node. %s", node->number,
					    &place.key, node->p.node->number,
					    place.pos.item, place.pos.unit, 
					    &d_key, mode == RM_BUILD ? 
					    "Left delimiting key is fixed." : 
					    "");
			
			if (mode == RM_BUILD) {
				res = repair_node_ld_key_update(node, &d_key);
				if (res)
					return res;
			}
		}
	}
	
	if ((res = repair_node_rd_key(node, &d_key))) {
		aal_exception_error("Node (%llu): Failed to get the right "
				    "delimiting key.", node->number);
		return res;
	}
	
	pos->item = reiser4_node_items(node) - 1;
	
	if ((res = reiser4_place_fetch(&place))) {
		aal_exception_error("Node (%llu): Failed to open the item "
				    "(%llu).", node->number, pos->item);
		return res;
	}
	
	if ((res = reiser4_item_maxreal_key(&place, &key))) {
		aal_exception_error("Node (%llu): Failed to get the max real "
				    "key of the last item.", node->number);
		return res;
	}
	
	if (reiser4_key_compare(&key, &d_key) >= 0) {
		aal_exception_error("Node (%llu): The last key %k in the node "
				    "is greater then the right delimiting key "
				    "%k.", node->number, &key, &d_key);
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
	place.pos.unit = MAX_UINT32;
	count = reiser4_node_items(node);
	
	for (pos->item = 0; pos->item < count; pos->item++) {
		if ((res = reiser4_place_fetch(&place)))
			return res;
		
		if ((res = reiser4_key_assign(&key, &place.key))) {
			aal_exception_error("Node (%llu): Failed to get the "
					    "key of the item (%u).",
					    node->number, pos->item);
			return res;
		}
		
		if (reiser4_key_valid(&key)) {
			aal_exception_error("Node (%llu): The key %k of the "
					    "item (%u) is not valid. Item "
					    "removed.", node->number, &key,
					    pos->item);
			
			if ((res = reiser4_node_remove(node, pos, 1))) {
				aal_exception_bug("Node (%llu): Failed to "
						  "delete the item (%d).",
						  node->number, pos->item);
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
				/* FIXME-VITALY: Which part does put the rule 
				   that neighbour keys could be equal? */
				aal_exception_error("Node (%llu), items (%u) "
						    "and (%u): Wrong order of "
						    "keys.", node->number, 
						    pos->item - 1, pos->item);
				
				return RE_FATAL;
			}
		}
		
		prev_key = key;
	}
	
	return RE_OK;
}

/*  Checks the node content.
    Returns values according to repair_error_codes_t. */
errno_t repair_node_check_struct(reiser4_node_t *node, uint8_t mode) {
	errno_t res = RE_OK;
	
	aal_assert("vpf-494", node != NULL);
	aal_assert("vpf-193", node->entity != NULL);    
	aal_assert("vpf-220", node->entity->plug != NULL);
	
	res |= plug_call(node->entity->plug->o.node_ops, check_struct, 
			 node->entity, mode);
	
	if (repair_error_fatal(res))
		return res;
	
	res |= repair_node_keys_check(node, mode);
	
	if (repair_error_fatal(res))
		return res;
	
	res |= repair_node_items_check(node, mode);
	
	if (repair_error_fatal(res))
		return res;    
	
	if (res & RE_FIXED) {
		reiser4_node_mkdirty(node);
		res &= ~RE_FIXED;
	}
	
	return res;
}

/* Traverse through all items of the gived node. */
errno_t repair_node_traverse(reiser4_node_t *node, node_func_t func, 
			     void *data)
{
	reiser4_place_t place;
	pos_t *pos = &place.pos;
	uint32_t items;
	errno_t res;
	
	aal_assert("vpf-744", node != NULL);
	
	pos->unit = MAX_UINT32;
	
	for (pos->item = 0; pos->item < reiser4_node_items(node); pos->item++) {
		if ((res = reiser4_place_open(&place, node, pos))) {
			aal_exception_error("Node (%llu), item (%u): failed to "
					    "open the item by its place.", 
					    node->number, pos->item);
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
	
	plug_call(node->entity->plug->o.node_ops, print, node->entity, 
		  &stream, start, count, options);
	
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
	aal_assert("vpf-964", dst->entity->plug->id.id == 
		   src->entity->plug->id.id);
	aal_assert("vpf-967", dst_pos != NULL);
	aal_assert("vpf-968", src_pos != NULL);
    
	return plug_call(dst->entity->plug->o.node_ops, copy, dst->entity,
			 dst_pos, src->entity, src_pos, hint);
}

