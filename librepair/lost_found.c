/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   repair/lost_found.c -- lost&found pass recovery code. */

#include <repair/lost_found.h>

/* Use the semantic pass callback. */
extern errno_t callback_check_struct(object_entity_t *object, place_t *place, void *data);

static errno_t callback_object_open(reiser4_object_t *parent, 
				    reiser4_object_t **object, 
				    entry_hint_t *entry, void *data)
{
	reiser4_plugin_t *plugin;
	repair_lost_found_t *lf;
	errno_t res;
	
	aal_assert("vpf-1101", parent != NULL);
	aal_assert("vpf-1102", entry != NULL);
	aal_assert("vpf-1104", data != NULL);
	
	lf = (repair_lost_found_t *)data;
	
	if (!(*object = aal_calloc(sizeof(**object), 0)))
		return -EINVAL;
	
	aal_memcpy(&(*object)->info.object, &entry->object, sizeof(entry->object));
	(*object)->info.tree = parent->info.tree;
	(*object)->info.parent = parent->info.object;
	
	/* Cannot detect the object plugin, rm the entry. */
	if ((plugin = repair_object_realize(*object)) == NULL) {
		reiser4_object_close(*object);
		return -EINVAL;
	}
	
	res = repair_object_check_struct(*object, plugin, callback_check_struct, 
					 lf->repair->mode, lf);
	
	if (res > 0) {
		errno_t result;
		
		if ((result = reiser4_object_rem_entry(parent, entry))) {
			aal_exception_error("Semantic traverse failed to remove the "
					    "entry %k (%s) pointing to %k.", 
					    &entry->offset, entry->name,
					    &entry->object);
			return result;
		}
	} else if (res < 0) {
		aal_exception_error("Check of the object pointed by %k from the "
				    "%k (%s) failed.", &entry->object, &entry->offset,
				    entry->name);
		
		goto error_close_object;
	}
	
	return 0;
	
 error_close_object:
	reiser4_object_close(*object);
	return res;
}

static errno_t repair_lost_found_object_check(reiser4_place_t *place, 
					      void *data) 
{
	reiser4_object_t parent, object;
	repair_lost_found_t *lf;
	errno_t res = 0;
	bool_t checked;
	
	aal_assert("vpf-1059", place != NULL);
	aal_assert("vpf-1037", data != NULL);
	
	checked = repair_item_test_flag(place, ITEM_CHECKED);
	
	/* CHECKED items belong to objects with StatData or reached from its parent. 
	   For the former, wait for their StatDatas. For the later, they are CHECKED 
	   and REACHABLE -- nothing to do anymore. So continue only for not CHECKED 
	   items -- their StatDatas was not found on Semantic pass -- and for not 
	   REACHABLE StatDatas. */
	if (checked) {
		if (!reiser4_item_statdata(place))
			return 0;
		
		if (repair_item_test_flag(place, ITEM_REACHABLE))
			return 0;
	}
	
	lf = (repair_lost_found_t *)data;    
	
	repair_object_init(&object, lf->repair->fs->tree, place, NULL, NULL);
	
	if (!checked) {
		reiser4_plugin_t *plugin;
		
		/* Some not CHECKED item. Try to realize the plugin. */
		if ((plugin = repair_object_realize(&object)) == NULL)
			return 0;
		
		/* This is really an object, check its structure. */
		if ((res = repair_object_check_struct(&object, plugin, 
						      callback_check_struct,
						      lf->repair->mode, lf)))
		{
			aal_exception_error("Node %llu, item %u: structure check "
					    "of the object pointed by %k failed. "
					    "Plugin %s.", place->node->blk, 
					    place->pos.item, &place->item.key, 
					    plugin->h.label);
			return res;
		}
	} else {
		/* CHECKED and not REACHABLE StatData item. */
		if ((res = repair_object_launch(&object))) {
			aal_exception_error("Node %llu, item %u: failed to open an "
					    "object pointed by %k.", place->node->blk, 
					    place->pos.item, &place->item.key);
			return res;
		}
	}
	
	aal_memmove(object.name + 10, object.name, OBJECT_NAME_SIZE - 10);
	aal_memcpy(object.name, "lost_name_", 10);
	
	/* Object is openned and if it stores its parent somewhere it was obtained 
	   to @object.info.parent, try to link the object to its parent or if it 
	   fails link it to to the "lost+found". */
	if (object.info.parent.plugin) {
		repair_object_init(&parent, object.info.tree, NULL, 
				   &object.info.parent, &object.info.object);
		
		if (!reiser4_object_stat(&parent) && !reiser4_object_guess(&parent)) {
			/* Parent found by parent pointer. */
			res = reiser4_object_link(&parent, &object, object.name);
			
			if (res) {
				aal_exception_error("Node %llu, item %u: failed to "
						    "link the object pointed by %k "
						    "to the object pointed by %k.",
						    place->node->blk, place->pos.item, 
						    &place->item.key, 
						    &parent.info.object);
				goto error_close_parent;
			}
			
			/* Check the uplink - '..' in directories. */
			res = repair_object_check_link(&object, &parent, 
						       lf->repair->mode);
			
			if (res) {
				aal_exception_error("Node %llu, item %u: failed to "
						    "check the link of the object "
						    "pointed by %k to the object "
						    "pointed by %k.", 
						    place->node->blk, 
						    place->pos.item,
						    &place->item.key, 
						    &parent.info.object);
				goto error_close_parent;
			}
			
			repair_item_set_flag(reiser4_object_start(&object), 
					     ITEM_CHECKED);
			
			plugin_call(parent.entity->plugin->o.object_ops, close, 
				    parent.entity);
		} else {
			/* No parent found by parent pointer. */
			res = reiser4_object_link(lf->lost, &object, object.name);
			
			if (res) {
				aal_exception_error("Node %llu, item %u: failed to "
						    "link the object pointed by %k "
						    "to the object pointed by %k.",
						    place->node->blk, place->pos.item, 
						    &place->item.key, 
						    &lf->lost->info.object);
				goto error_close_object;
			}
			
			if ((res = repair_object_check_link(&object, lf->lost, 
							    lf->repair->mode))) 
			{
				aal_exception_error("Node %llu, item %u: failed to "
						    "check the link of the object "
						    "pointed by %k to the object "
						    "pointed by %k.", 
						    place->node->blk, 
						    place->pos.item,
						    &place->item.key, 
						    &lf->lost->info.object);
					goto error_close_object;
			}
		}
	}
	
	if ((res = repair_object_traverse(&object, callback_object_open, lf)))
		goto error_close_object;
	
	/* The whole reachable subtree must be recovered for now and marked as 
	   REACHABLE. */
	
	plugin_call(object.entity->plugin->o.object_ops, close, object.entity);
	
	return 0;
	
 error_close_parent:
	plugin_call(parent.entity->plugin->o.object_ops, close, parent.entity);
	
 error_close_object:
	plugin_call(object.entity->plugin->o.object_ops, close, object.entity);
	
	return res;
}

static errno_t repair_lost_found_node_traverse(reiser4_node_t *node, void *data) {
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
	
	lf->lost = reiser4_object_open(fs->tree, "/lost+found", FALSE);
	
	if (lf->lost == NULL) {
		/* 'lost+found' directory openning failed. Try to open '/' */
		
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
