/*  Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
    reiser4progs/COPYING.
    
    cleanup.c -- repair/cleanup.c -- cleanup pass recovery code.
    
    The pass is intended for cleanuping the storage reiser4 tree from
    not reacoverable garbage. */

#include <repair/cleanup.h>

#if 0
static errno_t repair_cleanup_check(place_t *place, void *data) {
	repair_cleanup_t *cleanup;
	errno_t res = 0;
	
	aal_assert("vpf-1060", place != NULL);
	aal_assert("vpf-1061", data != NULL);
	
	cleanup = (repair_cleanup_t *)data;

	if (reiser4_item_branch(place->plug))
		return 0;
	
	if (!repair_item_test_flag(place, OF_CHECKED)) {
		trans_hint_t hint;
		
		/* Not checked item does not belong to any object. Remove. */
		hint.count = 1;
		hint.place_func = NULL;
		
		res = reiser4_node_remove(place->node, &place->pos, &hint);
		if (res) return res;
		
		cleanup->stat.removed++;
		place->pos.item--;
		
		return 0;	
	}

	repair_item_clear_flag(place, MAX_UINT16);
	
	return 0;
}

static errno_t repair_cleanup_node_traverse(reiser4_tree_t *tree, 
					    node_t *node, void *data)
{
	repair_cleanup_t *cleanup = (repair_cleanup_t *)data;

	if (cleanup->progress_handler && 
	    reiser4_node_get_level(node) != LEAF_LEVEL) 
	{
		cleanup->progress->state = PROGRESS_START;
		cleanup->progress->u.tree.i_total = reiser4_node_items(node);
		cleanup->progress->u.tree.u_total = 0;
		cleanup->progress->u.tree.item = 0;
		cleanup->progress->u.tree.unit = 0;
		cleanup->progress_handler(cleanup->progress);
	}

	return repair_node_traverse(node, repair_cleanup_check, data);
}

static node_t *repair_cleanup_open_traverse(reiser4_tree_t *tree,
					    place_t *place,
					    void *data) 
{
	repair_cleanup_t *cleanup = (repair_cleanup_t *)data;
	
	if (cleanup->progress_handler &&
	    reiser4_node_get_level(place->node) != LEAF_LEVEL) 
	{
		cleanup->progress->state = PROGRESS_UPDATE;
		cleanup->progress->u.tree.i_total = reiser4_node_items(place->node);
		cleanup->progress->u.tree.u_total = reiser4_item_units(place);
		cleanup->progress->u.tree.item = place->pos.item;
		cleanup->progress->u.tree.unit = place->pos.unit;
		cleanup->progress_handler(cleanup->progress);
	}

	return reiser4_tree_child_node(tree, place);
}

static errno_t repair_cleanup_after_traverse(reiser4_tree_t *tree, 
					     node_t *node, 
					     void *data) 
{
	repair_cleanup_t *cleanup = (repair_cleanup_t *)data;
	
	if (cleanup->progress_handler && 
	    reiser4_node_get_level(node) != LEAF_LEVEL) 
	{
		cleanup->progress->state = PROGRESS_END;
		cleanup->progress_handler(cleanup->progress);
	}

	return 0;
}


errno_t repair_cleanup(repair_cleanup_t *cleanup) {
	repair_progress_t progress;
	reiser4_fs_t *fs;
	errno_t res;
	
	cleanup->progress = &progress;
	repair_cleanup_setup(cleanup);
	
	fs = cleanup->repair->fs;
	
	if (reiser4_tree_fresh(fs->tree)) {
		aal_exception_warn("No reiser4 metadata were found. Cleanup "
				   "pass is skipped.");
		return 0;
	}
	
	if ((res = reiser4_tree_load_root(fs->tree)))
		return res;
	
	if (fs->tree->root == NULL)
		return -EINVAL;
	
	/* Cut the corrupted, unrecoverable parts of the tree off. */
	res = reiser4_tree_trav_node(fs->tree, fs->tree->root, 
				     repair_cleanup_open_traverse, 
				     repair_cleanup_node_traverse, 
				     NULL, repair_cleanup_after_traverse, 
				     cleanup);
	
	repair_cleanup_update(cleanup);
	reiser4_fs_sync(fs);
	
	return res;
}
#endif

static void repair_cleanup_setup(repair_cleanup_t *cleanup) {
	aal_assert("vpf-1045", cleanup != NULL);

	aal_memset(cleanup->progress, 0, sizeof(*cleanup->progress));
	
	if (!cleanup->progress_handler)
		return;
	
	cleanup->progress->type = GAUGE_TREE;
	cleanup->progress->text = "***** Cleanup Pass: cleaning "
		"up the reiser4 storage tree.";
	cleanup->progress->state = PROGRESS_STAT;
	time(&cleanup->stat.time);
	cleanup->progress_handler(cleanup->progress);
	cleanup->progress->text = NULL;
}

static void repair_cleanup_update(repair_cleanup_t *cleanup) {
	aal_stream_t stream;
	char *time_str;
	
	aal_assert("vpf-1063", cleanup != NULL);
	
	if (!cleanup->progress_handler)
		return;
	
	cleanup->progress->state = PROGRESS_STAT;
	cleanup->progress_handler(cleanup->progress);
	
	aal_stream_init(&stream, NULL, &memory_stream);
	
	aal_stream_format(&stream, "\tRemoved items %llu\n", 
			  cleanup->stat.removed);
	
	time_str = ctime(&cleanup->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	aal_stream_format(&stream, "\tTime interval: %s - ", time_str);
	time(&cleanup->stat.time);
	time_str = ctime(&cleanup->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	aal_stream_format(&stream, time_str);
	
	cleanup->progress->state = PROGRESS_STAT;
	cleanup->progress->text = (char *)stream.entity;
	cleanup->progress_handler(cleanup->progress);
	
	aal_stream_fini(&stream);
}

static errno_t repair_cleanup_next_key(place_t *place, reiser4_key_t *key) {
	place_t temp;
	errno_t res;
	
	temp = *place;
	temp.pos.item++;
	
	if (reiser4_place_rightmost(&temp))
		return repair_tree_parent_rkey(temp.node->tree, 
					       temp.node, key);
	
	if ((res = reiser4_place_fetch(&temp)))
		return res;

	reiser4_key_assign(key, &temp.key);
	
	return 0;
}

errno_t repair_cleanup(repair_cleanup_t *cleanup) {
	repair_progress_t progress;
	reiser4_key_t key, max;
	reiser4_tree_t *tree;
	uint32_t count;
	errno_t res;
	
	aal_assert("vpf-1407", cleanup != NULL);
	aal_assert("vpf-1407", cleanup->repair != NULL);
	aal_assert("vpf-1407", cleanup->repair->fs != NULL);
	
	cleanup->progress = &progress;
	repair_cleanup_setup(cleanup);
	
	tree = cleanup->repair->fs->tree;
	
	if (reiser4_tree_fresh(tree)) {
		aal_exception_warn("No reiser4 metadata were found. Cleanup "
				   "pass is skipped.");
		return 0;
	}
	
	if ((res = reiser4_tree_load_root(tree)))
		return res;
	
	if (tree->root == NULL)
		return -EINVAL;
	
	key.plug = max.plug = tree->key.plug;
	reiser4_key_minimal(&key);
	reiser4_key_maximal(&max);
	
	while (TRUE) {
		trans_hint_t hint;
		lookup_t lookup;
		place_t place;
		
		/* FIXME-VITALY: this is not key-collision-safe. To solve it on 
		   the next item should be set flag NEXT. */
		
		/* Lookup the @key. */
		if ((lookup = reiser4_tree_lookup(tree, &key,  LEAF_LEVEL,
						  FIND_EXACT, &place)) < 0)
			return lookup;

		place.pos.unit = MAX_UINT32;
		count = reiser4_node_items(place.node);
		
		for (; place.pos.item < count; place.pos.item++) {
			if ((res = reiser4_place_fetch(&place)))
				return res;

			/* Leave only branches and marked as CHECKED. */
			if (!reiser4_item_branch(place.plug) && 
			    !repair_item_test_flag(&place, OF_CHECKED))
			{
				/* Remove. */
				res = repair_cleanup_next_key(&place, &key);
				
				if (res) return res;
				
				cleanup->stat.removed++;
				
				hint.count = 1;
				hint.place_func = NULL;
				
				if ((res = reiser4_tree_remove(tree, &place, &hint)))
					return res;

				goto cont;
			}

			repair_item_clear_flag(&place, MAX_UINT16);
		}

		place.pos.item--;
		
		if ((res = repair_cleanup_next_key(&place, &key)))
			return res;
	cont:
		if (!reiser4_key_compfull(&key, &max))
			break;
	}
	
	repair_cleanup_update(cleanup);
	reiser4_fs_sync(cleanup->repair->fs);
	
	return res;
}

