/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   repair/semantic.c -- semantic pass recovery code. */

#include <repair/semantic.h>

static void repair_semantic_lost_name(reiser4_object_t *object, char *name) {
	char *key;
	uint8_t len;

	len = aal_strlen(LOST_PREFIX);
	key = reiser4_print_key(&object->info->object, PO_INO);
	
	aal_memcpy(name, LOST_PREFIX, len);
	aal_memcpy(name + len, key, aal_strlen(key));
}

/* Callback for repair_object_check_struct. Mark the passed item as CHECKED. */
static errno_t callback_check_struct(void *object, place_t *place,
				     void *data) 
{
	aal_assert("vpf-1114", object != NULL);
	aal_assert("vpf-1115", place != NULL);
	
	if (repair_item_test_flag((reiser4_place_t *)place, OF_CHECKED)) {
		aal_exception_error("Node (%llu), item (%u): item registering "
				    "failed--it belongs to another object "
				    "already. Plugin (%s).",
				    place->block->nr, place->pos.unit,
				    ((object_entity_t *)object)->plug->label);
		return 1;
	}
	
	repair_item_set_flag((reiser4_place_t *)place, OF_CHECKED);
	
	return 0;
}

/* Callback for repair_object_check_struct. Mark the passed item as CHECKED. */
static errno_t callback_register_item(void *object,
                                      place_t *place, void *data)
{
        aal_assert("vpf-1114", object != NULL);
        aal_assert("vpf-1115", place != NULL);
         
        if (repair_item_test_flag((reiser4_place_t *)place, OF_CHECKED)) {
                aal_exception_error("Node (%llu), item (%u): item registering "
                                    "failed--it belongs to another object "
                                    "already. Plugin (%s).",
				    place->block->nr, place->pos.unit,
				    ((object_entity_t *)object)->plug->label);
                return 1;
        }
         
        repair_item_set_flag((reiser4_place_t *)place, OF_CHECKED);
         
        return 0;
}
 
/* Callback for repair_object_check_struct. Register blocks of object layout. 
   All these blocks are marked used here, not on twig_scan pass, because it's
   easy to write regular file plugin objects of which will share some blocks.
   In this case this callback should take one parameter more--kind of region
   to be registered--REGION_SHARED. Not needed for now. */
static errno_t callback_register_region(void *o, uint64_t start, 
					uint64_t count, void *data)
{
	repair_semantic_t *sem = (repair_semantic_t *)data;
	object_entity_t *object = (object_entity_t *)o;
	
	aal_assert("vpf-1114", object != NULL);
	aal_assert("vpf-1217", data != NULL);
	
	if (reiser4_alloc_available(sem->repair->fs->alloc, start, count)) {
		/* FIXME-VITALY: print object key when available through @object. */
		aal_exception_error("Object failed to register the region "
				    "[%llu-%llu] -- it belongs to another "
				    "object already. Plugin (%s).", 
				    start, start + count - 1, 
				    object->plug->label);
		return 1;
	}
	
	reiser4_alloc_permit(sem->repair->fs->alloc, start, count);
	reiser4_alloc_occupy(sem->repair->fs->alloc, start, count);
	
	return 0;
}

static errno_t repair_semantic_check_struct(repair_semantic_t *sem, 
					    reiser4_object_t *object) 
{
	reiser4_place_t *start;
	errno_t res;
	
	aal_assert("vpf-1169", sem != NULL);
	aal_assert("vpf-1170", object != NULL);
	
	start = object_start(object);
	
	/* This is really an object, check its structure. */	
	if (repair_item_test_flag(start, OF_CHECKED))
		return 0;
	
	if ((res = repair_object_check_struct(object, callback_register_item,
					      callback_register_region,
					      sem->repair->mode, sem)) < 0)
		return res;
	
	if (res & RE_FATAL)
		sem->repair->fatal++;
	else if (res & RE_FIXABLE)
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
	
	start = object_start(object);
	
	/* Even if this object is ATTACHED already it may allow many names
	   to itself -- check the attach with this @parent. */
	if ((res = repair_object_check_attach(parent, object,
					      sem->repair->mode)) < 0)
		return res;
	
	if (res & RE_FATAL) {
		sem->repair->fatal++;
		return res;
	} else if (res & RE_FIXABLE)
		sem->repair->fixable++;
	
	if (sem->repair->mode != RM_BUILD)
		return res;
	
	/* Increment the link. */
	if ((res = plug_call(object->entity->plug->o.object_ops, link, 
			     object->entity)))
		return res;

	if (object->info->parent.plug == NULL)
		return 0;
	
	/* If parent pointed does not exists in the object or matches the 
	   parent mark as ATTACHED. */
	if (!reiser4_key_compare(&object->info->parent, &parent->info->object))
		repair_item_set_flag(start, OF_ATTACHED);
	
	return 0;
}

static errno_t repair_semantic_add_entry(reiser4_object_t *parent, 
					 reiser4_object_t *object, 
					 char *name)
{
	entry_hint_t entry;
	errno_t res;

	aal_memset(&entry, 0, sizeof(entry));
	aal_strncpy(entry.name, name, sizeof(entry.name));
	reiser4_key_assign(&entry.object, &object->info->object);

	if ((res = reiser4_object_add_entry(object, &entry)))
		aal_exception_error("Can't add entry %s to %s.",
				    name, parent->name);
	
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

static reiser4_object_t *repair_semantic_uplink(repair_semantic_t *sem, 
						reiser4_object_t *object) 
{
	bool_t checked, name_missed = FALSE;
	reiser4_object_t *parent, *found;
	reiser4_place_t *start;
	entry_hint_t entry;
	errno_t res;
	
	aal_assert("vpf-1184", object != NULL);

	if (!object->info->parent.plug)
		return NULL;
	
	/* Must be found exact matched plugin. Ambigious plugins will 
	   be recovered later on CLEANUP pass. */
	parent = repair_object_launch(object->info->tree, NULL, 
				      &object->info->parent);
	if (parent == INVAL_PTR)
		return INVAL_PTR;
	
	if (parent == NULL)
		goto error_object_detach;
	
	start = object_start(parent);
	
	/* If ATTACHING -- parent is in the loop, break it here. */
	if (repair_item_test_flag(start, OF_ATTACHING))
		goto error_object_detach;
	
	checked = repair_item_test_flag(start, OF_CHECKED);
	
	if (checked) {
		/* Reached object was checked but the @object was not reached
		   then --> there is no entry pointing to @object. Add_entry. */
		repair_semantic_lost_name(object, object->name);
		
		if ((res = repair_semantic_link(sem, parent, object, 
						object->name)))
			goto error_parent_close;
		
		/* If object get linked to "lost+found" clear ATTACHED flag to
		   relink it to some parent later if one will be found. */
		if (!reiser4_key_compare(&parent->info->object, 
					&sem->lost->info->object))
			repair_item_clear_flag(start, OF_ATTACHED);
		
		/* Parent was checked and traversed already, stop here to not
		   traverse it another time. */
		reiser4_object_close(parent);
		return NULL;
	}
	
	/* Some parent was found, check it and attach to it. */
	res = repair_semantic_check_struct(sem, parent);
	
	if (repair_error_fatal(res))
		goto error_parent_close;
	
	/* Check that parent has a link to the object. */
	while (!(res = reiser4_object_readdir(parent, &entry))) {
		if (reiser4_key_compare(&object->info->object,
					&entry.object))
			break;
	}

	if (res) {
		repair_semantic_lost_name(object, object->name);
		
		if ((res = repair_semantic_add_entry(parent, object, 
						     object->name)))
			goto error_parent_close;
	}
	
	/* To not get into the loop. */
	repair_item_set_flag(object_start(object), OF_ATTACHING);
	/* Get the @parent's parent. */
	found = repair_semantic_uplink(sem, parent);
	/* Clear garbage. */
	repair_item_clear_flag(object_start(object), OF_ATTACHING);
	
	if (found) {
		reiser4_object_close(parent);
		return found;
	}
	
	/* Parent of @parent == NULL, that means it is not found or ATTACHED 
	   object has been reached. If former -- attach it to "lost+found". */
	if (!repair_item_test_flag(start, OF_ATTACHED)) {
		repair_semantic_lost_name(parent, parent->name);
		
		if ((res = repair_semantic_link(sem, sem->lost, parent, parent->name)))
			goto error_parent_close;
		
		/* Clear ATTACHED flag to relink it to some parent later if one 
		   will be found. */
		repair_item_clear_flag(start, OF_ATTACHED);
	}
	
	return parent;
	
 error_object_detach:
	if (object->entity->plug->o.object_ops->detach) {
		if (plug_call(object->entity->plug->o.object_ops,
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
	reiser4_place_t *start;
	errno_t res = 0;
	bool_t flag;
	
	aal_assert("vpf-1172", parent != NULL);
	aal_assert("vpf-1173", entry != NULL);
	
	/* Traverse names only. Other entries should be recovered at 
	   check_struct and check_attach time. */
	if (entry->type != ET_NAME)
		return NULL;

	/* Try to recognize the object by the key. */
	if ((object = repair_object_launch(parent->info->tree, parent, 
					   &entry->object)) == INVAL_PTR)
		return INVAL_PTR;
	
	if (object == NULL) {
		if (sem->repair->mode != RM_CHECK)
			goto error_rem_entry;
		
		sem->repair->fixable++;
		return NULL;
	}
	
	start = object_start(object);
	flag = repair_item_test_flag(start, OF_CHECKED);
	
	res = repair_semantic_check_struct(sem, object);
	
	if (repair_error_fatal(res))
		goto error_close_object;
	
	/* If object knows about the object it was attached to, check_struct 
	   has saved it into the info->parent key. Check that this parent 
	   matches the given @parent, otherwise try to get the pointed parent
	   and its parent and so on and traverse that subtree also. */
	if (!repair_item_test_flag(start, OF_ATTACHED) && 
	    object->info->parent.plug && 
	    reiser4_key_compare(&object->info->parent, &parent->info->object))
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
	
	/* If ATTACHED after uptraverse, skip another traversing. */
	flag = repair_item_test_flag(start, OF_ATTACHED);
	
	/* Check the attach with @parent. */
	if ((res = repair_semantic_check_attach(sem, parent, object)) < 0)
		goto error_close_object;
	else if (res & RE_FATAL && sem->repair->mode == RM_BUILD) {
		sem->repair->fatal--;
		goto error_rem_entry;
	}
	
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
				    "entry \"%s\" [%s] pointing to [%s].", 
				    entry->name, 
				    reiser4_print_key(&entry->offset, PO_INO),
				    reiser4_print_key(&entry->object, PO_INO));
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

	if ((upper = repair_semantic_uplink(sem, object)) == INVAL_PTR) {
		reiser4_object_close(object);
		return -EINVAL;
	}
	
	res = reiser4_object_traverse(upper, callback_object_traverse, sem);
	
	reiser4_object_close(upper);

	return res;
}

static errno_t callback_node_traverse(reiser4_place_t *place, void *data) {
	repair_semantic_t *sem = (repair_semantic_t *)data;
	reiser4_object_t *object, *upper;
	errno_t res;
	
	aal_assert("vpf-1171", place != NULL);
	aal_assert("vpf-1037", sem != NULL);
	
	/* Objects w/out SD get recovered only when reached from the parent. */
	if (!reiser4_item_statdata(place))
		return 0;
		
	/* If this item was checked already, skip it. */
	if (repair_item_test_flag(place, OF_CHECKED))
		return 0;
	
	/* Try to open the object by its SD. */
	object = repair_object_recognize(sem->repair->fs->tree, NULL, place);
	
	if (object == NULL)
		return 0;
	
	res = repair_semantic_check_struct(sem, object);

	if (repair_error_fatal(res))
		goto error_close_object;
	
	/* Try to attach it somewhere -- at least to lost+found -- and 
	   traverse from the upper parent. */
	if ((upper = repair_semantic_uplink(sem, object)) == INVAL_PTR) {
		reiser4_object_close(object);
		return -EINVAL;
	}
	
	if (upper == NULL) {
		/* Connect object to "lost+found". */
		repair_semantic_lost_name(object, object->name);
		
		if ((res = repair_semantic_link(sem, sem->lost, object,
						object->name)))
			goto error_close_object;

		/* Clear ATTACHED flag to relink it to some parent later
		   if one will be found. */
		repair_item_clear_flag(object_start(object), 
				       OF_ATTACHED);
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

static reiser4_object_t *repair_semantic_dir_open(repair_semantic_t *sem,
						  reiser4_object_t *parent,
						  reiser4_key_t *key)
{
	reiser4_object_t *object;
	reiser4_plug_t *plug;
	reiser4_tree_t *tree;
	rid_t pid;
	
	aal_assert("vpf-1250", sem != NULL);
	aal_assert("vpf-1251", key != NULL);
	
	tree = sem->repair->fs->tree;
	
	if ((object = repair_object_launch(tree, parent, key)) == INVAL_PTR)
		return INVAL_PTR;
	
	if (object) {
		/* Check that the object was recognized by the dir plugin. */
		if (object->entity->plug->id.group == DIR_OBJECT)
			return object;

		aal_exception_error("The directory [%s] is recognized by the "
				    "%s plugin which is not a directory one.", 
				    reiser4_print_key(key, PO_INO), 
				    object->entity->plug->label);
		
		reiser4_object_close(object);
	} else {
		/* No plugin was recognized. */
		aal_exception_error("Failed to recognize the plugin for the "
				    "directory [%s].", 
				    reiser4_print_key(key, PO_INO));
	}
	
	if (sem->repair->mode != RM_BUILD)
		return NULL;
	
	if ((pid = reiser4_profile_value("directory")) == INVAL_PID) {
		aal_exception_error("Can't get the valid plugin id "
				    "for the directory plugin.");
		return INVAL_PTR;
	}

	if (!(plug = reiser4_factory_ifind(OBJECT_PLUG_TYPE, pid))) {
		aal_exception_error("Can't find item plugin by its "
				    "id 0x%x.", pid);
		return INVAL_PTR;
	}


	aal_exception_error("Trying to recover the directory [%s] "
			    "with the default plugin--%s.",
			    reiser4_print_key(key, PO_INO), plug->label);

	
	return repair_object_fake(tree, parent, key, plug);
}

static reiser4_object_t *repair_semantic_lost_open(repair_semantic_t *sem, 
						   reiser4_object_t *root) 
{
	reiser4_object_t *lost;
	entry_hint_t entry;
	errno_t res;
	
	aal_assert("vpf-1193", sem != NULL);
	aal_assert("vpf-1194", root != NULL);
	
	/* Look for the entry "lost+found" in the "/". */
	switch (reiser4_object_lookup(root, "lost+found", &entry)) {
	case ABSENT:
		return NULL;
	case FAILED:
		return INVAL_PTR;
	default:
		break;
	}

	/* The entry was found, take the key of "lost+found" and try to 
	   open the object. */
	lost = repair_semantic_dir_open(sem, root, &entry.object);

	if (lost == INVAL_PTR || lost != NULL)
		return lost;

	/* "lost+found" has not been openned, remove the entry from "/". */
	if ((res = reiser4_object_rem_entry(root, &entry)))
		return INVAL_PTR;
	
	return NULL;
}

static errno_t repair_semantic_lost_prepare(repair_semantic_t *sem, 
					    reiser4_object_t *root)
{
	uint8_t len = aal_strlen(LOST_PREFIX);
	entry_hint_t entry;

	aal_assert("vpf-1252", sem != NULL);
	aal_assert("vpf-1253", root != NULL);

	if ((sem->lost = repair_semantic_lost_open(sem, root)) == INVAL_PTR) {
		sem->lost = NULL;
		return -EINVAL;
	}
	
	/* There is no "lost+found" entry in the "/". Create a new one. */
	if (!sem->lost) {
		/* Create 'lost+found' directory. */
		if (!(sem->lost = reiser4_dir_create(sem->repair->fs, root, 
						     "lost+found"))) 
		{
			aal_exception_error("Semantic pass failed: cannot "
					    "create 'lost+found' directory.");
			return -EINVAL;
		}
		
		return 0;
	}
	
	/* Lost was openned, check the structure. This is the BUILD mode -- no 
	   error should appear. */
	if (repair_semantic_check_struct(sem, sem->lost))
		goto error_close_lost;
	
	/* Remove all "lost_found_" names from "lost+found" directory. 
	   This is needed to not have any special case later -- when 
	   some object gets linked to "lost+found" it is not marked as 
	   ATTCHED to relink it later to some another object having 
	   the valid name if such is found. */
	while (!reiser4_object_readdir(sem->lost, &entry)) {
		if (!aal_memcmp(entry.name, LOST_PREFIX, len)) {
			if (reiser4_object_rem_entry(sem->lost, &entry))
				goto error_close_lost;
		}
	}
	
	/* FIXME-VITALY: how it will be handled later at traverse time?
	   l&f will be CHECKED&ATTACHED already, root will have nlink++ed. */
	if (repair_semantic_check_attach(sem, root, sem->lost))
		goto error_close_lost;
	
	return 0;
	
 error_close_lost:
	reiser4_object_close(sem->lost);
	sem->lost = NULL;
	return -EINVAL;
}

errno_t repair_semantic(repair_semantic_t *sem) {
	repair_progress_t progress;
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
	
	/* Trying to recognize the root dir by the given key. 
	   If it fails create a fake one  */
	if (!(root = repair_semantic_dir_open(sem, NULL, &fs->tree->key))) {
		sem->repair->fatal++;
		return 0;
	} else if (root == INVAL_PTR) {
		return -EINVAL;
	}
	
	/* '/' exists, check it and its subtree. */
	res = repair_semantic_check_struct(sem, root);
	
	/* Do not traverse "/" if fatal corruptions exists there. */
	if (repair_error_fatal(res))
		goto error_close_root;

	/* Open "lost+found" directory in BUILD mode. */
	if (sem->repair->mode == RM_BUILD) {
		if ((res = repair_semantic_lost_prepare(sem, root)))
			goto error_close_root;
	}

	/* Traverse the root dir -- recover all objects which can be 
	   reached from the root. */
	if ((res = reiser4_object_traverse(root, callback_object_traverse, sem)))
		goto error_close_lost;

	res = repair_semantic_check_attach(sem, root, root);
	
	if (repair_error_fatal(res))
		goto error_close_lost;

	reiser4_object_close(root);
	
	/* Connect lost objects to their parents -- if parents can be 
	   identified -- or to "lost+found". */
	return reiser4_tree_down(fs->tree, fs->tree->root, NULL, 
				 repair_semantic_node_traverse, 
				 NULL, NULL, sem);
 error_close_lost:
	reiser4_object_close(sem->lost);
 error_close_root:
	reiser4_object_close(root);
	return res;
}

