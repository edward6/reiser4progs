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
#include <stdio.h>

typedef struct repair_control {
	repair_data_t *repair;
	
	aux_bitmap_t *bm_used;		/* Formatted area + formatted nodes. */
	aux_bitmap_t *bm_leaf;		/* Leaf bitmap not in the tree yet.  */
	aux_bitmap_t *bm_twig;		/* Twig nodes 			     */
	aux_bitmap_t *bm_met;		/* frmt | used | leaf | twig. 	     */
	aux_bitmap_t *bm_scan;
	
	aux_bitmap_t *bm_alloc;

	bool_t check_node;
	uint64_t oid;
} repair_control_t;

static errno_t repair_bitmap_compare(aux_bitmap_t *bm1, aux_bitmap_t *bm2, 
				     int verbose) 
{
	uint64_t j, i, diff, bytes, bits;

	aal_assert("vpf-1325",	bm1->size  == bm2->size && 
				bm1->total == bm2->total);

	diff = 0;

	/* compare full bytes */
	bytes = bm1->total / 8;
	bits = bytes * 8;
	
	if (aal_memcmp(bm1->map, bm2->map, bytes)) {
		/* Do not match, compare byte-by-byte. */
		for (j = 0; j < bytes; j++) {
			if (bm1->map[j] == bm2->map[j])
				continue;

			for (i = j * 8; i < (j + 1) * 8; i ++) {
				if (aux_bitmap_test(bm1, i) != 
				    aux_bitmap_test(bm2, i))
				{
					diff ++;

					if (!verbose)
						continue;
					
					fprintf(stderr, "Block (%llu) is %s "
						"in on-disk bitmap, should "
						"be not.\n", i, 
						aux_bitmap_test(bm1, i) ? 
						"marked" : "free");
				}
			}
		}
	}

	/* compare last byte of bitmap which can be used partially */
	bits = bm1->total % 8;
	
	for (i = bm1->size; i < bm1->size + bits; i ++) {
		if (aux_bitmap_test(bm1, i) != aux_bitmap_test(bm2, i)) {
			diff ++;

			if (!verbose)
				continue;

			fprintf(stderr, "Block (%llu) is %s in on-disk bitmap, "
				"should be not.\n", i, aux_bitmap_test(bm1, i) ?
				"marked" : "free");
		}
	}
	

	return diff;
}

/* Callback for the format_ops.layout method - mark all blocks in the bitmap. */
static errno_t callback_format_mark(void *format, blk_t start,
				    count_t width, void *data)
{
	aux_bitmap_t *format_layout = (aux_bitmap_t *)data;
	aux_bitmap_mark_region(format_layout, start, width);
	return 0;
}


static errno_t callback_alloc(reiser4_alloc_t *alloc, uint64_t start, 
			      uint64_t count, void *data) 
{
	repair_control_t *control = (repair_control_t *)data;
	
	aal_assert("vpf-1332", data != NULL);

	aux_bitmap_mark_region(control->bm_used, start, count);

	return 0;
}

static errno_t callback_release(reiser4_alloc_t *alloc, uint64_t start, 
				uint64_t count, void *data) 
{
	repair_control_t *control = (repair_control_t *)data;
	
	aal_assert("vpf-1333", data != NULL);

	aux_bitmap_clear_region(control->bm_used, start, count);

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
	filter->check_node = &control->check_node;
	
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
	
	control->repair->fs->alloc->hook.alloc = callback_alloc;
	control->repair->fs->alloc->hook.release = callback_release;
	control->repair->fs->alloc->hook.data = control;
	
	/* Allocate a bitmap of twig blocks in the tree. */
	if (!(control->bm_twig = filter->bm_twig = aux_bitmap_create(fs_len))) {
		aal_exception_error("Failed to allocate a bitmap of twig "
				    "blocks.");
		return -EINVAL;
	}

	if (control->repair->mode != RM_BUILD) 
		return 0;
	
	/* A bitmap of leaves removed from the tree and to be inserted back. */
	control->bm_leaf = filter->bm_leaf = aux_bitmap_create(fs_len);
	if (!control->bm_leaf) {
		aal_exception_error("Failed to allocate a bitmap of leaves "
				    "unconnected from the tree.");
		return -EINVAL;
	}
	
	/* Allocate a bitmap of formatted blocks which cannot be pointed by 
	   extents, which are not in the used nor twig not leaf bitmaps. */
	if (!(control->bm_met = filter->bm_met = 
	      aux_bitmap_clone(filter->bm_used))) 
	{
		aal_exception_error("Failed to allocate a bitmap of blocks "
				    "that are met on the filesystem.");
		return -EINVAL;
	}
	
	return 0; 
}

/* Mark blk in the scan bitmap if not marked in used bitmap (in this case it 
   is a node). */
static errno_t callback_region_mark(void *object, blk_t blk, 
				    uint64_t count, void *data)
{
	repair_control_t *control = (repair_control_t *)data;
	uint32_t i;
	
	aal_assert("vpf-561", control != NULL);
	
	aux_bitmap_mark_region(control->bm_alloc, blk, count);
	
	for (i = blk; i < blk + count; i++) {
		if (!aux_bitmap_test(control->bm_met, i))
			aux_bitmap_mark(control->bm_scan, i);
	}
	
	return 0;
}

static errno_t callback_layout_bad(void *object, blk_t blk, 
				   uint64_t count, void *data) 
{
	repair_control_t *control = (repair_control_t *)data;

	aal_assert("vpf-1324", control != NULL);

	aux_bitmap_mark_region(control->bm_scan, blk, count);

	return 0;
}

/* Setup the pass to be performed - create 2 new bitmaps for blocks to be 
   scanned, leaves, and formatted blocks which cannot be pointed by nodeptr's 
   and not accounted anywhere else; fill the scan bitmap with what should be 
   scanned. */
static errno_t repair_ds_prepare(repair_control_t *control, repair_ds_t *ds) {
	repair_data_t *repair;
	uint64_t fs_len, i;
	errno_t res;

	aal_assert("vpf-826", ds != NULL);
	aal_assert("vpf-825", control != NULL);
	aal_assert("vpf-840", control->repair != NULL);
	aal_assert("vpf-841", control->repair->fs != NULL);
	
	aal_memset(ds, 0, sizeof(*ds));
	ds->repair = control->repair;
	ds->bm_leaf = control->bm_leaf;
	ds->bm_twig = control->bm_twig;
	ds->bm_met = control->bm_met;
	ds->bm_scan = control->bm_scan;
	ds->check_node = &control->check_node;
	
	ds->progress_handler = control->repair->progress_handler;
	
	repair = ds->repair;
	
	fs_len = reiser4_format_get_len(repair->fs->format);
	
	if (!(control->bm_alloc = aux_bitmap_create(fs_len))) {
		aal_exception_error("Failed to allocate a bitmap of allocated "
				    "blocks.");
		return -EINVAL;
	}
	
	/* Allocate a bitmap of blocks to be scanned on this pass. */ 
	if (!(control->bm_scan = aux_bitmap_create(fs_len))) {
		aal_exception_error("Failed to allocate a bitmap of blocks "
				    "unconnected from the tree.");
		return -EINVAL;
	}

	if ((res = reiser4_alloc_extract(repair->fs->alloc, control->bm_alloc)))
		return res;
	
	/* Mark all broken regions of allocator as to be scanned. */
	if ((res = repair_alloc_layout_bad(repair->fs->alloc,
					   callback_layout_bad,
					   control)))
		return res;

	/* Build a bitmap of what was met already. */
	for (i = 0; i < control->bm_met->size; i++) {
		/* All used blocks are met also. */
		aal_assert("vpf-817",  (control->bm_used->map[i] & 
					~control->bm_met->map[i]) == 0);
		
		/* All twig blocks are met also. */
		aal_assert("vpf-1326", (control->bm_twig->map[i] & 
					~control->bm_met->map[i]) == 0);

		/* All leaf blocks are met also. */
		aal_assert("vpf-1329", (control->bm_leaf->map[i] & 
					~control->bm_met->map[i]) == 0);
		
		/* Zeroing leaf & twig bitmaps of ndoes taht are in the tree. */
		control->bm_twig->map[i] = 0;
		control->bm_leaf->map[i] = 0;
			
		/* Build a bitmap of blocks which are not in the tree yet.
		   Block was met as formatted, but unused in on-disk block
		   allocator. Looks like the bitmap block of the allocator 
		   has not been synced on disk. Scan through all its blocks. */
		if (~control->bm_alloc->map[i] & control->bm_met->map[i]) {
			repair_alloc_region(repair->fs->alloc, i * 8,
					    callback_region_mark, 
					    control);
		} else {
			control->bm_scan->map[i] |= control->bm_alloc->map[i]
				& ~control->bm_met->map[i];
		}
	}
	
	aux_bitmap_close(control->bm_alloc);
	aux_bitmap_calc_marked(control->bm_scan);
	aux_bitmap_calc_marked(control->bm_leaf);
	aux_bitmap_calc_marked(control->bm_twig);
	
	return 0;
}

static errno_t repair_ts_prepare(repair_control_t *control, repair_ts_t *ts, 
				 bool_t mark_used)
{
	aal_assert("vpf-854", ts != NULL);
	aal_assert("vpf-856", control != NULL);
	aal_assert("vpf-858", control->repair != NULL);
	aal_assert("vpf-860", control->repair->fs != NULL);
	
	aal_memset(ts, 0, sizeof(*ts));
	
	/* Not for the BUILD mode move used bitmap to the met one. */
	if (control->repair->mode != RM_BUILD)
		control->bm_met = control->bm_used;
	
	ts->repair= control->repair;
	
	/* If twigs which are not in the tree are scanned -- do not mark 
	   them as used, just as met. */
	ts->bm_used = mark_used ? control->bm_used : NULL;
	ts->bm_twig = control->bm_twig;
	ts->bm_met = control->bm_met;
	
	ts->progress_handler = control->repair->progress_handler;
	
	if (control->bm_scan) {
		/* If this is the twig scan that goes after disk_scan, 
		   close scan bitmap. */
		aux_bitmap_close(control->bm_scan);
		control->bm_scan = NULL;
	}
	
	return 0;
}

static void repair_ts_fini(repair_control_t *control) {
	/* Not for the BUILD mode met points to the bm_used. */
	control->bm_met = NULL;
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
	am->bm_used = control->bm_used;
	
	am->progress_handler = control->repair->progress_handler;
	
	for (i = 0; i < control->bm_met->size; i++) {
		/* Leave there twigs and leaves that are not in the tree. */
		control->bm_twig->map[i] &= ~(control->bm_used->map[i]);
		control->bm_leaf->map[i] &= ~(control->bm_used->map[i]);
	}
	
	/* Assign the met bitmap to the block allocator. */
	reiser4_alloc_assign(control->repair->fs->alloc, control->bm_met);
	
	aux_bitmap_close(control->bm_met);
	
	control->bm_met = NULL;
	
	aux_bitmap_calc_marked(control->bm_twig);
	aux_bitmap_calc_marked(control->bm_leaf);
	
	return 0;
}

static errno_t repair_sem_prepare(repair_control_t *control, 
				  repair_semantic_t *sem) 
{
	aal_assert("vpf-1274", sem != NULL);
	aal_assert("vpf-1275", control != NULL);
	aal_assert("vpf-1276", control->repair != NULL);
	aal_assert("vpf-1277", control->repair->fs != NULL);
	
	aal_memset(sem, 0, sizeof(*sem));
	
	sem->repair = control->repair;
	sem->progress_handler = control->repair->progress_handler;
	
	aal_assert("vpf-1335", control->repair->mode != RM_BUILD ||
		   !aux_bitmap_marked(control->bm_twig));

	aux_bitmap_close(control->bm_twig);
	control->bm_twig = NULL;
	
	if (control->repair->mode == RM_BUILD) {
		aal_assert("vpf-1335", control->repair->mode != RM_BUILD || 
			   !aux_bitmap_marked(control->bm_leaf));
		
		aux_bitmap_close(control->bm_leaf);
		control->bm_leaf = NULL;

		/* Assign the used bitmap to the block allocator. */
		reiser4_alloc_assign(control->repair->fs->alloc, control->bm_used);
		reiser4_alloc_sync(control->repair->fs->alloc);

		aux_bitmap_close(control->bm_used);
		control->bm_used = NULL;
	} else {
		aux_bitmap_t *bm_temp;
		uint64_t fs_len, i;
		errno_t res;
		
		fs_len = reiser4_format_get_len(control->repair->fs->format);

		if (!(control->bm_alloc = aux_bitmap_create(fs_len))) {
			aal_exception_error("Failed to allocate a bitmap of "
					    "allocated blocks.");
			return -EINVAL;
		}

		if ((res = reiser4_alloc_extract(control->repair->fs->alloc,
						 control->bm_alloc)))
			return res;

		if (control->repair->mode == RM_CHECK)
			return 0;
		
		if (!(bm_temp = aux_bitmap_clone(control->bm_alloc))) {
			aal_exception_error("Failed to allocate a backup of "
					    "allocated blocks bitmap.");
			return -EINVAL;
		}
		
		for (i = 0; i < control->bm_used->size; i++)
			bm_temp->map[i] |= control->bm_used->map[i];


		/* All blocks that are met are forbidden for allocation. */
		reiser4_alloc_assign(control->repair->fs->alloc, bm_temp);

		aux_bitmap_close(bm_temp);
	}

	return 0;
}

static errno_t repair_sem_fini(repair_control_t *control) {
	uint64_t fs_len;

	/* Build alloc on the base of bm_used, deallocate all bitmaps, 
	   clear forbidden blocks in alloc. 
	   In CHECK mode -- compare alloc bitmap and bm_used, sware, 
	   error++ 
	   In fixable mode == CHECK, but fix bitmaps if no fatal errors.
	 */
	
	if (control->repair->mode == RM_BUILD)
		return 0;
	
	fs_len = reiser4_format_get_len(control->repair->fs->format);
	
	if (repair_bitmap_compare(control->bm_alloc, control->bm_used, 0)) {
		aal_exception_error("On-disk used blocks and really used "
				    "blocks differ.%s", 
				    control->repair->mode == RM_FIX && 
				    !control->repair->fatal ? " Fixed." 
				    : "");

		if (control->repair->mode == RM_FIX && !control->repair->fatal)
		{
			/* Assign the bm_used bitmap to the block allocator. */
			reiser4_alloc_assign(control->repair->fs->alloc, 
					     control->bm_used);

			reiser4_alloc_sync(control->repair->fs->alloc);
		} else 
			control->repair->fixable++;
	}

	aux_bitmap_close(control->bm_used);
	aux_bitmap_close(control->bm_alloc);
	control->bm_used = control->bm_alloc = NULL;
	
	return 0;
}

static errno_t repair_cleanup_prepare(repair_control_t *control, 
				      repair_cleanup_t *cleanup) 
{
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

/* FIXME: update the max_oid in SB. */
static errno_t repair_update(repair_control_t *control) {
	if (control->repair->mode == RM_CHECK)
		return 0;
	
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

	control->bm_used = control->bm_leaf = control->bm_twig = 
		control->bm_met = NULL;
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
	
	/* Scan the storage reiser4 tree. Cut broken parts out. */
	if ((res = repair_filter_prepare(&control, &filter)))
		goto error;
	
	if ((res = repair_filter(&filter)))
		goto error;
	
	/* Scan twigs which are in the tree to avoid scanning the unformatted 
	   blocks at BUILD pass which are pointed by extents and preparing the 
	   allocable blocks. */
	if ((res = repair_ts_prepare(&control, &ts, repair->mode == RM_BUILD)))
		goto error;

	if ((res = repair_twig_scan(&ts)))
		goto error;

	if (repair->mode == RM_BUILD) {
		/* Scanning blocks which are used but not in the tree yet. */
		if ((res = repair_ds_prepare(&control, &ds)))
			goto error;
		
		if ((res = repair_disk_scan(&ds)))
			goto error;
		
		/* Scanning twigs which are not in the tree and fix if they 
		   point to some used block or some met formatted block. */
		if ((res = repair_ts_prepare(&control, &ts, 0)))
			goto error;

		if ((res = repair_twig_scan(&ts)))
			goto error;
		
		/* Inserting missed blocks into the tree. */
		if ((res = repair_am_prepare(&control, &am)))
			goto error;
		
		if ((res = repair_add_missing(&am)))
			goto error;
	} else {
		repair_ts_fini(&control);
	}

	if (repair->mode != RM_BUILD && repair->fatal) {
		aal_exception_mess("\nFatal corruptions were found. "
				   "Semantic pass is skipped.");
	} else {
		/* Check the semantic reiser4 tree. */
		if ((res = repair_sem_prepare(&control, &sem)))
			goto error;

		if ((res = repair_semantic(&sem)))
			goto error;

		if ((res = repair_sem_fini(&control)))
			goto error;
	}
		
	if (repair->mode == RM_BUILD) {
		/* Throw the garbage away. */
		if ((res = repair_cleanup_prepare(&control, &cleanup)))
			goto error;
		
		if ((res = repair_cleanup(&cleanup)))
			goto error;
	}
	
	/* Update SB data */
	if ((res = repair_update(&control))) 
		goto error;
	
 error:
	repair_control_release(&control);
	
	return res;
}

