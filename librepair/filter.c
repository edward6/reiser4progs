/* 
    librepair/filter.c - methods are needed for the fsck pass1. 
    
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

/*
    The first fsck pass - filter - fsck filters corrupted parts of 
    a reiser4 tree out, repairs all recoverable corruptions, builds
    a map of all used blocks, but extents (format + formatted nodes). 
    Extents may still be not correct.    
*/

#include <repair/librepair.h>
#include <repair/filter.h>

/* This is extention for repair_error_t. */
typedef enum repair_error_filter {
    /* Pointer to the wrong place. */
    REPAIR_BAD_PTR	= (REPAIR_ERROR_LAST),
    /* Node is ok, but dkeys are wrong, remove from the tree and insert back 
     * later item-by-item. */
    REPAIR_BAD_DKEYS	= (REPAIR_ERROR_LAST << 1)
} repair_error_filter_t;

/* Open callback for traverse. It opens a node at passed blk, creates a node 
 * on it. It does nothing if REPAIR_BAD_PTR is set and set this flag if 
 * node cannot be opeened. Returns error if any. */
static errno_t repair_filter_node_open(reiser4_node_t **node, blk_t blk, 
    void *data)
{
    repair_filter_t *fd = (repair_filter_t *)data;

    aal_assert("vpf-379", fd != NULL);
    aal_assert("vpf-432", node != NULL);
    aal_assert("vpf-433", fd->fs != NULL);
    aal_assert("vpf-591", fd->fs->format != NULL);

    if ((*node = repair_node_open(fd->fs, blk)) == NULL)
	fd->flags |= REPAIR_BAD_PTR;

    return 0;
}

/* Before callback for traverse. It checks node level, node consistency, and 
 * delimiting keys. If any check reveals a problem with the data consistency
 * it sets REPAIR_BAD_PTR flag. */
static errno_t repair_filter_node_check(reiser4_node_t *node, void *data) {
    repair_filter_t *fd = (repair_filter_t *)data;
    errno_t res = 0;
    uint16_t level;
    
    aal_assert("vpf-252", data  != NULL);
    aal_assert("vpf-409", node != NULL);

    level = reiser4_node_get_level(node); 
    
    /* Initialize the level for the root node before traverse. */
    if (!fd->level)
	fd->level = level;
    
    /* Skip this check if level is not set (root node only). */
    if (fd->level != level) {
	aal_exception_error("Level of the node (%u) mismatches to the expected "
	    "one (%u).", level, fd->level);
	/* Should not be check for now as it may lie in unused space. 
	 * It is just a wrong pointer. Skip it. */
	fd->flags |= REPAIR_BAD_PTR;
	return 1;
    }
    
    if ((res = repair_node_check(node, fd->mode)) < 0)
	return res;
    
    if (reiser4_node_items(node) == 0)
	res |= REPAIR_FATAL;

    aal_assert("vpf-799", (res & REPAIR_REMOVED) == 0);
    
    if (res | REPAIR_FATAL) {
	fd->flags |= REPAIR_FATAL;
	return 1;
    } else if (res | REPAIR_FIXABLE) {
	fd->info.check.fixable++;
	/* Do not break traverse. */
	res = 0;
    } else if (res | REPAIR_FIXED) {
	reiser4_node_mkdirty(node);
	res = 0;
    }
    
    /* There are no fatal errors, check delimiting keys. */
    if ((res = repair_node_dkeys_check(node)) < 0)
	return res;
    
    if (res) {
	fd->flags |= REPAIR_BAD_DKEYS;
	return 1;
    }
	    
    return 0;
}

/* Setup callback for traverse. Prepares essential information for a child of 
 * a node - level. */
static errno_t repair_filter_setup_traverse(reiser4_place_t *place, void *data) {
    repair_filter_t *fd = (repair_filter_t *)data;
    reiser4_ptr_hint_t ptr;

    aal_assert("vpf-255", data != NULL);
    aal_assert("vpf-531", place != NULL);
    aal_assert("vpf-703", reiser4_item_branch(place));
    
    if (plugin_call(place->item.plugin->item_ops, read, &place->item, &ptr, 
	place->pos.unit, 1) != 1) 
    {
	aal_exception_fatal("Node (%llu), item (%u), unit(%u): Failed to fetch "
	    "the node pointer.", place->node, place->pos.item, place->pos.unit);
	return -1;
    }
    
    /* FIXME: as a result layout of nodeptr items is checked automatically, what
     * is not very well as read does not mean that we get pointed block. */
    
    /* The validness of this node pointer must be checked at node_check time. */
    if (ptr.ptr < fd->bm_used->total && ptr.width < fd->bm_used->total &&
	ptr.ptr < fd->bm_used->total - ptr.width && 
	aux_bitmap_test_region_cleared(fd->bm_used, ptr.ptr, ptr.width))
    {
	aux_bitmap_mark_region(fd->bm_used, ptr.ptr, ptr.width);
    } else {
	/* Bad pointer detected. Remove if possible. */
	aal_exception_error("Node (%llu), item (%u), unit (%u): Points to "
	    "invalid region [%llu..%llu] or some blocks are used already. %s", 
	    place->node->blk, place->pos.item, place->pos.unit, ptr.ptr, 
	    ptr.ptr + ptr.width - 1, fd->mode == REPAIR_REBUILD ? 
	    "Removed." : "The whole subtree is skipped.");
	
	if (fd->mode == REPAIR_REBUILD) {
	    pos_t ppos;
	    
	    repair_place_get_lpos(place, ppos);
	
	    if (reiser4_node_remove(place->node, &place->pos, 1)) {
		aal_exception_error("Node (%llu), pos (%u, %u): Remove failed.",
		    place->node->blk, place->pos.item, place->pos.unit);
		return -1;
	    }
	
	    place->pos = ppos;
	} else
	    fd->info.check.fatal++;
	
	return 1;
    }

    fd->level--;
 
    return 0;
}

/* Update callback for traverse. It rollback changes made in setup_traverse
 * callback and do some essential stuff after traversing through the child -
 * level, if REPAIR_BAD_PTR flag is set - deletes the child pointer and 
 * mark the pointed block as unused in bm_used bitmap. */
static errno_t repair_filter_update_traverse(reiser4_place_t *place, void *data) {
    repair_filter_t *fd = (repair_filter_t *)data;
    reiser4_ptr_hint_t ptr;
    uint8_t level;
    
    aal_assert("vpf-257", fd != NULL);
    aal_assert("vpf-434", place != NULL);
    
    /* Clear pointed block in the formatted bitmap. */
    if (plugin_call(place->item.plugin->item_ops, read, &place->item, &ptr, 
	place->pos.unit, 1) != 1) 
    {
	aal_exception_fatal("Node (%llu), item (%u), unit(%u): Failed to "
	    "fetch the node pointer.", place->node, place->pos.item, 
	    place->pos.unit);
	return -1;
    }

    if (fd->flags) {
	aux_bitmap_clear_region(fd->bm_used, ptr.ptr, ptr.width);
	
	if (fd->flags | REPAIR_BAD_PTR) {
	    aal_exception_error("Node (%llu), item (%u), unit (%u): Points to "
		"the invalid node [%llu]. %s", place->node->blk, 
		place->pos.item, place->pos.unit, ptr.ptr, 
		fd->mode == REPAIR_REBUILD ? "Removed." : 
		"The whole subtree is skipped.");
	} else if (fd->flags | REPAIR_FATAL) {
	    aal_exception_error("Node (%llu), item (%u), unit (%u): Points to "
		"the unrecoverable node [%llu]. %s", place->node->blk, 
		place->pos.item, place->pos.unit, ptr.ptr, 
		fd->mode == REPAIR_REBUILD ? "Removed." : 
		"The whole subtree is skipped.");
	    
	    /* Extents cannot point to this node. */
	    aux_bitmap_mark_region(fd->bm_met, ptr.ptr, ptr.width);
	} else if (fd->flags | REPAIR_BAD_DKEYS) {
	    aal_exception_error("Node (%llu), item (%u), unit (%u): Points to "
		"the node [%llu] with wrong delimiting keys. %s", 
		place->node->blk, place->pos.item, place->pos.unit, ptr.ptr, 
		fd->mode == REPAIR_REBUILD ? "Removed." : 
		"The whole subtree is skipped.");

	    level = reiser4_node_get_level(place->node);
	    
	    /* Insert it later. FIXME: This is hardcoded, should be changed. */
	    if (level == LEAF_LEVEL) 
		aux_bitmap_mark_region(fd->bm_leaf, ptr.ptr, ptr.width);
	    else if (level == TWIG_LEVEL)
		aux_bitmap_mark_region(fd->bm_twig, ptr.ptr, ptr.width);
	    else
		aux_bitmap_mark_region(fd->bm_met, ptr.ptr, ptr.width);
	} else
	    aal_assert("vpf-827: Not expected case.", FALSE);
	
	if (fd->mode == REPAIR_REBUILD) {
	    pos_t prev;
	    
	    /* The node corruption was not fixed - delete the internal item. */
	    repair_place_get_lpos(place, prev);
	
	    if (reiser4_node_remove(place->node, &place->pos, 1)) {
		aal_exception_error("Node (%llu), pos (%u, %u): Remove failed.", 
		    place->node->blk, place->pos.item, place->pos.unit);
		return -1;
	    }
	
	    place->pos = prev;
	} else
	    fd->info.check.fatal++;
	
	fd->flags = 0;
    } else {
	/* FIXME-VITALY: hardcoded level, should be changed. */
	if (reiser4_node_get_level(place->node) == TWIG_LEVEL + 1) 
	    aux_bitmap_mark_region(fd->bm_twig, ptr.ptr, ptr.width);
	else if (reiser4_node_get_level(place->node) == TWIG_LEVEL)
	    aux_bitmap_mark_region(fd->bm_leaf, ptr.ptr, ptr.width);
    }
    
    fd->level++;

    return 0;
}

/* After callback for traverse. Does needed stuff after traversing through all 
 * children - if no child left, set REPAIR_BAD_PTR flag to force deletion of 
 * the pointer to this block in update_traverse callback. */
static errno_t repair_filter_after_traverse(reiser4_node_t *node, void *data) {
    repair_filter_t *fd = (repair_filter_t *)data;
     
    aal_assert("vpf-393", node != NULL);
    aal_assert("vpf-256", fd != NULL);    

    if (reiser4_node_items(node) == 0) {
	fd->flags |= REPAIR_FATAL;
	reiser4_node_mkclean(node);
    }

    return 0;
}

/* Setup data (common and specific) before traverse through the tree. */
static void repair_filter_setup(repair_filter_t *fd) {
    blk_t root;
        
    root = reiser4_format_get_root(fd->fs->format);

    /* Check the root pointer to be valid block. */
    if (root < reiser4_format_start(fd->fs->format) || 
	root > reiser4_format_get_len(fd->fs->format))
	/* Wrong pointer. */
	fd->flags |= REPAIR_BAD_PTR;
    else if (aux_bitmap_test(fd->bm_used, 
	reiser4_format_get_root(fd->fs->format))) 
	/* This block is from format area. */
	fd->flags |= REPAIR_BAD_PTR;
    else	
	/* We meet the block for the first time. */
	aux_bitmap_mark(fd->bm_used, reiser4_format_get_root(fd->fs->format));
}

/* Does some update stuff after traverse through the internal tree - deletes 
 * the pointer to the root block from the specific super block if 
 * REPAIR_BAD_PTR flag is set, mark that block used in bm_used bitmap 
 * otherwise. */
static void repair_filter_update(repair_filter_t *fd) {
    aal_assert("vpf-421", fd != NULL);
    
    if (fd->flags & REPAIR_BAD_PTR) {
	aux_bitmap_clear(fd->bm_used, reiser4_format_get_root(fd->fs->format));
	reiser4_format_set_root(fd->fs->format, INVAL_BLK);
	/* FIXME: sync it to disk. */
	fd->flags = 0;
    } 
}

/* The pass itself - goes through the existent tree trying to filter all 
 * corrupted parts off, and fixing what can be fixed. Account all kind of 
 * nodes in corresponding bitmaps. */
errno_t repair_filter(repair_filter_t *fd) {
    traverse_hint_t hint;
    errno_t res = -1;

    aal_assert("vpf-536", fd != NULL);
    aal_assert("vpf-814", fd->fs != NULL);
    aal_assert("vpf-816", fd->fs->tree != NULL);
    aal_assert("vpf-815", fd->bm_used != NULL);
    aal_assert("vpf-814", fd->bm_leaf != NULL);
    aal_assert("vpf-814", fd->bm_twig != NULL);
    aal_assert("vpf-814", fd->bm_met != NULL);

    repair_filter_setup(fd);
    
    if ((res = repair_filter_node_open(&fd->fs->tree->root, 
	reiser4_format_get_root(fd->fs->format), fd)) < 0)
	return res;
    
    if (res == 0 && fd->fs->tree->root != NULL) {
	hint.data = fd;
	hint.cleanup = 1;

	/* Cut the corrupted, unrecoverable parts of the tree off. */ 	
	res = reiser4_tree_down(fd->fs->tree, fd->fs->tree->root, &hint,
	    repair_filter_node_open,	    repair_filter_node_check,
	    repair_filter_setup_traverse,   repair_filter_update_traverse,
	    repair_filter_after_traverse);

	reiser4_node_close(fd->fs->tree->root);

	if (res < 0)
	    return res;
    }

    repair_filter_update(fd);
    
    return 0;
}

