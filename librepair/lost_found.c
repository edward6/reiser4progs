/*
    repair/lost_found.c -- lost&found pass recovery code.
  
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/lost_found.h>

static errno_t repair_lost_found_object_check(reiser4_place_t *place, 
    void *data) 
{
    reiser4_object_t *object;
    repair_lost_found_t *lf;
    repair_object_t hint;
    reiser4_key_t parent;
    errno_t res = 0;
    bool_t checked, reachable;
    
    aal_assert("vpf-1059", place != NULL);
    aal_assert("vpf-1037", data != NULL);
    
    checked = repair_node_test_flag(place->node, place->pos.item, ITEM_CHECKED);
    if (checked) {
	/* This item was checked already, if SD, is it REACHABLE? */
	if (!reiser4_item_statdata(place))
	    return 0;
	
	reachable = repair_node_test_flag(place->node, place->pos.item, 
	    ITEM_REACHABLE);
	
	if (reachable)
	    return 0;
    }
    
    lf = (repair_lost_found_t *)data;    

    /* This is not checked item or not reachable SD. Try to recover it. */
    if (!checked) {
	repair_object_init(&hint, lf->repair->fs->tree);
    
	/* Try to realize the plugin. */
	if (repair_object_realize(&hint, place))
	    return 0;
    
	/* This is really an object, check its structure. */
	if ((res = repair_object_check_struct(&hint, lf->repair->mode))) {
	    aal_exception_error("Node %llu, item %u: Check of the object "
		"openned on the item failed.", place->node->blk, 
		place->pos.item);
	    return res;
	}
    
	if ((object = repair_object_open(&hint)) == NULL) {
	    aal_exception_error("Node %llu, item %u: failed to open an object "
		"%k.", place->node->blk, place->pos.item, &hint.place.item.key);
	    return -EINVAL;
	}
    } else {
	/* Not reachable. */
	if ((object = reiser4_object_launch(lf->repair->fs, place)) == NULL) {
	    aal_exception_error("Node %llu, item %u: failed to open an object "
		"%k.", place->node->blk, place->pos.item, &hint.place.item.key);
	    return -EINVAL;
	}
    }
    
    /* link the object to its parent or to the "lost+found" directory. */
    if (object->parent.plugin) {
	/* Parent key was obtained from the object. Try to find the parent 
	 * object, if it fails, link the object to lost+found. */
    }
    
    /* The whole reachable subtree must be recovered for now and marked as 
     * REACHABLE. */
    
    return 0;

error_close_object:
    reiser4_object_close(object);

    return res;
}

static errno_t repair_lost_found_node_traverse(reiser4_node_t *node, 
    void *data) 
{
    return repair_node_traverse(node, repair_lost_found_object_check, data);
}

errno_t repair_lost_found(repair_lost_found_t *lf) {
    repair_progress_t progress;
    reiser4_object_t *root;
    traverse_hint_t hint;
    reiser4_fs_t *fs;
    errno_t res;
    
    aal_assert("vpf-1025", lf != NULL);
    aal_assert("vpf-1026", lf->repair != NULL);
    aal_assert("vpf-1027", lf->repair->fs != NULL);
    aal_assert("vpf-1028", lf->repair->fs->tree != NULL);
    
    lf->progress = &progress;
    aal_memset(lf->progress, 0, sizeof(*lf->progress));
    lf->progress->type = PROGRESS_TREE;
    lf->progress->title = "***** Lost&Found Pass: reiser4 fs recovering of "
	"lost objects.";
    lf->progress->text = "";
    time(&lf->stat.time);
    
    fs = lf->repair->fs;
    
    if (reiser4_tree_fresh(fs->tree)) {
	aal_exception_warn("No reiser4 metadata were found. Semantic pass is "
	    "skipped.");
	return 0;
    }
    
    reiser4_tree_load_root(fs->tree);
    
    if (fs->tree->root == NULL)
	return -EINVAL;
    
    if ((lf->lost = reiser4_object_open(fs, "/lost+found", FALSE)) == NULL) {
	/* 'lost+found' directory openning failed. Try to open '/' */
	
	if ((root = reiser4_object_open(fs, "/", FALSE)) == NULL) {
	    aal_exception_error("Lost&Found pass failed: no root directory "
		"found.");
	    return -EINVAL;
	}

	lf->lost = reiser4_dir_create(fs, "lost+found", root, fs->profile);
	
	if (lf->lost == NULL) {
	    aal_exception_error("Lost&Found pass failed: cannot create "
		"'lost+found' directory.");
	    reiser4_object_close(root);
	    return -EINVAL;
	}
	
	reiser4_object_close(root);
    }
    
    hint.data = lf;
    hint.cleanup = 1;

    /* Cut the corrupted, unrecoverable parts of the tree off. */ 	
    res = reiser4_tree_down(fs->tree, fs->tree->root, &hint, NULL, 
	repair_lost_found_node_traverse, NULL, NULL, NULL);
    
    if (res)
	return res;
    
    return 0;
    
error_close_root:
    reiser4_object_close(root);
    return res;
}

