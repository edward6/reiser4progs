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

static errno_t callback_data_level(reiser4_plugin_t *plugin, void *data) {
	uint8_t *level = (uint8_t *)data;
	
	aal_assert("vpf-746", data != NULL);
	
	if (plugin->id.type != ITEM_PLUGIN_TYPE)
		return 0;
	
	if (!repair_tree_legal_level(plugin->id.group, *level))
		return 0;
	
	return reiser4_item_data(plugin);
}

bool_t repair_tree_data_level(uint8_t level) {
	if (level == 0)
		return FALSE;
	
	return (libreiser4_factory_cfind(callback_data_level,
					 &level, FALSE) != NULL);
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
	
	if (reiser4_place_realize(&place)) {
		aal_exception_error("Node (%llu): Failed to open the item "
				    "(%u).", node->number, place.pos.item);
		return -EINVAL;
	}
	
	if (reiser4_item_branch(&place)) {
		ptr_hint_t ptr;
		uint32_t blksize;
		item_entity_t *item = &place.item;
		
		place.pos.unit = reiser4_item_units(&place) - 1;
		
		if (plugin_call(item->plugin->o.item_ops, read, item,
				&ptr, place.pos.unit, 1) != 1 || 
		    ptr.start == INVAL_BLK)
		{
			return -EINVAL;
		}

		blksize = reiser4_master_blksize(tree->fs->master);
		
		child = reiser4_node_open(place.node->device, 
					  blksize, ptr.start,
					  node->entity->plugin->id.id);
		
		if (!child)
			return -EINVAL;
		
		res = repair_tree_maxreal_key(tree, child, key);
		
		if (reiser4_node_close(child))
			return -EINVAL;
	} else 
		res = reiser4_item_maxreal_key(&place, key);
	
	return res;
}

/* This function creates nodeptr item on the base of 'node' and insert it 
   to the tree. */
errno_t repair_tree_attach(reiser4_tree_t *tree, reiser4_node_t *node) {
	reiser4_key_t rkey, key;
	reiser4_place_t place;
	create_hint_t hint;
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
			if ((res = repair_node_rd_key(place.node, &key)))
				return res;
		} else {			
			if ((res = reiser4_place_realize(&place)))
				return res;

			reiser4_key_assign(&key, &place.item.key);
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
	
	hint.type_specific = &ptr;
	hint.count = 1;
	hint.flags = HF_FORMATD;
	ptr.start = node->number;
	ptr.width = 1;
	
	pid = reiser4_profile_value(tree->fs->profile, "nodeptr");
	
	if (!(hint.plugin = libreiser4_factory_ifind(ITEM_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find item plugin by its id 0x%x.", 
				    pid);
		return -EINVAL;
	}
	
	if ((res = reiser4_tree_insert(tree, &place, level, &hint))) {
		aal_exception_error("Can't insert nodeptr item to the tree.");
		return res;
	}
	
	/* Setting needed links between nodes in the tree cashe. */
	if ((res = reiser4_tree_connect(tree, place.node, node))) {
		aal_exception_error("Can't attach the node %llu in tree "
				    "cache.", node->number);
		return res;
	}
	
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
			needed = src->item.len;
		
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
				    src->node->number, src->pos.item, 
				    dst->node->number, dst->pos.item);
		return res;
	}
	
	if (reiser4_place_leftmost(dst) && dst->node->p.node) {
		reiser4_place_t p;
		
		reiser4_place_init(&p, tree, dst->node->p.node, 
				   &dst->node->p.pos);
		
		if ((res = reiser4_tree_ukey(tree, &p, &src->item.key)))
			return res;
	}
	
	if (dst->node != tree->root && !dst->node->p.node) {
		if (!old.node->p.node)
			reiser4_tree_growup(tree);
		
		if ((res = reiser4_tree_attach(tree, dst->node))) {
			aal_exception_error("Can't attach node %llu to the "
					    "tree.", dst->node->number);
			
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
	
	if (reiser4_item_branch(src))
		return -EINVAL;
	
	src_units = reiser4_item_units(src);
	reiser4_key_assign(&start_key, &src->item.key);
	
	src_pos = src->pos;
	
	if ((res = reiser4_item_maxreal_key(src, &src_max)))
		return res;
	
	while (1) {
		lookup = reiser4_tree_lookup(tree, &start_key, 
					     LEAF_LEVEL, &dst);
		
		if (lookup == PRESENT) {
			/* whole data can not be inserted */
			whole = 0;
		} else if (dst.pos.item == reiser4_node_items(dst.node)) {
			/* If right neighbour is overlapped with src item, 
			   move dst there. */	    
			pos_t rpos = {0, -1};
			reiser4_place_t rplace;
			reiser4_node_t *rnode;
			
			rnode = reiser4_tree_neigh(tree, dst.node, D_RIGHT);
			
			if (rnode != NULL) {
				if (reiser4_place_open(&rplace, tree, 
						       rnode, &rpos))
					return -EINVAL;
				
				res = reiser4_key_compare(&src_max, 
							  &rplace.item.key);
				
				if (res >= 0) {
					whole = 0;
					dst = rplace;
				}
			}
		} else {
			if ((res = reiser4_place_realize(&dst)))
				return res;
			
			if (dst.pos.unit == reiser4_item_units(&dst)) {
				/* Insert another item as between the last real
				   key in the found item and start_key can be a
				   hole. Items which can be merged will be at
				   semantic pass. */
				dst.pos.item++;
				dst.pos.unit = MAX_UINT32;
			} else {
				if ((res = reiser4_place_realize(&dst)))
					return res;
				
				res = reiser4_key_compare(&src_max, 
							  &dst.item.key);
				
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
			
			if ((res = reiser4_place_realize(&dst)))
				return res;
			
			if (dst.item.plugin->id.id != src->item.plugin->id.id) {
				/* FIXME: relocation code should be here. */		
				aal_exception_error("Tree Overwrite failed to "
						    "overwrite items of different "
						    "plugins. Source: node (%llu), "
						    "item (%u), unit (%u). "
						    "Destination: node (%llu), "
						    "items (%u), unit (%u). "
						    "Relocation is not supported "
						    "yet.", src->node->number, 
						    src->pos.item, src->pos.unit, 
						    dst.node->number, dst.pos.item, 
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
						    "%k - %k.", src->node->number, 
						    src->pos.item, dst.node->number, 
						    dst.pos.item,  &hint.start, 
						    &hint.end);
			} else {
				aal_exception_error("Tree Copy failed: from the node "
						    "(%llu), item (%u) to the node "
						    "(%llu), item (%u).", 
						    src->node->number, src->pos.item, 
						    dst.node->number,  dst.pos.item);
			}
			
			return ret;
		}
		
		if (whole || !src->item.plugin->o.item_ops->lookup)
			break;
		
		/* Lookup by end_key. */
		res = src->item.plugin->o.item_ops->lookup(&src->item, 
							   &hint.end, 
							   &src->pos.unit);
		
		if (src->pos.unit >= reiser4_item_units(src))
			break;
		
		reiser4_key_assign(&start_key, &hint.end);
	}
	
	src->pos = src_pos;
	
	return 0;
}

