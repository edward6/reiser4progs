/*
    repair/semantic.c -- semantic pass recovery code.
  
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/semantic.h>

static errno_t repair_semantic_object_check(reiser4_place_t *place, void *data) {
    reiser4_object_t object;
    repair_semantic_t *sem;
    reiser4_key_t parent;
    errno_t res = 0;
    
    aal_assert("vpf-1059", place != NULL);
    aal_assert("vpf-1037", data != NULL);
    
    /* Try to rebuild objects with Statdata only on semantic pass. */
    if (!reiser4_item_statdata(place))
	return 0;
    
    /* If this item was checked already, skip it. */
    if (repair_node_test_flag(place->node, place->pos.item, ITEM_CHECKED))
	return 0;
    
    sem = (repair_semantic_t *)data;
    
    aal_memset(&object, 0, sizeof(object));
    object.info.tree = sem->repair->fs->tree;
    aal_memcpy(&object.info.start, place, sizeof(*place));
    
    /* Try to realize the plugin. */
    if (repair_object_realize(&object))
	return 0;
    
    /* This is really an object, check its structure. */
    if ((res = repair_object_check_struct(&object, sem->repair->mode))) {
	aal_exception_error("Node %llu, item %u: check of the object structure "
	    "failed.", place->node->blk, place->pos.item);
	return res;
    }
    
    if ((res = repair_object_traverse(&object)))
	goto error_close_object;
    
    /* The whole reachable subtree must be recovered for now and marked as 
     * REACHABLE. */

    plugin_call(object.entity->plugin->o.object_ops, close, object.entity);

    return 0;
    
error_close_object:
    plugin_call(object.entity->plugin->o.object_ops, close, object.entity);
    return res;
}

static errno_t repair_semantic_node_traverse(reiser4_node_t *node, void *data) {
    return repair_node_traverse(node, repair_semantic_object_check, data);
}

errno_t repair_semantic(repair_semantic_t *sem) {
    repair_progress_t progress;
    reiser4_object_t object;
    traverse_hint_t hint;
    reiser4_fs_t *fs;
    errno_t res;
    
    aal_assert("vpf-1025", sem != NULL);
    aal_assert("vpf-1026", sem->repair != NULL);
    aal_assert("vpf-1027", sem->repair->fs != NULL);
    aal_assert("vpf-1028", sem->repair->fs->tree != NULL);
    
    sem->progress = &progress;
    aal_memset(sem->progress, 0, sizeof(*sem->progress));
    sem->progress->type = PROGRESS_TREE;
    sem->progress->title = "***** Semantic Traverse Pass: reiser4 semantic tree "
	"recovering.";
    sem->progress->text = "";
    time(&sem->stat.time);
    
    fs = sem->repair->fs;
    
    if (reiser4_tree_fresh(fs->tree)) {
	aal_exception_warn("No reiser4 metadata were found. Semantic pass is "
	    "skipped.");
	return 0;
    }
    
    reiser4_tree_load_root(fs->tree);
    
    if (fs->tree->root == NULL)
	return -EINVAL;
    
    aal_memset(&object, 0, sizeof(object));
    object.info.tree = fs->tree;
    object.info.parent = fs->tree->key;
    object.info.object = fs->tree->key;
    
    /* Make sure that '/' exists. */
    if (repair_object_realize(&object)) {
	reiser4_object_t *root;
	
	/* Failed to realize the root directory, create a new one. */	
	if (!(root = reiser4_dir_create(fs, NULL, NULL, fs->profile))) {
	    aal_exception_error("Failed to create the root directory.");
	    return -EINVAL;
	}
	
	reiser4_object_close(root);
    }
    
    hint.data = sem;
    hint.cleanup = 1;

    /* Cut the corrupted, unrecoverable parts of the tree off. */ 	
    res = reiser4_tree_down(fs->tree, fs->tree->root, &hint, NULL, 
	repair_semantic_node_traverse, NULL, NULL, NULL);
    
    if (res)
	return res;
    
    return 0;
}

