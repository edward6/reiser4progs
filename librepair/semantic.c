/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   repair/semantic.c -- semantic pass recovery code. */

#include <repair/semantic.h>

static void repair_semantic_lost_name(reiser4_object_t *object, char *name) {
	uint8_t len = aal_strlen(LOST_PREFIX);
	
	reiser4_key_string(&object->info.object, name);
	aal_memmove(object->name + len, object->name, OBJECT_NAME_SIZE - len);
	aal_memcpy(object->name, LOST_PREFIX, len);
}

/* Callback for repair_object_check_struct. Mark the passed item as CHECKED. */
static errno_t callback_check_struct(object_entity_t *object, 
				     place_t *place, void *data) 
{
	aal_assert("vpf-1114", object != NULL);
	aal_assert("vpf-1115", place != NULL);
	
	repair_item_set_flag((reiser4_place_t *)place, OF_CHECKED);
	
	return 0;
}

static errno_t repair_semantic_check_struct(repair_semantic_t *sem, 
					    reiser4_object_t *object) 
{
	reiser4_place_t *start;
	errno_t res;
	
	aal_assert("vpf-1169", sem != NULL);
	aal_assert("vpf-1170", object != NULL);
	
	start = reiser4_object_start(object);
	
	/* This is really an object, check its structure. */	
	if (repair_item_test_flag(start, OF_CHECKED))
		return 0;
	
	if ((res = repair_object_check_struct(object, callback_check_struct,
					      sem->repair->mode, NULL)))
	{
		aal_exception_error("Node (%llu), item (%u): check of "
				    "the object pointed by %s failed. "
				    "Plugin %s.", start->node->number,
				    start->pos.item, object->name,
				    object->entity->plugin->label);
	} else if (res & REPAIR_FATAL) {
		if (sem->repair->mode != REPAIR_REBUILD)
			sem->repair->fatal++;
	} else if (res & REPAIR_FIXABLE)
		sem->repair->fixable++;
	
	return res;
}

static errno_t repair_semantic_check_attach(repair_semantic_t *sem,
					    reiser4_object_t *parent,
					    reiser4_object_t *object) 
{
	reiser4_place_t *start;
	errno_t res;
	
	aal_assert("vpf-1182", sem != NULL);
	aal_assert("vpf-1183", object != NULL);
	
	start = reiser4_object_start(object);
	
	/* Even if this object is ATTACHED already it may allow many names
	   to itself -- check the attach with this @parent. */
	if ((res = repair_object_check_attach(parent, object,
					      sem->repair->mode)))
	{
		aal_exception_error("Node (%llu), item (%u): attach check "
				    "of the object pointed by %s failed. "
				    "Plugin %s.", start->node->number,
				    start->pos.item, object->name,
				    object->entity->plugin->label);
		return res;
	}
	
	/* If parent pointed does not exists in the object or matches the 
	   parent mark as ATTACHED. */
	if (sem->repair->mode == REPAIR_REBUILD && 
	    (object->info.parent.plugin == NULL ||
	     !reiser4_key_compare(&object->info.parent, &parent->info.object)))
	{
		repair_item_set_flag(start, OF_ATTACHED);
	}
		
	/* Increment the link. */
	return plugin_call(object->entity->plugin->o.object_ops, link, 
			   object->entity);
}

static errno_t repair_semantic_add_entry(reiser4_object_t *parent, 
					 reiser4_object_t *object, 
					 char *name)
{
	entry_hint_t entry;
	errno_t res;

	aal_memset(&entry, 0, sizeof(entry));
	aal_strncpy(entry.name, name, sizeof(entry.name));
	reiser4_key_assign(&entry.object, &object->info.object);

	if ((res = reiser4_object_add_entry(object, &entry))) {
		aal_exception_error("Can't add entry %s to %s.",
				    name, parent->name);
	}
	
	return res;
}

static errno_t repair_semantic_link(repair_semantic_t *sem, 
				    reiser4_object_t *parent,
				    reiser4_object_t *object,
				    char *name)
{
	errno_t res;

	aal_assert("vpf-1178", sem != NULL);
	aal_assert("vpf-1179", parent != NULL);
	aal_assert("vpf-1180", object != NULL);
	aal_assert("vpf-1181", name != NULL);
	
	if ((res = repair_semantic_add_entry(parent, object, name)))
		return res;
	
	return repair_semantic_check_attach(sem, parent, object);
}

static reiser4_object_t *repair_semantic_attach(repair_semantic_t *sem, 
						reiser4_object_t *object) 
{
	bool_t checked, name_missed = FALSE;
	reiser4_object_t *parent, *found;
	entry_hint_t entry;
	errno_t res;
	
	aal_assert("vpf-1184", object != NULL);

	if (!object->info.parent.plugin)
		return NULL;
	
	parent = repair_object_launch(object->info.tree, &object->info.parent);
	
	if (parent == NULL)
		goto error_object_detach;
	
	/* If ATTACHING -- parent is in the loop, break it here. */
	if (repair_item_test_flag(reiser4_object_start(parent), OF_ATTACHING))
		goto error_object_detach;
	
	checked = repair_item_test_flag(reiser4_object_start(parent), 
					OF_CHECKED);

	if (!checked) {
		/* Some parent was found, check it and attach to it. */
		res = repair_semantic_check_struct(sem, parent);

		if (res < 0)
			goto error_parent_close;
		else if (res & REPAIR_FATAL) {
			if (sem->repair->mode == REPAIR_REBUILD)
				goto error_object_detach;
			else
				goto error_parent_close;
		}

		/* Check that parent has a link to the object. */
		while (!(res = reiser4_object_readdir(parent, &entry))) {
			if (reiser4_key_compare(&object->info.object,
						&entry.object))
				break;
		}
		
		if (res) {
			repair_semantic_lost_name(object, object->name);
			if ((res = repair_semantic_add_entry(parent, object, 
							     object->name)))
				goto error_parent_close;
		}
	} else {
		/* Reached object was checked but the @object was not reached
		   then --> there is no entry pointing to @object. Add_entry. */
		repair_semantic_lost_name(object, object->name);
		if ((res = repair_semantic_add_entry(parent, object, 
						     object->name)))
			goto error_parent_close;
	}
		
	/* Parent was checked already, in other words - was traversed, 
	   and it is already ATTACHED or LOST, stop here to not traverse
	   it another time. */
	if (checked)
		goto error_parent_close;
	
	/* To not get into the loop. */
	repair_item_set_flag(reiser4_object_start(object), OF_ATTACHING);
	/* Get the @parent's parent. */
	found = repair_semantic_attach(sem, parent);
	/* Clear garbage. */
	repair_item_clear_flag(reiser4_object_start(object), OF_ATTACHING);
	
	if (found) {
		reiser4_object_close(parent);
		return found;
	}
	
	/* Connect parent to lost+found. */
	repair_semantic_lost_name(parent, parent->name);
	if ((res = repair_semantic_link(sem, sem->lost, parent, parent->name)))
		goto error_parent_close;
	
	return parent;
	
 error_object_detach:
	if (object->entity->plugin->o.object_ops->detach) {
		if (plugin_call(object->entity->plugin->o.object_ops,
				detach, object->entity, NULL))
			return INVAL_PTR;
	}
	return NULL;
	
 error_parent_close:
	reiser4_object_close(parent);
	return res < 0 ? INVAL_PTR : NULL;
}

static errno_t repair_semantic_uptraverse(repair_semantic_t *sem,
					  reiser4_object_t *object);

static reiser4_object_t *callback_object_traverse(reiser4_object_t *parent, 
						  entry_hint_t *entry,
						  void *data)
{
	repair_semantic_t *sem = (repair_semantic_t *)data;
	reiser4_object_t *object, *upper = NULL;
	errno_t res = REPAIR_OK;
	reiser4_place_t *start;
	bool_t flag;

	aal_assert("vpf-1172", parent != NULL);
	aal_assert("vpf-1173", entry != NULL);
	
	/* Traverse names only. Other entries should be recovered at 
	   check_struct and check_attach time. */
	if (entry->type != ET_NAME)
		return NULL;

	/* Try to realize unambiguously the object by the place. */
	if ((object = repair_object_launch(parent->info.tree, 
					   &entry->object)) == NULL)
	{
		if (sem->repair->mode == REPAIR_REBUILD)
			goto error_rem_entry;
		
		sem->repair->fixable++;
		return NULL;
	}
	
	start = reiser4_object_start(object);
	flag = repair_item_test_flag(start, OF_CHECKED);
	
	res = repair_semantic_check_struct(sem, object);
	
	if (res > 0 && res & REPAIR_FATAL && 
	    sem->repair->mode == REPAIR_REBUILD)
		goto error_rem_entry;
	else if (repair_error_fatal(res))
		goto error_close_object;
	
	/* If object knows about the object it was attached to, check_struct 
	   has saved it into the info->parent key. Check that this parent 
	   matches the given @parent, otherwise try to get the pointed parent
	   and its parent and so on and traverse that subtree also. */
	if (!repair_item_test_flag(start, OF_ATTACHED) && 
	    object->info.parent.plugin &&
	    reiser4_key_compare(&object->info.parent, &parent->info.object))
	{
		/* The object has not been ATTACHED yet [ a) was just checked;
		   b) is linked to "lost+found" ] and @object's parent key 
		   differs from @parent key. */
		if (flag) {
			/* Detach from "lost+found". */
			repair_semantic_lost_name(object, object->name);
			if ((res = reiser4_object_unlink(sem->lost, 
							 object->name)))
				goto error_close_object;
		} else {
			/* Check the parent pointed by info->parent. */
			if ((res = repair_semantic_uptraverse(sem, object)))
				goto error_close_object;
		}
	}
	
	flag = repair_item_test_flag(start, OF_ATTACHED);
	
	/* Check the attach with @parent. */
	if ((res = repair_semantic_check_attach(sem, parent, object)) < 0)
		goto error_close_object;
	else if (res & REPAIR_FATAL && sem->repair->mode == REPAIR_REBUILD)
		goto error_rem_entry;
	
	/* If object has been attached already -- it was traversed already. 
	   close the object here to avoid another traversing. */
	if (flag) {
		reiser4_object_close(object);
		return NULL;
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

/* Try to get to the root of not yet ATTACHED subtree by info->parent keys, 
   attach to the object pointed by that key or to "lost+found" and traverse
   from there. */
static errno_t repair_semantic_uptraverse(repair_semantic_t *sem,
					  reiser4_object_t *object)
{
	reiser4_object_t *upper;
	errno_t res;
	
	aal_assert("vpf-1191", sem != NULL);
	aal_assert("vpf-1192", object != NULL);

	if ((upper = repair_semantic_attach(sem, object)) == INVAL_PTR) {
		reiser4_object_close(object);
		return -EINVAL;
	}
	
	res = reiser4_object_traverse(upper, callback_object_traverse, sem);
	
	if (res)
		return res;
	
	reiser4_object_close(upper);

	return 0;
}

static errno_t callback_node_traverse(reiser4_place_t *place, void *data) {
	repair_semantic_t *sem = (repair_semantic_t *)data;
	reiser4_object_t *object, *upper;
	errno_t res;
	
	aal_assert("vpf-1171", place != NULL);
	aal_assert("vpf-1037", sem != NULL);
	
	/* If this item was checked already, skip it. */
	if (repair_item_test_flag(place, OF_CHECKED))
		return 0;
	
	/* Try to realize unambiguously the object by the place. */
	if ((object = repair_object_realize(sem->repair->fs->tree, 
					    place, TRUE)) == NULL)
		return 0;
	
	res = repair_semantic_check_struct(sem, object);
	
	if (res)
		goto error_close_object;
	
	/* Try to attach it somewhere -- at least to lost+found -- and 
	   traverse from the upper parent. */
	
	if ((upper = repair_semantic_attach(sem, object)) == INVAL_PTR) {
		reiser4_object_close(object);
		return -EINVAL;
	}
	
	if (upper == NULL) {
		/* Connect object to "lost+found". */
		repair_semantic_lost_name(object, object->name);
		if ((res = repair_semantic_link(sem, sem->lost, object,
						object->name)))
			goto error_close_object;
	}
	
	/* Traverse the found parent if any or the openned object. */
	res = reiser4_object_traverse(upper ? upper : object, 
				     callback_object_traverse, sem);
	if (upper)
		reiser4_object_close(upper);
	reiser4_object_close(object);
	
	return res;
	
 error_close_object:
	reiser4_object_close(object);	
	return res < 0 ? res : 0;
}

static errno_t repair_semantic_node_traverse(reiser4_tree_t *tree, 
					     reiser4_node_t *node, 
					     void *data) 
{
	return repair_node_traverse(node, callback_node_traverse, data);
}

static reiser4_object_t *repair_semantic_open_lost_found(repair_semantic_t *sem,
							 reiser4_object_t *root)
{
	/* FIXME-VITALY: Not ready yet. */
	return NULL;
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
	
	if (root) {
		/* '/' exists, check it and its subtree. */
		res = repair_semantic_check_struct(sem, root);
		
		if (repair_error_fatal(res))
			goto error_close_root;
		
		/* Open "lost+found" directory. */
		/* Create 'lost+found' directory. */
		sem->lost = repair_semantic_open_lost_found(sem, root);

		if (sem->lost == INVAL_PTR) {
			reiser4_object_close(root);
			return -EINVAL;
		}
		
		res = reiser4_object_traverse(root, callback_object_traverse, 
					      sem);
		if (res)
			goto error_close_lost;
	} else {
		/* Failed to realize the root directory, create a new one. */
		if (!(root = reiser4_dir_create(fs, NULL, NULL))) {
			aal_exception_error("Failed to create the root "
					    "directory.");
			return -EINVAL;
		}

	}
	
	/* If lost+found dir is not openned yet, that means that it failed to 
	   be openned&checked or the root directory has been just created. */
	if (sem->lost == NULL) {
		/* Create 'lost+found' directory. */
		sem->lost = reiser4_dir_create(fs, root, "lost+found");

		if (sem->lost == NULL) {
			aal_exception_error("Semantic pass failed: cannot "
					    "create 'lost+found' directory.");
			reiser4_object_close(root);
			return -EINVAL;
		}
	}

	res = repair_semantic_check_attach(sem, root, root);
	
	if (repair_error_fatal(res))
		goto error_close_lost;

	reiser4_object_close(root);
	
	/* Cut the corrupted, unrecoverable parts of the tree off. */ 	
	return reiser4_tree_down(fs->tree, fs->tree->root, NULL, 
				 repair_semantic_node_traverse, 
				 NULL, NULL, sem);
 error_close_lost:
	reiser4_object_close(sem->lost);
 error_close_root:
	reiser4_object_close(root);
	return res;
}

