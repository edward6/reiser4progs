/*
    librepair/twig_scan.c - methods are needed for the second fsck pass. 
    Copyright (C) 1996-2002 Hans Reiser.

    The twig_scan pass - fsck zeros extent pointers which point to an already 
    used block. Builds a map of used blocks.
*/

#include <repair/librepair.h>

/* 
    Zero extent pointers which point to an already used block (leaf, format 
    area) or out of format area. .
*/
static errno_t callback_ptr_handler(reiser4_coord_t *coord, void *data) {
    repair_data_t *rd = (repair_data_t *)data;
    reiser4_ptr_hint_t ptr;
    repair_ts_t *ts;
    int res;
 
    aal_assert("vpf-384", coord != NULL, return -1);
    aal_assert("vpf-385", rd != NULL, return -1);

    if (!reiser4_item_extent(coord))
	return 0;
 
    ts = repair_ts(rd);

    aal_assert("vpf-567", ts->bm_met != NULL, return -1);
 
    if (plugin_call(return -1, coord->item.plugin->item_ops,
	fetch, &coord->item, &ptr, coord->pos.unit, 1) != 1)
	return -1;

    /* This must be fixed at the first pass. */
    aal_assert("vpf-387", 
	(ptr.ptr < ts->bm_met->total) && (ptr.width < ts->bm_met->total) && 
	(ptr.ptr < ts->bm_met->total - ptr.width), return -1);

    /* If extent item points to a leaf, to format area or out of format area. */
    res = repair_item_ptr_unused(coord, ts->bm_met);
 
    if (res < 0)
	/* Fatal error. */
	return res;
    else if ((res > 0) && repair_item_handle_ptr(coord)) 
	return -1;
    else if (aux_bitmap_test(ts->bm_used, coord->node->blk))
	aux_bitmap_mark_range(ts->bm_unfm_tree, ptr.ptr, ptr.ptr + ptr.width);
    else
	aux_bitmap_mark_range(ts->bm_unfm_out, ptr.ptr, ptr.ptr + ptr.width);
	
    return 0;
}

static errno_t repair_ts_setup(traverse_hint_t *hint, repair_data_t *rd) {
    repair_ts_t *ts;
    uint32_t i;
    
    aal_assert("vpf-526", hint != NULL, return -1);
    aal_assert("vpf-565", rd != NULL, return -1);
    
    ts = repair_ts(rd);
    
    aal_assert("vpf-566", ts->bm_used != NULL, return -1);
    aal_assert("vpf-568", ts->bm_twig != NULL, return -1);
    aal_assert("vpf-569", ts->bm_leaf != NULL, return -1);
    aal_assert("vpf-570", ts->bm_met != NULL, return -1);
    aal_assert("vpf-571", ts->bm_unfm_tree != NULL, return -1);

    hint->data = rd;
    hint->objects = 1 << EXTENT_ITEM;
    hint->cleanup = 1;
 
    /* Build the map of blocks which cannot be pointed by extent. */
    for (i = 0; i < ts->bm_met->size; i++) {
	/* bm_met is bm_frmt | bm_used | bm_leaf | bm_twig */
	ts->bm_met->map[i] |= (ts->bm_used->map[i] | ts->bm_twig->map[i] | 
	    ts->bm_leaf->map[i]);
    }
 
    aux_bitmap_clear_all(ts->bm_unfm_tree);
 
    if (!(ts->bm_unfm_out = 
	aux_bitmap_create(reiser4_format_get_len(rd->fs->format)))) 
    {
	aal_exception_error("Failed to allocate a bitmap for extents of "
	    "unconnected twigs.");
	return -1;
    }

    return 0;
}

static errno_t repair_ts_update(repair_data_t *rd) {
    repair_ts_t *ts;
    uint32_t i;
 
    aal_assert("vpf-577", rd != NULL, return -1);
    aal_assert("vpf-593", rd->fs != NULL, return -1);

    ts = repair_ts(rd);
 
    for (i = 0; i < ts->bm_met->size; i++) {
	aal_assert("vpf-576", (ts->bm_met->map[i] & 
	    (ts->bm_unfm_tree->map[i] | ts->bm_unfm_out->map[i])) == 0, 
	    return -1);

	ts->bm_met->map[i] |= ts->bm_unfm_tree->map[i] | ts->bm_unfm_out->map[i];
	ts->bm_used->map[i] |= ts->bm_unfm_tree->map[i];
    }

    /* Assign the bm_met bitmap to the block allocator. */
    reiser4_alloc_assign(rd->fs->alloc, ts->bm_used);
    reiser4_alloc_assign_forb(rd->fs->alloc, ts->bm_met);

    aux_bitmap_close(ts->bm_met);
    aux_bitmap_close(ts->bm_unfm_tree);
    aux_bitmap_close(ts->bm_unfm_out);
    
    return 0;
}

errno_t repair_ts_pass(repair_data_t *rd) {
    reiser4_node_t *node;
    object_entity_t *entity;
    traverse_hint_t hint;
    repair_ts_t *ts;
    errno_t res;
    blk_t blk = 0;

    aal_assert("vpf-533", rd != NULL, return -1);
    aal_assert("vpf-534", rd->fs != NULL, return -1);
    
    ts = repair_ts(rd);
    
    if (repair_ts_setup(&hint, rd))
	return -1;

    /* There were found overlapped extents. Look through twigs, build list of
     * extents for each problem region. */ 
    while ((blk = aux_bitmap_find_marked(ts->bm_twig, blk)) != INVAL_BLK) {
	if ((node = repair_node_open(rd->fs->format, blk)) == NULL) {
	    aal_exception_fatal("Twig scan pass failed to open the twig (%llu)",
		blk);
	    return -1;
	}

	entity = node->entity;
	
	/* This block must contain twig. */
	aal_assert("vpf-544", plugin_call(goto error_node_free, 
	    entity->plugin->node_ops, get_level, entity) == TWIG_LEVEL, 
	    goto error_node_free);

	/* Lookup the node. */	
	if ((res = reiser4_node_traverse(node, &hint, NULL, NULL,
	    callback_ptr_handler, NULL, NULL)))
	    goto error_node_free;

	if (!node->counter)
	    reiser4_node_close(node);

	/* Do not keep twig marked in bm_twigs if it is in the tree already. */
	if (aux_bitmap_test(ts->bm_used, blk))
	    aux_bitmap_clear(ts->bm_twig, blk);
    }

    if (repair_ts_update(rd))
	return -1;
    
    return 0;

error_node_free:
    reiser4_node_close(node);

    return -1;
}


#if 0

static int comp_extent_blks(const void *elem, const void *needle, void *data) {
    repair_ovrl_t *ovrl = (repair_ovrl_t *)elem;
    repair_ovrl_t *new = (repair_ovrl_t *)needle;
    
    /* FIXME-VITALY: Correct this when Ymka set all list callbacks of the same 
     * interface */
    return ovrl->ptr.ptr >= new->ptr.ptr ? 1 : 0;
}

/*
static int comp_ovrl_index(const void *elem, const void *needle, void *data) {
    repair_ovrl_region_t *region = (repair_ovrl_region_t *)elem;
    repair_ovrl_region_t *new = (repair_ovrl_region_t *)needle;
 
    // FIXME-VITALY: Correct this when Ymka set all list callbacks of the same interface 
    return region->index >= new->index ? 1 : 0;
}
*/

/* Coord pints to a problem extent. Save it into ovrl_list for futher handling. */
static errno_t repair_ts_ovrl_add(reiser4_coord_t *coord, repair_data_t *rd) {
    repair_ovrl_t *ovrl;
    reiser4_node_t *node;
    aal_list_t *list;
    repair_ts_t *ts;
    
    aal_assert("vpf-520", coord != NULL, return -1);
    aal_assert("vpf-521", rd != NULL, return -1);
    aal_assert("vpf-524", rd->alloc != NULL, return -1);
    aal_assert("vpf-525", rd->alloc->entity != NULL, return -1);

    if (!(node = coord->node)) {
	aal_exception_fatal("Failed to get the node from the coord, but "
	    "it is expected that the coord is build on the node.");
	return -1;
    }

    /* Pin it to avoid later closing. */
    node->counter++;

    ts = repair_ts(rd);

    ovrl = aal_malloc(sizeof(*ovrl));
    
    if (plugin_call(goto error_free_ovrl, coord->item.plugin->item_ops, fetch, 
	&coord->item, coord->pos.unit, &ovrl->ptr, 1) != 1)
	goto error_free_ovrl;

    ovrl->node = coord->node;
    ovrl->pos = coord->pos;	
    ovrl->conflicts = 0;
    
    /* Insert it into the extent list in sorted by ptr.ptr order. */ 
    /* FIXME-VITALY: This is too slow, tree should be used here instead. */
    list = aal_list_insert_sorted(ts->ovrl_list, ovrl, comp_extent_blks, NULL);

    if (!ts->ovrl_list)
	ts->ovrl_list = list;

    ts->ovrl_list = aal_list_first(ts->ovrl_list);
    
    return 0;
    
error_free_ovrl:    
    aal_free(ovrl);
    
    return -1;
}

static errno_t handle_ovrl_extents(aal_list_t **ovrl_list) {
    aal_list_t *left, *right;
    repair_ovrl_t *l_ovrl, *r_ovrl, *max_conflict = NULL;
    blk_t r_bound;
    
    aal_assert("vpf-552", ovrl_list != NULL, return -1);
    aal_assert("vpf-553", *ovrl_list != NULL, return -1);
   
    /* Calculate the initial conflics into ovrl_coord's */
    for (left = aal_list_first(*ovrl_list); left != NULL; left = left->next) {
	l_ovrl = (repair_ovrl_t *)left->data;
	    
	r_bound = l_ovrl->ptr.ptr + l_ovrl->ptr.width;

	for (right = left; right != NULL; right = right->next) {
	    r_ovrl = (repair_ovrl_t *)right->data;

	    if (r_bound > r_ovrl->ptr.ptr) {
		l_ovrl->conflicts++;
		r_ovrl->conflicts++;
	    } else 
		break;		
	}
    }

    /* Build the list of repair_ovrl, sorted by conflict value. */
    for (left = aal_list_first(*ovrl_list); left != NULL; left = left->next) {
	l_ovrl = (repair_ovrl_t *)left->data;
	
	if (max_conflict == NULL) {
	    max_conflict = l_ovrl;
	    continue;
	}
	
	r_ovrl = max_conflict;

	if (r_ovrl->conflicts < l_ovrl->conflicts) {
	    l_ovrl->next = r_ovrl;
	    r_ovrl->prev = l_ovrl;
	    max_conflict = l_ovrl;
	    continue;
	}

	while (r_ovrl->next && r_ovrl->next->conflicts > l_ovrl->conflicts) 
	    r_ovrl = r_ovrl->next;

	if (!r_ovrl->next) {
	    l_ovrl->prev = r_ovrl;
	    r_ovrl->next = l_ovrl;
	    continue;
	}

	r_ovrl->next->prev = l_ovrl;
	l_ovrl->next = r_ovrl->next;
	r_ovrl->next = l_ovrl;
	l_ovrl->prev = r_ovrl;
    }
    
     /* Choose the coord with the largest conflicts value, remove it, 
     * recalculate conflicts. Continue it unless there is no any conflict. */
    while (max_conflict && max_conflict->conflicts) {
	reiser4_coord_t coord;
	
	if (reiser4_coord_open(&coord, max_conflict->node, CT_JOINT, 
	    &max_conflict->pos)) 
	{
	    aal_exception_error("Can't open item by coord. Node %llu, item %u.",
		max_conflict->node->node->blk, max_conflict->pos.item);
	    return -1;
	}
	
	repair_item_handle_ptr(&coord);

	/* FIXME-VITALY: sync it somehow. */
	r_ovrl = max_conflict;
	max_conflict = max_conflict->next;

	if (r_ovrl->next)
	    r_ovrl->next->prev = r_ovrl->prev;
	if (r_ovrl->prev)
	    r_ovrl->prev->next = r_ovrl->next;
	
	*ovrl_list = aal_list_remove(*ovrl_list, r_ovrl);
	
	r_ovrl->node->counter--;
	reiser4_node_close(r_ovrl->node);
	aal_free(r_ovrl);

	/* Descrement conflict counters of all coords around max_conflict, 
	 * which conflicted with r_ptr. */

    }
   
    return 0;
}

static errno_t repair_ts_ovrl_list_free(aal_list_t **ovrl_list, 
    ovrl_region_func_t func)
{
    repair_ovrl_region_t *region;
    repair_ovrl_coord_t *oc;

    aal_assert("vpf-548", ovrl_list != NULL, return -1);
    aal_assert("vpf-549", *ovrl_list != NULL, return -1);

    // If we had bad unfm blocks we should not get 0-length overlapped extent list.
    aal_assert("vpf-550", aal_list_length(*ovrl_list) != 0, return -1);    
    
    while (*ovrl_list != NULL) {
	region = (repair_ovrl_region_t *)aal_list_first(*ovrl_list)->data;

	aal_assert("vpf-551", aal_list_length(region->extents) != 0, 
	    return -1);

	if (func(region))
	    return -1;
	
	*ovrl_list = aal_list_remove(*ovrl_list, region);
	
	while (region->extents != NULL) {

	    oc = (repair_ovrl_coord_t *)aal_list_first(region->extents)->data;
	    region->extents = aal_list_remove(region->extents, oc);
	    
	    // FIXME-VITALY: close the node, sync it if needed. 
	    reiser4_node_close(reiser4_coord_node(oc->coord));
	    
	    aal_free(oc->coord);
	    aal_free(oc);
	}

	aal_free(region);
    }
    
    return 0;
}
#endif
