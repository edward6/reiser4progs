/*  Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
    
    cleanup.c -- repair/cleanup.c -- cleanup pass recovery code.
    
    The pass is intended for cleanuping the storage reiser4 tree from
    not reacoverable garbage. */

#include <repair/cleanup.h>

static void repair_cleanup_update(repair_cleanup_t *cleanup) {
	aal_stream_t stream;
	char *time_str;
	
	aal_assert("vpf-1063", cleanup != NULL);
	
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
	aal_mess(stream.entity);
	aal_stream_fini(&stream);
}

static errno_t cb_free_extent(void *object, uint64_t start, 
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


static errno_t cb_node_cleanup(reiser4_place_t *place, void *data) {
	repair_cleanup_t *cleanup = (repair_cleanup_t *)data;
	trans_hint_t hint;
	int next_node;
	errno_t res;

	aal_assert("vpf-1425", place != NULL);
	aal_assert("vpf-1426", cleanup != NULL);
	aal_assert("vpf-1429", !reiser4_item_branch(place->plug));

	next_node = (!place->pos.item || !cleanup->neigh.node || 
		     place_blknr(&cleanup->neigh) != place_blknr(place));
	
	if (next_node) {
		aal_gauge_set_data(cleanup->gauge, place);
		aal_gauge_touch(cleanup->gauge);
	}
	
	/* Clear checked items. */
	if (reiser4_item_test_flag(place, OF_CHECKED)) {
		reiser4_item_clear_flags(place);

		if (!place->pos.item)
			goto next;
		
		/* Check if neighbour items are mergable. */
		if (next_node) {
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
			
			/*
			aal_mess("Node (%llu), items (%u, %u): fuse items "
				 "[%s], [%s].", place_blknr(place), 
				 cleanup->neigh.pos.item, place->pos.item,
				 reiser4_print_key(&cleanup->neigh.key, PO_DEFAULT),
				 reiser4_print_key(&place->key, PO_DEFAULT));
			*/

			place->pos.item--;
			
			if (reiser4_place_fetch(&cleanup->neigh))
				return -EINVAL;

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
	hint.region_func = cb_free_extent;
	hint.data = cleanup;
	hint.shift_flags = SF_DEFAULT;

	/*
	aal_mess("Node (%llu), item (%u): remove not used '%s' item [%s].",
		 place_blknr(place), place->pos.item, place->plug->label,
		 reiser4_print_key(&place->key, PO_DEFAULT));
	*/

	/* Remove not checked item. */
	res = reiser4_tree_remove(cleanup->repair->fs->tree, place, &hint);
	
	aal_memset(&cleanup->neigh, 0, sizeof(cleanup->neigh));
	
	return res ? res : 1;
}

extern void cb_gauge_tree_percent(aal_gauge_t *gauge);

errno_t repair_cleanup(repair_cleanup_t *cleanup) {
	errno_t res;
	
	aal_assert("vpf-1407", cleanup != NULL);
	aal_assert("vpf-1407", cleanup->repair != NULL);
	aal_assert("vpf-1407", cleanup->repair->fs != NULL);
	
	if (reiser4_tree_fresh(cleanup->repair->fs->tree)) {
		aal_fatal("No reiser4 metadata were found. Cleanup "
			 "pass is skipped.");
		return 0;
	}
	
	aal_mess("CLEANUPING STORAGE TREE");
	cleanup->gauge = aal_gauge_create(aux_gauge_handlers[GT_PROGRESS],
					  cb_gauge_tree_percent, NULL, 500, NULL);
	aal_gauge_set_value(cleanup->gauge, 0);
	aal_gauge_touch(cleanup->gauge);
	time(&cleanup->stat.time);

	if ((res = repair_tree_scan(cleanup->repair->fs->tree, 
				    cb_node_cleanup, cleanup)))
		goto error;

	aal_gauge_done(cleanup->gauge);
	aal_gauge_free(cleanup->gauge);
	repair_cleanup_update(cleanup);
	reiser4_fs_sync(cleanup->repair->fs);
	
	return 0;
	
 error:
	aal_gauge_done(cleanup->gauge);
	aal_gauge_free(cleanup->gauge);
	return res;
}

