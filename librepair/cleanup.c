/*  Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
    reiser4progs/COPYING.
    
    cleanup.c -- repair/cleanup.c -- cleanup pass recovery code.
    
    The pass is intended for cleanuping the storage reiser4 tree from
    not reacoverable garbage. */

#include <repair/cleanup.h>

static void repair_cleanup_setup(repair_cleanup_t *cleanup) {
	aal_assert("vpf-1045", cleanup != NULL);

	aal_memset(cleanup->progress, 0, sizeof(*cleanup->progress));
	
	if (!cleanup->progress_handler)
		return;
	
	cleanup->progress->type = GAUGE_TREE;
	cleanup->progress->text = "***** Cleanup Pass: cleaning "
		"up the reiser4 storage tree.";
	cleanup->progress->state = PROGRESS_STAT;
	time(&cleanup->stat.time);
	cleanup->progress_handler(cleanup->progress);
	cleanup->progress->text = NULL;
}

static void repair_cleanup_update(repair_cleanup_t *cleanup) {
	aal_stream_t stream;
	char *time_str;
	
	aal_assert("vpf-1063", cleanup != NULL);
	
	if (!cleanup->progress_handler)
		return;
	
	cleanup->progress->state = PROGRESS_STAT;
	cleanup->progress_handler(cleanup->progress);
	
	aal_stream_init(&stream, NULL, &memory_stream);
	
	aal_stream_format(&stream, "\tRemoved items %llu\n", 
			  cleanup->stat.removed);
	
	time_str = ctime(&cleanup->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	aal_stream_format(&stream, "\tTime interval: %s - ", time_str);
	time(&cleanup->stat.time);
	time_str = ctime(&cleanup->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	aal_stream_format(&stream, time_str);
	
	cleanup->progress->state = PROGRESS_STAT;
	cleanup->progress->text = (char *)stream.entity;
	cleanup->progress_handler(cleanup->progress);
	
	aal_stream_fini(&stream);
}

static errno_t callback_free_extent(void *object, uint64_t start, 
				    uint64_t count, void *data)
{
	repair_cleanup_t *cleanup = (repair_cleanup_t *)data;
	reiser4_alloc_t *alloc = cleanup->repair->fs->alloc;
	reiser4_place_t *place = (reiser4_place_t *)object;

	aal_assert("vpf-1420", place != NULL);

	if (reiser4_item_branch(place->plug))
		return 0;
	
	aal_assert("vpf-1418", reiser4_alloc_occupied(alloc, start, count));

	reiser4_alloc_release(alloc, start, count);
	
	return 0;
}

static errno_t callback_node_cleanup(reiser4_place_t *place, void *data) {
	repair_cleanup_t *cleanup = (repair_cleanup_t *)data;
	trans_hint_t hint;
	errno_t res;

	aal_assert("vpf-1425", place != NULL);
	aal_assert("vpf-1426", cleanup != NULL);
	aal_assert("vpf-1429", !reiser4_item_branch(place->plug));

	/* Clear checked items. */
	if (repair_item_test_flag(place, OF_CHECKED)) {
		repair_item_clear_flag(place, MAX_UINT16);

		if (!place->pos.item)
			goto next;
		
		/* Check if neighbour items are mergable. */
		if (!cleanup->neigh.node || 
		    node_blocknr(cleanup->neigh.node) != 
		    node_blocknr(place->node))
		{
			cleanup->neigh = *place;
			cleanup->neigh.pos.item--;
		
			if ((res = reiser4_place_fetch(&cleanup->neigh)))
				return res;
		}

		if (reiser4_item_mergeable(&cleanup->neigh, place)) {
			/* Fuse neighbour items. */
			if ((res = reiser4_node_fuse(place->node,
						     &cleanup->neigh.pos,
						     &place->pos)))
				return res;
			
			place->pos.item--;
			
			aal_memset(&cleanup->neigh, 0, 
				   sizeof(cleanup->neigh));
			return 0;
		} 
		
	next:
		/* Save the current place. */
		aal_memcpy(&cleanup->neigh, place, sizeof(*place));
		return 0;
	}

	/* Not checked. */
	cleanup->stat.removed++;
	
	place->pos.unit = MAX_UINT32;
	hint.count = 1;
	hint.place_func = NULL;
	hint.region_func = callback_free_extent;
	hint.data = cleanup;
	hint.shift_flags = SF_DEFAULT;

	/* Remove not checked item. */
	res = reiser4_tree_remove(cleanup->repair->fs->tree, place, &hint);
	
	aal_memset(&cleanup->neigh, 0, sizeof(cleanup->neigh));
	
	return res ? res : 1;
}

errno_t repair_cleanup(repair_cleanup_t *cleanup) {
	repair_progress_t progress;
	errno_t res;
	
	aal_assert("vpf-1407", cleanup != NULL);
	aal_assert("vpf-1407", cleanup->repair != NULL);
	aal_assert("vpf-1407", cleanup->repair->fs != NULL);
	
	if (reiser4_tree_fresh(cleanup->repair->fs->tree)) {
		aal_warn("No reiser4 metadata were found. Cleanup "
			 "pass is skipped.");
		return 0;
	}
	
	cleanup->progress = &progress;
	repair_cleanup_setup(cleanup);
	
	if ((res = repair_tree_scan(cleanup->repair->fs->tree, 
				    callback_node_cleanup, cleanup)))
		return res;

	repair_cleanup_update(cleanup);
	reiser4_fs_sync(cleanup->repair->fs);
	
	return 0;
}

