/*  Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
    reiser4progs/COPYING.
    
    cleanup.c -- repair/cleanup.c -- cleanup pass recovery code.
    
    The pass is intended for cleanuping the storage reiser4 tree from
    not reacoverable garbage. */

#include <repair/cleanup.h>

static errno_t repair_cleanup_check(reiser4_place_t *place, void *data) {
	repair_cleanup_t *cleanup;
	errno_t res = 0;
	uint8_t i;
	
	aal_assert("vpf-1060", place != NULL);
	aal_assert("vpf-1061", data != NULL);
	
	cleanup = (repair_cleanup_t *)data;
	
	if (!repair_item_test_flag(place, OF_CHECKED)) {
		trans_hint_t hint;
		
		/* Not checked item does not belong to any object. Remove. */
		hint.count = 1;
		if ((res = reiser4_node_remove(place->node, &place->pos, &hint)))
			return res;
		
		cleanup->stat.removed++;
		place->pos.item--;
		
		return 0;	
	}

	for (i = 0; i < OF_LAST; i++)
		repair_item_clear_flag(place, i);
	return 0;
}

static errno_t repair_semantic_node_traverse(reiser4_tree_t *tree, 
					     reiser4_node_t *node, 
					     void *data) 
{
    return repair_node_traverse(node, repair_cleanup_check, data);
}

static void repair_cleanup_setup(repair_cleanup_t *cleanup) {
	aal_assert("vpf-1045", cleanup != NULL);
	aal_assert("vpf-1046", cleanup->repair != NULL);
	aal_assert("vpf-1047", cleanup->repair->fs != NULL);
	aal_assert("vpf-1048", cleanup->repair->fs->tree != NULL);

	if (!cleanup->progress_handler)
		return;
	
	aal_memset(cleanup->progress, 0, sizeof(*cleanup->progress));
	cleanup->progress->type = GAUGE_TREE;
	cleanup->progress->text = "***** Cleanup Pass: cleaning reiser4 "
		"storage tree up.";
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
	
	cleanup->progress->state = PROGRESS_END;
	cleanup->progress_handler(cleanup->progress);
	
	aal_stream_init(&stream);
	
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
	cleanup->progress->text = (char *)stream.data;
	cleanup->progress_handler(cleanup->progress);
	
	aal_stream_fini(&stream);
}

errno_t repair_cleanup(repair_cleanup_t *cleanup) {
	repair_progress_t progress;
	reiser4_fs_t *fs;
	errno_t res;
	
	cleanup->progress = &progress;
	repair_cleanup_setup(cleanup);
	
	fs = cleanup->repair->fs;
	
	if (reiser4_tree_fresh(fs->tree)) {
		aal_exception_warn("No reiser4 metadata were found. Cleanup "
				   "pass is skipped.");
		return 0;
	}
	
	if ((res = reiser4_tree_load_root(fs->tree)))
		return res;
	
	if (fs->tree->root == NULL)
		return -EINVAL;
	
	/* Cut the corrupted, unrecoverable parts of the tree off. */
	res = reiser4_tree_down(fs->tree, fs->tree->root, NULL, 
				repair_semantic_node_traverse, 
				NULL, NULL, cleanup);
	
	repair_cleanup_update(cleanup);
	reiser4_fs_sync(fs);
	
	return res;
}

