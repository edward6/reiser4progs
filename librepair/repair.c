/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/repair.c - control methods for broken filesystem recovery. */

#include <repair/librepair.h>
#include <repair/filter.h>
#include <repair/disk_scan.h>
#include <repair/twig_scan.h>
#include <repair/add_missing.h>
#include <repair/semantic.h>
#include <repair/cleanup.h>

typedef struct repair_control {
	repair_data_t *repair;

	aux_bitmap_t *bm_used;		/* Formatted area + formatted nodes. */
	aux_bitmap_t *bm_leaf;		/* Leaf bitmap not in the tree yet.  */
	aux_bitmap_t *bm_twig;		/* Twig nodes 			     */
	aux_bitmap_t *bm_met;		/* frmt | used | leaf | twig. 	     */
	aux_bitmap_t *bm_unfm_tree;	/* Unfmatted pointed from tree.      */
	aux_bitmap_t *bm_unfm_out;	/* Unfoamatted pointed out of tree.  */
} repair_control_t;

/* Callback for the format_ops.layout method - mark all blocks in the bitmap. */
static errno_t callback_format_mark(object_entity_t *format,
				    blk_t blk, void *data)
{
	aux_bitmap_t *format_layout = (aux_bitmap_t *)data;
	
	aux_bitmap_mark(format_layout, blk);
	
	return 0;
}

static errno_t repair_filter_prepare(repair_control_t *control, 
				     repair_filter_t *filter) 
{
	uint64_t fs_len;
	
	aal_assert("vpf-592", filter != NULL);
	aal_assert("vpf-423", control != NULL);
	aal_assert("vpf-838", control->repair != NULL);
	aal_assert("vpf-839", control->repair->fs != NULL);
	
	aal_memset(filter, 0, sizeof(*filter));    
	filter->repair = control->repair;
	
	filter->progress_handler = control->repair->progress_handler;
	
	fs_len = reiser4_format_get_len(control->repair->fs->format);
	
	/* Allocate a bitmap of blocks belong to the format area - skipped, 
	   super block, journal, bitmaps. */
	if (!(control->bm_used = filter->bm_used = aux_bitmap_create(fs_len))) {
		aal_exception_error("Failed to allocate a bitmap of format "
				    "layout.");
		return -EINVAL;
	}
	
	/* Mark all format area block in the bm_used bitmap. */
	if (reiser4_fs_layout(control->repair->fs, callback_format_mark,
			      filter->bm_used)) 
	{
		aal_exception_error("Failed to mark the filesystem area as "
				    "used in the bitmap.");
		return -EINVAL;
	}
	
	/* A bitmap of leaves removed from the tree and to be inserted back. */
	control->bm_leaf = filter->bm_leaf = aux_bitmap_create(fs_len);
	if (!control->bm_leaf) {
		aal_exception_error("Failed to allocate a bitmap of leaves "
				    "removed from the tree and to be inserted "
				    "later back item-by-item.");
		return -EINVAL;
	}
	
	/* Allocate a bitmap of twig blocks in the tree. */
	if (!(control->bm_twig = filter->bm_twig = aux_bitmap_create(fs_len))) {
		aal_exception_error("Failed to allocate a bitmap of twig "
				    "blocks.");
		return -EINVAL;
	}
	
	/* Allocate a bitmap of formatted blocks which cannot be pointed by 
	   extents, which are not in the used nor twig not leaf bitmaps. */
	if (!(control->bm_met = filter->bm_met = aux_bitmap_create(fs_len))) {
		aal_exception_error("Failed to allocate a bitmap of broken "
				    "formatted blocks.");
		return -EINVAL;
	}
	
	/* Allocate a bitmap of blocks to be scanned on this pass. */ 
	if (!(control->bm_unfm_tree = aux_bitmap_create(fs_len))) {
		aal_exception_error("Failed to allocate a bitmap of blocks "
				    "unconnected from the tree.");
		return -EINVAL;
	}
	
	return 0; 
}

/* Mark blk in the scan bitmap if not marked in used bitmap (in this case it 
   is a node). */
static errno_t callback_region_mark(void *object, blk_t blk, uint64_t count, 
				    void *data)
{
	repair_ds_t *ds = (repair_ds_t *)data;
	uint64_t i;
	
	aal_assert("vpf-561", ds != NULL);
	
	reiser4_alloc_occupy(ds->repair->fs->alloc, blk, count);
	
	for (i = blk; i < blk + count; i++) {
		if (!aux_bitmap_test(ds->bm_met, i))
			aux_bitmap_mark(ds->bm_scan, i);
	}
	
	return 0;
}

/* Setup the pass to be performed - create 2 new bitmaps for blocks to be 
   scanned, leaves, and formatted blocks which cannot be pointed by nodeptr's 
   and not accounted anywhere else; fill the scan bitmap with what should be 
   scanned. */
static errno_t repair_ds_prepare(repair_control_t *control, repair_ds_t *ds) {
	repair_data_t *repair;
	uint64_t fs_len, i;
	uint8_t avail;
	
	aal_assert("vpf-826", ds != NULL);
	aal_assert("vpf-825", control != NULL);
	aal_assert("vpf-840", control->repair != NULL);
	aal_assert("vpf-841", control->repair->fs != NULL);
	
	aal_memset(ds, 0, sizeof(*ds));
	ds->repair = control->repair;
	ds->bm_leaf = control->bm_leaf;
	ds->bm_twig = control->bm_twig;
	ds->bm_met = control->bm_met;
	ds->bm_scan = control->bm_unfm_tree;
	
	ds->progress_handler = control->repair->progress_handler;
	
	repair = ds->repair;
	
	fs_len = reiser4_format_get_len(repair->fs->format);
	
	/* Build a bitmap of what was met already. */
	for (i = 0; i < control->bm_met->size; i++) {
		/* If block is marked as met, it is not marked as used. */
		aal_assert("vpf-817", ( control->bm_met->map[i] & 
				        ( control->bm_used->map[i] | 
					  control->bm_leaf->map[i] | 
					  control->bm_twig->map[i] ) ) == 0);
		
		/* bm_met is bm_met | bm_used | bm_leaf | bm_twig */
		control->bm_met->map[i] |= (control->bm_used->map[i] | 
					    control->bm_leaf->map[i] | 
					    control->bm_twig->map[i]);
	}
	
	aux_bitmap_calc_marked(control->bm_met);
	
	i = reiser4_format_start(repair->fs->format);
	
	/* Build a bitmap of blocks which are not in the tree yet. */
	for (; i < fs_len; i++) {
		/* Block was met as formatted, but unused in on-disk block
		   allocator. Looks like the bitmap block of the allocator 
		   has not been synced on disk. Scan through all its blocks. */
		avail = reiser4_alloc_available(repair->fs->alloc, i, 1);
		
		if (avail && aux_bitmap_test(ds->bm_met, i)) {
			repair_alloc_related_region(repair->fs->alloc, i,
						    callback_region_mark, ds);
		} else if (!avail && !aux_bitmap_test(ds->bm_met, i)) {
			aux_bitmap_mark(ds->bm_scan, i);
		}
	}
	
	aux_bitmap_calc_marked(ds->bm_scan);
	
	return 0;
}

static errno_t repair_ts_prepare(repair_control_t *control, repair_ts_t *ts) {
	uint64_t fs_len;
	
	aal_assert("vpf-854", ts != NULL);
	aal_assert("vpf-856", control != NULL);
	aal_assert("vpf-858", control->repair != NULL);
	aal_assert("vpf-860", control->repair->fs != NULL);
	
	aal_memset(ts, 0, sizeof(*ts));
	
	ts->repair= control->repair;
	ts->bm_used = control->bm_used;
	ts->bm_twig = control->bm_twig;
	ts->bm_met = control->bm_met;
	ts->bm_unfm_tree = control->bm_unfm_tree;
	
	ts->progress_handler = control->repair->progress_handler;
	
	aux_bitmap_clear_region(control->bm_unfm_tree, 0, 
				control->bm_unfm_tree->total);
	
	fs_len =  reiser4_format_get_len(ts->repair->fs->format);
	
	if (control->repair->mode != RM_BUILD) {
		uint32_t i;
		
		for (i = 0; i < control->bm_met->size; i++) {
			aal_assert("vpf-864", ( control->bm_met->map[i] & 
					        ( control->bm_used->map[i] | 
						  control->bm_leaf->map[i] | 
						  control->bm_twig->map[i])) == 0);
			
			/* bm_met is bm_met | bm_used | bm_leaf | bm_twig */
			control->bm_met->map[i] |= (control->bm_used->map[i] | 
						    control->bm_leaf->map[i] | 
						    control->bm_twig->map[i]);
		}
	}
	
	aux_bitmap_calc_marked(control->bm_met);
	
	control->bm_unfm_out = ts->bm_unfm_out = aux_bitmap_create(fs_len);
	if (!control->bm_unfm_out) {
		aal_exception_error("Failed to allocate a bitmap of "
				    "unformatted blocks pointed by "
				    "extents which are not in the tree.");
		return -EINVAL;
	}
	
	return 0;
}

static errno_t repair_am_prepare(repair_control_t *control, repair_am_t *am) {
	uint64_t i;
	
	aal_assert("vpf-855", am != NULL);
	aal_assert("vpf-857", control != NULL);
	aal_assert("vpf-859", control->repair != NULL);
	aal_assert("vpf-861", control->repair->fs != NULL);
	
	aal_memset(am, 0, sizeof(*am));
	
	am->repair = control->repair;
	am->bm_leaf = control->bm_leaf;
	am->bm_twig = control->bm_twig;
	
	am->progress_handler = control->repair->progress_handler;
	
	for (i = 0; i < control->bm_met->size; i++) {
		aal_assert("vpf-576", ( control->bm_met->map[i] & 
				        ( control->bm_unfm_tree->map[i] | 
					  control->bm_unfm_out->map[i])) == 0);
		
		/* met is leaves, twigs and unfm. */
		control->bm_met->map[i] = (control->bm_leaf->map[i] | 
					   control->bm_twig->map[i] | 
					   control->bm_unfm_out->map[i] | 
					   control->bm_unfm_tree->map[i]);
		
		/* Leave there twigs, leaves, met which are not in the tree. */
		control->bm_met->map[i] &= ~(control->bm_used->map[i]);
		control->bm_twig->map[i] &= ~(control->bm_used->map[i]);
		control->bm_leaf->map[i] &= ~(control->bm_used->map[i]);
	}
	
	/* Assign the bm_met bitmap to the block allocator. */
	reiser4_alloc_assign(control->repair->fs->alloc, control->bm_used);
	reiser4_alloc_assign_forb(control->repair->fs->alloc, control->bm_met);
	
	aux_bitmap_close(control->bm_used);
	aux_bitmap_close(control->bm_met);
	aux_bitmap_close(control->bm_unfm_tree);
	aux_bitmap_close(control->bm_unfm_out);
	
	aux_bitmap_calc_marked(control->bm_twig);
	aux_bitmap_calc_marked(control->bm_leaf);
	
	control->bm_used = control->bm_met = control->bm_unfm_tree = 
		control->bm_unfm_out = NULL;
	
	return 0;
}

errno_t repair_sem_prepare(repair_control_t *control, repair_semantic_t *sem) {
	return 0;
}

static errno_t repair_cleanup_prepare(repair_control_t *control, 
				      repair_cleanup_t *cleanup) 
{
	uint64_t i;
	
	aal_assert("vpf-855", cleanup != NULL);
	aal_assert("vpf-857", control != NULL);
	aal_assert("vpf-859", control->repair != NULL);
	aal_assert("vpf-861", control->repair->fs != NULL);
	
	aal_memset(cleanup, 0, sizeof(*cleanup));
	
	cleanup->repair = control->repair;    
	cleanup->progress_handler = control->repair->progress_handler;
	
	return 0;
}

/* Debugging. */
static errno_t debug_am_prepare(repair_control_t *control, repair_am_t *am) {
	uint64_t fs_len;
	uint64_t i;
	
	aal_assert("vpf-855", am != NULL);
	aal_assert("vpf-857", control != NULL);
	aal_assert("vpf-859", control->repair != NULL);
	aal_assert("vpf-861", control->repair->fs != NULL);
	
	aal_memset(am, 0, sizeof(*am));
	
	am->repair = control->repair;
	
	fs_len = reiser4_format_get_len(control->repair->fs->format);
	
	if (!(am->bm_leaf = aux_bitmap_create(fs_len))) {
		aal_exception_error("Failed to allocate a bitmap of leaves "
				    "removed from the tree and to be inserted "
				    "later back item-by-item.");
		return -EINVAL;
	}
	
	if (!(am->bm_twig = aux_bitmap_create(fs_len))) {
		aal_exception_error("Failed to allocate a bitmap of twig "
				    "blocks.");
		return -EINVAL;
	}
	
	am->progress_handler = control->repair->progress_handler;
	
	aux_bitmap_calc_marked(am->bm_twig);
	aux_bitmap_calc_marked(am->bm_leaf);
	
	return 0;
}

static void repair_control_release(repair_control_t *control) {
	aal_assert("vpf-738", control != NULL);

	if (control->bm_used)
		aux_bitmap_close(control->bm_used);
	if (control->bm_leaf)
		aux_bitmap_close(control->bm_leaf);
	if (control->bm_twig)
		aux_bitmap_close(control->bm_twig);
	if (control->bm_met)
		aux_bitmap_close(control->bm_met);
	if (control->bm_unfm_tree)
		aux_bitmap_close(control->bm_unfm_tree);
	if (control->bm_unfm_out)
		aux_bitmap_close(control->bm_unfm_out);

	control->bm_used = control->bm_leaf = control->bm_twig = 
		control->bm_met = control->bm_unfm_tree = 
		control->bm_unfm_out = NULL;
}

errno_t repair_check(repair_data_t *repair) {
	repair_control_t control;
	repair_filter_t filter;
	repair_ds_t ds;
	repair_ts_t ts;
	repair_am_t am;
	repair_semantic_t sem;
	repair_cleanup_t cleanup;
	errno_t res;
	
	aal_assert("vpf-852", repair != NULL);
	aal_assert("vpf-853", repair->fs != NULL);
	
	aal_memset(&control, 0, sizeof(control));
	
	control.repair = repair;
	
	if (repair->debug_flag) {
		/* Debugging */
		if ((res = debug_am_prepare(&control, &am)))
			goto error;
		
		if ((res = repair_add_missing(&am)))
			goto error;
		
		return 0;
	}
	
	if ((res = repair_filter_prepare(&control, &filter)))
		goto error;
	
	if ((res = repair_filter(&filter)))
		goto error;
	
	if (repair->mode == RM_BUILD) {
		if ((res = repair_ds_prepare(&control, &ds)))
			goto error;
		
		if ((res = repair_disk_scan(&ds)))
			goto error;
	}
	
	if ((res = repair_ts_prepare(&control, &ts)))
		goto error;
	
	if ((res = repair_twig_scan(&ts)))
		goto error;
	
	if (repair->mode == RM_BUILD) {
		if ((res = repair_am_prepare(&control, &am)))
			goto error;
		
		if ((res = repair_add_missing(&am)))
			goto error;
	}
	
	if ((res = repair_sem_prepare(&control, &sem)))
		goto error;
	
	if ((res = repair_semantic(&sem)))
		goto error;
	
	if (repair->mode == RM_BUILD) {
		if ((res = repair_cleanup_prepare(&control, &cleanup)))
			goto error;
		
		if ((res = repair_cleanup(&cleanup)))
			goto error;
	}
	
 error:
	repair_control_release(&control);
	
	return res;
}

