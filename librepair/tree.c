/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   tree.c -- repair/tree.c -- tree auxiliary code. */

#include <repair/librepair.h>

/* This function returns TRUE if passed item @group corresponds to passed 
   @level Hardcoded method, valid for the current tree imprementation only. */
bool_t repair_tree_legal_level(reiser4_item_group_t group,
			       uint8_t level)
{
	if (group == NODEPTR_ITEM)
		return level != LEAF_LEVEL;
	
	if (group == EXTENT_ITEM)
		return level == TWIG_LEVEL;
	
	return level == LEAF_LEVEL;
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
				       node_t *node, reiser4_key_t *key)
{
	place_t place;
	node_t *child;
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
		blk_t blk;
		uint32_t blksize;
		
		place.pos.unit = reiser4_item_units(&place) - 1;

		if ((blk = reiser4_item_down_link(&place)) == INVAL_BLK)
			return -EINVAL;
			
		blksize = reiser4_master_get_blksize(tree->fs->master);
		
		if (!(child = reiser4_node_open(tree, blk)))
			return -EINVAL;
		
		res = repair_tree_maxreal_key(tree, child, key);
		
		if (reiser4_node_close(child))
			return -EINVAL;
	} else 
		res = reiser4_item_maxreal_key(&place, key);
	
	return res;
}

errno_t repair_tree_parent_lkey(reiser4_tree_t *tree,
				node_t *node, 
				reiser4_key_t *key) 
{
	errno_t res;
	
	aal_assert("vpf-501", node != NULL);
	aal_assert("vpf-344", key != NULL);
	
	if (node->p.node != NULL) {
		if ((res = reiser4_place_fetch(&node->p)))
			return res;
		
		aal_memcpy(key, &node->p.key, sizeof(*key));
	} else {
		key->plug = tree->key.plug;
		reiser4_key_minimal(key);
	}
	
	return 0;
}

/* Sets to the @key the right delimiting key of the node kept in the parent. */
errno_t repair_tree_parent_rkey(reiser4_tree_t *tree, node_t *node, 
				reiser4_key_t *key) 
{
	place_t place;
	errno_t ret;
	
	aal_assert("vpf-502", node != NULL);
	aal_assert("vpf-347", key != NULL);
	
	if (node->p.node != NULL) {
		/* Take the right delimiting key from the parent. */
		
		if ((ret = reiser4_node_realize(node)))
			return ret;
		
		place = node->p;
		reiser4_place_inc(&place, 0);
		
		/* If the rightmost place, call recursevely for the parent. */ 
		if (reiser4_place_rightmost(&place)) {
			ret = repair_tree_parent_rkey(tree, node->p.node, key);
			
			if (ret) return ret;
		} else {
			if ((ret = reiser4_place_fetch(&place)))
				return ret;
			
			if ((ret = reiser4_key_assign(key, &place.key)))
				return ret;
		}
	} else {
		key->plug = tree->key.plug;
		reiser4_key_maximal(key);
	}
	
	return 0;
}

/* Checks the delimiting keys of the node kept in the parent. */
errno_t repair_tree_dknode_check(reiser4_tree_t *tree, 
				 node_t *node, 
				 uint8_t mode) 
{
	place_t place;
	reiser4_key_t key_max, dkey;
	pos_t pos = {0, MAX_UINT32};
	int res;
	
	aal_assert("vpf-1281", tree != NULL);
	aal_assert("vpf-248", node != NULL);
	aal_assert("vpf-249", node->entity != NULL);
	aal_assert("vpf-250", node->entity->plug != NULL);
	aal_assert("vpf-1280", node->tree != NULL);
	
	if ((res = repair_tree_parent_lkey(tree, node, &dkey))) {
		aal_exception_error("Node (%llu): Failed to get the left "
				    "delimiting key.", node_blocknr(node));
		return res;
	}
	
	if ((res = reiser4_place_open(&place, node, &pos)))
		return res;
	
	res = reiser4_key_compfull(&dkey, &place.key);
	
	/* Left delimiting key should match the left key in the node. */
	if (res > 0) {
		/* The left delimiting key is much then the left key in the 
		   node - not legal */
		aal_exception_error("Node (%llu): The first key [%s] is not "
				    "equal to the left delimiting key [%s].",
				    node_blocknr(node), 
				    reiser4_print_key(&place.key, PO_DEFAULT),
				    reiser4_print_key(&dkey, PO_DEFAULT));
		return RE_FATAL;
	}
	
	while (res < 0) {
		/* It is legal to have the left key in the node much then its 
		   left delimiting key - due to removing some items from the 
		   node, for example. Fix the delemiting key if we have parent. */
		if (node->p.node == NULL) 
			break;
		
		aal_exception_error("Node (%llu): The left delimiting key [%s] "
				    "in the parent node (%llu), pos (%u/%u) do "
				    "not match the first key [%s] in the node."
				    "%s", node_blocknr(node),
				    reiser4_print_key(&place.key, PO_DEFAULT),
				    node_blocknr(node->p.node),
				    place.pos.item, place.pos.unit, 
				    reiser4_print_key(&dkey, PO_DEFAULT),
				    mode == RM_BUILD ? "Fixed." : "");

		if (mode != RM_BUILD) 
			break;
		
		if ((res = reiser4_tree_update_key(tree, &place, &dkey)))
			return res;

		break;
	}
	
	if ((res = repair_tree_parent_rkey(tree, node, &dkey))) {
		aal_exception_error("Node (%llu): Failed to get the right "
				    "delimiting key.", node_blocknr(node));
		return res;
	}
	
	if ((res = reiser4_item_maxreal_key(&place, &key_max))) {
		aal_exception_error("Node (%llu): Failed to get the max real "
				    "key of the last item.", node_blocknr(node));
		return res;
	}
	
	if (reiser4_key_compfull(&key_max, &dkey) >= 0) {
		aal_exception_error("Node (%llu): The last key [%s] in the node "
				    "is greater then the right delimiting key "
				    "[%s].", node_blocknr(node), 
				    reiser4_print_key(&key_max, PO_DEFAULT),
				    reiser4_print_key(&dkey, PO_DEFAULT));
		return -ESTRUCT;
	}
	
	return 0;
}

/* This function creates nodeptr item on the base of @node and insert it 
   to the tree. */
errno_t repair_tree_attach(reiser4_tree_t *tree, node_t *node) {
	reiser4_key_t rkey, key;
	place_t place;
	trans_hint_t hint;
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
	
	reiser4_node_leftmost_key(node, &hint.offset);
	
	/* Key should not exist in the tree yet. */
	lookup = reiser4_tree_lookup(tree, &hint.offset,  LEAF_LEVEL, 
				     FIND_EXACT, &place);
	
	switch(lookup) {
	case PRESENT:
		return -ESTRUCT;
	case ABSENT:
		break;
	default:
		return lookup;
	}
	
	/* If some node was found and it is not of higher level then the node
	   being attached, try to split nodes to be able to attach the node as a
	   whole. */
	level = reiser4_node_get_level(node) + 1;
    
	if (place.node != NULL && reiser4_node_get_level(place.node) < level) {
		/* Check by keys that the whole node can be attached. */
		if (reiser4_place_right(&place))
			reiser4_place_inc(&place, 0);
		
		if (reiser4_place_rightmost(&place)) {
			res = repair_tree_parent_rkey(tree, place.node, &key);

			if (res) return res;
		} else {
			if ((res = reiser4_place_fetch(&place)))
				return res;
			
			if ((res = reiser4_item_get_key(&place, &key)))
				return res;
		}
		
		/* Get the maxreal existing key of the node being inserted. */
		if ((res = repair_tree_maxreal_key(tree, node, &rkey)))
			return res;
		
		/* If the most right key from the node being inserted is 
		   greater then the key found by lookup, it is not possible 
		   to insert the node as a whole. */
		if (reiser4_key_compfull(&rkey, &key) >= 0)
			return -ESTRUCT;
	}
	
	hint.specific = &ptr;
	hint.count = 1;
	hint.tree = tree;
	ptr.start = node_blocknr(node);
	ptr.width = 1;
	
	pid = reiser4_param_value("nodeptr");
	
	if (!(hint.plug = reiser4_factory_ifind(ITEM_PLUG_TYPE, pid))) {
		aal_exception_error("Can't find item plugin by its id 0x%x.", 
				    pid);
		return -EINVAL;
	}
	
	if ((res = reiser4_tree_insert(tree, &place, &hint, level)) < 0) {
		aal_exception_error("Can't insert nodeptr item to the tree.");
		return res;
	}
	
	/* Setting needed links between nodes in the tree cashe. */
	if ((res = reiser4_tree_connect_node(tree, place.node, node)))
		return res;
	
	reiser4_tree_neigh_node(tree, node, D_LEFT);
	reiser4_tree_neigh_node(tree, node, D_RIGHT);
	
	return 0;
}

/* Check that conversion is needed. */
static bool_t repair_tree_should_conv(reiser4_tree_t *tree, 
				      reiser4_plug_t *from,
				      reiser4_plug_t *to)
{
	aal_assert("vpf-1293", tree != NULL);
	aal_assert("vpf-1294", from != NULL);
	aal_assert("vpf-1295", to != NULL);
	aal_assert("vpf-1296", to != NULL);
		
	/* Conversion is not needed for equal plugins. */
	if (plug_equal(from, to))
		return FALSE;
	
	/* Conversion is needed for equal plugin groups. */
	if (from->id.group == to->id.group)
		return TRUE;

	/* TAIL->EXTENT conversion is needed. */
	if (from->id.group == TAIL_ITEM && to->id.group == EXTENT_ITEM)
		return TRUE;

	/* EXTENT->TAIL conversion is not needed. */
	if (from->id.group == TAIL_ITEM && to->id.group == EXTENT_ITEM)
		return FALSE;
	
	/* Other kind of conversions are impossible. */
	return -EINVAL;
}

/* Prepare repair convertion and perform it. */
static errno_t repair_tree_conv(reiser4_tree_t *tree, 
				place_t *place,
				reiser4_plug_t *plug) 
{
	conv_hint_t hint;

	/* Set bytes, plug, offset and count in @hint */
	hint.chunk = 0;
	hint.bytes = 0;
	hint.plug = plug;
	reiser4_key_assign(&hint.offset, &place->key);

	hint.count = plug_call(place->plug->o.item_ops->object,
			       size, place);

	return reiser4_tree_conv_flow(tree, &hint);
}

/* Copy @src item data over the @dst from the key pointed by @key through the
   @dst maxreal key. After the coping @key is set to the @dst maxreal key. */
errno_t repair_tree_copy(reiser4_tree_t *tree, place_t *dst,
				place_t *src, reiser4_key_t *key)
{
	reiser4_key_t dmax;
	trans_hint_t hint;
	errno_t res;
	
	aal_assert("vpf-1298", tree != NULL);
	aal_assert("vpf-1299", dst != NULL);
	aal_assert("vpf-1300", src != NULL);

	aal_memset(&hint, 0, sizeof(hint));
	reiser4_key_assign(&hint.offset, key);
	
	/* FIXME-VITALY: some hint fields need to be initialized by item 
	   estimate_insert methods--count, offset, etc. I should just 
	   initialize start and end keys. For now I calculate it here. */
	hint.tree = tree;
	hint.plug = dst->plug;
	
	if ((res = reiser4_item_maxreal_key(dst, &dmax)))
		return res;
	
	return 0;
}

/* Lookup for the correct @place place by the @start key in the @tree. */
static errno_t repair_tree_insert_lookup(reiser4_tree_t *tree, 
					 place_t *place,
					 trans_hint_t *hint)
{
	reiser4_key_t dkey, end;
	place_t prev;
	errno_t res;
	
	aal_assert("vpf-1364", tree  != NULL);
	aal_assert("vpf-1365", place != NULL);
	aal_assert("vpf-1367", hint  != NULL);
	
	res = reiser4_tree_lookup(tree, &hint->offset, LEAF_LEVEL,
				  FIND_EXACT, place);
	
	switch(res) {
	case PRESENT:
		/* The whole item can not be inserted. */
		if (place->pos.unit == MAX_UINT32)
			place->pos.unit = 0;

		return 0;
	case ABSENT:
		break;
	default:
		return res;
	}

	/* Absent. If non-existent unit or item, there is nothing mergable 
	   from the right side--lookup would go down there in that case.  */
	if (reiser4_place_right(place))
		/* Step to right. */
		reiser4_place_inc(place, 1);

	if (reiser4_place_rightmost(place)) {
		prev = *place;

		if ((res = reiser4_tree_next_node(tree, place, place))) {
			aal_exception_error("vpf-1363: Failed to "
					    "get the next node.");
			return res;
		}

		/* No right node. */
		if (!place->node) {
			*place = prev;
			return 0;
		}
	} else 
		aal_memset(&prev, 0, sizeof(prev));

	/* Get the current key of the @place. */
	if ((res = reiser4_place_fetch(place)))
		return res;

	if ((res = reiser4_item_get_key(place, &dkey)))
		return res;

	if ((res = reiser4_item_maxreal_key((place_t *)hint->specific, &end)))
		return res;
	
	/* If @end key is not less than the lookuped, items are overlapped. 
	   Othewise move to the previous pos. */
	if ((res = reiser4_key_compfull(&end, &dkey)) >= 0) {
		if (place->pos.unit == MAX_UINT32)
			place->pos.unit = 0;
	} else if (prev.node) {
		*place = prev;
	}

	return 0;
}

static errno_t callback_prep_merge(place_t *place, trans_hint_t *hint) {
	return plug_call(place->plug->o.item_ops->repair,
			 prep_merge, place, hint);
}

static errno_t callback_merge(node_t *node, pos_t *pos, trans_hint_t *hint) {
	return plug_call(node->entity->plug->o.node_ops, 
			 merge, node->entity, pos, hint);
}

/* Insert the item into the tree overwriting an existent in the tree item 
   if needed. Does not insert branches. */
errno_t repair_tree_insert(reiser4_tree_t *tree, place_t *src) {
	place_t dst;
	trans_hint_t hint;
	uint32_t scount;
	uint8_t level;
	errno_t res;
	
	aal_assert("vpf-654", tree != NULL);
	aal_assert("vpf-655", src != NULL);
	aal_assert("vpf-657", src->node != NULL);
	
	if (reiser4_item_branch(src->plug))
		return -EINVAL;
	
	scount = reiser4_item_units(src);
	src->pos.unit = 0;
	
	/* Init the src maxreal key. */
	
	aal_memset(&hint, 0, sizeof(hint));
	hint.tree = tree;
	hint.specific = src;
	hint.plug = src->plug;
	reiser4_key_assign(&hint.offset, &src->key);
	level = reiser4_node_get_level(src->node);

	/* FIXME-VITALY: be sure that the tree is of enough level. If not, 
	   @dst place will be changed in tree_modify during growing up. */
	while (1) {
		if ((res = repair_tree_insert_lookup(tree, &dst, &hint)))
			return res;
		
		/* Convert @dst if needed. */
		if (dst.pos.unit != MAX_UINT32) {
			switch(repair_tree_should_conv(tree, dst.plug, src->plug)) {
			case TRUE:
				if ((res = repair_tree_conv(tree, &dst, src->plug)))
					return res;

				/* Repeat lookup after @dst conversion. */
				continue;
			case FALSE:
				   break;
			default:
				   /* Conversion cannot be performed. */
				   aal_exception_error("Node (%llu), item (%u): the item "
						       "(%s) [%s] is overlapped by keys "
						       "with the item (%s) [%s] being "
						       "inserted [node %llu, item %u]. "
						       "Skip insertion.", 
						       node_blocknr(dst.node), dst.pos.item,
						       dst.plug->label, 
						       reiser4_print_key(&dst.key, PO_INODE),
						       src->plug->label,
						       reiser4_print_key(&src->key, PO_INODE),
						       node_blocknr(src->node), 
						       src->pos.item);

				   return 0;
			}
		}

		if (plug_equal(dst.plug, src->plug) && 
		    hint.plug->o.item_ops->repair->merge) 
		{
			if ((res = reiser4_tree_modify(tree, &dst, &hint, level,
						       callback_prep_merge,
						       callback_merge)))
				goto error;
		} else if (hint.plug->o.item_ops->repair->merge == NULL) {
			/* For items without merge method implemented, like SD. */
			return 0;
		} else {
			/* For not equal plugins do coping. */
			aal_exception_error("Node (%llu), item (%u): the item "
					    "(%s) [%s] is overlapped by keys "
					    "with the item (%s) [%s] being "
					    "inserted [node %llu, item %u]. "
					    "Copying is not ready yet, skip "
					    "insertion.", 
					    node_blocknr(dst.node), dst.pos.item,
					    dst.plug->label, 
					    reiser4_print_key(&dst.key, PO_INODE),
					    src->plug->label,
					    reiser4_print_key(&src->key, PO_INODE),
					    node_blocknr(src->node), 
					    src->pos.item);

			/*
			if ((res = repair_tree_copy(tree, &dst, src, &key)))
				return res;
			*/
			return 0;
		}

		if (!src->plug->o.item_ops->balance->lookup)
			break;
		
		/* Lookup by end_key. */
		if ((res = src->plug->o.item_ops->balance->lookup(src, &hint.maxkey, 
								  FIND_EXACT)) < 0)
			return res;
		
		if (src->pos.unit >= scount)
			break;

		reiser4_key_assign(&hint.offset, &hint.maxkey);
	}
	
	return 0;
	
 error:
	if (dst.pos.unit == MAX_UINT32) {
		aal_exception_error("Node (%llu), item (%u): insertion to the "
				    "node (%llu), item (%u) failed.", 
				    node_blocknr(src->node), src->pos.item, 
				    node_blocknr(dst.node),  dst.pos.item);
	} else {
		aal_exception_error("Node (%llu), item (%u): merging with node "
				    "(%llu), item (%u) by the key [%s] failed.",
				    node_blocknr(src->node), src->pos.item, 
				    node_blocknr(dst.node), dst.pos.item,
				    reiser4_print_key(&hint.offset, PO_INODE));
	}
	
	return res;
}

