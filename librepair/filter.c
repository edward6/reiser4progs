/* 
    librepair/filter.c - methods are needed for the fsck pass1. 
    Copyright (C) 1996-2002 Hans Reiser.
    The first fsck pass - filter - fsck filters corrupted parts of 
    a reiser4 tree out, repairs all recoverable corruptions, builds
    a map of all used blocks, but extents (format + formatted nodes). 
    Extents may still be not correct.
*/

#include <repair/librepair.h>

/* Open callback for traverse. It opens a node at passed blk, creates a node 
 * on it. It does nothing if REPAIR_BAD_POINTER is set and set this flag if 
 * node cannot be opeened. Returns error if any. */
static errno_t repair_filter_node_open(reiser4_node_t **node, blk_t blk, 
    void *data)
{
    repair_data_t *repair_data = (repair_data_t *)data;

    aal_assert("vpf-379", repair_data != NULL, return -1);
    aal_assert("vpf-432", node != NULL, return -1);
    aal_assert("vpf-433", repair_data->fs != NULL, return -1);
    aal_assert("vpf-591", repair_data->fs->format != NULL, return -1);

    if (repair_test_flag(repair_data, REPAIR_BAD_PTR))
	return 0;

    if ((*node = repair_node_open(repair_data->fs->format, blk)) == NULL) 
	repair_set_flag(repair_data, REPAIR_BAD_PTR);    

    return 0;
}

/* Before callback for traverse. It checks node level, node consistency, and 
 * delimiting keys. If any check reveals a problem with the data consistency
 * it sets REPAIR_BAD_PTR flag. */
static errno_t repair_filter_node_check(reiser4_node_t *node, void *data) {
    repair_data_t *rd = (repair_data_t *)data;
    repair_filter_t *fd;
    object_entity_t *entity;    
    errno_t res = 0;
    uint16_t level;
    
    aal_assert("vpf-252", data  != NULL, return -1);
    aal_assert("vpf-409", node != NULL, return -1);
    aal_assert("vpf-411", node->entity != NULL, return -1);    
    aal_assert("vpf-412", node->entity->plugin != NULL, return -1);

    fd = repair_filter((repair_data_t *)data);
    entity = node->entity;

    level = plugin_call(return -1, entity->plugin->node_ops, get_level, entity);
    
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

    if (res > 0)
	repair_set_flag(rd, REPAIR_BAD_PTR);

    return res;
}

/* Setup callback for traverse. Prepares essential information for a child of 
 * a node - level. */
static errno_t repair_filter_setup_traverse(reiser4_coord_t *coord, void *data) {
    repair_filter_t *fd;
    reiser4_ptr_hint_t ptr;

    aal_assert("vpf-255", data != NULL, return -1);
    aal_assert("vpf-531", coord != NULL, return -1);

    fd = repair_filter((repair_data_t *)data);
    if (plugin_call(return -1, coord->entity.plugin->item_ops, fetch, 
	&coord->entity, coord->pos.unit, &ptr, 1)) 
    {
	aal_exception_fatal("Failed to fetch the item pointer.");
	return -1;
    }

    /* The validness of this pointer must be checked at node_check time. */
    aux_bitmap_mark_range(fd->bm_used, ptr.ptr, ptr.ptr + ptr.width);

    fd->level--;
 
    return 0;
}

/* Update callback for traverse. It rollback changes made in setup_traverse
 * callback and do some essential stuff after traversing through the child -
 * level, if REPAIR_BAD_PTR flag is set - deletes the child pointer and 
 * mark the pointed block as unused in bm_used bitmap. */
static errno_t repair_filter_update_traverse(reiser4_coord_t *coord, void *data) {
    reiser4_pos_t prev;
    repair_data_t *rd = (repair_data_t *)data;
    
    aal_assert("vpf-257", rd != NULL, return -1);
    aal_assert("vpf-434", coord != NULL, return -1);
    
    if (repair_test_flag(rd, REPAIR_BAD_PTR)) {
	reiser4_ptr_hint_t ptr;
	
	/* Clear pointed block in the formatted bitmap. */
	if (plugin_call(return -1, coord->entity.plugin->item_ops,
	    fetch, &coord->entity, coord->pos.unit, &ptr, 1))
	    return -1;
	
	aux_bitmap_clear(repair_filter(rd)->bm_used, ptr.ptr);
	
	/* The node corruption was not fixed - delete the internal item. */
	repair_coord_left_pos_save(coord, &prev);
	if (reiser4_node_remove(coord->node, &coord->pos)) {
	    aal_exception_error("Node (%llu), pos (%u, %u): Remove failed.", 
		coord->node->blk, coord->pos.item, coord->pos.unit);
	    return -1;
	}
	coord->pos = prev;
	repair_clear_flag(rd, REPAIR_BAD_PTR);
    } else {
        object_entity_t *entity = coord->node->entity;
	/* Mark all twigs in the bm_twig bitmap. */
	if (plugin_call(return -1, entity->plugin->node_ops, 
	    get_level, entity) == TWIG_LEVEL) 
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
    repair_data_t *repair_data = (repair_data_t *)data;
     
    aal_assert("vpf-393", node != NULL, return -1);
    aal_assert("vpf-256", repair_data != NULL, return -1);    

    if (reiser4_node_items(node) == 0)
	repair_set_flag(repair_data, REPAIR_BAD_PTR);
    /* FIXME-VITALY: else - sync the node */

    return 0;
}

/* Setup data (common and specific) before traverse through the tree. */
static errno_t repair_filter_setup(traverse_hint_t *hint, repair_data_t *rd) {
    reiser4_ptr_hint_t ptr;
    
    aal_assert("vpf-420", hint != NULL, return -1);
    aal_assert("vpf-423", rd != NULL, return -1);
    aal_assert("vpf-592", rd->fs != NULL, return -1);

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
    if (reiser4_format_layout(rd->fs->format, callback_mark_format_block, 
	repair_filter(rd)->bm_used)) 
    {
	aal_exception_error("Failed to mark all format blocks in the bitmap as "
	    "unused.");
	return -1;
    }
    
    /* Allocate a bitmap for twig blocks in the tree. */
    if (!(repair_filter(rd)->bm_twig = aux_bitmap_create(
	reiser4_format_get_len(rd->fs->format)))) 
    {
	aal_exception_error("Failed to allocate a bitmap for twig blocks.");
	return -1;
    }
    
    /* Hint for objects to be traversed - node pointers only here. */
    hint->objects = 1 << NODEPTR_ITEM;
    
    rd->flags = 0;

    /* Check the root pointer to be valid block. */
    if (aux_bitmap_test(repair_filter(rd)->bm_used, 
	reiser4_format_get_root(rd->fs->format))) 
	/* This block is from format area. */
	repair_set_flag(rd, REPAIR_BAD_PTR);
    else	
	/* We meet the block for the first time. */
	aux_bitmap_mark(repair_filter(rd)->bm_used, 
	    reiser4_format_get_root(rd->fs->format));
 
    return 0;
}

/* Does some updata stuff after traverse through the internal tree - deletes 
 * the pointer to the root block from the specific super block if 
 * REPAIR_BAD_PTR flag is set, mark that block used in bm_used bitmap 
 * otherwise. */
static errno_t repair_filter_update(traverse_hint_t *hint) {
    repair_data_t *rd;

    aal_assert("vpf-421", hint != NULL, return -1);
    aal_assert("vpf-422", hint->data != NULL, return -1);
    
    rd = hint->data;
    
    if (repair_test_flag(rd, REPAIR_BAD_PTR)) {
	reiser4_format_set_root(rd->fs->format, INVAL_BLK);
	repair_clear_flag(rd, REPAIR_BAD_PTR);
    } else {
	/* Mark the root block as a formatted block in the bitmap. */
	aux_bitmap_mark(repair_filter(rd)->bm_used, 
	    reiser4_format_get_root(rd->fs->format));
    }

    return 0;
}

errno_t repair_filter_pass(repair_data_t *rd) {
    traverse_hint_t hint;
    reiser4_node_t *node = NULL;
    errno_t res;

    aal_assert("vpf-536", rd != NULL, return -1);

    if (repair_filter_setup(&hint, rd))
	return -1;

    if ((res = repair_filter_node_open(&node, 
	reiser4_format_get_root(rd->fs->format), rd)) < 0)
	return res;
    
    if (res == 0 && node != NULL) {
	/* Cut the corrupted, unrecoverable parts of the tree off. */ 	
	res = reiser4_node_traverse(node, &hint, repair_filter_node_open,
	    repair_filter_node_check,	    repair_filter_setup_traverse,  
	    repair_filter_update_traverse,  repair_filter_after_traverse);

	reiser4_node_close(node);

	if (res < 0)
	    return res;
    } else 
	repair_set_flag(rd, REPAIR_BAD_PTR);

    if ((res = repair_filter_update(&hint)))
	return res;
    
    return 0;
}
