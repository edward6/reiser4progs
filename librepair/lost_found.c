/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   repair/lost_found.c -- lost&found pass recovery code. */

#include <repair/lost_found.h>

extern errno_t callback_check_struct(object_entity_t *object, place_t *place, 
				     void *data);

static errno_t callback_object_open(reiser4_object_t *parent, 
				    reiser4_object_t **object, 
				    entry_hint_t *entry, void *data)
{
	repair_lost_found_t *lf;
	entry_hint_t lost_entry;
	reiser4_place_t *start;
	reiser4_key_t key;
	bool_t checked;
	errno_t res;
	
	aal_assert("vpf-1101", parent != NULL);
	aal_assert("vpf-1102", entry != NULL);
	aal_assert("vpf-1104", data != NULL);
	
	lf = (repair_lost_found_t *)data;
	
	aal_memset(&key, 0, sizeof(key));
	aal_memcpy(&key, &entry->object, sizeof(entry->object));
	
	if (!(*object = repair_object_launch(parent->info.tree, parent, &key))) {
		if (lf->repair->mode == REPAIR_REBUILD) {
			if ((res = reiser4_object_rem_entry(parent, entry))) {
				aal_exception_error("Lost&Found traverse failed to "
						    "remove the entry %k (%s) "
						    "pointing to %k.", &entry->offset, 
						    entry->name, &entry->object);
				return res;
			}
		} else
			lf->repair->fixable++;
		
		return 0;
	}
	
	start = reiser4_object_start(*object);
	
	/* If the object was checked already, close it at the end to avoid subtree
	   walking as subtree was checked also. */
	checked = repair_item_test_flag(start, ITEM_CHECKED);
	
	if ((res = repair_object_check(*object, parent, entry, lf->repair))) {
		res = res < 0 ? res : 0;
		goto error_close_object;
	}
	
	/* Object was reached already, link it to @parent. */
	res = repair_object_check_link(*object, parent, lf->repair->mode);
	
	if (res != -ESTRUCT) {
		/* Error occured. */
		aal_exception_error("Node %llu, item %u: failed to check the link "
				    "of the object pointed by %k to the object "
				    "pointed by %k.", start->node->number,
				    start->pos.item, &((*object)->info.object),
				    &parent->info.object);
		
		goto error_close_object;
	} else if (res < 0)
		goto error_close_object;

	/* This is a correct parent of the object, attach the object to it. */
	if (!repair_item_test_flag(start, ITEM_REACHABLE) && checked) {
		/* If it's not REACHABLE, it can be linked to 'lost+found'. If 
		   so, unlink it from 'lost+found' and link to the @parent. */
		if (!reiser4_object_seekdir(lf->lost, &key)) {
			if ((res = reiser4_object_readdir(lf->lost, &lost_entry))) {
				aal_exception_error("Node %llu, item %u: readdir"
						    "of the object pointed by %k"
						    "from 'lost+found' %k failed.",
						    start->node->number,
						    start->pos.item,
						    &((*object)->info.object),
						    lf->lost->info.object);
				goto error_close_object;
			}

			if ((res = reiser4_object_unlink(lf->lost, lost_entry.name)))
			{
				aal_exception_error("Node %llu, item %u: unlink"
						    "of the object pointed by %k"
						    "from 'lost+found' %k failed.",
						    start->node->number,
						    start->pos.item,
						    &((*object)->info.object),
						    lf->lost->info.object);
				
				goto error_close_object;
			}
		}
	}

	repair_item_set_flag(start, ITEM_REACHABLE);

	if (checked)
		reiser4_object_close(*object);
	
	return 0;
	
 error_close_object:
	reiser4_object_close(*object);
	return res;
}

static errno_t repair_lost_found_object_check(reiser4_place_t *place, 
					      void *data) 
{
	reiser4_object_t *object, *parent;
	repair_lost_found_t *lf;
	errno_t res = 0;
	
	aal_assert("vpf-1059", place != NULL);
	aal_assert("vpf-1037", data != NULL);
	
	
	lf = (repair_lost_found_t *)data;
	
	/* CHECKED items belong to objects with StatData or reached from its parent. 
	   For the former, wait for their StatDatas. For the later, they are CHECKED 
	   and REACHABLE -- nothing to do anymore. So continue only for not CHECKED 
	   items -- their StatDatas was not found on Semantic pass -- and for not 
	   REACHABLE StatDatas. */
	if (repair_item_test_flag(place, ITEM_CHECKED)) {
		if (!reiser4_item_statdata(place))
			return 0;
		
		if (repair_item_test_flag(place, ITEM_REACHABLE))
			return 0;

		/* CHECKED and not REACHABLE StatData item. */
		if (!(object = reiser4_object_realize(lf->repair->fs->tree, place))) {
			aal_exception_error("Node %llu, item %u: failed to open an "
					    "object pointed by %k.", place->node->number, 
					    place->pos.item, &place->item.key);
			return res;
		}
	} else {
		/* Some not CHECKED item. Try to realize the plugin. */
		if (!(object = repair_object_realize(lf->repair->fs->tree, place)))
			return 0;
		
		/* This is really an object, check its structure. */
		if ((res = repair_object_check_struct(object, callback_check_struct,
						      lf->repair->mode, lf)))
		{
			aal_exception_error("Node %llu, item %u: structure check "
					    "of the object pointed by %k failed. "
					    "Plugin %s.", place->node->number, 
					    place->pos.item, &place->item.key, 
					    object->entity->plugin->h.label);
			return res;
		}
	} 
	
	aal_memmove(object->name + 10, object->name, OBJECT_NAME_SIZE - 10);
	aal_memcpy(object->name, "lost_name_", 10);
	
	/* Object is openned and if it keeps its parent it put it into 
	   @object.info.parent at , try to link the object to its parent or if it 
	   fails link it to to the "lost+found". */
	if (object->info.parent.plugin) {
		/* Try to open the parent by the parent key, obtained from the object. */
		parent = reiser4_object_launch(lf->repair->fs->tree, NULL, 
					      &object->info.parent);

		/* If there is no parent found, zero parent object to link to 
		   lost+found later. */
		if (!parent)
			parent = lf->lost;
	} else
		parent = lf->lost;
	
	/* This will create '..', so after the check_struct should rm '..' */
	if ((res = reiser4_object_link(parent, object, object->name))) {
		aal_exception_error("Node %llu, item %u: failed to link the object "
				    "pointed by %k to the object pointed by %k.",
				    place->node->number, place->pos.item, 
				    &place->item.key, &parent->info.object);
		goto error_close_parent;
	}
	
	/* Do not mark objects linked to 'lost+found' as REACHABLE. */
	if (parent != lf->lost) {
		repair_item_set_flag(reiser4_object_start(object), ITEM_REACHABLE);
		reiser4_object_close(parent);
	}
	
	if ((res = repair_object_traverse(object, callback_object_open, lf)))
		goto error_close_object;
	
	/* The whole reachable subtree must be recovered for now and marked as 
	   REACHABLE. */
	
	reiser4_object_close(object);
	return 0;
	
 error_close_parent:
	if (parent != lf->lost)
		reiser4_object_close(parent);

 error_close_object:
	reiser4_object_close(object);
	return res;
}

static errno_t repair_lost_found_node_traverse(reiser4_tree_t *tree, 
					       reiser4_node_t *node, 
					       void *data) 
{
	return repair_node_traverse(node, repair_lost_found_object_check, data);
}

errno_t repair_lost_found(repair_lost_found_t *lf) {
	repair_progress_t progress;
	reiser4_object_t *root;
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
	
	lf->lost = reiser4_object_open(fs->tree, "/lost+found", FALSE);
	
	if (lf->lost == NULL) {
		/* 'lost+found' directory openning failed. That means that it has 
		   not been reached on semantic pass from '/'. Create a new one. */
		
		if ((root = reiser4_object_open(fs->tree, "/", FALSE)) == NULL) {
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
	
	/* Cut the corrupted, unrecoverable parts of the tree off. */ 	
	res = reiser4_tree_down(fs->tree, fs->tree->root, NULL, 
				repair_lost_found_node_traverse, 
				NULL, NULL, lf);
	
	if (res)
		return res;
	
	return 0;
	
 error_close_root:
	reiser4_object_close(root);
	
	return res;
}
