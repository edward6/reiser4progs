/*
    tree.c -- repair/tree.c -- tree auxiliary code.
  
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/librepair.h>

/*
  This function returns TRUE if passed item @group corresponds to passed @level
  Hardcoded method, valid for the current tree imprementation only.
*/
bool_t repair_tree_legal_level(reiser4_item_group_t group,
			       uint8_t level)
{
    if (group == NODEPTR_ITEM) {
	if (level == LEAF_LEVEL)
	    return FALSE;
    } else if (group == EXTENT_ITEM) {
	if (level != TWIG_LEVEL)
	    return FALSE;
    } else
	return level == LEAF_LEVEL;

    return TRUE;
}

static errno_t callback_data_level(reiser4_plugin_t *plugin,
    void *data)
{
    uint8_t *level = (uint8_t *)data;

    aal_assert("vpf-746", data != NULL);

    if (plugin->h.type != ITEM_PLUGIN_TYPE)
	return 0;
    
    if (!repair_tree_legal_level(plugin->h.group, *level))
	return 0;

    return reiser4_item_data(plugin);
}

bool_t repair_tree_data_level(uint8_t level) {

    if (level == 0)
	return FALSE;
	
    return (libreiser4_factory_cfind(callback_data_level,
	&level) != NULL);
}

/* Corrects place for insertion over the base reiser4_tree_lookup method. */
/*
lookup_t repair_tree_lookup(reiser4_tree_t *tree, reiser4_key_t *key, 
    reiser4_place_t *place) 
{
    uint32_t items;
    lookup_t lookup;
     
    if ((lookup = reiser4_tree_lookup(tree, key, LEAF_LEVEL, place)) == LP_FAILED) {
	aal_stream_t stream;
	
	aal_stream_init(&stream);
	reiser4_key_print(key, &stream);
	aal_exception_error("Lookup of key %s failed.", stream.data);			
	aal_stream_fini(&stream);

	return lookup;
    } else if (lookup == LP_PRESENT) {
	return lookup;
    }

    items = reiser4_node_items(place->node);

    // Position was not found - place could point to a non existent position, 
    // move to the right item then. 
    if (place->pos.item < items) {
	if (reiser4_place_realize(place))
	    return LP_FAILED;

	if (place->pos.unit == reiser4_item_units(place)) {
	    place->pos.item++; 
	    place->pos.unit = ~0ul;

	    if (place->pos.item < items) {
		if (reiser4_place_realize(place))
		    return LP_FAILED;
	    }
	} 
    }   

    return LP_ABSENT;
}
*/

/* This function creates nodeptr item on the nase of 'node' and insert it to 
 * the tree. */
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
    if ((lookup = reiser4_tree_lookup(tree, &hint.key, LEAF_LEVEL, &place)) 
	!= LP_ABSENT)
	return lookup;
    
    /* If some node was found and it is not of higher level then the node being 
     * attached, try to split nodes to be able to attach the node as a whole. */
    level = reiser4_node_get_level(node) + 1;
    
    if (place.node != NULL && reiser4_node_get_level(place.node) < level) {
	/* Get the key of the found position or the nearest right key. */
	if (reiser4_place_rightmost(&place)) {
	    if ((res = repair_node_rd_key(place.node, &key)))
		return res;
	} else if ((res = reiser4_item_get_key(&place, &key)))
	    return res;
	
	/* Get the maximum key existing in the node being inserted. */
	if ((res = repair_node_max_real_key(node, &rkey)))
	    return res;
	
	/* If the most right key from the node being inserted is greater then 
	 * the key found by lookup, it is not possible to insert the node as 
	 * a whole. */
	if (reiser4_key_compare(&rkey, &key) >= 0)
	    return -ESTRUCT;
	
	/*
	if ((res = reiser4_tree_split(tree, &place, level)))
	    return res;
	*/
    }
    
    hint.type_specific = &ptr;
    hint.count = 1;
    hint.flags = HF_FORMATD;
    ptr.start = node->blk;
    ptr.width = 1;
    
    pid = reiser4_profile_value(tree->fs->profile, "nodeptr");

    if (!(hint.plugin = libreiser4_factory_ifind(ITEM_PLUGIN_TYPE, pid))) {
	aal_exception_error("Can't find item plugin by its id 0x%x.", pid);
	return -EINVAL;
    }

    if ((res = reiser4_tree_insert(tree, &place, level, &hint))) {
	aal_exception_error("Can't insert nodeptr item to the tree.");
	return res;
    }

    /* Setting needed links between nodes in the tree cashe. */
    if ((res = reiser4_tree_connect(tree, place.node, node))) {
	aal_exception_error("Can't attach the node %llu in tree cache.", 
	    node->blk);
	return res;
    }
    
    reiser4_tree_ltrt(tree, node, D_LEFT);
    reiser4_tree_ltrt(tree, node, D_RIGHT);
    
    return 0;
}

/* Copies item's data pointed by @src to @dst, from the key pointed by @src
 * place though the @end one. After the coping @end key points to the data
 * of the @src which has not being copied. */
errno_t repair_tree_copy(reiser4_tree_t *tree, reiser4_place_t *dst,
    reiser4_place_t *src, copy_hint_t *hint)
{
    reiser4_place_t old;
    uint32_t needed;
    errno_t res;
	
    aal_assert("vpf-948", tree != NULL); 
    aal_assert("vpf-949", dst != NULL);
    aal_assert("vpf-950", src != NULL);
    aal_assert("vpf-951", hint != NULL);
    
    if (hint->src_count == 0)
	return 0;
    
    if (reiser4_tree_fresh(tree)) {
	aal_exception_error("Tree copy failed. Tree is empty.");
	    return -EINVAL;
    }

    old = *dst;

    if (hint->len_delta > 0) {
	needed = hint->len_delta + (dst->pos.unit == ~0ul ? 
	    reiser4_node_overhead(dst->node) : 0);

	if ((res = reiser4_tree_expand(tree, dst, needed, SF_DEFAULT))) {
	    aal_exception_error("Tree expand for coping failed.");
	    return res;
	}
    }
    
    if ((res = repair_node_copy(dst->node, &dst->pos, src->node, 
	&src->pos, hint))) 
    {
	aal_exception_error("Node copying failed from node %llu, item %u to "
	    "node %llu, item %u one.", src->node->blk, src->pos.item, 
	    dst->node->blk, dst->pos.unit);
	
	return res;
    }
    
    if (reiser4_place_leftmost(dst) && dst->node->parent.node) {
	reiser4_place_t p;

	reiser4_place_init(&p, dst->node->parent.node, &dst->node->parent.pos);		
	if ((res = reiser4_tree_ukey(tree, &p, &src->item.key)))
	    return res;
    }
    
    if (dst->node != tree->root && !dst->node->parent.node) {		
	if (!old.node->parent.node)
	    reiser4_tree_growup(tree);
	
	if ((res = reiser4_tree_attach(tree, dst->node))) {
	    aal_exception_error("Can't attach node %llu to the tree.", 
		dst->node->blk);
	    
	    reiser4_tree_release(tree, dst->node);	    
	    return res;
	}
    }
    
    return 0;
}

/* Insert the item into the tree overwriting an existent in the tree item 
 * if needed. Does not insert branches. */
errno_t repair_tree_insert(reiser4_tree_t *tree, reiser4_place_t *src) {
    reiser4_key_t src_max, start_key;
    reiser4_place_t dst;
    copy_hint_t hint;
    lookup_t lookup;
    uint32_t src_units;
    errno_t ret;
    int res;
    bool_t whole = 1;

    aal_assert("vpf-654", tree != NULL);
    aal_assert("vpf-655", src != NULL);
    aal_assert("vpf-657", src->node != NULL);

    if (reiser4_item_branch(src))
	return -EINVAL;
    
    src_units = reiser4_item_units(src);
    reiser4_key_assign(&start_key, &src->item.key);
    
    while (1) {
	lookup = reiser4_tree_lookup(tree, &start_key, LEAF_LEVEL, &dst);
	
	/* Check if the whole item can be inserted at once. */
	do {
	    /* Item was checked once already. */
	    if (!whole)
		break;
	    
	    /* If lookup returns PRESENT or unit position is set */
	    if (lookup == LP_PRESENT) {
		whole = 0;
		break;
	    }

	    /* If we are on the last position, insert the whole. */
	    /* FIXME-VITALY: Lookup does not move to the right neighbour yet
	     * if it exists. So right neighbour should be checked here. */
	    if (dst.pos.item == reiser4_node_items(dst.node))
		break;

	    if ((res = reiser4_place_realize(&dst)))
		return res;
	    
	    /* It is not possible to say here if these items are mergable 
	     * or not(e.g. tail40 may get a hole here), so just insert a 
	     * new item. */
	    if (dst.pos.unit == reiser4_item_units(&dst)) {
		dst.pos.item++;
		dst.pos.unit = ~0ul;
		break;
	    } 
	    
	    if ((res = reiser4_item_get_key(&dst, NULL)))
		return res;
	    
	    if ((res = reiser4_item_maxreal_key(src, &src_max)))
		return res;

	    if (reiser4_key_compare(&src_max, &dst.item.key) >= 0)
		whole = 0;
	} while (0);
	
	aal_memset(&hint, 0, sizeof(hint));
	reiser4_key_assign(&hint.start, &start_key);
	
	if (whole) {
	    hint.len_delta = src->item.len;
	    hint.src_count = reiser4_item_units(src);
	    hint.dst_count = 0;
	    src->pos.unit = ~0ul;
	} else {
	    if ((res = reiser4_place_realize(&dst)))
		return res;
	    
	    if (dst.item.plugin->h.id != src->item.plugin->h.id) {
		/* FIXME: relocation code should be here. */
		aal_exception_error("Tree Overwrite failed to overwrite items "
		    "of different plugins. Source: node (%llu), item (%u), "
		    "unit (%u). Destination: node (%llu), items (%u), unit "
		    "(%u). Relocation is not supported yet.", src->node->blk, 
		    src->pos.item, src->pos.unit, dst.node->blk, dst.pos.item, 
		    dst.pos.unit);
		return 0;
	    }
	    
	    if ((res = reiser4_item_maxreal_key(&dst, &hint.end)))
		return res;
	    
	    if ((res = repair_item_feel_copy(&dst, src, &hint)))
		return res;
	}
	
	if ((ret = repair_tree_copy(tree, &dst, src, &hint))) {
	    aal_exception_error("Tree Copy failed. Source: node (%llu), "
		"item (%u), unit (%u). Destination: node (%llu), items "
		"(%u), unit (%u). Key interval %k - %k.", src->node->blk, 
		src->pos.item, src->pos.unit, dst.node->blk, dst.pos.item,
		dst.pos.unit, &hint.start, &hint.end);
	    return ret;
	}
	
	if (whole || !src->item.plugin->o.item_ops->lookup)
	    break;
	
	/* Lookup by end_key. */
	res = src->item.plugin->o.item_ops->lookup(&src->item, &hint.end, 
	    &src->pos.unit);

	if (src->pos.unit >= reiser4_item_units(src))
	    break;
	    
	reiser4_key_assign(&start_key, &hint.end);
    }

    return 0;
}

/* 
 * Check the place for key range insertion possibility. 
 * Split found node if it helps. 
 * 
 * FIXME: This method implies that it is run after lookup and the wanted key 
 * was not found. So start_key is not checked here.
 */
/*
static errno_t repair_tree_fits(reiser4_place_t *place,
    reiser4_key_t *start_key, reiser4_key_t *end_key)
{
    reiser4_key_t key;
    uint32_t items, units;    

    aal_assert("vpf-596", place != NULL);
    aal_assert("vpf-660", place->node != NULL);
    
    if (place->pos.item == items) {
	if (repair_node_rd_key(place->node, &key))
	    return -1;
    } else {
	if (reiser4_place_realize(place)) {
	    aal_exception_error("Node (%llu): failed to open the item on "
		"the parent node.", place->node->blk);
	    return -1;
	}

	aal_assert("vpf-671", place->pos.unit < reiser4_item_units(place));
	
	if (reiser4_item_get_key(place, &key)) {
	    aal_exception_error("Node (%llu): failed to get the item key "
		"by its place.", place->node->blk);	    
	    return -1;
	}
    }

    if (reiser4_key_compare(&key, end_key) >= 0)
	return 1;
    
    return 0;
}
*/

/*
 
static errno_t repair_tree_shift(reiser4_tree_t *tree, reiser4_place_t *place) {
    reiser4_node_t *node;
    uint32_t level;
    
    aal_assert("vpf-665", place != NULL);
    aal_assert("vpf-666", place->node != NULL);
    aal_assert("vpf-667", place->node->tree != NULL);

    if ((place->pos.item == 0 && place->pos.unit == 0) || 
	(place->pos.item == reiser4_node_items(place->node)))
	return 0;

    // Insertable but split should be performed. 
    // FIXME: place could be realised already. Optimise it later. 
    if (reiser4_place_realize(place))
	return -1;
		
    level = reiser4_node_level(place->node);
    
    if ((node = reiser4_tree_allocate(tree, level)) == NULL) {
	aal_exception_error("Tree failed to allocate a new node.");
	return -1;
    }

    // set flush_id 
    reiser4_node_set_flush_stamp(node, 
	reiser4_node_get_flush_stamp(place->node));
    
    if (reiser4_tree_shift(tree, place, node, SF_RIGHT)) {
	aal_exception_error("Tree failed to shift into a newly "
	    "allocated node.");
	goto error_node_free;
    }
	    
    aal_assert("vpf-640", reiser4_node_items(node) != 0);

    if (reiser4_tree_attach(tree, node)) {
	aal_exception_error("Tree failed to attach a newly allocated "
	    "node to the tree.");
	goto error_node_free;
    }

    return 0;

error_node_free:
    reiser4_node_release(node);
    return -1;
}

   // Check the level of the node we are inserting the internal item pointed 
   // to twig in.     
    level = reiser4_node_get_level(place.node);
 
    // FIXME: Do not forget to put this into the reiser4_tree_insert 
    if (stop.top != LEAF_LEVEL || level != TWIG_LEVEL) {
	// We lookuped till the child. If we are on the border - no split is 
	// needed, otherwise - split the child to 2 parts and insert the new 
	// node between them. 

	aal_assert("vpf-663", level == stop.top);

	if ((place.pos.item != 0 || place.pos.unit != 0) && 
	    (place.pos.item != reiser4_node_items(place.node))) 
	{	    
	    // Insertable but split should be performed. 
	    if (repair_tree_shift(tree, &place))
		return -1;

	    // Parent got changed after splitting, lookup the wanted key again 
	    // to have place updated. 
	    stop.top = stop.bottom = level + 1;
    
	    if (reiser4_tree_lookup(tree, &hint.key, &stop, &place)) {
		aal_exception_error("Lookup failed to find the proper place "
		    "for item insertion.");
		return -1;
	    }

	    aal_assert("vpf-664", 
		reiser4_node_get_level(place.node) == stop.top);
	} else {
	    // Correct place to point to parent. 
	    if (reiser4_node_pos(place.node, &place.pos)) 
		return -1;

	    place.node = place.node->parent;

	    aal_assert("vpf-662", place.node != NULL);
	}
    } else {
	// Nothing to do - there is no one leaf yet, we found the position in 
	// the parent and node is insertable. 
    }

*/


