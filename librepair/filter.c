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

/* Temporary flags set during recovery. */
#define REPAIR_BAD_PTR			0x1

/* Open callback for traverse. It opens a node at passed blk, creates a node 
 * on it. It does nothing if REPAIR_BAD_PTR is set and set this flag if 
 * node cannot be opeened. Returns error if any. */
static errno_t repair_filter_node_open(reiser4_node_t **node, blk_t blk, 
    void *data)
{
    repair_data_t *rd = (repair_data_t *)data;

    aal_assert("vpf-379", rd != NULL);
    aal_assert("vpf-432", node != NULL);
    aal_assert("vpf-433", rd->fs != NULL);
    aal_assert("vpf-591", rd->fs->format != NULL);

    if (aal_test_bit(&repair_filter(rd)->flags, REPAIR_BAD_PTR))
	return 0;

    if ((*node = repair_node_open(rd->fs, blk)) == NULL)
	aal_set_bit(&repair_filter(rd)->flags, REPAIR_BAD_PTR);    

    return 0;
}

/* Before callback for traverse. It checks node level, node consistency, and 
 * delimiting keys. If any check reveals a problem with the data consistency
 * it sets REPAIR_BAD_PTR flag. */
static errno_t repair_filter_node_check(reiser4_node_t *node, void *data) {
    repair_data_t *rd = (repair_data_t *)data;
    repair_filter_t *fd;
    errno_t res = 0;
    uint16_t level;
    
    aal_assert("vpf-252", data  != NULL);
    aal_assert("vpf-409", node != NULL);

    fd = repair_filter((repair_data_t *)data);

    level = reiser4_node_get_level(node); 
    
    /* Initialize the level for the root node before traverse. */
    if (!fd->level)
	fd->level = level;
    
    /* Skip this check if level is not set (root node only). */
    if (fd->level != level) {
	aal_exception_error("Level of the node (%u) mismatches to the expected "
	    "one (%u).", level, fd->level);
	res = 1;
    }

    if (!res && (res = repair_node_check(node, fd->bm_used)) < 0)
	return res;
	
    if (!res && (res = repair_node_dkeys_check(node, data)) < 0)
	return res;

    if (res > 0) {
	aal_set_bit(&repair_filter(rd)->flags, REPAIR_BAD_PTR);
	/* FIXME-VITALY: if a node was changed, all changes should be forgot. */
    }

    return res;
}

/* Setup callback for traverse. Prepares essential information for a child of 
 * a node - level. */
static errno_t repair_filter_setup_traverse(reiser4_place_t *coord, void *data) {
    repair_filter_t *fd;
    reiser4_ptr_hint_t ptr;

    aal_assert("vpf-255", data != NULL);
    aal_assert("vpf-531", coord != NULL);
    aal_assert("vpf-703", reiser4_item_nodeptr(coord));

    fd = repair_filter((repair_data_t *)data);
    if (plugin_call(coord->item.plugin->item_ops, read, 
	&coord->item, &ptr, coord->pos.unit, 1) != 1) 
    {
	aal_exception_fatal("Failed to fetch the node pointer.");
	return -1;
    }

    /* The validness of this pointer must be checked at node_check time. */
    aux_bitmap_mark_region(fd->bm_used, ptr.ptr, ptr.width);

    fd->level--;
 
    return 0;
}

/* Update callback for traverse. It rollback changes made in setup_traverse
 * callback and do some essential stuff after traversing through the child -
 * level, if REPAIR_BAD_PTR flag is set - deletes the child pointer and 
 * mark the pointed block as unused in bm_used bitmap. */
static errno_t repair_filter_update_traverse(reiser4_place_t *coord, void *data) {
    rpos_t prev;
    repair_data_t *rd = (repair_data_t *)data;
    
    aal_assert("vpf-257", rd != NULL);
    aal_assert("vpf-434", coord != NULL);
    
    if (aal_test_bit(&repair_filter(rd)->flags, REPAIR_BAD_PTR)) {
	reiser4_ptr_hint_t ptr;
	
	/* Clear pointed block in the formatted bitmap. */
	if (plugin_call(coord->item.plugin->item_ops,
	    read, &coord->item, &ptr, coord->pos.unit, 1) != 1)
	    return -1;
	
	aux_bitmap_clear(repair_filter(rd)->bm_used, ptr.ptr);
	
	/* The node corruption was not fixed - delete the internal item. */
	repair_coord_left_pos_save(coord, &prev);
	if (reiser4_node_remove(coord->node, &coord->pos, 1)) {
	    aal_exception_error("Node (%llu), pos (%u, %u): Remove failed.", 
		coord->node->blk, coord->pos.item, coord->pos.unit);
	    return -1;
	}
	coord->pos = prev;
	aal_clear_bit(&repair_filter(rd)->flags, REPAIR_BAD_PTR);
    } else {
	/* Mark all twigs in the bm_twig bitmap. */
	if (reiser4_node_get_level(coord->node) == TWIG_LEVEL) 
	    aux_bitmap_mark(repair_filter(rd)->bm_twig, 
		coord->node->blk);
    }
    
    repair_filter(rd)->level++;

    return 0;
}

/* After callback for traverse. Does needed stuff after traversing through all 
 * children - if no child left, set REPAIR_BAD_PTR flag to force deletion of 
 * the pointer to this block in update_traverse callback. */
static errno_t repair_filter_after_traverse(reiser4_node_t *node, void *data) {
    repair_data_t *rd = (repair_data_t *)data;
     
    aal_assert("vpf-393", node != NULL);
    aal_assert("vpf-256", rd != NULL);    

    if (reiser4_node_items(node) == 0)
	aal_set_bit(&repair_filter(rd)->flags, REPAIR_BAD_PTR);
    /* FIXME-VITALY: else - sync the node */

    return 0;
}

/* If a fatal error occured, release evth, what was allocated by this moment 
 * - not only on this pass, smth was allocated on some previous one. */
static void repair_filter_release(repair_data_t *rd) {
    aal_assert("vpf-738", rd != NULL);

    if (repair_filter(rd)->bm_used)
	aux_bitmap_close(repair_filter(rd)->bm_used);
    if (repair_filter(rd)->bm_twig)
	aux_bitmap_close(repair_filter(rd)->bm_twig);
}

/* Callback for the format_ops.layout method to mark all its blocks in the 
 * bitmap. */
static errno_t callback_format_mark(object_entity_t *format, blk_t blk, 
    void *data)
{
    aux_bitmap_t *format_layout = (aux_bitmap_t *)data;

    aux_bitmap_mark(format_layout, blk);
    
    return 0;
}

/* Setup data (common and specific) before traverse through the tree. */
static errno_t repair_filter_setup(traverse_hint_t *hint, repair_data_t *rd) {
    reiser4_ptr_hint_t ptr;
    blk_t root;
    
    aal_assert("vpf-420", hint != NULL);
    aal_assert("vpf-423", rd != NULL);
    aal_assert("vpf-592", rd->fs != NULL);

    hint->data = rd;
    hint->cleanup = 1;

    /* Allocate a bitmap for blocks belonged to the format area - skipped, 
     * super block, journal, bitmaps. */
    if (!(repair_filter(rd)->bm_used = aux_bitmap_create(
	reiser4_format_get_len(rd->fs->format)))) 
    {
	aal_exception_error("Failed to allocate a bitmap for format layout.");
	return -1;
    }

    /* Mark all format area block in the bm_used bitmap. */
    if (repair_fs_layout(rd->fs, callback_format_mark, 
	repair_filter(rd)->bm_used)) 
    {
	aal_exception_error("Failed to mark the filesystem area as used in "
	    "the bitmap.");
	goto error;
    }
    
    /* Allocate a bitmap for twig blocks in the tree. */
    if (!(repair_filter(rd)->bm_twig = aux_bitmap_create(
	reiser4_format_get_len(rd->fs->format)))) 
    {
	aal_exception_error("Failed to allocate a bitmap for twig blocks.");
	goto error;
    }
 
    repair_filter(rd)->flags = 0;

    root = reiser4_format_get_root(rd->fs->format);

    /* Check the root pointer to be valid block. */
    if (root < reiser4_format_start(rd->fs->format) || 
	root > reiser4_format_get_len(rd->fs->format))
	/* Wrong pointer. */
	aal_set_bit(&repair_filter(rd)->flags, REPAIR_BAD_PTR);
    else if (aux_bitmap_test(repair_filter(rd)->bm_used, 
	reiser4_format_get_root(rd->fs->format))) 
	/* This block is from format area. */
	aal_set_bit(&repair_filter(rd)->flags, REPAIR_BAD_PTR);
    else	
	/* We meet the block for the first time. */
	aux_bitmap_mark(repair_filter(rd)->bm_used, 
	    reiser4_format_get_root(rd->fs->format));
 
    return 0;
    
error:
    repair_filter_release(rd);
    
    return -1;
}

/* Does some updata stuff after traverse through the internal tree - deletes 
 * the pointer to the root block from the specific super block if 
 * REPAIR_BAD_PTR flag is set, mark that block used in bm_used bitmap 
 * otherwise. */
static errno_t repair_filter_update(traverse_hint_t *hint) {
    repair_data_t *rd;

    aal_assert("vpf-421", hint != NULL);
    aal_assert("vpf-422", hint->data != NULL);
    
    rd = hint->data;
    
    if (aal_test_bit(&repair_filter(rd)->flags, REPAIR_BAD_PTR)) {
	reiser4_format_set_root(rd->fs->format, INVAL_BLK);
	aal_clear_bit(&repair_filter(rd)->flags, REPAIR_BAD_PTR);
    } else {
	/* Mark the root block as a formatted block in the bitmap. */
	aux_bitmap_mark(repair_filter(rd)->bm_used, 
	    reiser4_format_get_root(rd->fs->format));
    }

    return 0;
}

/* The pass itself - goes through the existent tree trying to filter all 
 * corrupted parts off, and fixing what can be fixed. Account all kind of 
 * nodes in corresponding bitmaps. */
errno_t repair_filter_pass(repair_data_t *rd) {
    traverse_hint_t hint;
    reiser4_node_t *node = NULL;
    errno_t res = -1;

    aal_assert("vpf-536", rd != NULL);

    if (repair_filter_setup(&hint, rd))
	return -1;

    if ((res = repair_filter_node_open(&node, 
	reiser4_format_get_root(rd->fs->format), rd)) < 0)
	goto error;
    
    if (res == 0 && node != NULL) {
	/* Cut the corrupted, unrecoverable parts of the tree off. */ 	
	res = reiser4_node_traverse(node, &hint, repair_filter_node_open,
	    repair_filter_node_check,	    repair_filter_setup_traverse,  
	    repair_filter_update_traverse,  repair_filter_after_traverse);

	reiser4_node_close(node);

	if (res < 0)
	    goto error;
    } else 
	aal_set_bit(&repair_filter(rd)->flags, REPAIR_BAD_PTR);

    if ((res = repair_filter_update(&hint)))
	return res;
    
    return 0;

error:
    repair_filter_release(rd);

    return -1;
}

