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
static errno_t callback_register_item(void *object, place_t *place, void *data)
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

static errno_t repair_semantic_check_struct(repair_semantic_t *sem, 
					    reiser4_object_t *object) 
{
	errno_t res = 0;
	
	aal_assert("vpf-1169", sem != NULL);
	aal_assert("vpf-1170", object != NULL);
	
	if (sem->repair->mode != RM_BUILD || 
	    !repair_item_test_flag(object_start(object), OF_CHECKED)) 
	{
		res = repair_object_check_struct(object, 
						 sem->repair->mode == RM_BUILD ?
						 callback_register_item : NULL,
						 sem->repair->mode, sem);
		if (res < 0)
			return res;
		
		repair_error_count(sem->repair, res);
	}
	
	/* Update the @object->info. */
	res |= repair_object_form(object);
	return res;
}

static errno_t repair_semantic_check_attach(repair_semantic_t *sem,
					    reiser4_object_t *parent,
					    reiser4_object_t *object) 
{
	errno_t res;
	
	aal_assert("vpf-1182", sem != NULL);
	aal_assert("vpf-1183", object != NULL);
	aal_assert("vpf-1255", parent != NULL);
	
	/* Even if this object is ATTACHED already it may allow many names
	   to itself -- check the attach with this @parent. */
	if ((res = repair_object_check_attach(parent, object,
					      sem->repair->mode)) < 0)
		return res;
	
	repair_error_count(sem->repair, res);
	
	if (res & RE_FATAL)
		return res;
	
	if (sem->repair->mode != RM_BUILD)
		return res;
	
	return repair_object_mark(object, OF_ATTACHED);
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
	
	if (name && (res = repair_semantic_add_entry(parent, object, name)))
		return res;
	
	/* Increment the link. */
	if ((res = plug_call(object->entity->plug->o.object_ops, 
			     link, object->entity)))
		return res;
	
	return repair_semantic_check_attach(sem, parent, object);
}

/* Link the @object to the "lost+found". */
static errno_t repair_semantic_link_lost(repair_semantic_t *sem, 
					 reiser4_object_t *object) 
{
	errno_t res;

	aal_assert("vpf-1262", sem != NULL);
	aal_assert("vpf-1263", object != NULL);
	
	/* Detach if possible. */
	if (object->entity->plug->o.object_ops->detach) {
		if ((res = plug_call(object->entity->plug->o.object_ops,
				     detach, object->entity, NULL)))
			return res;
	}

	/* Make the lost name. */
	repair_semantic_lost_name(object, object->name);
	
	/* Link to "lost+found". */
	if ((res = repair_semantic_link(sem, sem->lost, object, object->name)))
		return res;

	/* Get the start place correct. */
	return repair_object_clear(object, OF_ATTACHED);
}

/* If the @object keeps the info about its parent, look for it and recover the
   link between them. Continue it untill an object with unknown or unreachable
   parent is found. */
static reiser4_object_t *repair_semantic_uplink(repair_semantic_t *sem, 
						reiser4_object_t *object) 
{
	bool_t checked;
	reiser4_object_t *parent, *found;
	reiser4_place_t *pstart;
	entry_hint_t entry;
	errno_t res;
	
	aal_assert("vpf-1184", object != NULL);

	if (!object->info->parent.plug)
		goto error_link_lost;
	
	/* Must be found exact matched plugin. Ambigious plugins will 
	   be recovered later on CLEANUP pass. */
	if ((parent = repair_object_launch(object->info->tree, NULL, 
					   &object->info->parent)) == INVAL_PTR)
		return INVAL_PTR;
	
	if (parent == NULL)
		goto error_link_lost;
	
	pstart = object_start(parent);
	
	/* If ATTACHING -- parent is in the loop, break it here. */
	if (repair_item_test_flag(pstart, OF_ATTACHING))
		goto error_link_lost;
	
	checked = repair_item_test_flag(pstart, OF_CHECKED);
	
	if (checked) {
		/* Reached object was checked but the @object was not reached
		   then --> there is no entry pointing to @object. Add_entry. */
		repair_semantic_lost_name(object, object->name);
		
		if ((res = repair_semantic_link(sem, parent, object, 
						object->name)))
			goto error_parent_close;
		
		/* If linked to "lost+found", clear ATTACHED flag to be 
		   able to relink if any valid link to it will be found. */
		if (!reiser4_key_compfull(&parent->info->object,
					  &sem->lost->info->object))
		{
			if ((res = repair_object_clear(parent, OF_ATTACHED)))
				return INVAL_PTR;
		}

		/* Parent was checked and traversed already, stop here to not 
		   traverse it another time. */
		reiser4_object_close(parent);
		return NULL;
	}
	
	/* Some parent was found, check it and attach to it. */
	if ((res = repair_semantic_check_struct(sem, parent)) < 0)
		goto error_parent_close;
	
	aal_assert("vpf-1261", res == 0);
	
	/* Check that parent has a link to the object. */
	while ((res = reiser4_object_readdir(parent, &entry)) > 0) {
		if (reiser4_key_compfull(&object->info->object,
					 &entry.object))
			break;
	}
	
	if (!res) {
		/* EOF was reached. Add entry to the parent. */
		repair_semantic_lost_name(object, object->name);
		
		if ((res = repair_semantic_add_entry(parent, object, 
						     object->name)))
			goto error_parent_close;
	} else if (res < 0)
		goto error_parent_close;
	
	/* Get the start place correct. */
	if ((res = repair_object_mark(object, OF_ATTACHING)))
		goto error_parent_close;
	
	/* Get the @parent's parent. */
	found = repair_semantic_uplink(sem, parent);
	
	/* Get the start place correct. */
	if ((res = repair_object_clear(object, OF_ATTACHING))) 
		goto error_parent_close;
	
	if (!found)
		return parent;
	
	reiser4_object_close(parent);
	return found;
	
 error_link_lost:
	if ((res = repair_semantic_link_lost(sem, object)))
		return INVAL_PTR;

	return NULL;
	
 error_parent_close:
	reiser4_object_close(parent);
	
	return res < 0 ? INVAL_PTR : NULL;
}

static errno_t repair_semantic_uptraverse(repair_semantic_t *sem,
					  reiser4_object_t *object);

static errno_t repair_semantic_unlink(repair_semantic_t *sem, 
				      reiser4_object_t *parent,
				      reiser4_object_t *object)
{
	entry_hint_t entry;
	
	aal_assert("vpf-1336", sem != NULL);
	aal_assert("vpf-1337", object != NULL);

	if (!parent) {
		/* If there is no parent, just detach the object. */
		if (!object->entity->plug->o.object_ops->detach) 
			return 0;

		return plug_call(object->entity->plug->o.object_ops,  detach,
				 object->entity, NULL);
	}
	
	/* unlink from the parent. */
	aal_strncpy(entry.name, object->name, sizeof(entry.name));
	return reiser4_object_unlink(parent, &entry);
}

static reiser4_object_t *callback_object_traverse(reiser4_object_t *parent, 
						  entry_hint_t *entry,
						  void *data)
{
	repair_semantic_t *sem = (repair_semantic_t *)data;
	reiser4_object_t *object;
	bool_t checked, attached;
	reiser4_place_t *start;
	errno_t res = 0;
	
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
		aal_exception_error("Failed to open the object [%s].", 
				    reiser4_print_key(&entry->offset, PO_INO));
		
		if (sem->repair->mode != RM_CHECK)
			goto error_rem_entry;
		
		sem->repair->fixable++;
		return NULL;
	}
	
	start = object_start(object);
	checked = repair_item_test_flag(start, OF_CHECKED);
	attached = repair_item_test_flag(start, OF_ATTACHED);
	
	res = repair_semantic_check_struct(sem, object);

	if (repair_error_fatal(res))
		goto error_close_object;

	/* If @object is not attached yet, [ a) was just checked; b) is linked
	   to "lost+found" ]. If not ATTACHED @object knows about its parent, 
	   this parent matches @parent, otherwise do uptraverse() */
	while (sem->repair->mode == RM_BUILD && !attached) {
		/* If @object knows nothing about its parent, just attach 
		   it to the @parent. */
		if (!object->info->parent.plug)
			break;
		
		/* If parent of the @object matches @parent, just 
		   check_attach. */
		if (!reiser4_key_compfull(&object->info->parent, 
					  &parent->info->object))
			break;
		
		if (!checked) {
			/* If @object was just checked, probably its real 
			   parent can be found, figure it out. */
			if ((res = repair_semantic_uptraverse(sem, object)))
				goto error_close_object;
			
			if ((res = repair_object_test(object, OF_ATTACHED)) < 0)
				goto error_close_object;
			
			attached = res;
		}
		
		if (!attached) {
			/* If @object still is not attached [ a) was already 
			   checked; b) not attached after uptraverse ] -- it is 
			   linked to "lost+found". Unlink it from there. */
			
			repair_semantic_lost_name(object, object->name);
			
			if ((res = repair_semantic_unlink(sem, !checked ? NULL :
							  sem->lost, object)))
				goto error_close_object;
		}

		break;
	}
	
	/* Check the attach with @parent. */
	if ((res = repair_semantic_check_attach(sem, parent, object)) < 0)
		goto error_close_object;
	
	/* If @object cannot be attached to @parent, remove this entry. */
	if (res & RE_FATAL && sem->repair->mode == RM_BUILD) {
		sem->repair->fatal--;
		goto error_rem_entry;
	}
	
	/* If object has been attached already -- it was traversed already. 
	   close the object here to avoid another traversing. */
	if (sem->repair->mode == RM_BUILD && attached) {
		reiser4_object_close(object);
		return NULL;
	}
	
	return object;
	
 error_rem_entry:
	res = reiser4_object_rem_entry(parent, entry);
	sem->stat.rm_entries++;
	
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
	errno_t res = 0;
	
	aal_assert("vpf-1191", sem != NULL);
	aal_assert("vpf-1192", object != NULL);

	if ((upper = repair_semantic_uplink(sem, object)) == INVAL_PTR) {
		reiser4_object_close(object);
		return -EINVAL;
	}
	
	if (upper) {
		res = reiser4_object_traverse(upper, callback_object_traverse,
					      sem);

		reiser4_object_close(upper);
	}

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
	if ((object = repair_object_recognize(sem->repair->fs->tree, 
					      NULL, place)) == INVAL_PTR)
		return -EINVAL;
	
	if (object == NULL)
		return 0;
	
	res = repair_semantic_check_struct(sem, object);

	if (repair_error_fatal(res))
		goto error_close_object;

	/* Try to attach it somewhere -- at least to lost+found. */
	if ((upper = repair_semantic_uplink(sem, object)) == INVAL_PTR) {
		reiser4_object_close(object);
		return -EINVAL;
	}
	
	/* Traverse the found parent if any or the openned object. */
	res = reiser4_object_traverse(upper ? upper : object, 
				      callback_object_traverse, sem);
	
	reiser4_object_close(object);
	
	if (upper)
		reiser4_object_close(upper);
	
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

/* Trying to recognize a directory by the given @key. 
   If fails a fake one is created for BUILD mode. */
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
	
	if ((pid = reiser4_param_value("directory")) == INVAL_PID) {
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

static errno_t repair_semantic_dir_prepare(repair_semantic_t *sem, 
					   reiser4_object_t *parent,
					   reiser4_object_t *object) 
{
	errno_t res;
	
	aal_assert("vpf-1266", sem != NULL);
	aal_assert("vpf-1268", object != NULL);
	
	/* Check the object. */
	res = repair_semantic_check_struct(sem, object);
	
	if (repair_error_fatal(res))
		return res;
	
	if (!sem->repair->mode != RM_BUILD) {
		if (!parent) return 0;

		return repair_semantic_check_attach(sem, parent, object);
	}
	
	/* Detach if possible. */
	if (object->entity->plug->o.object_ops->detach) {
		if ((res = plug_call(object->entity->plug->o.object_ops,
				     detach, object->entity, NULL)))
			return res;
	}
	
	if (!parent) return 0;
	
	return repair_semantic_link(sem, parent, object, NULL);
}

static errno_t repair_semantic_root_prepare(repair_semantic_t *sem) {
	reiser4_fs_t *fs;
	errno_t res;
	
	aal_assert("vpf-1264", sem != NULL);
	
	fs = sem->repair->fs;
	
	/* Force the root dir to be recognized. */
	sem->root = repair_semantic_dir_open(sem, NULL, &fs->tree->key);
	
	if (sem->root == NULL) {
		sem->repair->fatal++;
		aal_exception_error("No root directory openned.");
		
		return 0;
	} else if (sem->root == INVAL_PTR)
		return -EINVAL;
	
	if ((res = repair_semantic_dir_prepare(sem, sem->root, sem->root))) {
		reiser4_object_close(sem->root);
		sem->root = NULL;
		return res;
	}
	
	return 0;
}

static errno_t repair_semantic_lost_prepare(repair_semantic_t *sem) {
	entry_hint_t entry;
	errno_t res;
	
	aal_assert("vpf-1193", sem != NULL);
	aal_assert("vpf-1194", sem->root != NULL);
	
	/* Look for the entry "lost+found" in the "/". */
	if ((res = reiser4_object_lookup(sem->root, "lost+found", &entry)) < 0)
		return res;

	if (res == ABSENT) return 0;

	/* The entry was found, take the key of "lost+found" and try to open the
	   object. */
	sem->lost = repair_semantic_dir_open(sem, sem->root, &entry.object);
	
	if (sem->lost == INVAL_PTR) {
		sem->lost = NULL;
		return -EINVAL;
	}
	
	if (sem->lost == NULL) {
		/* "lost+found" has not been openned, remove the entry 
		   from "/". */
		if ((res = reiser4_object_rem_entry(sem->root, &entry)))
			return res;
		
		return 0;
	}
	
	if ((res = repair_semantic_dir_prepare(sem, NULL, sem->lost))) {
		reiser4_object_close(sem->lost);
		sem->lost = NULL;
		return res;
	}

	return 0;
}

static errno_t repair_semantic_lost_open(repair_semantic_t *sem) {
	uint8_t len = aal_strlen(LOST_PREFIX);
	entry_hint_t entry;
	reiser4_fs_t *fs;
	errno_t res;

	aal_assert("vpf-1252", sem != NULL);
	aal_assert("vpf-1265", sem->root != NULL);
	
	fs = sem->repair->fs;
	
	if ((res = repair_semantic_lost_prepare(sem)))
		return res;

	/* There is no "lost+found" entry in the "/". Create a new one. */
	if (sem->lost == NULL) {
		/* Create 'lost+found' directory. */
		if (!(sem->lost = reiser4_dir_create(fs, sem->root,
						     "lost+found")))
		{
			aal_exception_error("Semantic pass failed: cannot "
					    "create 'lost+found' directory.");
			return -EINVAL;
		}
		
		return 0;
	}
	
	/* Remove all "lost_found_" names from "lost+found" directory. 
	   This is needed to not have any special case later -- when 
	   some object gets linked to "lost+found" it is not marked as 
	   ATTCHED to relink it later to some another object having 
	   the valid name if such is found. */
	while (!reiser4_object_readdir(sem->lost, &entry)) {
		if (!aal_memcmp(entry.name, LOST_PREFIX, len)) {
			if (( res = reiser4_object_rem_entry(sem->lost, 
							     &entry)))
				goto error_close_lost;
		}
	}
	
	return 0;
	
 error_close_lost:
	reiser4_object_close(sem->lost);
	sem->lost = NULL;
	return res;
}

static void repair_semantic_setup(repair_semantic_t *sem) {
	aal_memset(sem->progress, 0, sizeof(*sem->progress));

	if (!sem->progress_handler)
		return;

	sem->progress->type = GAUGE_SEM;
	sem->progress->text = "***** Semantic Traverse Pass: reiser4 semantic "
		"tree recovering.";
	sem->progress->state = PROGRESS_STAT;
	time(&sem->stat.time);
	sem->progress_handler(sem->progress);
	sem->progress->text = NULL;
}

static void repair_semantic_update(repair_semantic_t *sem) {
	repair_semantic_stat_t *stat;
	aal_stream_t stream;
	char *time_str;

	if (!sem->progress_handler)
		return;
	
	stat = &sem->stat;
	aal_stream_init(&stream, NULL, &memory_stream);
	
	if (stat->dirs || stat->files || stat->syms || stat->spcls) {
		aal_stream_format(&stream, "\tObject found:\n");
		aal_stream_format(&stream, "\tDirectories %llu, Files %llu, "
				  "Symlinks %llu, Special %llu\n", stat->dirs, 
				  stat->files, stat->syms, stat->spcls);
	}

	if (stat->ldirs || stat->lfiles || stat->lsyms || stat->lspcls) {
		aal_stream_format(&stream, "\tLost&found of them:\n");
		aal_stream_format(&stream, "\tDirectories %llu, Files %llu, "
				  "Symlinks %llu, Special %llu\n", stat->ldirs, 
				  stat->lfiles, stat->lsyms, stat->lspcls);
	}

	if (stat->shared)
		aal_stream_format(&stream, "\tObjects relocated to another "
				  "object id %llu\n", stat->shared);

	if (stat->rm_entries)
		aal_stream_format(&stream, "\tRemoved names pointing to "
				  "nowhere %llu\n", stat->rm_entries);

	if (stat->broken)
		aal_stream_format(&stream, "\tUnrecoverable objects found "
				  "%llu\n", stat->broken);
	
	time_str = ctime(&sem->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	aal_stream_format(&stream, "\tTime interval: %s - ", time_str);
	time(&sem->stat.time);
	time_str = ctime(&sem->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	aal_stream_format(&stream, time_str);

	sem->progress->state = PROGRESS_STAT;
	sem->progress->text = (char *)stream.entity;
	sem->progress_handler(sem->progress);

	aal_stream_fini(&stream);
}

errno_t repair_semantic(repair_semantic_t *sem) {
	repair_progress_t progress;
	reiser4_tree_t *tree;
	errno_t res = 0;
	
	aal_assert("vpf-1025", sem != NULL);
	aal_assert("vpf-1026", sem->repair != NULL);
	aal_assert("vpf-1027", sem->repair->fs != NULL);
	aal_assert("vpf-1028", sem->repair->fs->tree != NULL);
	
	sem->progress = &progress;
	repair_semantic_setup(sem);
	
	tree = sem->repair->fs->tree;
	
	if (reiser4_tree_fresh(tree)) {
		aal_exception_warn("No reiser4 metadata were found. Semantic "
				   "pass is skipped.");
		goto error;
	}
	
	if ((res = reiser4_tree_load_root(tree)))
		return res;
	
	if (tree->root == NULL) {
		res = -EINVAL;
		goto error;
	}
	
	/* Open "/" directory. */
	if ((res = repair_semantic_root_prepare(sem)))
		goto error;
	
	/* Open "lost+found" directory in BUILD mode. */
	if (sem->repair->mode == RM_BUILD) {
		if ((res = repair_semantic_lost_open(sem)))
			goto error_close_root;
	}

	/* Traverse "/" and recover all reachable subtree. */
	if ((res = reiser4_object_traverse(sem->root, callback_object_traverse,
					   sem)))
		goto error_close_lost;

	reiser4_object_close(sem->root);
	sem->root = NULL;
	
	/* Connect lost objects to their parents -- if parents can be 
	   identified -- or to "lost+found". */
	if (sem->repair->mode == RM_BUILD) {
		if ((res = reiser4_tree_trav_node(tree, tree->root, NULL, 
						  repair_semantic_node_traverse,
						  NULL, NULL, sem)))
		{
			goto error_close_lost;
		}
	}
	
 error_close_lost:
	if (sem->lost) {
		reiser4_object_close(sem->lost);
		sem->lost = NULL;
	}
 error_close_root:
	if (sem->root) {
		reiser4_object_close(sem->root);
		sem->root = NULL;
	}
 error:
	repair_semantic_update(sem);	
	
	if (sem->repair->mode != RM_CHECK)
		reiser4_fs_sync(sem->repair->fs);
	
	return res < 0 ? res : 0;
}

