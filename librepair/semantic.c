/* repair/semantic.c -- semantic pass recovery code.
   Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING. */

#include <repair/semantic.h>

static errno_t callback_object_open(reiser4_object_t *parent, 
				    reiser4_object_t **object, 
				    entry_hint_t *entry, void *data)
{
	reiser4_plugin_t *plugin;
	repair_semantic_t *sem;
	errno_t res;
	
	aal_assert("vpf-1101", parent != NULL);
	aal_assert("vpf-1102", entry != NULL);
	aal_assert("vpf-1104", data != NULL);
	
	sem = (repair_semantic_t *)data;
	
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
	
	res = repair_object_check_struct(*object, plugin, sem->repair->mode);
	
	if (res > 0) {
		errno_t result;
		
		if ((result = reiser4_object_rem_entry(parent, entry))) {
			aal_exception_error("Semantic traverse failed to remove the "
					    "entry %k (%s) pointing to %k.", 
					    &entry->offset, entry->name,
					    &entry->object);
			return result;
		}
		
		goto error_close_object;
	} else if (res < 0) {
		aal_exception_error("Check of the object pointed by %k from the "
				    "%k (%s) failed.", &entry->object, &entry->offset, 
				    entry->name);
		
		goto error_close_object;
	}
	
	/* Check the uplink - '..' in directories. */
	res = repair_object_check_link(*object, parent, sem->repair->mode);
	
	if (res) {
		aal_exception_error("Node %llu, item %u: failed to check the link of the"
				    " object pointed by %k to the object pointed by %k.",
				    ((reiser4_node_t *)(*object)->info.start.node)->blk,
				    (*object)->info.start.pos.item, 
				    &((*object)->info.object),
				    &parent->info.object);
		
		goto error_close_object;
	}
	
	return 0;
	
 error_close_object:
	reiser4_object_close(*object);
	return res;
}

static errno_t repair_semantic_object_check(reiser4_place_t *place, void *data) {
	reiser4_plugin_t *plugin;
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
	
	repair_object_init(&object, sem->repair->fs->tree, place, NULL, NULL);
	
	/* Try to realize the plugin. */
	if ((plugin = repair_object_realize(&object)) == NULL)
		return 0;
	
	/* This is really an object, check its structure. */
	res = repair_object_check_struct(&object, plugin, sem->repair->mode);
	
	if (res){
		aal_exception_error("Node %llu, item %u: structure check of the "
				    "object pointed by %k failed. Plugin %s.", 
				    place->node->blk, place->pos.item, 
				    &place->item.key, plugin->h.label);
		return res;
	}
	
	if ((res = repair_object_traverse(&object, callback_object_open, sem)))
		goto error_close_object;
	
	/* The whole reachable subtree must be recovered for now and marked as 
	   REACHABLE. */
	
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
	reiser4_plugin_t *plugin;
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
	
	repair_object_init(&object, fs->tree, NULL, &fs->tree->key, &fs->tree->key);
	
	/* Make sure that '/' exists. */
	if ((plugin = repair_object_realize(&object)) == NULL) {
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

