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
    level = reiser4_node_get_level(node);
    
    if (place.node != NULL && reiser4_node_get_level(place.node) <= level) {
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
	
	if ((res = reiser4_tree_split(tree, &place, level)))
	    return res;
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

    if ((res = reiser4_tree_insert(tree, &place, level + 1, &hint))) {
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

/* Insert the item into the tree overwriting an existent in the tree item 
 * if needed. Does not insert branches. */
errno_t repair_tree_insert(reiser4_tree_t *tree, reiser4_place_t *src) {
    reiser4_key_t end_key, max_real_key;
    reiser4_place_t dst;
    lookup_t lookup;
    errno_t ret;
    int res;

    aal_assert("vpf-654", tree != NULL);
    aal_assert("vpf-655", src != NULL);
    aal_assert("vpf-657", src->node != NULL);

    if (reiser4_item_branch(src))
	return -EINVAL;
		
    if ((ret = reiser4_item_maxreal_key(src, &max_real_key)))
	return ret;
    
    while (1) {
	if ((ret = reiser4_item_get_key(src, NULL))) {
	    aal_exception_error("Node (%llu), item (%u), unit (%u): failed to "
		"get the item key.", src->node->blk, src->pos.item, 
		src->pos.unit);
	    return ret;
	}
	
	lookup = reiser4_tree_lookup(tree, &src->item.key, LEAF_LEVEL, &dst);
	
	switch (lookup) {
	case LP_ABSENT:
	    /* Start key does not exist in the tree. Prepare the insertion. */
	    if (reiser4_place_rightmost(&dst)) {
		if ((ret = repair_node_rd_key(dst.node, &end_key)))
		    return ret;
	    } else if ((ret = reiser4_item_get_key(&dst, &end_key)))
		return ret;
	    
	    if (src->item.plugin->h.id == ITEM_EXTENT40_ID)
		return 1;

	    if ((ret = reiser4_tree_copy(tree, &dst, src, &end_key))) {
		aal_exception_error("Tree Copy failed. Source: node (%llu), "
		    "item (%u), unit (%u). Destination: node (%llu), items "
		    "(%u), unit (%u). Key interval %k - %k.", src->node->blk, 
		    src->pos.item, src->pos.unit, dst.node->blk, dst.pos.item,
		    dst.pos.unit, &src->item.key, &end_key);
		return ret;
	    }
	    break;
	case LP_PRESENT:
	    /* Start key exists in the tree. Prepare the overwriting. */
	    
	    /* There are some item plugins which have gaps in keys between their 
	     * units - like direntry40 - check that. Use the special method - 
	     * item_ops.gap_key - which get the max real key stored continously 
	     * from the key specified in the dst. */	    
	    if (dst.item.plugin->h.id != src->item.plugin->h.id) {
		/* FIXME: relocation code should be here. */
		aal_exception_error("Tree Overwrite failed to overwrite items "
		    "of different plugins. Source: node (%llu), item (%u), "
		    "unit (%u). Destination: node (%llu), items (%u), unit "
		    "(%u). Key interval %k - %k. Relocation is not supported "
		    "yet.", src->node->blk, src->pos.item, src->pos.unit, 
		    dst.node->blk, dst.pos.item, dst.pos.unit, &src->item.key,
		    &end_key);
		return -EINVAL;
	    }
	    
	    if (src->item.plugin->h.id == ITEM_EXTENT40_ID)
		return 1;
		
	    if ((ret = reiser4_item_gap_key(&dst, &end_key))) 
		return ret;
	    
	    /* If the max_real_key is less than gap source key - overwrite 
	     * until max_real_key. */
	    res = reiser4_key_compare(&end_key, &max_real_key);
	    if (res > 0)
		end_key = max_real_key;
	    
	    if ((ret = reiser4_tree_overwrite(tree, &dst, src, &end_key))) {
		aal_exception_error("Tree Overwrite failed. Source: node (%llu), "
		    "item (%u), unit (%u). Destination: node (%llu), items "
		    "(%u), unit (%u). Key interval %k - %k.", src->node->blk, 
		    src->pos.item, src->pos.unit, dst.node->blk, dst.pos.item, 
		    dst.pos.unit, &src->item.key, &end_key);
		return ret;
	    }
	    break;
	default:
	    return lookup;
	}
	
	/* Lookup by end_key. */
	if (!src->item.plugin->item_ops.lookup)
	    break;
	
	res = src->item.plugin->item_ops.lookup(&src->item, &end_key, 
	    &src->pos.unit);
	
	if (src->pos.unit >= reiser4_item_units(src))
	    break;
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


