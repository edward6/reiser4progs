/*
    librepair/twig_scan.c - methods are needed for the second fsck pass. 

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

/*
    Description: fsck on this pass zeroes extent pointers which point to an 
    already used block. Builds a map of used blocks.
*/

#include <repair/twig_scan.h>

/* Check unfm block pointer if it points to an already used block (leaf, format 
 * area) or out of format area. Return 1 if it does, 0 - does not, -1 error. */
static errno_t callback_item_region_check(void *object, blk_t start, 
    uint64_t count, void *data) 
{
    item_entity_t *item = (item_entity_t *)object;
    repair_ts_t *ts = (repair_ts_t *)data;
    int res;

    aal_assert("vpf-385", ts != NULL);
    aal_assert("vpf-567", ts->bm_met != NULL);
    
    /* This must be fixed at the first pass. */
    if (start >= ts->bm_met->total || count > ts->bm_met->total ||
	start >= ts->bm_met->total - count)
    {
	aal_exception_error("Node (%llu), item (%u): Pointed region "
	    "[%llu..%llu] is invalid.", item->context.blk, item->pos.item, 
	    start, start + count - 1);
	ts->stat.bad_unfm_ptrs++;
	return 1;
    }
    
    if (!start)
	return 0;

    res = aux_bitmap_test_region(ts->bm_met, start, count, 0);

    /* Pointed region is used already. */
    if (res == 0) {
	aal_exception_error("Node (%llu), item (%u): Pointed region "
	    "[%llu..%llu] is used already or contains a formatted block.", 
	    item->context.blk, item->pos.item, start, start + count - 1);
	ts->stat.bad_unfm_ptrs++;
	return 1;
    } 
    
    if (aux_bitmap_test(ts->bm_used, item->context.blk))
	aux_bitmap_mark_region(ts->bm_unfm_tree, start, count);
    else
	aux_bitmap_mark_region(ts->bm_unfm_out, start, count);

    return 0;
}

/* Callback for the traverse which calls item_ops.layout_check method if layout 
 * exists for all items which can contain data, not tree index data only. 
 * Shrink the node if item lenght is changed. */
static errno_t callback_item_layout_check(reiser4_place_t *place, void *data) {
    repair_ts_t *ts = (repair_ts_t *)data;
    reiser4_node_t *node;
    errno_t res;
 
    aal_assert("vpf-384", place != NULL);
    aal_assert("vpf-727", place->node != NULL);
    aal_assert("vpf-797", data != NULL);

    node = place->node;
    
    if (!reiser4_item_data(place->item.plugin))
	return 0;
    
    res = repair_item_layout_check(place, callback_item_region_check, 
	ts, ts->repair->mode);
    
    if (res < 0) 
	return res;
    
    if (res & REPAIR_FATAL) 
	ts->repair->fatal++;
    else if (res & REPAIR_FIXABLE)
	ts->repair->fixable++;
    else if (res & REPAIR_FIXED)
	reiser4_node_mkdirty(place->node);
    else if (res & REPAIR_REMOVED) {
	/* FIXME: Empty nodes may be left in the tree. Cleanup the tree 
	 * afterwords. */
	reiser4_node_mkdirty(place->node);
	place->pos.item--;
    }
    
    return 0;
}

static void repair_twig_scan_setup(repair_ts_t *ts) {
    aal_assert("vpf-884", ts != NULL);
    
    aal_memset(ts->progress, 0, sizeof(*ts->progress));
    ts->progress->type = PROGRESS_RATE;
    ts->progress->title = "***** TwigScan Pass: checking extent pointers of "
	"all twigs.";
    ts->progress->text = "";
    time(&ts->stat.time);
    
    if (!ts->progress_handler)
	return;

    ts->progress->state = PROGRESS_START;
    ts->progress->u.rate.total = aux_bitmap_marked(ts->bm_twig);
    ts->progress_handler(ts->progress);
    
    ts->progress->state = PROGRESS_UPDATE;
}

static void repair_twig_scan_update(repair_ts_t *ts) {
    aal_stream_t stream;
    char *time_str;

    aal_assert("vpf-885", ts != NULL);
    
    if (!ts->progress_handler)
	return;

    ts->progress->state = PROGRESS_END;
    ts->progress_handler(ts->progress);
    
    aal_stream_init(&stream);
    aal_stream_format(&stream, "\tRead twigs %llu\n", ts->stat.read_twigs);
    if (ts->stat.fixed_twigs) {
	aal_stream_format(&stream, "\tCorrected nodes %llu\n", 
	    ts->stat.fixed_twigs);	
    }
    if (ts->stat.bad_unfm_ptrs) {
	aal_stream_format(&stream, "\tFixed invalid extent pointers %llu\n", 
	    ts->stat.bad_unfm_ptrs);
    }
    time_str = ctime(&ts->stat.time);
    time_str[aal_strlen(time_str) - 1] = '\0';
    aal_stream_format(&stream, "\tTime interval: %s - ", time_str);
    time(&ts->stat.time);
    time_str = ctime(&ts->stat.time);
    time_str[aal_strlen(time_str) - 1] = '\0';
    aal_stream_format(&stream, time_str);

    ts->progress->state = PROGRESS_STAT;
    ts->progress->text = (char *)stream.data;
    ts->progress_handler(ts->progress);
    
    aal_stream_fini(&stream);
    
}

/* The pass itself, goes through all twigs, check block pointers which items may have 
 * and account them in proper bitmaps. */
errno_t repair_twig_scan(repair_ts_t *ts) {
    repair_progress_t progress;
    object_entity_t *entity;
    reiser4_node_t *node;
    errno_t res = -1;
    blk_t blk = 0;

    aal_assert("vpf-533", ts != NULL);
    aal_assert("vpf-534", ts->repair != NULL);
    aal_assert("vpf-845", ts->repair->fs != NULL);
    
    ts->progress = &progress;
    
    repair_twig_scan_setup(ts);
    
    while ((blk = aux_bitmap_find_marked(ts->bm_twig, blk)) != INVAL_BLK) {
	ts->stat.read_twigs++;
	if (ts->progress_handler)
	    ts->progress_handler(&progress);	
	
	node = repair_node_open(ts->repair->fs, blk);
	if (node == NULL) {
	    aal_exception_fatal("Twig scan pass failed to open the twig (%llu)",
		blk);
	    return -EINVAL;
	}

	entity = node->entity;

	/* Lookup the node. */	
	if ((res = repair_node_traverse(node, callback_item_layout_check, ts)))
	    goto error_node_free;
	
	if (reiser4_node_isdirty(node))
	    ts->stat.fixed_twigs++;
	    
	if (!reiser4_node_locked(node))
	    reiser4_node_close(node);

	blk++;
    }

    repair_twig_scan_update(ts);
    return 0;

error_node_free:
    reiser4_node_close(node);
    repair_twig_scan_update(ts);

    return -EINVAL;
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

/* Coord pints to a problem extent. Save it into ovrl_list for further handling. */
static errno_t repair_ts_ovrl_add(reiser4_place_t *place, repair_data_t *rd) {
    repair_ovrl_t *ovrl;
    reiser4_node_t *node;
    aal_list_t *list;
    repair_ts_t *ts;
    
    aal_assert("vpf-520", place != NULL);
    aal_assert("vpf-521", rd != NULL);
    aal_assert("vpf-524", rd->alloc != NULL);
    aal_assert("vpf-525", rd->alloc->entity != NULL);

    if (!(node = place->node)) {
	aal_exception_fatal("Failed to get the node from the place, but "
	    "it is expected that the place is build on the node.");
	return -1;
    }

    /* Pin it to avoid later closing. */
    reiser4_node_lock(node);

    ts = repair_ts(rd);

    ovrl = aal_malloc(sizeof(*ovrl));
    
    if (plugin_call(place->item.plugin->item_ops, fetch, 
	&place->item, place->pos.unit, &ovrl->ptr, 1) != 1)
	goto error_free_ovrl;

    ovrl->node = place->node;
    ovrl->pos = place->pos;	
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
    
    aal_assert("vpf-552", ovrl_list != NULL);
    aal_assert("vpf-553", *ovrl_list != NULL);
   
    /* Calculate the initial conflics into ovrl_place's */
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
    
     /* Choose the place with the largest conflicts value, remove it, 
     * recalculate conflicts. Continue it unless there is no any conflict. */
    while (max_conflict && max_conflict->conflicts) {
	reiser4_place_t place;
	
	if (reiser4_place_open(&place, max_conflict->node, CT_JOINT, 
	    &max_conflict->pos)) 
	{
	    aal_exception_error("Can't open item by place. Node %llu, item %u.",
		max_conflict->node->node->blk, max_conflict->pos.item);
	    return -1;
	}
	
	repair_item_handle_ptr(&place);

	/* FIXME-VITALY: sync it somehow. */
	r_ovrl = max_conflict;
	max_conflict = max_conflict->next;

	if (r_ovrl->next)
	    r_ovrl->next->prev = r_ovrl->prev;
	if (r_ovrl->prev)
	    r_ovrl->prev->next = r_ovrl->next;
	
	*ovrl_list = aal_list_remove(*ovrl_list, r_ovrl);
	
	reiser4_node_unlock(r_ovrl->node);
	reiser4_node_release(r_ovrl->node);
	aal_free(r_ovrl);

	/* Descrement conflict counters of all places around max_conflict, 
	 * which conflicted with r_ptr. */

    }
   
    return 0;
}

static errno_t repair_ts_ovrl_list_free(aal_list_t **ovrl_list, 
    ovrl_region_func_t func)
{
    repair_ovrl_region_t *region;
    repair_ovrl_place_t *oc;

    aal_assert("vpf-548", ovrl_list != NULL);
    aal_assert("vpf-549", *ovrl_list != NULL);

    // If we had bad unfm blocks we should not get 0-length overlapped extent list.
    aal_assert("vpf-550", aal_list_length(*ovrl_list) != 0);    
    
    while (*ovrl_list != NULL) {
	region = (repair_ovrl_region_t *)aal_list_first(*ovrl_list)->data;

	aal_assert("vpf-551", aal_list_length(region->extents) != 0);

	if (func(region))
	    return -1;
	
	*ovrl_list = aal_list_remove(*ovrl_list, region);
	
	while (region->extents != NULL) {

	    oc = (repair_ovrl_place_t *)aal_list_first(region->extents)->data;
	    region->extents = aal_list_remove(region->extents, oc);
	    
	    // FIXME-VITALY: close the node, sync it if needed. 
	    reiser4_node_release(reiser4_place_node(oc->place));
	    
	    aal_free(oc->place);
	    aal_free(oc);
	}

	aal_free(region);
    }
    
    return 0;
}

#endif

