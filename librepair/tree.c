/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   tree.c -- repair/tree.c -- tree auxiliary code. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/librepair.h>

/* This function returns TRUE if passed item @group corresponds to passed 
   @level Hardcoded method, valid for the current tree imprementation only. */
bool_t repair_tree_legal_level(reiser4_plug_t *plug, uint8_t level) {
	if (reiser4_item_branch(plug))
		return level != LEAF_LEVEL;
	
	if (plug->id.group == EXTENT_ITEM)
		return level == TWIG_LEVEL;
	
	return level == LEAF_LEVEL;
}

static errno_t cb_data_level(reiser4_plug_t *plug, void *data) {
	uint8_t *level = (uint8_t *)data;
	
	aal_assert("vpf-746", data != NULL);
	
	if (plug->id.type != ITEM_PLUG_TYPE)
		return 0;
	
	if (!repair_tree_legal_level(plug, *level))
		return 0;
	
	return !reiser4_item_branch(plug);
}

bool_t repair_tree_data_level(uint8_t level) {
	if (level == 0)
		return 0;
	
	return (reiser4_factory_cfind(cb_data_level, &level) != NULL);
}

/* Get the max real key existed in the tree. Go down through all right-most 
   child to get it. */
static errno_t repair_tree_maxreal_key(reiser4_tree_t *tree, 
				       reiser4_node_t *node, reiser4_key_t *key)
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
		aal_error("Node (%llu): Failed to open the item (%u).", 
			  node->block->nr, place.pos.item);
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
		
		reiser4_node_lock(child);
		
		res = repair_tree_maxreal_key(tree, child, key);
		
		reiser4_node_unlock(child);
		
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
		
		aal_memcpy(key, &node->p.key, sizeof(*key));
	} else {
		key->plug = tree->key.plug;
		reiser4_key_minimal(key);
	}
	
	return 0;
}

reiser4_node_t *repair_tree_load_node(reiser4_tree_t *tree, 
				      reiser4_node_t *parent,
				      blk_t blk, bool_t check)
{
	reiser4_node_t *node;
	
	aal_assert("vpf-1500", tree != NULL);
	aal_assert("vpf-1502", tree->fs != NULL);
	
	if (!(node = reiser4_tree_load_node(tree, parent, blk)))
		return NULL;

	/* If @check, check the mkfs_id. */
	if (check && reiser4_format_get_stamp(tree->fs->format) != 
	    reiser4_node_get_mstamp(node))
	{
		goto error_unload_node;
	}

	return node;
	
 error_unload_node:
	reiser4_tree_unload_node(tree, node);
	return NULL;
}

/* Checks the delimiting keys of the node kept in the parent. */
errno_t repair_tree_dknode_check(reiser4_tree_t *tree, 
				 reiser4_node_t *node, 
				 uint8_t mode) 
{
	reiser4_key_t key_max, dkey;
	reiser4_place_t place;
	errno_t res;
	int comp;
	
	aal_assert("vpf-1281", tree != NULL);
	aal_assert("vpf-248", node != NULL);
	aal_assert("vpf-250", node->plug != NULL);
	aal_assert("vpf-1280", node->tree != NULL);
	
	/* Initialize to the rightmost position. */
	place.node = node;
	place.pos.unit = MAX_UINT32;
	place.pos.item = reiser4_node_items(node);
	
	/* Get the right delimiting key. */
	if ((res = reiser4_tree_place_key(tree, &place, &dkey))) {
		aal_error("Node (%llu): Failed to get the right "
			  "delimiting key.", node->block->nr);
		return res;
	}

	/* Move to the last item. */
	place.pos.item--;
	if ((res = reiser4_place_fetch(&place)) < 0) 
		return res;

	/* Get the maxreal key of the last item. */
	if ((res = reiser4_item_maxreal_key(&place, &key_max)) < 0) {
		aal_error("Node (%llu): Failed to get the max real "
			  "key of the last item.", node->block->nr);
		return res;
	}
	
	/* Fatal error if the maxreal key greater than the right 
	   delimiting key. */
	if (reiser4_key_compfull(&key_max, &dkey) > 0) {
		fsck_mess("Node (%llu): The last key [%s] in the node "
			  "is greater then the right delimiting key "
			  "[%s].", node->block->nr, 
			  reiser4_print_key(&key_max, PO_DEFAULT),
			  reiser4_print_key(&dkey, PO_DEFAULT));
		return RE_FATAL;
	}
	
	/* Get the left delimiting key. */
	if ((res = repair_tree_parent_lkey(tree, node, &dkey))) {
		aal_error("Node (%llu): Failed to get the left "
			  "delimiting key.", node->block->nr);
		return res;
	}

	/* Move to the 0-th item. */
	place.pos.item = 0;
	if ((res = reiser4_place_fetch(&place)))
		return res;
	
	if (!(comp = reiser4_key_compfull(&dkey, &place.key)))
		return 0;
	
	/* Left delimiting key should match the left key in the node. */
	if (comp > 0) {
		/* The left delimiting key is much then the left key in the 
		   node - not legal */
		fsck_mess("Node (%llu): The first key [%s] is not equal to the "
			  "left delimiting key [%s].", node->block->nr, 
			  reiser4_print_key(&place.key, PO_DEFAULT),
			  reiser4_print_key(&dkey, PO_DEFAULT));
		return RE_FATAL;
	}
	

	/* It is legal to have the left key in the node much then its left 
	   delimiting key - due to removing some items from the node, for 
	   example. Fix the delemiting key if we have parent. */
	if (node->p.node == NULL) 
		return 0;

	fsck_mess("Node (%llu): The left delimiting key [%s] in the "
		  "parent node (%llu), pos (%u/%u) does not match the "
		  "first key [%s] in the node.%s", node->block->nr,
		  reiser4_print_key(&place.key, PO_DEFAULT),
		  place_blknr(&node->p), place.pos.item, 
		  place.pos.unit, reiser4_print_key(&dkey, PO_DEFAULT),
		  mode == RM_BUILD ? " Fixed." : "");

	if (mode != RM_BUILD)
		return RE_FATAL;

	/* Update the left delimiting key if if less than the key of the 0-th
	   item in the node and this is BUILD mode. */
	return reiser4_tree_update_keys(tree, &node->p, &place.key);
}

/* This function creates nodeptr item on the base of @node and insert it 
   to the tree. */
errno_t repair_tree_attach_node(reiser4_tree_t *tree, reiser4_node_t *node) {
	reiser4_key_t rkey, key;
	reiser4_place_t place;
	lookup_hint_t lhint;
	lookup_t lookup;
	errno_t res;
	
	aal_assert("vpf-658", tree != NULL);
	aal_assert("vpf-659", node != NULL);
	
	/* If there is no root in the tree yet, set it to @node. */
	if (reiser4_tree_fresh(tree))
		return reiser4_tree_assign_root(tree, node);
	
	aal_memset(&lhint, 0, sizeof(lhint));
	
	lhint.key = &key;
	lhint.level = LEAF_LEVEL;
	reiser4_node_leftmost_key(node, lhint.key);
	
	/* Key should not exist in the tree yet. */
	lookup = reiser4_tree_lookup(tree, &lhint, FIND_EXACT, &place);
	
	switch (lookup) {
	case PRESENT:
		return -ESTRUCT;
	case ABSENT:
		break;
	default:
		return lookup;
	}
	
	/* Check that the node does not overlapped anything by keys. */
	if (place.node != NULL) {
		if (reiser4_place_right(&place))
			reiser4_place_inc(&place, 1);

		if ((res = reiser4_tree_place_key(tree, &place, &key)))
			return res;
		
		/* Get the maxreal existing key of the node being inserted. */
		if ((res = repair_tree_maxreal_key(tree, node, &rkey)))
			return res;
		
		/* If the most right key from the node being inserted is 
		   greater then the key found by lookup, it is not possible 
		   to insert the node as a whole. */
		if (reiser4_key_compfull(&rkey, &key) >= 0)
			return -ESTRUCT;
	}
	
	return reiser4_tree_attach_node(tree, node, &place, SF_DEFAULT);
}

/* Check that conversion is needed. */
static bool_t repair_tree_need_conv(reiser4_tree_t *tree, 
				    reiser4_plug_t *from,
				    reiser4_plug_t *to)
{
	aal_assert("vpf-1293", tree != NULL);
	aal_assert("vpf-1294", from != NULL);
	aal_assert("vpf-1295", to != NULL);
	aal_assert("vpf-1296", to != NULL);
		
	/* Conversion is not needed for equal plugins. */
	if (plug_equal(from, to))
		return 0;
	
	/* Conversion is needed for equal plugin groups. */
	if (from->id.group == to->id.group)
		return 1;

	/* TAIL->EXTENT conversion is needed. */
	if (from->id.group == TAIL_ITEM && to->id.group == EXTENT_ITEM)
		return 1;

	/* EXTENT->TAIL conversion is not needed. */
	if (from->id.group == EXTENT_ITEM && to->id.group == TAIL_ITEM)
		return 0;
	
	/* Other kind of conversions are impossible. */
	return -EINVAL;
}

/* Prepare repair convertion and perform it. */
static errno_t repair_tree_conv(reiser4_tree_t *tree, 
				reiser4_place_t *dst,
				reiser4_place_t *src) 
{
	conv_hint_t hint;

	/* Set bytes, plug, offset and count in @hint */
	hint.chunk = 0;
	hint.bytes = 0;
	hint.plug = src->plug;
	hint.place_func = NULL;
	
	reiser4_key_assign(&hint.offset, &dst->key);
	reiser4_key_set_offset(&hint.offset, 
			       reiser4_key_get_offset(&src->key));

	hint.count = plug_call(src->plug->o.item_ops->object,
			       size, src);

	return reiser4_flow_convert(tree, &hint);
}

/* Lookup for the correct @place place by the @start key in the @tree. */
static errno_t repair_tree_lookup(reiser4_tree_t *tree, 
				  reiser4_place_t *dst,
				  reiser4_place_t *src,
				  reiser4_key_t *key)
{
	reiser4_key_t dkey, end;
	lookup_hint_t lhint;
	reiser4_place_t prev;
	errno_t res;
	int skip = 0;
	
	aal_assert("vpf-1364", tree  != NULL);
	aal_assert("vpf-1365", dst != NULL);
	aal_assert("vpf-1367", src != NULL);
	aal_assert("vpf-1689", key != NULL);

	aal_memset(&lhint, 0, sizeof(lhint));
	lhint.level = LEAF_LEVEL;
	lhint.key = key;
	lhint.collision = NULL;
	lhint.hint = NULL;
	
	res = reiser4_tree_lookup(tree, &lhint, FIND_EXACT, dst);
	
	if (res != ABSENT)
		return res;

	/* Absent. If non-existent unit or item, there is nothing mergable 
	   from the right side--lookup would go down there in that case.  */
	if (reiser4_place_right(dst))
		/* Step to right. */
		reiser4_place_inc(dst, 1);

	aal_memset(&prev, 0, sizeof(prev));
	
	if (reiser4_place_rightmost(dst)) {
		prev = *dst;

		reiser4_node_lock(prev.node);
		
		if ((res = reiser4_tree_next_place(tree, dst, dst))) {
			aal_error("Failed to get the next node.");
			reiser4_node_unlock(prev.node);
			return res;
		}
		
		reiser4_node_unlock(prev.node);
		
		/* No right node. */
		if (!dst->node)
			skip = 1;
	} 

	if (skip == 0) {
		/* Get the current key of the @dst. */
		if (reiser4_place_fetch(dst))
			return -EIO;

		if ((res = reiser4_item_get_key(dst, &dkey)))
			return res;

		if ((res = reiser4_item_maxreal_key(src, &end)))
			return res;

		/* If @end key is not less than the lookuped, items are overlapped. 
		   Othewise move to the previous pos. */
		if ((res = reiser4_key_compfull(&end, &dkey)) >= 0) 
			return PRESENT;
	}

	if (prev.node)
		*dst = prev;

	/* Do not care about the  */
	dst->plug = NULL;

	return ABSENT;
}

static errno_t cb_prep_insert_raw(reiser4_place_t *place, 
				  trans_hint_t *hint) 
{
	return plug_call(hint->plug->o.item_ops->repair, 
			 prep_insert_raw, place, hint);
}

static errno_t cb_insert_raw(reiser4_node_t *node, pos_t *pos, 
			     trans_hint_t *hint) 
{
	return plug_call(node->plug->o.node_ops, insert_raw, node, pos, hint);
}

/* Insert the item into the tree overwriting an existent in the tree item 
   if needed. Does not insert branches. */
errno_t repair_tree_insert(reiser4_tree_t *tree, reiser4_place_t *src, 
			   region_func_t func, void *data)
{
	reiser4_place_t dst;
	lookup_hint_t lhint;
	trans_hint_t hint;
	uint32_t scount;
	uint8_t level;
	errno_t res;
	
	aal_assert("vpf-654", tree != NULL);
	aal_assert("vpf-655", src != NULL);
	aal_assert("vpf-657", src->node != NULL);
	
#ifdef ENABLE_DEBUG
	if (reiser4_tree_fresh(tree)) {
		aal_bug("vpf-1518", "Failed to insert into the fresh tree.");
	}
#endif
		
	if (reiser4_item_branch(src->plug))
		return -EINVAL;
	
	scount = reiser4_item_units(src);
	src->pos.unit = 0;
	
	/* Init the src maxreal key. */
	
	aal_memset(&hint, 0, sizeof(hint));
	hint.specific = src;
	hint.plug = src->plug;
	hint.place_func = NULL;
	hint.region_func = func;
	hint.data = data;
	hint.shift_flags = SF_DEFAULT;
	
	reiser4_key_assign(&hint.offset, &src->key);
	level = reiser4_node_get_level(src->node);
	
	while (1) {
		if ((res = repair_tree_lookup(tree, &dst, 
					      src, &hint.offset)) < 0)
		{
			return res;
		}
		
		/* Convert @dst if needed. */
		if (dst.plug) {
			bool_t conv;

			conv = repair_tree_need_conv(tree, dst.plug, src->plug);
			
			if (conv < 0) {
				/* Convertion is not possible. */
				fsck_mess("Node (%llu), item (%u): the item (%s) "
					  "[%s] is overlapped by keys with the "
					  "item (%s) [%s] being inserted [node "
					  "%llu, item %u]. Skip insertion.",
					  place_blknr(&dst), dst.pos.item,
					  dst.plug->label,
					  reiser4_print_key(&dst.key, PO_DEFAULT),
					  src->plug->label,
					  reiser4_print_key(&src->key, PO_DEFAULT),
					  place_blknr(src), src->pos.item);

				   return RE_FATAL;
			}
			
			if (conv) {
				res = repair_tree_conv(tree, &dst, src);
				if (res) return res;

				/* Repeat lookup after @dst conversion. */
				continue;
			}
			
			/* Convertion is not needed, places are 
			   overlapped, adjust the position. */
			if (dst.pos.unit == MAX_UINT32)
				dst.pos.unit = 0;
		}

		/* Insert_raw can insert the whole item (!dst.plug) or if dst &
		   src items are of the same plugin. */
		if ((dst.plug && plug_equal(dst.plug, src->plug)) || !dst.plug)
		{
			res = reiser4_tree_modify(tree, &dst, &hint, level,
						  cb_prep_insert_raw,
						  cb_insert_raw);
			
			if (res) goto error;
		} else {
			/* For not equal plugins do coping. */
			fsck_mess("Node (%llu), item (%u): the item (%s) [%s] "
				  "is overlapped by keys with the item (%s) "
				  "[%s] being inserted [node %llu, item %u]. "
				  "Copying is not ready yet, skip insertion.",
				  place_blknr(&dst), dst.pos.item, 
				  dst.plug->label,
				  reiser4_print_key(&dst.key, PO_DEFAULT),
				  src->plug->label,
				  reiser4_print_key(&src->key, PO_DEFAULT),
				  place_blknr(src), src->pos.item);

			return RE_FATAL;
		}

		if (!src->plug->o.item_ops->balance->lookup)
			break;

		lhint.key = &hint.maxkey;
			
		/* Lookup by end_key. */
		res = src->plug->o.item_ops->balance->lookup(src, &lhint,
							     FIND_EXACT);

		if (res < 0) return res;
		
		if (src->pos.unit >= scount)
			break;

		reiser4_key_assign(&hint.offset, &hint.maxkey);
	}
	
	return 0;
	
 error:
	if (dst.pos.unit == MAX_UINT32) {
		aal_error("Node (%llu), item (%u): insertion to the "
			  "node (%llu), item (%u) failed.", 
			  place_blknr(src), src->pos.item, 
			  place_blknr(&dst),  dst.pos.item);
	} else {
		aal_error("Node (%llu), item (%u): merging with node "
			  "(%llu), item (%u) by the key [%s] failed.",
			  place_blknr(src), src->pos.item, 
			  place_blknr(&dst), dst.pos.item,
			  reiser4_print_key(&hint.offset, PO_INODE));
	}
	
	return res;
}

