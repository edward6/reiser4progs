/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   repair/semantic.c -- semantic pass recovery code. */

#include <repair/semantic.h>

/* Callback for repair_object_check_struct. Mark the passed item as CHECKED. */
errno_t callback_check_struct(object_entity_t *object, place_t *place, 
			      void *data) 
{
	aal_assert("vpf-1114", object != NULL);
	aal_assert("vpf-1115", place != NULL);
	
	repair_item_set_flag((reiser4_place_t *)place, OF_CHECKED);
	
	return 0;
}

static errno_t callback_object_check_link(reiser4_object_t *object,
					  reiser4_object_t *parent,
					  entry_type_t link, 
					  void *data)
{
	repair_semantic_t *sem = (repair_semantic_t *)data;
	repair_ancestor_t *ancestor;
	
	aal_assert("vpf-1163", sem != NULL);
	aal_assert("vpf-1164", parent != NULL);
	aal_assert("vpf-1165", object != NULL);
	
	ancestor = (repair_ancestor_t *)aal_list_first(sem->path)->data;
	
	/* If @parent was reached before from the @object now we get here 
	   by the backlink, which is chacked already, skip another check. */
	if (!reiser4_key_compare(&ancestor->object->info.object, 
				 &object->info.object) &&
	    ET_BACKLINK(ancestor->link, link))
		return 0;
	
	/* Check the back link from the object to the parent. */
	return repair_object_check_backlink(object, parent, link, 
					    sem->repair->mode);
}

reiser4_object_t *repair_semantic_open_child(reiser4_object_t *parent,
					     entry_hint_t *entry,
					     repair_data_t *repair,
					     semantic_link_func_t func,
					     void *data)
{
	reiser4_object_t *object;
	errno_t res = REPAIR_OK;
	reiser4_place_t *start;
	bool_t checked;
	
	aal_assert("vpf-1101", parent != NULL);
	aal_assert("vpf-1102", entry != NULL);
	aal_assert("vpf-1146", repair != NULL);
	
	/* Trying to open the object by the given @entry->object key. */
	object = repair_object_launch(parent->info.tree, &entry->object);
	
	if (object == NULL) {
		if (repair->mode == REPAIR_REBUILD)
			goto error_rem_entry;
		
		repair->fixable++;
		return NULL;
	}
	
	start = reiser4_object_start(object);
	checked = repair_item_test_flag(start, OF_CHECKED);
	
	if (!checked) {
		/* The openned object has not been checked yet. */
		res = repair_object_check_struct(object, callback_check_struct, 
						 repair->mode, NULL);
	}
	
	if (!repair_error_fatal(res) && func)
		res |= func(object, parent, repair->mode, data);
	
	if (res < 0) {
		aal_exception_error("Node (%llu), item (%u): check of the "
				    "object pointed by %k from the %k (%s) "
				    "failed.", start->node->number, 
				    start->pos.item, &entry->object,
				    &entry->offset, entry->name);
		
		goto error_close_object;
	} else if (res & REPAIR_FATAL) {
		if (repair->mode == REPAIR_REBUILD)
			goto error_rem_entry;
		
		repair->fatal++;
		goto error_close_object;
	} else if (res & REPAIR_FIXABLE)
		repair->fixable++;
	
	/* Increment the link. */
	if ((res = plugin_call(object->entity->plugin->o.object_ops, link, 
			   object->entity)))
		goto error_close_object;
	
	if (repair->mode == REPAIR_REBUILD && entry->type == ET_NAME)
		repair_item_set_flag(start, OF_HAS_NAME);

	/* The object was chacked before, skip the traversing of its subtree. */
	if (checked) {
		reiser4_object_close(object);
		object = NULL;
	}
	
	return object;
	
 error_rem_entry:
	res = reiser4_object_rem_entry(parent, entry);

	if (res < 0) {
		aal_exception_error("Semantic traverse failed to remove the "
				    "entry %k (%s) pointing to %k.", 
				    &entry->offset, entry->name, 
				    &entry->object);
	}
	
 error_close_object:
	if (object)
		reiser4_object_close(object);
	
	return res < 0 ? INVAL_PTR : NULL;
}

static reiser4_object_t *callback_semantic_open(reiser4_object_t *parent, 
						entry_hint_t *entry,
						void *data)
{
	repair_semantic_t *sem = (repair_semantic_t *)data;
	repair_ancestor_t *ancestor;
	reiser4_object_t *object;
	
	aal_assert("vpf-1144", sem != NULL);
	
	object = repair_semantic_open_child(parent, entry, sem->repair,
					    callback_object_check_link, sem);
	
	if (object == INVAL_PTR || object == NULL)
		return object;
	
	if (!(ancestor = aal_calloc(sizeof(*ancestor), 0)))
		goto error_object_close;
    	
	ancestor->object = parent;
	ancestor->link = entry->type;
	
	sem->path = aal_list_insert(sem->path, ancestor, 0);
	
	return object;
	
 error_object_close:
	reiser4_object_close(object);
	return INVAL_PTR;
}

static void callback_semantic_close(reiser4_object_t *object, void *data) {
	repair_semantic_t *sem = (repair_semantic_t *)data;
	
	aal_assert("vpf-1160", object != NULL);
	aal_assert("vpf-1161", data != NULL);
	
	sem->path = aal_list_remove(sem->path, aal_list_first(sem->path));
	reiser4_object_close(object);
}

static errno_t repair_semantic_object_check(reiser4_place_t *place,
					    void *data) 
{
	reiser4_object_t *object;
	repair_semantic_t *sem;
	errno_t res = 0;
	
	aal_assert("vpf-1059", place != NULL);
	aal_assert("vpf-1037", data != NULL);
	
	sem = (repair_semantic_t *)data;
	
	/* Try to rebuild objects with Statdata only on semantic pass.
	if (!reiser4_item_statdata(place))
		return 0;
	*/

	/* If this item was checked already, skip it. */
	if (repair_item_test_flag(place, OF_CHECKED))
		return 0;
	
	/* Try to realize unambiguously the object by the place. */
	object = repair_object_realize(sem->repair->fs->tree, place, TRUE);
	
	if (!object)
		return 0;
	
	/* This is really an object, check its structure. */
	if ((res = repair_object_check_struct(object, callback_check_struct,
					      sem->repair->mode, NULL))) 
	{
		aal_exception_error("Node %llu, item %u: structure check of "
				    "the object pointed by %k failed. Plugin "
				    "%s.", place->node->number, 
				    place->pos.item, &place->item.key, 
				    object->entity->plugin->label);
		return res;
	}
	
	if ((res = repair_object_traverse(object, callback_semantic_open, 
					  callback_semantic_close, sem)))
		goto error_close_object;
	
	/* The whole reachable subtree must be recovered for now and marked as 
	   HAS_NAME. */
	
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
	sem->progress->title = "***** Semantic Traverse Pass: reiser4 semantic "
		"tree recovering.";
	sem->progress->text = "";
	time(&sem->stat.time);
	
	fs = sem->repair->fs;
	
	if (reiser4_tree_fresh(fs->tree)) {
		aal_exception_warn("No reiser4 metadata were found. Semantic "
				   "pass is skipped.");
		return 0;
	}
	
	reiser4_tree_lroot(fs->tree);
	
	if (fs->tree->root == NULL)
		return -EINVAL;
	
	root = repair_object_launch(sem->repair->fs->tree, &fs->tree->key);
	
	/* Make sure that '/' exists. */
	if (root == NULL) {
		/* Failed to realize the root directory, create a new one. */
		if (!(root = reiser4_dir_create(fs, NULL, NULL))) {
			aal_exception_error("Failed to create the root "
					    "directory.");
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

