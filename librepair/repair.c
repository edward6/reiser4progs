/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by 
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
	uint64_t oid, files;
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
static errno_t cb_format_mark(blk_t start, count_t width, void *data) {
	aux_bitmap_t *format_layout = (aux_bitmap_t *)data;
	aux_bitmap_mark_region(format_layout, start, width);
	return 0;
}


static errno_t cb_alloc(reiser4_alloc_t *alloc, uint64_t start, 
			uint64_t count, void *data) 
{
	repair_control_t *control = (repair_control_t *)data;
	
	aal_assert("vpf-1332", data != NULL);

	aux_bitmap_mark_region(control->bm_used, start, count);

	return 0;
}

static errno_t cb_release(reiser4_alloc_t *alloc, uint64_t start, 
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
	filter->stat.files = &control->files;
	
	fs_len = reiser4_format_get_len(control->repair->fs->format);
	
	/* Allocate a bitmap of blocks belong to the format area - skipped, 
	   super block, journal, bitmaps. */
	if (!(control->bm_used = filter->bm_used = aux_bitmap_create(fs_len))) 
	{
		aal_error("Failed to allocate a bitmap of format layout.");
		return -EINVAL;
	}
	
	/* Mark all format area block in the bm_used bitmap. */
	if (reiser4_fs_layout(control->repair->fs, cb_format_mark,
			      filter->bm_used)) 
	{
		aal_error("Failed to mark the filesystem area as "
			  "used in the bitmap.");
		return -EINVAL;
	}
	
	control->repair->fs->alloc->hook.alloc = cb_alloc;
	control->repair->fs->alloc->hook.release = cb_release;
	control->repair->fs->alloc->hook.data = control;
	
	/* Allocate a bitmap of twig blocks in the tree. */
	if (!(control->bm_twig = filter->bm_twig = aux_bitmap_create(fs_len))) {
		aal_error("Failed to allocate a bitmap of twig blocks.");
		return -EINVAL;
	}

	if (control->repair->mode != RM_BUILD) 
		return 0;
	
	/* A bitmap of leaves removed from the tree and to be inserted back. */
	control->bm_leaf = filter->bm_leaf = aux_bitmap_create(fs_len);
	if (!control->bm_leaf) {
		aal_error("Failed to allocate a bitmap of leaves "
			  "unconnected from the tree.");
		return -EINVAL;
	}
	
	/* Allocate a bitmap of formatted blocks which cannot be pointed by 
	   extents, which are not in the used nor twig not leaf bitmaps. */
	if (!(control->bm_met = filter->bm_met = 
	      aux_bitmap_clone(filter->bm_used))) 
	{
		aal_error("Failed to allocate a bitmap of blocks "
			  "that are met on the filesystem.");
		return -EINVAL;
	}
	
	return 0; 
}

/* Mark blk in the scan bitmap if not marked in used bitmap (in this case it 
   is a node). */
static errno_t cb_region_mark(blk_t blk, uint64_t count, void *data) {
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

/* Setup the pass to be performed - create 2 new bitmaps for blocks to be 
   scanned, leaves, and formatted blocks which cannot be pointed by nodeptr's 
   and not accounted anywhere else; fill the scan bitmap with what should be 
   scanned. */
static errno_t repair_ds_prepare(repair_control_t *control, repair_ds_t *ds) {
	repair_data_t *repair;
	aal_stream_t stream;
	uint64_t fs_len, i;
	errno_t res;
	FILE *file;

	aal_assert("vpf-826", ds != NULL);
	aal_assert("vpf-825", control != NULL);
	aal_assert("vpf-840", control->repair != NULL);
	aal_assert("vpf-841", control->repair->fs != NULL);
	
	aal_memset(ds, 0, sizeof(*ds));
	ds->repair = control->repair;
	ds->bm_leaf = control->bm_leaf;
	ds->bm_twig = control->bm_twig;
	ds->bm_met = control->bm_met;
	ds->check_node = &control->check_node;
	ds->stat.files = &control->files;
	
	repair = ds->repair;
	
	fs_len = reiser4_format_get_len(repair->fs->format);
	
	
	if (control->repair->bitmap_file) {
		file = fopen(control->repair->bitmap_file, "r");
		
		if (file == NULL) {
			aal_error("Cannot not open the bitmap file (%s).",
				  control->repair->bitmap_file);
			return -EINVAL;
		}

		aal_stream_init(&stream, file, &file_stream);

		if (!(ds->bm_scan = aux_bitmap_unpack(&stream))) {
			aal_error("Can't unpack the bitmap of "
				  "packed blocks.");
			
			fclose(file);
			aal_stream_fini(&stream);
			return -EINVAL;
		}
		
		aal_stream_fini(&stream);
		fclose(file);

		control->bm_scan = ds->bm_scan;

		if (ds->bm_scan->total != fs_len) {
			aal_error("The bitmap in the file '%s' belongs to "
				  "another fs.", control->repair->bitmap_file);
			return -EINVAL;
		}
		
		/* Do not scan those blocks which are in the tree already. */
		for (i = 0; i < control->bm_met->size; i++)
			ds->bm_scan->map[i] &= ~control->bm_met->map[i];
		
		goto fini;
	} else {
		/* Allocate a bitmap of blocks to be scanned on this pass. */ 
		if (!(ds->bm_scan = control->bm_scan = aux_bitmap_create(fs_len))) {
			aal_error("Failed to allocate a bitmap of blocks "
				  "unconnected from the tree.");
			return -EINVAL;
		}
	}

	if (!(control->bm_alloc = aux_bitmap_create(fs_len))) {
		aal_error("Failed to allocate a bitmap of allocated blocks.");
		return -EINVAL;
	}

	if (control->repair->flags & (1 << REPAIR_WHOLE)) {
		aux_bitmap_invert(control->bm_alloc);
	} else {
		if ((res = reiser4_alloc_extract(repair->fs->alloc, 
						 control->bm_alloc)))
		{
			return res;
		}
	}
	
	/* Mark all broken regions of allocator as to be scanned. */
	if ((res = repair_alloc_layout_bad(repair->fs->alloc,
					   cb_region_mark, control)))
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
		
		/* Build a bitmap of blocks which are not in the tree yet.
		   Block was met as formatted, but unused in on-disk block
		   allocator. Looks like the bitmap block of the allocator 
		   has not been synced on disk. Scan through all its blocks. */
		if (~control->bm_alloc->map[i] & control->bm_met->map[i]) {
			reiser4_alloc_region(repair->fs->alloc, i * 8,
					     cb_region_mark, control);
		} else {
			control->bm_scan->map[i] |= 
				(control->bm_alloc->map[i] & 
				 ~control->bm_met->map[i]);
		}
	}
	
	aux_bitmap_close(control->bm_alloc);
	
 fini:
	aux_bitmap_calc_marked(control->bm_scan);
	
	/* Zeroing leaf & twig bitmaps of ndoes that are in the tree. */
	aux_bitmap_clear_region(control->bm_leaf, 0, control->bm_leaf->total);
	aux_bitmap_clear_region(control->bm_twig, 0, control->bm_twig->total);
	
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
	am->stat.files = &control->files;

	for (i = 0; i < control->bm_met->size; i++) {
		/* Leave there twigs and leaves that are not in the tree. */
		control->bm_twig->map[i] &= ~(control->bm_used->map[i]);
		control->bm_leaf->map[i] &= ~(control->bm_used->map[i]);
	}
	
	/* Assign the met bitmap to the block allocator. */
	aux_bitmap_calc_marked(control->bm_met);
	reiser4_alloc_assign(control->repair->fs->alloc, control->bm_met);

	/* Set the amount of used blocks, allow to allocate blocks on the 
	   add missing pass. */
	reiser4_format_set_free(control->repair->fs->format, 
				control->bm_met->total - 
				control->bm_met->marked);
	
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
	
	aal_assert("vpf-1335", control->repair->mode != RM_BUILD ||
		   !aux_bitmap_marked(control->bm_twig));

	aux_bitmap_close(control->bm_twig);
	control->bm_twig = NULL;
	sem->stat.files = control->files;
	
	if (control->repair->mode == RM_BUILD) {
		aal_assert("vpf-1335", control->repair->mode != RM_BUILD || 
			   !aux_bitmap_marked(control->bm_leaf));
		
		aux_bitmap_close(control->bm_leaf);
		control->bm_leaf = NULL;

		/* Assign the used bitmap to the block allocator. */
		reiser4_alloc_assign(control->repair->fs->alloc, control->bm_used);
		reiser4_alloc_sync(control->repair->fs->alloc);
	
		/* Set the amount of used blocks, allow to allocate blocks on 
		   the semantic pass */
		reiser4_format_set_free(control->repair->fs->format,
					control->bm_used->total - 
					control->bm_used->marked);

		aux_bitmap_close(control->bm_used);
		control->bm_used = NULL;
		
		control->repair->fs->alloc->hook.alloc = NULL;
		control->repair->fs->alloc->hook.release = NULL;
		control->repair->fs->alloc->hook.data = NULL;
	} else {
		aux_bitmap_t *bm_temp;
		uint64_t fs_len, i;
		errno_t res;
		
		fs_len = reiser4_format_get_len(control->repair->fs->format);

		if (!(control->bm_alloc = aux_bitmap_create(fs_len))) {
			aal_error("Failed to allocate a bitmap of "
				  "allocated blocks.");
			return -EINVAL;
		}

		if ((res = reiser4_alloc_extract(control->repair->fs->alloc,
						 control->bm_alloc)))
			return res;

		if (control->repair->mode == RM_CHECK)
			return 0;
		
		if (!(bm_temp = aux_bitmap_clone(control->bm_alloc))) {
			aal_error("Failed to allocate a backup of "
				  "allocated blocks bitmap.");
			return -EINVAL;
		}
		
		/* This is not the rebuild mode, do not throw away all unused
		   blocks mared as used in allocator. */
		for (i = 0; i < control->bm_used->size; i++)
			bm_temp->map[i] |= control->bm_used->map[i];


		/* All blocks that are met are forbidden for allocation. */
		reiser4_alloc_assign(control->repair->fs->alloc, bm_temp);

		aux_bitmap_close(bm_temp);
	}

	return 0;
}

static errno_t repair_sem_fini(repair_control_t *control, 
			       repair_semantic_t *sem) {
	uint64_t fs_len;

	control->oid = sem->stat.oid + 1;
	
	/* In the BUILD mode alloc was built on bm_used, nothing to do. */
	if (control->repair->mode == RM_BUILD)
		return 0;
	
	fs_len = reiser4_format_get_len(control->repair->fs->format);
	
	if (repair_bitmap_compare(control->bm_alloc, control->bm_used, 0)) {
		fsck_mess("On-disk used blocks and really used blocks "
			  "differ.%s", control->repair->mode == RM_FIX &&
			  !control->repair->fatal ? " Fixed." : "");

		if (control->repair->mode == RM_FIX && 
		    !control->repair->fatal)
		{
			/* Assign the bm_used bitmap to the block allocator. */
			reiser4_alloc_assign(control->repair->fs->alloc, 
					     control->bm_used);

			reiser4_alloc_sync(control->repair->fs->alloc);
		} else 
			control->repair->fixable++;
	}

	aux_bitmap_close(control->bm_alloc);
	control->bm_alloc = NULL;
	
	/* Do not close bm_used here to get the free blocks count in update. */

	control->repair->fs->alloc->hook.alloc = NULL;
	control->repair->fs->alloc->hook.release = NULL;
	control->repair->fs->alloc->hook.data = NULL;

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
		aal_error("Failed to allocate a bitmap of leaves "
			  "removed from the tree and to be inserted "
			  "later back item-by-item.");
		return -EINVAL;
	}
	
	if (!(am->bm_twig = aux_bitmap_create(fs_len))) {
		aal_error("Failed to allocate a bitmap of twig "
			  "blocks.");
		return -EINVAL;
	}
	
	aux_bitmap_calc_marked(am->bm_twig);
	aux_bitmap_calc_marked(am->bm_leaf);
	
	return 0;
}

static errno_t repair_update(repair_control_t *control) {
	uint64_t correct, val;
	reiser4_fs_t *fs;
	uint8_t mode;

	fs = control->repair->fs;
	mode = control->repair->mode;
	
	/* Get the correct free blocks count from the block allocator if BUILD
	   mode, otherwise from the used bitmap. */
	correct = mode == RM_BUILD ? reiser4_alloc_free(fs->alloc) :
		aux_bitmap_cleared(control->bm_used);
	
	val = reiser4_format_get_free(fs->format);
	
	if (correct != val) {
		if (mode != RM_BUILD) {
			fsck_mess("Free block count %llu found in the format is "
				  "wrong. %s %llu.", val, mode == RM_CHECK ? 
				  "Should be" : "Fixed to", correct);
		}

		if (mode != RM_CHECK)
			reiser4_format_set_free(fs->format, correct);
		else
			control->repair->fixable++;
	}
	
	/* Check the next free oid. */
	val = reiser4_oid_next(fs->oid);
	
	/* FIXME: This is oid40 specific fix, not correct. To be rewritten when
	   shared oid handling will be realy. */
	if (control->oid && control->oid > val) {
		if (mode != RM_BUILD) {
			fsck_mess("First not used oid %llu is wrong. %s %llu.",
				  val, mode == RM_CHECK ? "Should be" : 
				  "Fixed to", control->oid);
		}

		if (mode != RM_CHECK) {
			plug_call(fs->oid->ent->plug->o.oid_ops, set_next,
				  fs->oid->ent, control->oid);
		} else {
			control->repair->fixable++;
		}
	}
	
	/* The tree height should be set correctly at the filter pass. */
	/* FIXME: File count is not ready. What about flushes, mkfs_id? */
	
	return 0;
}

static void repair_control_release(repair_control_t *control) {
	aal_assert("vpf-738", control != NULL);

	if (control->bm_used) {
		aux_bitmap_close(control->bm_used);
		
		control->repair->fs->alloc->hook.alloc = NULL;
		control->repair->fs->alloc->hook.release = NULL;
		control->repair->fs->alloc->hook.data = NULL;
	}
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
	
	if (repair->flags & (1 << REPAIR_DEBUG)) {
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

	if (repair->fatal) {
		aal_warn("Fatal corruptions were found. "
			 "Semantic pass is skipped.");
		goto update;
	} 
	
	/* Check the semantic reiser4 tree. */
	if ((res = repair_sem_prepare(&control, &sem)))
		goto error;

	if ((res = repair_semantic(&sem)))
		goto error;

	if ((res = repair_sem_fini(&control, &sem)))
		goto error;

	if (repair->mode != RM_BUILD || repair->fatal) 
		goto update;

	/* Throw the garbage away. */
	if ((res = repair_cleanup_prepare(&control, &cleanup)))
		goto error;

	if ((res = repair_cleanup(&cleanup)))
		goto error;

	
 update:
	/* Update SB data */
	if (!repair->fatal && (res = repair_update(&control))) 
		goto error;
	
 error:
	repair_control_release(&control);
	
	return res;
}

