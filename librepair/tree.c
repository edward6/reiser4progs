/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   tree.c -- repair/tree.c -- tree auxiliary code. */

#include <repair/librepair.h>

/* This function returns TRUE if passed item @group corresponds to passed 
   @level Hardcoded method, valid for the current tree imprementation only. */
bool_t repair_tree_legal_level(reiser4_item_group_t group,
			       uint8_t level)
{
	if (group == NODEPTR_ITEM) {
		if (level == LEAF_LEVEL)
			return FALSE;
	} else if (group == EXTENT_ITEM) {
		if (level != TWIG_LEVEL)
			return FALSE;
	} else {
		return level == LEAF_LEVEL;
	}
	
	return TRUE;
}

static errno_t callback_data_level(reiser4_plug_t *plug, void *data) {
	uint8_t *level = (uint8_t *)data;
	
	aal_assert("vpf-746", data != NULL);
	
	if (plug->id.type != ITEM_PLUG_TYPE)
		return 0;
	
	if (!repair_tree_legal_level(plug->id.group, *level))
		return 0;
	
	return !reiser4_item_branch(plug);
}

bool_t repair_tree_data_level(uint8_t level) {
	if (level == 0)
		return FALSE;
	
	return (reiser4_factory_cfind(callback_data_level,
				      &level) != NULL);
}

/* Get the max real key existed in the tree. Go down through all right-most 
   child to get it. */
static errno_t repair_tree_maxreal_key(reiser4_tree_t *tree, 
				       reiser4_node_t *node, 
				       reiser4_key_t *key)
{
	reiser4_place_t place;
	reiser4_node_t *child;
	errno_t res;
	
	aal_assert("vpf-712", node != NULL);
	aal_assert("vpf-713", key != NULL);
	
	place.node = node;
	place.pos.item = reiser4_node_items(node) - 1;
	place.pos.unit = MAX_UINT32;
	
	if (reiser4_place_fetch(&place)) {
		aal_exception_error("Node (%llu): Failed to open the item "
				    "(%u).", node_blocknr(node), place.pos.item);
		return -EINVAL;
	}
	
	if (reiser4_item_branch(place.plug)) {
		ptr_hint_t ptr;
		uint32_t blksize;
		
		place.pos.unit = reiser4_item_units(&place) - 1;
		
		if (plug_call(place.plug->o.item_ops, read, (place_t *)&place,
			      &ptr, place.pos.unit, 1) != 1 || ptr.start == INVAL_BLK)
		{
			return -EINVAL;
		}

		blksize = reiser4_master_blksize(tree->fs->master);
		
		child = reiser4_node_open(tree->fs->device, blksize,
					  ptr.start, tree->key.plug);
		
		if (!child)
			return -EINVAL;
		
		res = repair_tree_maxreal_key(tree, child, key);
		
		if (reiser4_node_close(child))
			return -EINVAL;
	} else 
		res = reiser4_item_maxreal_key(&place, key);
	
	return res;
}

errno_t repair_tree_parent_lkey(reiser4_tree_t *tree,
				reiser4_node_t *node, 
				reiser4_key_t *key) 
{
	errno_t res;
	
	aal_assert("vpf-501", node != NULL);
	aal_assert("vpf-344", key != NULL);
	
	if (node->p.node != NULL) {
		if ((res = reiser4_place_fetch(&node->p)))
			return res;
		
		if ((res = reiser4_key_assign(key, &node->p.key)))
			return res;
	} else {
		reiser4_key_minimal(key);
	}
	
	return 0;
}

/* Sets to the @key the right delimiting key of the node kept in the parent. */
errno_t repair_tree_parent_rkey(reiser4_tree_t *tree, reiser4_node_t *node, 
				reiser4_key_t *key) 
{
	reiser4_place_t place;
	errno_t ret;
	
	aal_assert("vpf-502", node != NULL);
	aal_assert("vpf-347", key != NULL);
	
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
			if ((ret = repair_tree_parent_rkey(tree, node->p.node,
							   key)))
				return ret;
		} else {
			place.pos.item++;
			place.pos.unit = 0;
			
			if ((ret = reiser4_place_fetch(&place)))
				return ret;
			
			if ((ret = reiser4_key_assign(key, &place.key)))
				return ret;
		}
	} else {
		key->plug = node->tree->key.plug;
		reiser4_key_maximal(key);
	}
	
	return 0;
}

/* Checks the delimiting keys of the node kept in the parent. */
errno_t repair_tree_dknode_check(reiser4_tree_t *tree, 
				 reiser4_node_t *node, 
				 uint8_t mode) 
{
	reiser4_place_t place;
	reiser4_key_t key, d_key;
	pos_t *pos = &place.pos;
	int res;
	
	aal_assert("vpf-1281", tree != NULL);
	aal_assert("vpf-248", node != NULL);
	aal_assert("vpf-249", node->entity != NULL);
	aal_assert("vpf-250", node->entity->plug != NULL);
	aal_assert("vpf-1280", node->tree != NULL);
	
	if ((res = repair_tree_parent_lkey(tree, node, &d_key))) {
		aal_exception_error("Node (%llu): Failed to get the left "
				    "delimiting key.", node_blocknr(node));
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
		aal_exception_error("Node (%llu): The first key [%s] is not "
				    "equal to the left delimiting key [%s].",
				    node_blocknr(node), 
				    reiser4_print_key(&place.key, PO_DEF),
				    reiser4_print_key(&d_key, PO_DEF));
		return RE_FATAL;
	} else if (res < 0) {
		/* It is legal to have the left key in the node much then 
		   its left delimiting key - due to removing some items 
		   from the node, for example. Fix the delemiting key if 
		   we have parent. */
		if (node->p.node != NULL) {
			aal_exception_error("Node (%llu): The left delimiting "
					    "key [%s] in the parent node (%llu), "
					    "pos (%u/%u) mismatch the first key "
					    "[%s] in the node. %s", 
					    node_blocknr(node),
					    reiser4_print_key(&place.key, PO_DEF),
					    node_blocknr(node->p.node), 
					    place.pos.item,
					    place.pos.unit, 
					    reiser4_print_key(&d_key, PO_DEF),
					    mode == RM_BUILD ? 
					    "Left delimiting key is fixed." : "");
			
			if (mode == RM_BUILD) {
				if ((res = reiser4_tree_ukey(tree, &place, &d_key)))
					return res;
			}
		}
	}
	
	if ((res = repair_tree_parent_rkey(tree, node, &d_key))) {
		aal_exception_error("Node (%llu): Failed to get the right "
				    "delimiting key.", node_blocknr(node));
		return res;
	}
	
	pos->item = reiser4_node_items(node) - 1;
	
	if ((res = reiser4_place_fetch(&place))) {
		aal_exception_error("Node (%llu): Failed to open the item "
				    "(%u).", node_blocknr(node), pos->item);
		return res;
	}
	
	if ((res = reiser4_item_maxreal_key(&place, &key))) {
		aal_exception_error("Node (%llu): Failed to get the max real "
				    "key of the last item.", node_blocknr(node));
		return res;
	}
	
	if (reiser4_key_compare(&key, &d_key) >= 0) {
		aal_exception_error("Node (%llu): The last key [%s] in the node "
				    "is greater then the right delimiting key "
				    "[%s].", node_blocknr(node), 
				    reiser4_print_key(&key, PO_DEF),
				    reiser4_print_key(&d_key, PO_DEF));
		return -ESTRUCT;
	}
	
	return 0;
}

/* This function creates nodeptr item on the base of 'node' and insert it 
   to the tree. */
errno_t repair_tree_attach(reiser4_tree_t *tree, reiser4_node_t *node) {
	reiser4_key_t rkey, key;
	reiser4_place_t place;
	insert_hint_t hint;
	lookup_t lookup;
	ptr_hint_t ptr;
	uint32_t level;
	errno_t res;
	rid_t pid;
	
	aal_assert("vpf-658", tree != NULL);
	aal_assert("vpf-659", node != NULL);
	
	/* Preparing nodeptr item hint */
	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&ptr, 0, sizeof(ptr));
	
	reiser4_node_lkey(node, &hint.key);
	
	/* Key should not exist in the tree yet. */
	lookup = reiser4_tree_lookup(tree, &hint.key, LEAF_LEVEL, &place);
	
	if (lookup != ABSENT)
		return lookup;
	
	/* If some node was found and it is not of higher level then the node 
	   being attached, try to split nodes to be able to attach the node as 
	   a whole. */
	level = reiser4_node_get_level(node) + 1;
    
	if (place.node != NULL && reiser4_node_get_level(place.node) < level) {
		/* Get the key of the found position or the right key. */
		if (reiser4_place_rightmost(&place)) {
			if ((res = repair_tree_parent_rkey(tree, place.node, 
							   &key)))
				return res;
		} else {			
			if ((res = reiser4_place_fetch(&place)))
				return res;

			reiser4_key_assign(&key, &place.key);
		}
		
		/* Get the maximum key existing in the node being inserted. */
		if ((res = repair_tree_maxreal_key(tree, node, &rkey)))
			return res;
		
		/* If the most right key from the node being inserted is 
		   greater then the key found by lookup, it is not possible 
		   to insert the node as a whole. */
		if (reiser4_key_compare(&rkey, &key) >= 0)
			return -ESTRUCT;
	}
	
	hint.specific = &ptr;
	hint.count = 1;
	ptr.start = node_blocknr(node);
	ptr.width = 1;
	
	pid = reiser4_profile_value("nodeptr");
	
	if (!(hint.plug = reiser4_factory_ifind(ITEM_PLUG_TYPE, pid))) {
		aal_exception_error("Can't find item plugin by its id 0x%x.", 
				    pid);
		return -EINVAL;
	}
	
	if ((res = reiser4_tree_insert(tree, &place, &hint, level))) {
		aal_exception_error("Can't insert nodeptr item to the tree.");
		return res;
	}
	
	/* Setting needed links between nodes in the tree cashe. */
	if ((res = reiser4_tree_connect(tree, place.node, node)))
		return res;
	
	reiser4_tree_neigh(tree, node, D_LEFT);
	reiser4_tree_neigh(tree, node, D_RIGHT);
	
	return 0;
}

/* Copies item's data pointed by @src to @dst, from the key pointed by 
   @src place though the @end one. After the coping @end key points to 
   the data of the @src which has not being copied. */
errno_t repair_tree_copy_by_place(reiser4_tree_t *tree, reiser4_place_t *dst,
				  reiser4_place_t *src, copy_hint_t *hint)
{
	reiser4_place_t old;
	uint32_t needed;
	errno_t res;
	
	aal_assert("vpf-948", tree != NULL); 
	aal_assert("vpf-949", dst != NULL);
	aal_assert("vpf-950", src != NULL);
	
	if (hint && hint->src_count == 0)
		return 0;
	
	if (reiser4_tree_fresh(tree)) {
		aal_exception_error("Tree copy failed. Tree is empty.");
		return -EINVAL;
	}
	
	old = *dst;
	
	if (!hint || hint->len_delta > 0) {
		if (hint)
			needed = hint->len_delta;
		else
			needed = src->len;
		
		needed += (dst->pos.unit == MAX_UINT32 ? 
			   reiser4_node_overhead(dst->node) : 0);
		
		res = reiser4_tree_expand(tree, dst, needed, SF_DEFAULT);
		if (res) {
			aal_exception_error("Tree expand for coping failed.");
			return res;
		}
	}
	
	res = repair_node_copy(dst->node, &dst->pos, src->node, 
			       &src->pos, hint);
	
	if (res) {
		aal_exception_error("Node copying failed from node %llu, item "
				    "%u to node %llu, item %u one.", 
				    node_blocknr(src->node), src->pos.item, 
				    node_blocknr(dst->node), dst->pos.item);
		return res;
	}
	
	if (reiser4_place_leftmost(dst) && dst->node->p.node) {
		reiser4_place_t p;
		
		reiser4_place_init(&p, dst->node->p.node, 
				   &dst->node->p.pos);
		
		if ((res = reiser4_tree_ukey(tree, &p, &src->key)))
			return res;
	}
	
	if (dst->node != tree->root && !dst->node->p.node) {
		if (!old.node->p.node)
			reiser4_tree_growup(tree);
		
		if ((res = reiser4_tree_attach(tree, dst->node))) {
			aal_exception_error("Can't attach node %llu to the "
					    "tree.", node_blocknr(dst->node));
			
			reiser4_tree_release(tree, dst->node);	    
			return res;
		}
	}
	
	return 0;
}

/* Insert the item into the tree overwriting an existent in the tree item 
   if needed. Does not insert branches. */
errno_t repair_tree_copy(reiser4_tree_t *tree, reiser4_place_t *src) {
	reiser4_key_t src_max, start_key;
	reiser4_place_t dst;
	uint32_t src_units;
	bool_t whole = 1;
	copy_hint_t hint;
	lookup_t lookup;
	errno_t ret;
	pos_t src_pos;
	int res;
	
	aal_assert("vpf-654", tree != NULL);
	aal_assert("vpf-655", src != NULL);
	aal_assert("vpf-657", src->node != NULL);
	
	if (reiser4_item_branch(src->plug))
		return -EINVAL;
	
	src_units = reiser4_item_units(src);
	reiser4_key_assign(&start_key, &src->key);
	
	src_pos = src->pos;
	
	if ((res = reiser4_item_maxreal_key(src, &src_max)))
		return res;
	
	while (1) {
		lookup = reiser4_tree_lookup(tree, &start_key, 
					     LEAF_LEVEL, &dst);
		
		if (lookup == PRESENT) {
			/* Whole data can not be inserted */
			whole = 0;
		} else if (dst.pos.item == reiser4_node_items(dst.node)) {
			/* If right neighbour is overlapped with src item, move
			   dst there. */	    
			pos_t rpos = {0, -1};
			reiser4_place_t rplace;
			reiser4_node_t *rnode;
			
			rnode = reiser4_tree_neigh(tree, dst.node, D_RIGHT);
			
			if (rnode != NULL) {
				if (reiser4_place_open(&rplace, rnode, &rpos))
					return -EINVAL;
				
				res = reiser4_key_compare(&src_max, 
							  &rplace.key);
				
				if (res >= 0) {
					whole = 0;
					dst = rplace;
				}
			}
		} else {
			if ((res = reiser4_place_fetch(&dst)))
				return res;
			
			if (dst.pos.unit == reiser4_item_units(&dst)) {
				/* Insert another item as between the last real
				   key in the found item and start_key can be a
				   hole. Items which can be merged will be at
				   semantic pass. */
				dst.pos.item++;
				dst.pos.unit = MAX_UINT32;
			} else {
				if ((res = reiser4_place_fetch(&dst)))
					return res;
				
				res = reiser4_key_compare(&src_max, 
							  &dst.key);
				
				if (res >= 0) {
					/* Items overlapped. */
					whole = 0;
				}
			}
		}
	
		if (!whole) {
			aal_memset(&hint, 0, sizeof(hint));
			reiser4_key_assign(&hint.start, &start_key);
			
			if (dst.pos.unit == MAX_UINT32)
				dst.pos.unit = 0;
			if (src->pos.unit == MAX_UINT32)
				src->pos.unit = 0; 
			
			if ((res = reiser4_place_fetch(&dst)))
				return res;
			
			
			if (dst.plug->id.id != src->plug->id.id) {
				/* FIXME: relocation code should be here. */		
				aal_exception_error("Tree Overwrite failed to "
						    "overwrite items of different "
						    "plugins. Source: node (%llu), "
						    "item (%u), unit (%u). "
						    "Destination: node (%llu), "
						    "items (%u), unit (%u). "
						    "Relocation is not supported "
						    "yet.", node_blocknr(src->node), 
						    src->pos.item, src->pos.unit, 
						    node_blocknr(dst.node), dst.pos.item, 
						    dst.pos.unit);
				return 0;
			}
			
			if ((res = reiser4_item_maxreal_key(&dst, &hint.end)))
				return res;
			
			if ((res = repair_item_estimate_copy(&dst, src, &hint)))
				return res;
		}
		
		ret = repair_tree_copy_by_place(tree, &dst, src, whole ? NULL : &hint);
		
		if (ret) {
			if (whole) {				
				aal_exception_error("Tree Copy failed: from the node "
						    "(%llu), item (%u) to the node "
						    "(%llu), item (%u). Key interval "
						    "[%s] - [%s].", node_blocknr(src->node), 
						    src->pos.item, node_blocknr(dst.node), 
						    dst.pos.item,  reiser4_print_key(&hint.start, PO_DEF),
						    reiser4_print_key(&hint.end, PO_DEF));
			} else {
				aal_exception_error("Tree Copy failed: from the node "
						    "(%llu), item (%u) to the node "
						    "(%llu), item (%u).", 
						    node_blocknr(src->node), src->pos.item, 
						    node_blocknr(dst.node), dst.pos.item);
			}
			
			return ret;
		}
		
		if (whole || !src->plug->o.item_ops->lookup)
			break;
		
		/* Lookup by end_key. */
		res = src->plug->o.item_ops->lookup((place_t *)src, &hint.end,
						    &src->pos.unit);
		
		if (src->pos.unit >= src_units)
			break;
		
		reiser4_key_assign(&start_key, &hint.end);
	}
	
	src->pos = src_pos;
	
	return 0;
}

