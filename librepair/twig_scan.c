/*  Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
    reiser4progs/COPYING.
    
    librepair/twig_scan.c - methods are needed for the second fsck pass. 
    Description: fsck on this pass zeroes extent pointers which point to 
    an already used block. Builds a map of used blocks. */

#include <repair/twig_scan.h>

/* Check unfm block pointer if it points to an already used block (leaf, 
   format area) or out of format area. Return 1 if it does, 0 - does not,
   -1 error. */
static errno_t callback_item_region_check(void *object, blk_t start, 
					  uint64_t count, void *data) 
{
	repair_ts_t *ts = (repair_ts_t *)data;
	
	aal_assert("vpf-385", ts != NULL);
	aal_assert("vpf-567", ts->bm_met != NULL);

	/* This must be fixed at the first pass. */
	if (start >= ts->bm_met->total || 
	    count > ts->bm_met->total ||
	    start >= ts->bm_met->total - count)
	{
		ts->stat.bad_unfm_ptrs++;
		return RE_FATAL;
	}
	
	/* Pointed region is used already. */
	if (aux_bitmap_test_region(ts->bm_met, start, count, 0) == FALSE) {
		ts->stat.bad_unfm_ptrs++;
		return RE_FIXABLE;
	}
	
	if (ts->bm_used)
		aux_bitmap_mark_region(ts->bm_used, start, count);
	
	aux_bitmap_mark_region(ts->bm_met, start, count);
	
	return 0;
}

/* Callback for the traverse which calls item_ops.check_layout method if 
   layout exists for all items which can contain data, not tree index data
   only. Shrink the node if item lenght is changed. */
static errno_t callback_check_layout(reiser4_place_t *place, void *data) {
	repair_ts_t *ts = (repair_ts_t *)data;
	reiser4_node_t *node;
	errno_t res;
	
	aal_assert("vpf-384", place != NULL);
	aal_assert("vpf-727", place->node != NULL);
	aal_assert("vpf-797", data != NULL);
	
	node = place->node;
	
	if (reiser4_item_branch(place->plug))
		return 0;
	
	if ((res = repair_item_check_layout(place, callback_item_region_check,
					    ts, ts->repair->mode)) < 0)
		return res;
	
	if (res & RE_FATAL) {
		if (ts->repair->mode == RM_BUILD) {
			trans_hint_t hint;

			aal_error("Node (%llu), item (%u): broken "
				  "item layout. Remove the item.",
				  node_blocknr(node), 
				  place->pos.item);

			hint.count = 1;
			hint.place_func = NULL;
			hint.region_func = NULL;
			hint.shift_flags = SF_DEFAULT;

			res = reiser4_node_remove(node, &place->pos, &hint);

			if (res < 0) return res;
			
			place->pos.item--;
		} else {
			ts->repair->fatal++;
		}
	} else if (res & RE_FIXABLE) {
		ts->repair->fixable++;
	} 
	
	return 0;
}

static void repair_twig_scan_setup(repair_ts_t *ts) {
	aal_assert("vpf-884", ts != NULL);
	
	aal_memset(ts->progress, 0, sizeof(*ts->progress));

	if (!ts->progress_handler)
		return;
	
	ts->progress->type = GAUGE_PERCENTAGE;
	ts->progress->text = "***** TwigScan Pass: checking extent pointers "
		"of all twigs.";
	
	time(&ts->stat.time);
	
	ts->progress->state = PROGRESS_START;
	ts->progress->u.rate.total = aux_bitmap_marked(ts->bm_twig);
	ts->progress_handler(ts->progress);
	
	ts->progress->state = PROGRESS_UPDATE;
	ts->progress->text = NULL;
}

static void repair_twig_scan_update(repair_ts_t *ts) {
	aal_stream_t stream;
	char *time_str;
	
	aal_assert("vpf-885", ts != NULL);
	
	if (!ts->progress_handler)
		return;
	
	ts->progress->state = PROGRESS_END;
	ts->progress_handler(ts->progress);
	
	aal_stream_init(&stream, NULL, &memory_stream);
	aal_stream_format(&stream, "\tRead twigs %llu\n", ts->stat.read_twigs);
	
	if (ts->stat.fixed_twigs) {
		aal_stream_format(&stream, "\tCorrected nodes %llu\n", 
				  ts->stat.fixed_twigs);	
	}
	
	if (ts->stat.bad_unfm_ptrs) {
		aal_stream_format(&stream, "\t%s extent pointers %llu\n", 
				  ts->repair->mode != RM_CHECK ? "Fixed invalid"
				  : "Invaid", ts->stat.bad_unfm_ptrs);
	}
	
	time_str = ctime(&ts->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	
	aal_stream_format(&stream, "\tTime interval: %s - ", time_str);
	
	time(&ts->stat.time);
	time_str = ctime(&ts->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	
	aal_stream_format(&stream, time_str);
	
	ts->progress->state = PROGRESS_STAT;
	ts->progress->text = (char *)stream.entity;
	ts->progress_handler(ts->progress);
	
	aal_stream_fini(&stream);
    
}

/* The pass itself, goes through all twigs, check block pointers which items 
   may have and account them in proper bitmaps. */
errno_t repair_twig_scan(repair_ts_t *ts) {
	repair_progress_t progress;
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
		
		node = reiser4_node_open(ts->repair->fs->tree, blk);
		
		if (node == NULL) {
			aal_fatal("Twig scan pass failed to open "
				  "the twig (%llu)", blk);
			return -EINVAL;
		}
		
		/* Lookup the node. */	
		res = repair_reiser4_node_traverse(node, callback_check_layout, ts);
		
		if (res) goto error_node_free;
		
		if (reiser4_node_isdirty(node))
			ts->stat.fixed_twigs++;
		
		if (!reiser4_node_locked(node))
			reiser4_node_fini(node);
		
		blk++;
	}
	
	repair_twig_scan_update(ts);

	if (ts->repair->mode != RM_CHECK)
		reiser4_fs_sync(ts->repair->fs);
	
	return 0;
	
 error_node_free:
	reiser4_node_close(node);
	repair_twig_scan_update(ts);

	return -EINVAL;
}
