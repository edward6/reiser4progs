/*
    librepair/disk_scan.c - methods are needed for the third fsck pass. 

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
  
    The disk_scan pass - fsck scans the blocks which are used, but not 
    in the tree yet. 

    After filter pass:
    - some extent units may points to formatted blocks;
    - if some formatted block is unused in on-disk block allocator, 
    correct allocator;
    - if some unformatted block is used in on-disk block allocator,
    zero extent pointer.
*/

#include <repair/librepair.h>

/* Structure to be passed into the callback_blk_mark to reveal broken 
 * alloc region. */
struct ds_alloc_region {
    aux_bitmap_t *bm_used;
    aux_bitmap_t *bm_scan;
};

/* Mark blk in the scan bitmap if not marked in used bitmap (in this case it is
 * a node). */
static errno_t callback_blk_mark(object_entity_t *entity, blk_t blk, void *data)
{
    struct ds_alloc_region *region = (struct ds_alloc_region *)data;
    
    aal_assert("vpf-561", region != NULL);
    aal_assert("vpf-556", region->bm_used != NULL);
    aal_assert("vpf-558", region->bm_scan != NULL);
    aal_assert("vpf-560", entity != NULL);

    /* FIXME: what do we need this line for? */
    /* plugin_call(return -1, entity->plugin->alloc_ops, mark, entity, blk); */
    
    if (!aux_bitmap_test(region->bm_used, blk))
	aux_bitmap_mark(region->bm_scan, blk);

    return 0;
}

static void repair_disk_scan_release(repair_data_t *rd) {
    aal_assert("vpf-740", rd != NULL);
    
    if (repair_ds(rd)->bm_used)
	aux_bitmap_close(repair_ds(rd)->bm_used);
    if (repair_ds(rd)->bm_twig)
	aux_bitmap_close(repair_ds(rd)->bm_twig);
    if (repair_ds(rd)->bm_leaf)
	aux_bitmap_close(repair_ds(rd)->bm_leaf);
    if (repair_ds(rd)->bm_frmt)
	aux_bitmap_close(repair_ds(rd)->bm_frmt);
    if (repair_ds(rd)->bm_scan)
	aux_bitmap_close(repair_ds(rd)->bm_scan);
}

/*  Prepare the bitmap of blocks which are to be scanned. */
static errno_t repair_disk_scan_setup(repair_data_t *rd) {
    struct ds_alloc_region region;
    reiser4_format_t *format;
    repair_ds_t *ds;
    blk_t i;
    uint64_t fs_len;
 
    aal_assert("vpf-511", rd != NULL);
    aal_assert("vpf-704", rd->fs != NULL);
    
    ds = repair_ds(rd);
    
    aal_assert("vpf-633", ds->bm_used != NULL);
    
    format = rd->fs->format;

    fs_len = reiser4_format_get_len(format);
    
    /* Allocate a bitmap for blocks to be scanned on this pass. */ 
    if (!(ds->bm_scan = aux_bitmap_create(fs_len))) {
	aal_exception_error("Failed to allocate a bitmap for blocks unconnected"
	    " from the tree.");
	goto error;
    }

    /* Allocate a bitmap for leaves to be inserted into the tree later. */
    if (!(ds->bm_leaf = aux_bitmap_create(fs_len))) {
	aal_exception_error("Failed to allocate a bitmap for leaves unconnected"
	    " from the tree.");
	goto error;
    }

    /* Allocate a bitmap for formatted blocks which cannot be pointed by extents,
     * which are not in the used nor twig not leaf bitmaps. */
    if (!(ds->bm_frmt = aux_bitmap_create(fs_len))) {
	aal_exception_error("Failed to allocate a bitmap for unaccounted "
	    "formatted blocks.");
	goto error;
    }

    region.bm_used = ds->bm_used;	    
    region.bm_scan = ds->bm_scan;

    /* FIXME-VITALY: optimize it later somehow. */
    /* Build a bitmap of blocks which are not in the tree yet. */
    for (i = reiser4_format_start(format); i < fs_len; i++) {
	
	/* If block is marked in twig, then it is marked in used. */
	aal_assert("vpf-693", (!aux_bitmap_test(ds->bm_twig, i) || 
	    aux_bitmap_test(ds->bm_used, i)));

	if (aux_bitmap_test(ds->bm_used, i) && 
	    reiser4_alloc_unused_region(rd->fs->alloc, i, 1)) 
	{
	    /* Block was met as formatted, but unused in on-disk block 
	     * allocator. Looks like the bitmap block of the allocator
	     * has not been synced on disk. Scan through all its blocks. */
	    reiser4_alloc_related_region(rd->fs->alloc, i, callback_blk_mark, 
		&region);
	}

	if (reiser4_alloc_used_region(rd->fs->alloc, i, 1) && 
	    !aux_bitmap_test(ds->bm_used, i))
	    aux_bitmap_mark(ds->bm_scan, i);
    }

    return 0;

error:
    repair_disk_scan_release(rd);
    
    return -1;
}

errno_t repair_disk_scan_pass(repair_data_t *rd) {
    reiser4_node_t *node;
    reiser4_coord_t coord;
    rpos_t *pos = &coord.pos;
    repair_ds_t *ds;
    blk_t blk = 0;
    errno_t res;
    uint8_t level;
    
    aal_assert("vpf-514", rd != NULL);
    aal_assert("vpf-705", rd->fs != NULL);

    ds = repair_ds(rd);
    
    aal_assert("vpf-515", ds->bm_used != NULL);
    aal_assert("vpf-516", ds->bm_twig != NULL);

    if (repair_disk_scan_setup(rd))
	return -1;

    while ((blk = aux_bitmap_find_marked(ds->bm_scan, blk)) != INVAL_BLK) {
	aal_assert("vpf-694", !aux_bitmap_test(ds->bm_used, blk));
	aal_assert("vpf-695", !aux_bitmap_test(ds->bm_twig, blk));
	
	node = repair_node_open(rd->fs, blk);
	if (node == NULL)
	    goto next;

	level = reiser4_node_get_level(node);

	if (level != LEAF_LEVEL && level != TWIG_LEVEL) {
	    aux_bitmap_mark(ds->bm_frmt, blk);
	    reiser4_node_release(node);
	    goto next;
	}

	res = repair_node_check(node, ds->bm_used);
	
	if (res > 0) {
	    /* Node was not recovered, save it as formatted. */
	    aux_bitmap_mark(ds->bm_frmt, blk);
	    reiser4_node_release(node);
	    goto next;
	} else if (res < 0)
	    goto error_node_release;

	if (level == TWIG_LEVEL) {
	    uint32_t count;
	    
	    /* Remove all not extent items. */
	    coord.node = node;
	    pos->item = 0;
	    pos->unit = ~0ul;
	    count = reiser4_node_items(node);
	    
	    for (pos->item = 0; pos->item < count; pos->item++) {
		if (reiser4_coord_realize(&coord)) {
		    aal_exception_error("Node (%llu), item (%u): failed to open"
			" the item.", node->blk, pos->item);
		    goto error_node_release;
		}
		
		if (!reiser4_item_extent(&coord)) {
		    if (reiser4_node_remove(coord.node, pos, 1)) {
			aal_exception_error("Node (%llu), item (%u): failed to "
			    "remove the item.", node->blk, pos->item);
			goto error_node_release;
		    }

		    reiser4_node_mkdirty(coord.node);
		    pos->item--;
		    count = reiser4_node_items(node);
		}
	    }

	    if (reiser4_node_items(node) == 0)
		reiser4_node_mkclean(coord.node);
	    else 
		aux_bitmap_mark(ds->bm_twig, blk);
	} else
	    aux_bitmap_mark(ds->bm_leaf, blk);
	
	reiser4_node_release(node);
    next:
	blk++;	
    }
    
    return 0;
    
error_node_release:
    reiser4_node_release(node);

error:
    repair_disk_scan_release(rd);

    return -1;
}

