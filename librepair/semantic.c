/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   repair/semantic.c -- semantic pass recovery code. */

#include <repair/semantic.h>

static void repair_semantic_lost_name(reiser4_object_t *object, char *name) {
	char *key;
	uint8_t len;

	len = aal_strlen(LOST_PREFIX);
	key = reiser4_print_key(&object->ent->object, PO_INODE);
	
	aal_memcpy(name, LOST_PREFIX, len);
	aal_memcpy(name + len, key, aal_strlen(key));
}

/* Callback for repair_object_check_struct. Mark the passed item as CHECKED. */
static errno_t callback_register_item(reiser4_place_t *place, void *data) {
        aal_assert("vpf-1115", place != NULL);
         
        if (reiser4_item_test_flag(place, OF_CHECKED)) {
                aal_error("Node (%llu), item (%u): item registering "
			  "failed, it belongs to another object already.",
			  place_blknr(place), place->pos.item);
                return -EINVAL;
        }
         
        reiser4_item_set_flag(place, OF_CHECKED);
         
        return 0;
}

static void repair_semantic_register_oid(repair_semantic_t *sem, oid_t oid) {
	if (sem->stat.oid < oid)
		sem->stat.oid = oid;
}

static errno_t repair_semantic_check_struct(repair_semantic_t *sem, 
					    reiser4_object_t *object) 
{
	errno_t res = 0;
	oid_t oid;
	
	aal_assert("vpf-1169", sem != NULL);
	aal_assert("vpf-1170", object != NULL);
	
	oid = reiser4_key_get_objectid(&object->ent->object);
	repair_semantic_register_oid(sem, oid);
	
	/* Check struct if not the BUILD mode, if the fake object or 
	   if has not been checked yet. */
	if (sem->repair->mode != RM_BUILD || !object_start(object)->node ||
	    !reiser4_item_test_flag(object_start(object), OF_CHECKED)) 
	{
		place_func_t place_func = sem->repair->mode == RM_BUILD ?
			callback_register_item : NULL;
		
		res = repair_object_check_struct(object, place_func,
						 sem->repair->mode, sem);
		if (res < 0) return res;
		
		repair_error_count(sem->repair, res);
	}
	
	/* Update the @object->info.parent. */
	res |= repair_object_refresh(object);
	return res;
}

static errno_t repair_semantic_check_attach(repair_semantic_t *sem,
					    reiser4_object_t *parent,
					    reiser4_object_t *object) 
{
	place_func_t place_func;
	errno_t res;
	
	aal_assert("vpf-1182", sem != NULL);
	aal_assert("vpf-1183", object != NULL);
	aal_assert("vpf-1255", parent != NULL);
	
	place_func = sem->repair->mode == RM_BUILD ? 
		callback_register_item : NULL;
	
	/* Even if this object is ATTACHED already it may allow many names
	   to itself -- check the attach with this @parent. */
	res = repair_object_check_attach(parent, object, place_func, 
					 sem, sem->repair->mode);
	
	if (res < 0) return res;
	
	repair_error_count(sem->repair, res);
	
	if (res & RE_FATAL)
		return res;
	
	if (sem->repair->mode != RM_BUILD)
		return res;
	
	/* Increment the link. */
	if ((res = plug_call(object->ent->opset.plug[OPSET_OBJ]->o.object_ops,
			     link, object->ent)))
		return res;

	/* If @parent is "lost+found", do not mark as ATTACHED. */
	if (sem->lost && !reiser4_key_compfull(&parent->ent->object,
					       &sem->lost->ent->object))
		return 0;

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
	reiser4_key_assign(&entry.object, &object->ent->object);
	entry.place_func = callback_register_item;
	entry.data = NULL;
	
	if ((res = reiser4_object_add_entry(parent, &entry)))
		aal_error("Can't add entry %s to %s.",
			  name, parent->name);
	
	return res;
}

static errno_t repair_semantic_unlink(repair_semantic_t *sem, 
				      reiser4_object_t *parent,
				      reiser4_object_t *object)
{
	entry_hint_t entry;
	
	aal_assert("vpf-1336", sem != NULL);
	aal_assert("vpf-1337", object != NULL);

	if (!parent) {
		reiser4_plug_t *plug = object->ent->opset.plug[OPSET_OBJ];
		
		/* If there is no parent, just detach the object. */
		if (!plug->o.object_ops->detach) 
			return 0;

		return plug_call(plug->o.object_ops, detach, 
				 object->ent, NULL);
	}
	
	/* unlink from the parent. */
	aal_strncpy(entry.name, object->name, sizeof(entry.name));
	return reiser4_object_unlink(parent, &entry);
}

static errno_t repair_semantic_link_lost(repair_semantic_t *sem,
					 reiser4_object_t *parent,
					 reiser4_object_t *object)
{
	errno_t res;

	aal_assert("vpf-1178", sem != NULL);
	aal_assert("vpf-1179", parent != NULL);
	aal_assert("vpf-1180", object != NULL);
	
	/* Make the lost name. */
	repair_semantic_lost_name(object, object->name);
	
	/* Detach if possible. */
	if ((res = repair_semantic_unlink(sem, NULL, object)))
		return res;
	
	/* Add the entry to the @parent. */
	if ((res = repair_semantic_add_entry(parent, object, object->name)))
		return res;
	
	/* Check the attach of the @object to the @parent. */
	if ((res = repair_semantic_check_attach(sem, parent, object)))
		return res;

	return 0;
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

	if (!object->ent->parent.plug)
		return NULL;
	
	parent = repair_object_obtain((reiser4_tree_t *)object->ent->tree,
				      NULL, &object->ent->parent);
	
	if (parent == INVAL_PTR)
		return INVAL_PTR;
	
	if (parent == NULL)
		goto error_object_detach;
	
	pstart = object_start(parent);
	
	/* If ATTACHING -- parent is in the loop, break it here. */
	if (reiser4_item_test_flag(pstart, OF_ATTACHING)) {
		reiser4_object_close(parent);
		goto error_object_detach;
	}
	
	checked = reiser4_item_test_flag(pstart, OF_CHECKED);
	
	if (checked) {
		/* If parent is "lost+found" (already checked), 
		   detach from it. */
		if (!reiser4_key_compfull(&parent->ent->object,
					  &sem->lost->ent->object))
		{
			reiser4_object_close(parent);
			goto error_object_detach;
		}
		
		/* Init all needed info for the object. */
		if ((res = repair_object_refresh(parent)))
			goto error_parent_close;
		
		if ((res = repair_semantic_link_lost(sem, parent, object)))
			goto error_parent_close;
		
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
		if (reiser4_key_compfull(&object->ent->object,
					 &entry.object))
			break;
	}
	
	if (!res) {
		/* EOF was reached. Add entry to the parent. */
		repair_semantic_lost_name(object, object->name);
		
		if ((res = repair_semantic_add_entry(parent, object, 
						     object->name)))
			goto error_parent_close;

		/* Do not check attach here, as the pointer to parent 
		   will be returned and it will be traversed, all attaches
		   will be checked there. */
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
	
	/* If nothing above the parent is found, return parent. */
	if (!found) return parent;
	
	reiser4_object_close(parent);
	
	return found;
	
 error_object_detach:
	/* Detach if possible. */
	res = repair_semantic_unlink(sem, NULL, object);
	return res < 0 ? INVAL_PTR : NULL;
	
 error_parent_close:
	reiser4_object_close(parent);
	return res < 0 ? INVAL_PTR : NULL;
}

static errno_t repair_semantic_uptraverse(repair_semantic_t *sem,
					  reiser4_object_t *parent,
					  reiser4_object_t *object);

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
	object = repair_object_obtain((reiser4_tree_t *)parent->ent->tree,
				      parent, &entry->object);
	
	if (object == INVAL_PTR)
		return INVAL_PTR;
	
	if (object == NULL) {
		aal_error("Failed to find the object [%s] pointed "
			  "by the entry [%s].%s",
			  reiser4_print_key(&entry->object, PO_INODE),
			  reiser4_print_key(&entry->offset, PO_INODE),
			  sem->repair->mode != RM_CHECK ?
			  " Entry is removed." : "");
		
		if (sem->repair->mode != RM_CHECK)
			goto error_rem_entry;
		
		sem->repair->fixable++;
		return NULL;
	}
	
	start = object_start(object);
	checked = reiser4_item_test_flag(start, OF_CHECKED);
	attached = reiser4_item_test_flag(start, OF_ATTACHED);
	
	res = repair_semantic_check_struct(sem, object);

	if (repair_error_fatal(res))
		goto error_close_object;

	/* If @object is not attached yet, [ a) was just checked; b) is linked
	   to "lost+found" ]. If not ATTACHED @object knows about its parent, 
	   this parent matches @parent, otherwise do uptraverse(). */
	while (sem->repair->mode == RM_BUILD && !attached) {
		/* If @object knows nothing about its parent, just attach 
		   it to the @parent. */
		if (!object->ent->parent.plug)
			break;
		
		/* If parent of the @object matches @parent, just 
		   check_attach. */
		if (!reiser4_key_compfull(&object->ent->parent, 
					  &parent->ent->object))
			break;
		
		if (!checked) {
			/* If @object was just checked, probably its real 
			   parent can be found, figure it out. */
			res = repair_semantic_uptraverse(sem, NULL, object);
			
			if (res) goto error_close_object;
		} else {
			/* If @object is checked and not attached, detach from
			   the parent, if parent is "lost+found" -- unlink. */
			int lost;

			lost = !reiser4_key_compfull(&object->ent->parent,
						     &sem->lost->ent->object);
			
			repair_semantic_lost_name(object, object->name);
			
			if ((res = repair_semantic_unlink(sem, lost ? sem->lost
							  : 0, object)))
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
	
	/* If object has been traversed already, close the object here 
	   to avoid another traversing. */
	if (sem->repair->mode == RM_BUILD) {
		if (reiser4_item_test_flag(start, OF_TRAVERSED)) {
			reiser4_object_close(object);
			return NULL;
		} else {
			reiser4_item_set_flag(start, OF_TRAVERSED);
		}
	}
	
	return object;
	
 error_rem_entry:
	res = reiser4_object_rem_entry(parent, entry);
	sem->stat.rm_entries++;
	
	if (res < 0) {
		aal_error("Semantic traverse failed to remove the "
			  "entry \"%s\" [%s] pointing to [%s].", 
			  entry->name, 
			  reiser4_print_key(&entry->offset, PO_INODE),
			  reiser4_print_key(&entry->object, PO_INODE));
	}
	
 error_close_object:
	if (object)
		reiser4_object_close(object);
	
	return res < 0 ? INVAL_PTR : NULL;
}

/* Uplink + traverse. Looking for the most upper parent of the @object; 
   if found, link that parent to the "lost+found", if no parent of the 
   @object is found, link @object to the @parent. If @parent is found,
   traverse from there, if not, do not traverse from the @object. */
static errno_t repair_semantic_uptraverse(repair_semantic_t *sem,
					  reiser4_object_t *parent,
					  reiser4_object_t *object)
{
	reiser4_object_t *o = NULL;
	reiser4_object_t *p = NULL;
	reiser4_object_t *upper;
	errno_t res;
	
	aal_assert("vpf-1191", sem != NULL);
	aal_assert("vpf-1192", object != NULL);

	if ((upper = repair_semantic_uplink(sem, object)) == INVAL_PTR)
		return -EINVAL;
	
	if (upper) {
		o = upper;
		p = sem->lost;
	} else if (parent) {
		o = object;
		p = parent;
	} 
	
	if (!o) return 0;
	
	/* Link @object to the @parent if not attached yet. */
	if ((res = repair_object_test(o, OF_ATTACHED)) < 0)
		goto error_close_upper;

	if (!res && (res = repair_semantic_link_lost(sem, p, o)))
		goto error_close_upper;
	
	/* Traverse from the upper found object. */
	res = reiser4_object_traverse(o, callback_object_traverse, sem);

 error_close_upper:
	
	if (upper) reiser4_object_close(upper);
	
	return res;
}

static errno_t callback_tree_scan(reiser4_place_t *place, void *data) {
	repair_semantic_t *sem = (repair_semantic_t *)data;
	reiser4_object_t *object;
	errno_t res;
	
	aal_assert("vpf-1171", place != NULL);
	aal_assert("vpf-1037", sem != NULL);
	
	/* Objects w/out SD get recovered only when reached from the parent. */
	if (!reiser4_item_statdata(place))
		return 0;
		
	/* If this item was checked already, skip it. */
	if (reiser4_item_test_flag(place, OF_CHECKED))
		return 0;
	
	/* Try to open the object by its SD. */
	object = repair_object_fetch(sem->repair->fs->tree, NULL, place);
	
	if (object == INVAL_PTR)
		return -EINVAL;
	else if (object == NULL)
		return 0;
	
	/* Some object was openned. Check its structure and traverse from it. */
	res = repair_semantic_check_struct(sem, object);

	if (repair_error_fatal(res))
		goto error_close_object;

	/* Try to attach it somewhere -- at least to lost+found. */
	if ((res = repair_semantic_uptraverse(sem, sem->lost, object)))
		goto error_close_object;
	
	reiser4_object_close(object);
	
	return res < 0 ? res : 1;
	
 error_close_object:
	reiser4_object_close(object);	

	/* Return the error or that another lookup is needed. */
	return res < 0 ? res : 1;
}

/* Trying to recognize a directory by the given @key. 
   If fails a fake one is created for BUILD mode. */
static reiser4_object_t *repair_semantic_dir_open(repair_semantic_t *sem,
						  reiser4_object_t *parent,
						  reiser4_key_t *key)
{
	reiser4_object_t *object;
	reiser4_tree_t *tree;
	reiser4_plug_t *plug;
	
	aal_assert("vpf-1250", sem != NULL);
	aal_assert("vpf-1251", key != NULL);
	
	tree = sem->repair->fs->tree;
	
	if ((object = repair_object_obtain(tree, parent, key)) == INVAL_PTR)
		return INVAL_PTR;
	
	if (object) {
		/* Check that the object was recognized by the dir plugin. */
		if (object->ent->opset.plug[OPSET_OBJ]->id.group == DIR_OBJECT)
			return object;

		aal_error("The directory [%s] is recognized by the "
			  "%s plugin which is not a directory one.", 
			  reiser4_print_key(key, PO_INODE), 
			  object->ent->opset.plug[OPSET_OBJ]->label);
		
		reiser4_object_close(object);
	} else {
		/* No plugin was recognized. */
		aal_error("Failed to recognize the plugin for the directory "
			  "[%s].", reiser4_print_key(key, PO_INODE));
	}
	
	if (sem->repair->mode != RM_BUILD)
		return NULL;

	plug = reiser4_profile_plug(PROF_DIR);
	aal_error("Trying to recover the directory [%s] with the default "
		  "plugin--%s.", reiser4_print_key(key, PO_INODE), plug->label);

	return repair_object_fake(tree, parent, key, plug);
}

static errno_t repair_semantic_object_check(repair_semantic_t *sem, 
					    reiser4_object_t *parent,
					    reiser4_object_t *object) 
{
	errno_t res;
	
	aal_assert("vpf-1266", sem != NULL);
	aal_assert("vpf-1268", object != NULL);
	aal_assert("vpf-1419", parent != NULL);
	
	/* Check the object. */
	res = repair_semantic_check_struct(sem, object);
	
	if (repair_error_fatal(res))
		return res;
	
	while (sem->repair->mode == RM_BUILD) {
		/* Check the attach before. */
		res = repair_object_check_attach(parent, object, 
						 callback_register_item,
						 sem, RM_FIX);
		
		if (res < 0)  
			return res;
		else if (res == 0) 
			break;
		
		aal_info("Object [%s]: detaching.", 
			 reiser4_print_key(&object->ent->object, PO_INODE));
		
		if ((res = repair_semantic_unlink(sem, NULL, object)))
			return res;
		
		aal_info("Object [%s]: attaching to [%s].", 
			 reiser4_print_key(&object->ent->object, PO_INODE),
			 reiser4_print_key(&object->ent->parent, PO_INODE));

		break;
	}
	
	return repair_semantic_check_attach(sem, parent, object);
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
		aal_error("No root directory openned.");
		return RE_FATAL;
	} else if (sem->root == INVAL_PTR) {
		sem->root = NULL;
		return -EINVAL;
	}
	
	reiser4_opset_root(&sem->root->ent->opset);
	
	if ((res = repair_semantic_object_check(sem, sem->root, sem->root))) {
		reiser4_object_close(sem->root);
		sem->root = NULL;
		return res;
	}
	
	return 0;
}

static errno_t repair_semantic_lost_prepare(repair_semantic_t *sem) {
	reiser4_key_t lost;
	entry_hint_t entry;
	reiser4_fs_t *fs;
	errno_t lookup;
	errno_t res;
	uint8_t len;

	aal_assert("vpf-1252", sem != NULL);
	aal_assert("vpf-1265", sem->root != NULL);
	
	fs = sem->repair->fs;

	/* Look for the entry "lost+found" in the "/". */
	if ((lookup = reiser4_object_lookup(sem->root, "lost+found", &entry)) < 0)
		return lookup;

	/* Prepare the lost+found key. */
	if (lookup == ABSENT) {
		if ((res = repair_fs_lost_key(fs, &lost)))
			return res;
		
		aal_error("No 'lost+found' entry found. "
			  "Building a new object with the key %s.",
			  reiser4_print_key(&lost, PO_INODE));
	} else {
		reiser4_key_assign(&lost, &entry.object);
	}
	
	/* Try to open the "lost+found" object by its key. */
	sem->lost = repair_semantic_dir_open(sem, sem->root, &lost);
	
	if (sem->lost == INVAL_PTR) {
		sem->lost = NULL;
		return -EINVAL;
	} else if (sem->lost == NULL) {
		sem->repair->fatal++;
		aal_error("No 'lost+found' directory openned.");
		return RE_FATAL;
	}

	if ((res = repair_semantic_object_check(sem, sem->root, sem->lost)))
		goto error_close_lost;

	if (lookup == ABSENT) {
		/* Add the entry to the @parent. */
		if ((res = repair_semantic_add_entry(sem->root, sem->lost, 
						     "lost+found")))
			return res;
	}

	len = aal_strlen(LOST_PREFIX);
	
	/* Remove all "lost_found_" names from "lost+found" directory. 
	   This is needed to not have any special case later -- when 
	   some object gets linked to "lost+found" it is not marked as 
	   ATTCHED to relink it later to some another object having 
	   the valid name if such is found. */
	while (reiser4_object_readdir(sem->lost, &entry) > 0) {
		if (aal_memcmp(entry.name, LOST_PREFIX, len)) 
			continue;
		
		if (( res = reiser4_object_rem_entry(sem->lost, &entry)))
			goto error_close_lost;
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
	sem->progress->text = "***** Semantic Traverse Pass: reiser4 "
		"semantic tree checking.";
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
	
	
	tree = sem->repair->fs->tree;
	
	if (reiser4_tree_fresh(tree)) {
		aal_warn("No reiser4 metadata were found. Semantic "
			 "pass is skipped.");
		sem->repair->fatal++;
		goto error;
	}
	
	sem->progress = &progress;
	repair_semantic_setup(sem);
	
	if ((res = reiser4_tree_load_root(tree)))
		return res;
	
	if (tree->root == NULL) {
		res = -EINVAL;
		goto error_update;
	}
	
	/* Open "/" directory. */
	if ((res = repair_semantic_root_prepare(sem)))
		goto error_update;
	
	if ((res = reiser4_opset_init(tree, 0)))
		goto error_close_root;
	
	/* Open "lost+found" directory in BUILD mode. */
	if (sem->repair->mode == RM_BUILD) {
		if ((res = repair_semantic_lost_prepare(sem)))
			goto error_close_root;
	}

	/* Traverse "/" and recover all reachable subtree. */
	res = reiser4_object_traverse(sem->root, callback_object_traverse, sem);
	if (res) goto error_close_lost;

	reiser4_object_close(sem->root);
	sem->root = NULL;
	
	/* Connect lost objects to their parents -- if parents can be 
	   identified -- or to "lost+found". */
	if (sem->repair->mode == RM_BUILD) {
		if ((res = repair_tree_scan(tree, callback_tree_scan, sem)))
			goto error_close_lost;
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
	
 error_update:
	repair_semantic_update(sem);	
	
 error:
	if (sem->repair->mode != RM_CHECK)
		reiser4_fs_sync(sem->repair->fs);
	
	return res < 0 ? res : 0;
}

