/*
    librepair/repair.c - control methods for broken filesystem recovery.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/librepair.h>
#include <repair/filter.h>
#include <repair/disk_scan.h>
#include <repair/twig_scan.h>
#include <repair/add_missing.h>

typedef struct repair_data {
    reiser4_fs_t *fs;
    aux_bitmap_t *bm_used;	/* Formatted area + formatted nodes. */
    aux_bitmap_t *bm_leaf;	/* Leaf bitmap not in the tree yet. */
    aux_bitmap_t *bm_twig;	/* Twig nodes */
    aux_bitmap_t *bm_met;	/* frmt | used | leaf | twig. */
    aux_bitmap_t *bm_unfm_tree;	/* Unformatted blocks pointed from the tree. */
    aux_bitmap_t *bm_unfm_out;	/* Unformatted blocks pointed out of the tree. */
    
    repair_info_t *info;
    uint8_t mode;
} repair_data_t;

static void repair_check_setup(repair_data_t *rd, reiser4_fs_t *fs, 
    repair_info_t *info, uint8_t mode)
{
    aal_assert("vpf-505", rd != NULL);
    aal_assert("vpf-481", fs != NULL);
    aal_assert("vpf-823", info != NULL);
    aal_assert("vpf-824", mode == REPAIR_CHECK || mode == REPAIR_FIX || 
	mode == REPAIR_REBUILD);
 
    aal_memset(rd, 0, sizeof(*rd));
    
    rd->fs = fs;
    rd->mode = mode;
    rd->info = info;
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

static errno_t repair_filter_prepare(repair_data_t *rd, repair_filter_t *filter) {
    uint64_t fs_len;
    
    aal_assert("vpf-423", rd != NULL);
    aal_assert("vpf-592", filter != NULL);
    
    aal_memset(filter, 0, sizeof(*filter));    
    filter->fs = rd->fs;
    filter->mode = rd->mode;

    fs_len = reiser4_format_get_len(rd->fs->format);

    /* Allocate a bitmap of blocks belong to the format area - skipped, 
     * super block, journal, bitmaps. */
    if (!(rd->bm_used = filter->bm_used = aux_bitmap_create(fs_len))) {
	aal_exception_error("Failed to allocate a bitmap of format layout.");
	return -EINVAL;
    }

    /* Mark all format area block in the bm_used bitmap. */
    if (repair_fs_layout(rd->fs, callback_format_mark, rd->bm_used)) {
	aal_exception_error("Failed to mark the filesystem area as used in "
	    "the bitmap.");
	return -EINVAL;
    }
    
    /* A bitmap of leaves removed from the tree and to be inserted back. */
    if (!(rd->bm_leaf = filter->bm_leaf = aux_bitmap_create(fs_len))) {
	aal_exception_error("Failed to allocate a bitmap of leaves removed "
	    " from the tree and to be inserted later back item-by-item.");
	return -EINVAL;
    }
    
    /* Allocate a bitmap of twig blocks in the tree. */
    if (!(rd->bm_twig = filter->bm_twig = aux_bitmap_create(fs_len))) {
	aal_exception_error("Failed to allocate a bitmap of twig blocks.");
	return -EINVAL;
    }
 
    /* Allocate a bitmap of formatted blocks which cannot be pointed by 
     * extents, which are not in the used nor twig not leaf bitmaps. */
    if (!(rd->bm_met = filter->bm_met = aux_bitmap_create(fs_len))) {
	aal_exception_error("Failed to allocate a bitmap of broken formatted "
	    "blocks.");
	return -EINVAL;
    }

    return 0; 
}

/* Mark blk in the scan bitmap if not marked in used bitmap (in this case it is
 * a node). */
static errno_t callback_region_mark(void *object, blk_t blk, uint64_t count, 
    void *data)
{
    repair_ds_t *ds = (repair_ds_t *)data;
    uint64_t i;
    
    aal_assert("vpf-561", ds != NULL);

    for (i = blk; i < blk + count; i++) {
	if (!aux_bitmap_test(ds->bm_met, i))
	    aux_bitmap_mark(ds->bm_scan, i);
    }

    return 0;
}

/* Setup the pass to be performed - create 2 new bitmaps for blocks to be 
 * scanned, leaves, and formatted blocks which cannot be pointed by nodeptr's
 * and not accounted anywhere else; fill the scan bitmap with what should be 
 * scanned. */
static errno_t repair_ds_prepare(repair_data_t *rd, repair_ds_t *ds) {
    uint64_t fs_len, i;
    
    aal_assert("vpf-825", rd != NULL);
    aal_assert("vpf-826", ds != NULL);
    
    aal_memset(ds, 0, sizeof(*ds));
    ds->fs = rd->fs;
    ds->mode = rd->mode;
    ds->bm_leaf = rd->bm_leaf;
    ds->bm_twig = rd->bm_twig;
    ds->bm_met = rd->bm_met;
    
    fs_len = reiser4_format_get_len(rd->fs->format);
    
    /* Allocate a bitmap of blocks to be scanned on this pass. */ 
    if (!(rd->bm_unfm_tree = ds->bm_scan = aux_bitmap_create(fs_len))) {
	aal_exception_error("Failed to allocate a bitmap of blocks unconnected"
	    " from the tree.");
	return -EINVAL;
    }

    /* Build a bitmap of what was met already. */
    for (i = 0; i < rd->bm_met->size; i++) {	
	/* If block is marked as met, it should not be marked as used. */
	aal_assert("vpf-817", (rd->bm_met->map[i] & rd->bm_used->map[i]) == 0);

	/* bm_met is bm_met | bm_used | bm_leaf | bm_twig */
	rd->bm_met->map[i] |= (rd->bm_used->map[i] | rd->bm_leaf->map[i] | 
	    rd->bm_twig->map[i]);
    }
    
    /* Build a bitmap of blocks which are not in the tree yet. */
    for (i = reiser4_format_start(rd->fs->format); i < fs_len; i++) {
	    
	/* Block was met as formatted, but unused in on-disk block allocator. 
	 * Looks like the bitmap block of the allocator has not been synced on 
	 * disk. Scan through all its blocks. */
	if (aux_bitmap_test(ds->bm_met, i) && 
	    reiser4_alloc_unused_region(rd->fs->alloc, i, 1)) 
	{
	    reiser4_alloc_related_region(rd->fs->alloc, i, 
		callback_region_mark, ds);
	}

	if (reiser4_alloc_used_region(rd->fs->alloc, i, 1) && 
	    !aux_bitmap_test(ds->bm_met, i))
	    aux_bitmap_mark(ds->bm_scan, i);
    }
    
    return 0;
}

static errno_t repair_ts_prepare(repair_data_t *rd, repair_ts_t *ts) {
    uint64_t fs_len;

    aal_memset(ts, 0, sizeof(*ts));
    
    ts->fs = rd->fs;
    ts->mode = rd->mode;
    ts->bm_used = rd->bm_used;
    ts->bm_twig = rd->bm_twig;
    ts->bm_met = rd->bm_met;
    ts->bm_unfm_tree = rd->bm_unfm_tree;

    aux_bitmap_clear_all(rd->bm_unfm_tree);

    fs_len =  reiser4_format_get_len(ts->fs->format);
    
    if (!(rd->bm_unfm_out = ts->bm_unfm_tree = aux_bitmap_create(fs_len))) {
	aal_exception_error("Failed to allocate a bitmap of unformatted blocks "
	    "pointed by extents which are not in the tree.");
	return -EINVAL;
    }

    return 0;
}

static errno_t repair_am_prepare(repair_data_t *rd, repair_am_t *am) {
    uint64_t i;

    aal_memset(am, 0, sizeof(*am));
    
    am->fs = rd->fs;
    am->mode = rd->mode;

    am->bm_leaf = rd->bm_leaf;
    am->bm_twig = rd->bm_twig;
        
    for (i = 0; i < rd->bm_met->size; i++) {
	aal_assert("vpf-576", (rd->bm_met->map[i] & 
	    (rd->bm_unfm_tree->map[i] | rd->bm_unfm_out->map[i])) == 0);

	aal_assert("vpf-717", (rd->bm_used->map[i] & rd->bm_twig->map[i]) == 0);

	/* Let met will be leaves, twigs and unfm which are not in the tree. */
	rd->bm_met->map[i] = ((rd->bm_leaf->map[i] | rd->bm_twig->map[i] | 
	    rd->bm_unfm_out->map[i]) & ~(rd->bm_used->map[i]));
	
	/* Leave there only unused twigs. */
	rd->bm_twig->map[i] &= ~(rd->bm_used->map[i]);
	rd->bm_used->map[i] |= rd->bm_unfm_tree->map[i];
    }
    
    /* Assign the bm_met bitmap to the block allocator. */
    reiser4_alloc_assign(rd->fs->alloc, rd->bm_used);
    reiser4_alloc_assign_forb(rd->fs->alloc, rd->bm_met);
    
    aux_bitmap_close(rd->bm_used);
    aux_bitmap_close(rd->bm_met);
    aux_bitmap_close(rd->bm_unfm_tree);
    aux_bitmap_close(rd->bm_unfm_out);
    
    return 0;
}

static void fsck_release_info(repair_data_t *rd) {
    aal_assert("vpf-738", rd != NULL);

    if (rd->bm_used)
	aux_bitmap_close(rd->bm_used);
    if (rd->bm_leaf)
	aux_bitmap_close(rd->bm_leaf);
    if (rd->bm_twig)
	aux_bitmap_close(rd->bm_twig);
    if (rd->bm_met)
	aux_bitmap_close(rd->bm_met);
    if (rd->bm_unfm_tree)
	aux_bitmap_close(rd->bm_unfm_tree);
    if (rd->bm_unfm_out)
	aux_bitmap_close(rd->bm_unfm_out);
}

errno_t repair_check(reiser4_fs_t *fs, repair_info_t *info, uint8_t mode) {
    repair_data_t rd;
    repair_filter_t filter;
    repair_ds_t ds;
    repair_ts_t ts;
    repair_am_t am;
    errno_t res;

    repair_check_setup(&rd, fs, info, mode);
    
    if ((res = repair_filter_prepare(&rd, &filter)))
	goto error;
    
    if ((res = repair_filter(&filter)))
	goto error;
    
    if ((res = repair_ds_prepare(&rd, &ds)))
	goto error;
    
    if ((res = repair_disk_scan(&ds)))
	goto error;
     
    if ((res = repair_ts_prepare(&rd, &ts)))
	goto error;

    if ((res = repair_twig_scan(&ts)))
	goto error;
    
    if ((res = repair_am_prepare(&rd, &am)))
	goto error;

    if ((res = repair_add_missing(&am)))
	goto error;

    return 0;

error:
    return res;
}

