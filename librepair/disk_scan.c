/*
    librepair/disk_scan.c - methods are needed for the third fsck pass. 
    Copyright (C) 1996-2002 Hans Reiser.

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
    
    aal_assert("vpf-561", region != NULL, return -1);
    aal_assert("vpf-556", region->bm_used != NULL, return -1);
    aal_assert("vpf-558", region->bm_scan != NULL, return -1);
    aal_assert("vpf-560", entity != NULL, return -1);

    plugin_call(return -1, entity->plugin->alloc_ops, mark, entity, blk);
    
    if (!aux_bitmap_test(region->bm_used, blk))
	aux_bitmap_mark(region->bm_scan, blk);

    return 0;
}

/*  Prepare the bitmap of blocks which are to be scanned. */
static errno_t repair_ds_setup(repair_data_t *rd) {
    struct ds_alloc_region region;
    repair_ds_t *ds;
    blk_t i;
 
    aal_assert("vpf-511", rd != NULL, return -1);
    aal_assert("vpf-511", rd->alloc != NULL, return -1);
    
    ds = repair_ds(rd);
    
    /* Allocate a bitmap for blocks to be scanned on this pass. */
    if (!(ds->bm_scan = aux_bitmap_create(reiser4_format_get_len(rd->format))))
    {
	aal_exception_error("Failed to allocate a bitmap for blocks unconnected"
	    " from the tree.");
	return -1;
    }

    /* Allocate a bitmap for leaves to be inserted into the tree later. */
    if (!(ds->bm_leaf = aux_bitmap_create(reiser4_format_get_len(rd->format))))
    {
	aal_exception_error("Failed to allocate a bitmap for leaves unconnected"
	    " from the tree.");
	return -1;
    }

    /* Allocate a bitmap for formatted blocks which cannot be pointed by extents,
     * which are not in the used nor twig not leaf bitmaps. */
    if (!(ds->bm_frmt = aux_bitmap_create(reiser4_format_get_len(rd->format))))
    {
	aal_exception_error("Failed to allocate a bitmap for unaccounted "
	    "formatted blocks.");
	return -1;
    }

    /* FIXME-VITALY: optimize it later somehow. */
    /* Build a bitmap of blocks which are not in the tree yet. */
    for (i = 0; i < reiser4_format_get_len(rd->format); i++) {
	if (aux_bitmap_test(ds->bm_used, i) && 
	    !reiser4_alloc_test(rd->alloc, i)) 
	{
	    region.bm_used = ds->bm_used;
	    region.bm_scan = ds->bm_scan;

	    /* Block was met as formatted, but unused in on-disk block 
	     * allocator. Looks like the bitmap block of the allocator
	     * has not been synced on disk. Scan through all its blocks. */
	    reiser4_alloc_region_layout(rd->alloc, i, callback_blk_mark, 
		&region);
	}

	if (reiser4_alloc_test(rd->alloc, i) && 
	    !aux_bitmap_test(ds->bm_used, i))
	    aux_bitmap_mark(ds->bm_scan, i);
    }

    return 0;
}

errno_t repair_ds_pass(repair_data_t *rd) {
    reiser4_joint_t *joint;
    repair_ds_t *ds;
    blk_t blk = 0;
    errno_t res;
    uint8_t level;
    
    aal_assert("vpf-514", rd != NULL, return -1);
    aal_assert("vpf-514", rd->format != NULL, return -1);

    ds = repair_ds(rd);
    
    aal_assert("vpf-515", ds->bm_used != NULL, return -1);
    aal_assert("vpf-516", ds->bm_twig != NULL, return -1);

    if (repair_ds_setup(rd))
	return -1;

    while ((blk = aux_bitmap_find_marked(ds->bm_scan, blk)) != FAKE_BLK) {
	if ((joint = repair_joint_open(rd->format, blk)))
	    continue;

	level = plugin_call(return -1, joint->node->entity->plugin->node_ops, 
	    get_level, joint->node->entity);

	if (level != LEAF_LEVEL && level != TWIG_LEVEL) {
	    aux_bitmap_mark(ds->bm_frmt, blk);
	    reiser4_joint_close(joint);
	    continue;
	}

	res = repair_joint_check(joint, ds->bm_used);
	
	if (res < 0) {
	    reiser4_joint_close(joint);
	    return res;
	}

	/* If node was not recovered - mark it as formatted, otherwise mark it 
	 * as leaf or twig correspondently. */
	if (res > 0) 
	    aux_bitmap_mark(ds->bm_frmt, blk);
	else 
	    aux_bitmap_mark(level == LEAF_LEVEL ? ds->bm_leaf : ds->bm_twig, blk);

	reiser4_joint_close(joint);
    }
    
    return 0;
}
