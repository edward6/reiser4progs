/*
    cleanup.c -- repair/cleanup.c -- cleanup pass recovery code.
  
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

/* The pass is intended for cleanuping the storage reiser4 tree from garbage
 * and linking not reachable objects to 'lost+found' directory. */

#include <repair/cleanup.h>

static errno_t repair_cleanup_check(reiser4_place_t *place, void *data) {
    repair_cleanup_t *cleanup;
    reiser4_object_t *object;
    errno_t res = 0;

    aal_assert("vpf-1060", place != NULL);
    aal_assert("vpf-1061", data != NULL);
    
    cleanup = (repair_cleanup_t *)data;
    
    if (!repair_node_test_flag(place->node, place->pos.item, ITEM_CHECKED)) {	
	
	/* Not checked item does not belong to any object, remove it. */	
	if ((res = reiser4_node_remove(place->node, &place->pos, 1))) {
	    aal_exception_bug("Node (%llu): Failed to delete the item (%d).", 
		place->node->blk, place->pos.item);
	    
	    return res;
	}
	
	cleanup->stat.removed++;
	place->pos.item--;
	
	return 0;	
    } else {
	
	/* Checked item belongs to some object, clear the flag. */	
	repair_node_clear_flag(place->node, place->pos.item, ITEM_CHECKED);
    }
    
    if (!repair_node_test_flag(place->node, place->pos.item, ITEM_REACHABLE)) {
	
	/* Try to open an object. */
	if (reiser4_object_can_begin(place) == FALSE)
	    return 0;
	
	object = reiser4_object_launch(cleanup->repair->fs->tree, place);
	if (object == NULL)
	    return 0;
	
	/* Not reachable object should be linked to 'lost+found'. */
	if ((res = reiser4_object_link(cleanup->lost, object, object->name))) {
	    aal_exception_error("Node (%llu), item(%u): openned object is "
		"failed to be linked to 'lost+found'.", place->node->blk, 
		place->pos.item);

	    return res;
	}
	
	cleanup->stat.linked++;
    } else {	
	
	/* Reachable, clear the flag. */
	repair_node_clear_flag(place->node, place->pos.item, ITEM_REACHABLE);
    }
    
    return res;
}

static errno_t repair_semantic_node_traverse(reiser4_node_t *node, void *data) {
    return repair_node_traverse(node, repair_cleanup_check, data);
}

static void repair_cleanup_setup(repair_cleanup_t *cleanup) {
    aal_assert("vpf-1045", cleanup != NULL);
    aal_assert("vpf-1046", cleanup->repair != NULL);
    aal_assert("vpf-1047", cleanup->repair->fs != NULL);
    aal_assert("vpf-1048", cleanup->repair->fs->tree != NULL);

    aal_memset(cleanup->progress, 0, sizeof(*cleanup->progress));
    cleanup->progress->type = PROGRESS_TREE;
    cleanup->progress->title = "***** Cleanup Pass: cleaning reiser4 storage "
	"tree up.";
    cleanup->progress->text = "";
    time(&cleanup->stat.time);
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
    
    aal_stream_format(&stream, "\tRemoved items %llu\n", cleanup->stat.removed);
    aal_stream_format(&stream, "\tObjects list to 'lost+found' %llu\n",
	cleanup->stat.linked);
	
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
    reiser4_object_t *root;
    repair_progress_t progress;
    traverse_hint_t hint;
    reiser4_fs_t *fs;
    errno_t res;
    
    repair_cleanup_setup(cleanup);
    
    fs = cleanup->repair->fs;
    
    if (reiser4_tree_fresh(fs->tree)) {
	aal_exception_warn("No reiser4 metadata were found. Cleanup pass is "
	    "skipped.");
	return 0;
    }
    
    reiser4_tree_load_root(fs->tree);
    
    if (fs->tree->root == NULL)
	return -EINVAL;
    
    /* Make sure that '/' and 'lost+found' exist. */
    root = reiser4_object_open(fs->tree, "/", FALSE);
    if (root == NULL) {
	aal_exception_error("Cleanup failed to find '/' directory.");
	return -EINVAL;
    }    
    
    cleanup->lost = reiser4_object_open(fs->tree, "/lost+found", FALSE);
    if (cleanup->lost == NULL) {	
	cleanup->lost = reiser4_dir_create(fs, "lost+found", root, fs->profile);
	if (cleanup->lost == NULL) {
	    aal_exception_error("Cleanup failed to find '/' directory.");
	    reiser4_object_close(root);
	    return -EINVAL;
	}
    }
    
    reiser4_object_close(root);
    
    hint.data = cleanup;
    hint.cleanup = 1;

    /* Cut the corrupted, unrecoverable parts of the tree off. */ 	
    res = reiser4_tree_down(fs->tree, fs->tree->root, &hint, NULL, 
	NULL, NULL, NULL, NULL);
    
    if (res)
	goto error_close_lost;
    
    reiser4_object_close(cleanup->lost);
    
    repair_cleanup_update(cleanup);
    
    return 0;
	    
error_close_lost:
    reiser4_object_close(cleanup->lost);
        
    return res;
}

