/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   repair/lost_found.c -- lost&found pass recovery code. */

#include <repair/lost_found.h>
#include <repair/semantic.h>

extern errno_t callback_check_struct(object_entity_t *object, place_t *place, 
				     void *data);

static void repair_lost_found_make_lost_name(reiser4_object_t *object, 
					     char *name) 
{
	uint8_t len = aal_strlen(LOST_PREFIX);
	
	reiser4_key_string(&object->info.object, name);
	aal_memmove(object->name + len, object->name, OBJECT_NAME_SIZE - len);
	aal_memcpy(object->name, LOST_PREFIX, len);
}

static errno_t callback_object_check_link(reiser4_object_t *object,
					  reiser4_object_t *parent,
					  entry_type_t link,
					  void *data)
{
	repair_lost_found_t *lf = (repair_lost_found_t *)data;
	repair_ancestor_t *ancestor;
	reiser4_place_t *start;
	entry_hint_t entry;
	errno_t res;
	
	aal_assert("vpf-1148", object != NULL);
	aal_assert("vpf-1149", data != NULL);
	
	ancestor = (repair_ancestor_t *)aal_list_first(lf->path)->data;
	
	repair_lost_found_make_lost_name(object, object->name);
	
	/* If @parent was reached before from the @object now we get here 
	   by the backlink, which is chacked already, skip another check. */
	if (reiser4_key_compare(&ancestor->object->info.object, 
				&object->info.object) ||
	    !ET_BACKLINK(ancestor->link, link))
	{
		res = repair_object_check_backlink(object, parent, link, 
						   lf->repair->mode);
		if (repair_error_fatal(res))
			goto error;
	}
	
	start = reiser4_object_start(object);	
	
	/* If HAS_NAME, skip it as objects linked to L&F are not marked as such. */
	if (repair_item_test_flag(start, OF_HAS_NAME))
		goto error;
	
	/* If lookup cannot find the object in L&F, nothing to unlink. */
	if (reiser4_object_lookup(lf->lost, object->name, &entry) != PRESENT)
		goto error;
	
	/* Unlink from lost+found */
	res |= reiser4_object_unlink(lf->lost, object->name);
	if (repair_error_fatal(res)) {
		aal_exception_error("Node %llu, item %u: unlink from the object "
				    "%k of the object pointed by %k failed.",
				    start->node->number, start->pos.item,
				    &lf->lost->info.object, &entry.object);
	}
	
	
	reiser4_key_string(&object->info.object, object->name);
	
	return res;
	
 error:
	reiser4_key_string(&object->info.object, object->name);
	return res;
}

static reiser4_object_t *callback_traverse_open(reiser4_object_t *parent, 
						entry_hint_t *entry, 
						void *data)
{
	repair_lost_found_t *lf = (repair_lost_found_t *)data;
	repair_ancestor_t *ancestor;
	reiser4_object_t *object;
	
	aal_assert("vpf-1150", lf != NULL);
	
	object = repair_semantic_open_child(parent, entry, lf->repair,
					    callback_object_check_link, lf);
	
	if (object == INVAL_PTR || object == NULL)
		return object;
	
	if (!(ancestor = aal_calloc(sizeof(*ancestor), 0)))
		goto error_object_close;
	
	ancestor->object = parent;
	ancestor->link = entry->type;
	
	lf->path = aal_list_insert(lf->path, ancestor, 0);
	
	return object;
	
 error_object_close:
	reiser4_object_close(object);
	return INVAL_PTR;

}

static void callback_traverse_close(reiser4_object_t *object, void *data) {
	repair_lost_found_t *lf = (repair_lost_found_t *)data;
	
	aal_assert("vpf-1160", object != NULL);
	aal_assert("vpf-1161", data != NULL);
	
	lf->path = aal_list_remove(lf->path, aal_list_first(lf->path));
	reiser4_object_close(object);
}

static errno_t repair_lost_found_object_check(reiser4_place_t *place, 
					      void *data) 
{
	reiser4_object_t *object;
	repair_lost_found_t *lf;
	reiser4_place_t *start;
	errno_t res = 0;
	
	aal_assert("vpf-1059", place != NULL);
	aal_assert("vpf-1037", data != NULL);
	
	
	lf = (repair_lost_found_t *)data;
	
	/* If the object the item belongs to was reached already, skip it. */
	if (repair_item_test_flag(place, OF_HAS_NAME))
		return 0;
	
	if (repair_item_test_flag(place, OF_CHECKED)) {
		/* If the object the item belongs to was chacked already, its 
		   object plugin must be realized unambiguously. */
		if (!(object = repair_object_realize(lf->repair->fs->tree,
						     place, TRUE)))
		{
			aal_exception_error("Node %llu, item %u: failed to "
					    "open an object pointed by %k.", 
					    place->node->number, 
					    place->pos.item, 
					    &place->item.key);
			return -EINVAL;
		}
	} else {
		/* Some not CHECKED item -- try to realize the object plugin,
		   take the most appropriate if some plugins fit. */
		if (!(object = repair_object_realize(lf->repair->fs->tree, 
						     place, FALSE)))
			return 0;

		/* Some plugin is choosed, check/fix the object structure. */
		res = repair_object_check_struct(object, callback_check_struct,
						      lf->repair->mode, NULL);
		
		if (res) {
			aal_exception_error("Node %llu, item %u: structure check "
					    "of the object pointed by %k failed. "
					    "Plugin %s.", place->node->number, 
					    place->pos.item, &place->item.key, 
					    object->entity->plugin->label);
			return res;
		}
	}
	
	/* Traverse the object. */
	if ((res = repair_object_traverse(object, callback_traverse_open, 
					  callback_traverse_close, lf)))
		goto error_close_object;

	repair_lost_found_make_lost_name(object, object->name);
	
	start = reiser4_object_start(object);
	
	/* If '..' is valid, then the parent<->object link was recovered during 
	   traversing. Othewise, link the object to "lost+found". */
	if (!repair_item_test_flag(start, OF_HAS_NAME)) {
		if ((res = reiser4_object_link(lf->lost, object, object->name))) {
			aal_exception_error("Node %llu, item %u: failed to link "
					    "the object pointed by %k to the "
					    "'lost+found' pointed by %k.",
					    start->node->number, start->pos.item, 
					    &object->info.object, 
					    &lf->lost->info.object);
			goto error_close_object;
		}

	}
		
	/* The whole reachable subtree must be recovered for now and marked as 
	   HAS_NAME. */
	
	reiser4_object_close(object);
	return 0;
	
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
	
	reiser4_tree_lroot(fs->tree);
	
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
		
		lf->lost = reiser4_dir_create(fs, root, "lost+found");
		
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
