/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   repair/semantic.c -- semantic pass recovery code. */

#include <repair/semantic.h>

extern errno_t callback_check_struct(object_entity_t *object, place_t *place, 
				     void *data);

/* Checks backlinks. */
static errno_t repair_semantic_check_link(reiser4_object_t *object,
					  reiser4_object_t *parent,
					  entry_hint_t *entry,
					  void *data)
{
	repair_semantic_t *sem = (repair_semantic_t *)data;
	entry_hint_t prev, curr;
	lookup_t lookup;
	errno_t res;
	char *name;
	
	aal_assert("vpf-1155", object != NULL);
	aal_assert("vpf-1156", parent != NULL);
	aal_assert("vpf-1157", data != NULL);
	
	/* Check the backlink only if traverse has not got into the loop - 
	   the object was reached from the parent and then the parent is 
	   reached from the child again through '..'. */
	name = (char *)object->data;
	
	while (name) {
		/* Save the current position. */
		if ((res = reiser4_object_readdir(object, &curr)))
			return res;
		
		/* Lookup the entry read the last time. */
		lookup = reiser4_object_lookup(object, name, &prev);
		
		/* The name was read, it must exist. */
		aal_assert("vpf-1158", lookup == PRESENT);
		
		if (!reiser4_key_compare(&prev.object, &parent->info.object)) {
			/* Loop detected, we must be at the current position again
			   as name belonged to the last read entry. */
			return 0;
		}

		break;
	}
	
	res = repair_object_check_backlink(object, parent, entry->type, 
					   sem->repair->mode);
	
	/* Move back to the current position. */
	if (name) {
		lookup = reiser4_object_lookup(object, curr.name, NULL);
		aal_assert("vpf-1159", lookup == PRESENT);
	}
	
	return res;
}

static errno_t callback_semantic_open(reiser4_object_t *parent, entry_hint_t *entry,
				      reiser4_object_t **object, void *data)
{
	repair_semantic_t *sem = (repair_semantic_t *)data;
	
	aal_assert("vpf-1144", sem != NULL);
	
	return repair_object_open(parent, entry, object, sem->repair,
				  sem->callback_check, sem);
}

static errno_t repair_semantic_object_check(reiser4_place_t *place, void *data) {
	reiser4_object_t *object;
	repair_semantic_t *sem;
	reiser4_key_t parent;
	errno_t res = 0;
	
	aal_assert("vpf-1059", place != NULL);
	aal_assert("vpf-1037", data != NULL);
	
	sem = (repair_semantic_t *)data;
	
	/* Try to rebuild objects with Statdata only on semantic pass.
	if (!reiser4_item_statdata(place))
		return 0;
	*/

	/* If this item was checked already, skip it. */
	if (repair_item_test_flag(place, ITEM_CHECKED))
		return 0;
	
	/* Try to realize unambiguously the object by the place. */
	if (!(object = repair_object_realize(sem->repair->fs->tree, place, TRUE)))
		return 0;
	
	/* This is really an object, check its structure. */
	if ((res = repair_object_check_struct(object, callback_check_struct,
					      sem->repair->mode, NULL))) 
	{
		aal_exception_error("Node %llu, item %u: structure check of the "
				    "object pointed by %k failed. Plugin %s.", 
				    place->node->number, place->pos.item, 
				    &place->item.key, 
				    object->entity->plugin->h.label);
		return res;
	}
	
	if ((res = repair_object_traverse(object, callback_semantic_open, sem)))
		goto error_close_object;
	
	/* The whole reachable subtree must be recovered for now and marked as 
	   REACHABLE. */
	
	reiser4_object_close(object);

	return 0;
	
 error_close_object:
	reiser4_object_close(object);
	return res;
}

static errno_t repair_semantic_node_traverse(reiser4_tree_t *tree, 
					     reiser4_node_t *node, 
					     void *data) 
{
	return repair_node_traverse(node, repair_semantic_object_check, data);
}

errno_t repair_semantic(repair_semantic_t *sem) {
	repair_progress_t progress;
	reiser4_plugin_t *plugin;
	reiser4_object_t *root;
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
	sem->callback_check = NULL;
	
	if (reiser4_tree_fresh(fs->tree)) {
		aal_exception_warn("No reiser4 metadata were found. Semantic pass is "
				   "skipped.");
		return 0;
	}
	
	reiser4_tree_load_root(fs->tree);
	
	if (fs->tree->root == NULL)
		return -EINVAL;
	
	root = repair_object_launch(sem->repair->fs->tree, NULL, &fs->tree->key);
	
	/* Make sure that '/' exists. */
	if (root == NULL) {
		/* Failed to realize the root directory, create a new one. */
		if (!(root = reiser4_dir_create(fs, NULL, NULL, fs->profile))) {
			aal_exception_error("Failed to create the root directory.");
			return -EINVAL;
		}
	}
	
	reiser4_object_close(root);
	
	/* Cut the corrupted, unrecoverable parts of the tree off. */ 	
	res = reiser4_tree_down(fs->tree, fs->tree->root, NULL, 
				repair_semantic_node_traverse, 
				NULL, NULL, sem);
	
	return res;
}

