/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tree.c -- reiser4 tree code. */

#include <reiser4/libreiser4.h>

/* Return current fs blksize, which may be used in tree. */
uint32_t reiser4_tree_get_blksize(reiser4_tree_t *tree) {
	aal_assert("umka-2579", tree != NULL);
	aal_assert("umka-2580", tree->fs != NULL);
	aal_assert("umka-2581", tree->fs->master != NULL);

	return reiser4_master_get_blksize(tree->fs->master);
}

/* Returns TRUE if passed @node is tree root node. */
static bool_t reiser4_tree_root_node(reiser4_tree_t *tree,
				     reiser4_node_t *node)
{
	aal_assert("umka-2482", tree != NULL);
	aal_assert("umka-2483", node != NULL);
	
	return reiser4_tree_get_root(tree) == node->block->nr;
}

#ifndef ENABLE_MINIMAL
/* Updates root block number in format by passed @blk. Takes care about correct
   block number in loaded root node if any. */
void reiser4_tree_set_root(reiser4_tree_t *tree, blk_t blk) {
	aal_assert("umka-2409", tree != NULL);
	aal_assert("umka-2412", tree->fs != NULL);
	aal_assert("umka-2413", tree->fs->format != NULL);
	
	reiser4_format_set_root(tree->fs->format, blk);
}

/* Updates height in format by passed @height. */
void reiser4_tree_set_height(reiser4_tree_t *tree,
			     uint8_t height)
{
	aal_assert("umka-2410", tree != NULL);
	aal_assert("umka-2416", tree->fs != NULL);
	aal_assert("umka-2417", tree->fs->format != NULL);
	
	reiser4_format_set_height(tree->fs->format, height);
}

/* Unlocks @node and makes check if it is empty. If so and is not locked anymore
   it is detached from tree. */
errno_t reiser4_tree_unlock_node(reiser4_tree_t *tree, reiser4_node_t *node) {
	errno_t res;
	
	aal_assert("umka-3058", tree != NULL);
	aal_assert("umka-3059", node != NULL);
	
	reiser4_node_unlock(node);

	/* Check if we should release node as it is empty and node locked. */
	if (!reiser4_node_locked(node) && !reiser4_node_items(node) &&
	    (node->flags & NF_HEARD_BANSHEE))
	{
		/* Check if we should detach node from tree first. */
		if (reiser4_tree_attached_node(tree, node)) {
			if ((res = reiser4_tree_detach_node(tree, node,
							    SF_DEFAULT)))
			{
				return res;
			}
		}

		return reiser4_tree_release_node(tree, node);
	}
	
	return 0;
}
#endif

/* Returns tree root block number stored in format. */
blk_t reiser4_tree_get_root(reiser4_tree_t *tree) {
	aal_assert("umka-738", tree != NULL);
	aal_assert("umka-2414", tree->fs != NULL);
	aal_assert("umka-2415", tree->fs->format != NULL);

	return reiser4call(tree->fs->format, get_root);
}

#ifndef ENABLE_MINIMAL
/* Returns tree height stored in format. */
uint8_t reiser4_tree_get_height(reiser4_tree_t *tree) {
	aal_assert("umka-2411", tree != NULL);
	aal_assert("umka-2418", tree->fs != NULL);
	aal_assert("umka-2419", tree->fs->format != NULL);

	return reiser4call(tree->fs->format, get_height);
}

/* As @node already lies in @tree->nodes hash table and it is going to change
   its block number, we have to update its hash entry in @tree->nodes. This
   function does that job and also moves @node to @new_blk location. */
errno_t reiser4_tree_rehash_node(reiser4_tree_t *tree,
				 reiser4_node_t *node,
				 blk_t new_blk)
{
	blk_t old_blk;
	blk_t *set_blk;

	aal_assert("umka-3043", tree != NULL);
	aal_assert("umka-3044", node != NULL);
	aal_assert("umka-3045", reiser4_node_items(node) > 0);
	
	old_blk = node->block->nr;
	reiser4_node_move(node, new_blk);

	/* Allocating new key and assign new block number value to it. */
	if (!(set_blk = aal_calloc(sizeof(*set_blk), 0)))
		return -ENOMEM;

	*set_blk = new_blk;

	/* Remove old hash table entry and insert new allocated one. This is
	   quite cheapper operation. */
	if (aal_hash_table_remove(tree->nodes, &old_blk))
		return -EINVAL;

	return aal_hash_table_insert(tree->nodes, set_blk, node);
}
#endif

/* Puts @node to @tree->nodes hash table. */
static errno_t reiser4_tree_hash_node(reiser4_tree_t *tree,
				      reiser4_node_t *node)
{
	blk_t *blk;

	aal_assert("umka-3040", tree != NULL);
	aal_assert("umka-3041", node != NULL);
	
	/* Registering @node in @tree->nodes hash table with key equal to block
	   number of @node. */
	if (!(blk = aal_calloc(sizeof(*blk), 0)))
		return -ENOMEM;

	*blk = node->block->nr;

	return aal_hash_table_insert(tree->nodes, blk, node);
}

/* Removes @node from @tree->nodes hash table. Used when nodeis going to be
   disconnected from tree cache. */
static errno_t reiser4_tree_unhash_node(reiser4_tree_t *tree,
					reiser4_node_t *node)
{
	blk_t blk;

	aal_assert("umka-3046", tree != NULL);
	aal_assert("umka-3047", node != NULL);

	blk = node->block->nr;
	return aal_hash_table_remove(tree->nodes, &blk);
}

#ifndef ENABLE_MINIMAL
/* Acknowledles, that passed @place has nodeptr that points onto passed
   @node. This is needed for tree_rebind_node() function. */
static int tree_check_pos(reiser4_place_t *place, blk_t blocknr) {
	aal_assert("umka-3115", place != NULL);
	
	if (!(place->pos.item < reiser4_node_items(place->node)))
		return 0;
	       
	if (reiser4_place_fetch(place))
		return 0;

	if (!reiser4_item_branch(place->plug))
		return 0;

	return reiser4_item_down_link(place) == blocknr;
}

static void tree_next_child_pos(reiser4_node_t *left,
				reiser4_place_t *place)
{
	aal_assert("umka-3126", left != NULL);
	aal_assert("umka-3127", place != NULL);
	
	aal_memcpy(place, &left->p, sizeof(*place));
	place->pos.item++;
}
#endif

static errno_t tree_find_child_pos(reiser4_tree_t *tree,
				   reiser4_node_t *parent,
				   reiser4_node_t *child,
				   reiser4_place_t *place)
{
	lookup_hint_t hint;
        reiser4_key_t lkey;

#ifndef ENABLE_MINIMAL
	uint32_t i;
#endif
    
	aal_assert("umka-869", child != NULL);
	aal_assert("umka-3114", parent != NULL);
	aal_assert("umka-3038", reiser4_node_items(child) > 0);

	place->node = parent;
	
#ifndef ENABLE_MINIMAL
	/* Checking if we are in position already. */
	if (tree_check_pos(place, child->block->nr))
		goto out_correct_place;
#endif

	/* Getting position by means of using node lookup. */
        reiser4_node_leftmost_key(child, &lkey);
	hint.key = &lkey;

        if (reiser4_node_lookup(parent, &hint, FIND_EXACT,
				&place->pos) == PRESENT)
	{
#ifndef ENABLE_MINIMAL
		if (tree_check_pos(place, child->block->nr))
			goto out_correct_place;
#endif
	}

	/* Getting position by means of linear traverse. */
#ifndef ENABLE_MINIMAL
	for (i = 0; i < reiser4_node_items(place->node); i++) {
		uint32_t j;
		errno_t res;
			
		place->pos.item = i;

		if ((res = reiser4_place_fetch(place)))
			return res;

		if (!reiser4_item_branch(place->plug))
			continue;

		for (j = 0; j < reiser4_item_units(place); j++) {
			blk_t blocknr;
			
			place->pos.unit = j;

			blocknr = reiser4_item_down_link(place);

			if (child->block->nr == blocknr)
				goto out_correct_place;
		}
	}

	return -EINVAL;

out_correct_place:
#else
	if (reiser4_place_fetch(place))
		return -EINVAL;
#endif
	if (reiser4_item_units(place) == 1)
		place->pos.unit = MAX_UINT32;

	return 0;
}

/* Updates @node->p by position in parent node. */
static errno_t reiser4_tree_rebind_node(reiser4_tree_t *tree,
					reiser4_node_t *parent,
					reiser4_node_t *child)
{
	aal_assert("umka-3116", tree != NULL);
	aal_assert("umka-3117", child != NULL);
	aal_assert("umka-3122", parent != NULL);

	return tree_find_child_pos(tree, parent,
				   child, &child->p);
}

/* Loads root node and put it to @tree->nodes hash table. */
errno_t reiser4_tree_load_root(reiser4_tree_t *tree) {
	blk_t root_blk;
	
	aal_assert("umka-1870", tree != NULL);

	/* Check if root is loaded. */
	if (tree->root)
		return 0;

	/* Getting root node and loading it. */
	root_blk = reiser4_tree_get_root(tree);
	
	if (!(tree->root = reiser4_tree_load_node(tree, NULL, root_blk))) {
		aal_error("Can't load root node %llu.", root_blk);
		return -EIO;
	}

	tree->root->tree = (tree_entity_t *)tree;
	return 0;
}

#if 0
/* DEBUGGING */
static errno_t cb_count_children(reiser4_place_t *place, void *data) {
	uint32_t *count = (uint32_t *)data;
	blk_t blk;
	
	blk = reiser4_item_down_link(place);
	if (reiser4_tree_lookup_node(place->node->tree, blk))
		(*count)++;
	
	return 0;
}

/* Debugging code for catching wrong node->counter. */
uint32_t debug_node_loaded_children(reiser4_node_t *node) {
	uint32_t count = 0;
	if (!node) return 0;
	reiser4_node_trav(node, cb_count_children, &count);
	return count;
}

/* Debugging code for catching wrong order of keys. */
static errno_t debug_node_check_keys(reiser4_node_t *node) {
	reiser4_key_t key, prev;
	pos_t pos = {0, MAX_UINT32};
	uint32_t count;
	
	count = reiser4_node_items(node);

	for (pos.item = 0; pos.item < count; pos.item++) {
		plug_call(node->plug->pl.node, 
			  get_key, node, &pos, &key);

		if (pos.item && reiser4_key_compfull(&prev, &key) > 0)
			return 1;

		aa_memcpy(&prev, &key, sizeof(key));
	}

	return 0;
}

#endif

#ifndef ENABLE_MINIMAL
/* Assignes passed @node to root. Takes care about root block number and tree
   height in format. */
errno_t reiser4_tree_assign_root(reiser4_tree_t *tree,
				 reiser4_node_t *node)
{
	blk_t blk;
	uint32_t level;
	
	aal_assert("umka-1867", tree != NULL);
	aal_assert("umka-1868", node != NULL);

	/* Establishing connection between node and tree. */
	tree->root = node;
	node->tree = (tree_entity_t *)tree;
	node->p.node = NULL;

	if (reiser4_tree_connect_node(tree, NULL, node))
		return -EINVAL;
	
	/* Updating tree height. */
	level = reiser4_node_get_level(node);
	reiser4_tree_set_height(tree, level);

	/* Updating root block number. */
	blk = tree->root->block->nr;
	reiser4_tree_set_root(tree, blk);

	return 0;
}
#endif

errno_t reiser4_tree_mpressure(reiser4_tree_t *tree) {
	errno_t res;
	
	/* Check for memory pressure event. If memory pressure is uppon us, we
	   call memory cleaning function. For now we call tree_adjust() in order
	   to release not locked nodes. */
	if (!tree->mpc_func || !tree->mpc_func(tree))
		return 0;

	/* Adjusting the tree as memory pressure is here. */
	if ((res = reiser4_tree_adjust(tree))) {
		aal_error("Can't adjust tree.");
		return res;
	}

	return 0;
}

/* Registers passed node in tree and connects left and right neighbour
   nodes. This function does not do any tree modifications. */
errno_t reiser4_tree_connect_node(reiser4_tree_t *tree,
				  reiser4_node_t *parent, 
				  reiser4_node_t *node)
{
	errno_t res;
	
	aal_assert("umka-1857", tree != NULL);
	aal_assert("umka-2261", node != NULL);

	node->tree = (tree_entity_t *)tree;

	if (reiser4_tree_root_node(tree, node)) {
		/* This is the case when we connect root node, that is with no
		   parent. */
		tree->root = node;
	} else if (parent) {
		/* Updating node->p parent place. */
		if (reiser4_tree_rebind_node(tree, parent, node))
			return -EINVAL;

		reiser4_node_lock(parent);
	}
	
	if (reiser4_tree_hash_node(tree, node))
		return -EINVAL;

	reiser4_node_lock(node);
	if ((res = reiser4_tree_mpressure(tree))) {
		aal_error("Can't connect node %llu.",
			  node->block->nr);

		if (parent)
			reiser4_node_unlock(parent);
		
		reiser4_tree_unhash_node(tree, node);
	}
	reiser4_node_unlock(node);
	
	return res;
}

/* Remove specified child from the node children list. Updates all neighbour
   pointers and parent pointer.*/
errno_t reiser4_tree_disconnect_node(reiser4_tree_t *tree,
				     reiser4_node_t *node)
{
	aal_assert("umka-563", node != NULL);
	aal_assert("umka-1858", tree != NULL);
	
	/* Disconnecting left and right neighbours. */
	if (node->left) {
		node->left->right = NULL;
		node->left = NULL;
	}
	
	if (node->right) {
		node->right->left = NULL;
		node->right = NULL;
	}
	
	/* Disconnecting @node from @tree->nodes hash table. */
	if (reiser4_tree_unhash_node(tree, node))
		return -EINVAL;
	
	node->tree = NULL;

	if (tree->root == node) {
		/* The case when we're disconnecting root node for some
		   reasons. And we will let to do so? Yes, why not? */
		tree->root = NULL;
	}

	/* Unlock parent node. */
	if (node->p.node) {
		reiser4_node_unlock(node->p.node);
		node->p.node = NULL;
	}

	return 0;
}

#ifndef ENABLE_MINIMAL
/* Updates all internal node loaded children positions in parent. */
static errno_t reiser4_tree_update_node(reiser4_tree_t *tree,
					reiser4_node_t *node,
					uint8_t start, uint8_t end)
{
	uint32_t i;
	errno_t res;

	aal_assert("umka-3035", tree != NULL);
	aal_assert("umka-3036", node != NULL);
	aal_assert("umka-3037", reiser4_node_items(node) > 0);
	
	for (i = start; i < end; i++) {
		blk_t blk;
		uint32_t j;

		reiser4_node_t *child;
		reiser4_place_t place;

		/* Initializing item at @i. */
		reiser4_place_assign(&place, node, i, MAX_UINT32);

		if ((res = reiser4_place_fetch(&place)))
			return res;

		if (!reiser4_item_branch(place.plug))
			continue;

		for (j = 0; j < reiser4_item_units(&place); j++) {
			place.pos.unit = j;
			
			blk = reiser4_item_down_link(&place);

			/* Getting loaded child node. If it is not loaded, we
			   don't have to update its parent pos. */
			if (!(child = reiser4_tree_lookup_node(tree, blk)))
				continue;

			/* Unlock old parent node. There are some cases when
			   parent is not set yet (like tree_growup()). So, we
			   check parent for null. */
			if (child->p.node) {
				reiser4_node_unlock(child->p.node);
				reiser4_node_lock(node);
			}
			
			/* Updating position in parent node. */
			if ((res = reiser4_tree_rebind_node(tree, node, child)))
				return res;
			
			if (reiser4_node_items(node) == 0) {
				if (node->flags & NF_HEARD_BANSHEE)
					continue;

				aal_bug("umka-3060", "Node (%llu) is empty "
					"but not marked as 'heard banshee'.",
					node->block->nr);
			}
		}
	}

	return 0;
}
#endif

reiser4_node_t *reiser4_tree_lookup_node(reiser4_tree_t *tree, blk_t blk) {
	aal_assert("umka-3002", tree != NULL);
	return aal_hash_table_lookup(tree->nodes, &blk);
}

/* Loads node from @blk and connects it to @parent. */
reiser4_node_t *reiser4_tree_load_node(reiser4_tree_t *tree,
				       reiser4_node_t *parent, blk_t blk)
{
	reiser4_node_t *node = NULL;

	aal_assert("umka-1289", tree != NULL);

	/* Checking if node in the local cache of @parent. */
	if (!(node = reiser4_tree_lookup_node(tree, blk))) {
		aal_assert("umka-3004", !reiser4_fake_ack(blk));

		/* Node is not loaded yet. Loading it and connecting to @parent
		   node cache. */
		if (!(node = reiser4_node_open(tree, blk)))
			return NULL;

		/* Connect loaded node to cache. */
		if (reiser4_tree_connect_node(tree, parent, node))
			goto error_free_node;
	}

	return node;

 error_free_node:
	reiser4_node_close(node);
	return NULL;
}

/* Unloading node and unregistering it from @tree->nodes hash table. */
errno_t reiser4_tree_unload_node(reiser4_tree_t *tree, reiser4_node_t *node) {
	errno_t res;
	
	aal_assert("umka-1840", tree != NULL);
	aal_assert("umka-1842", node != NULL);

#ifndef ENABLE_MINIMAL
	/* Check if node is dirty. */
	if (reiser4_node_isdirty(node)) {
		aal_warn("Unloading dirty node %llu.",
			 node->block->nr);
	}
#endif

	/* Disconnecting @node from its parent node. */
	if ((res = reiser4_tree_disconnect_node(tree, node))) {
		aal_error("Can't disconnect node from "
			  "tree cache.");
		return res;
	}

	/* Releasing node instance. */
	return reiser4_node_close(node);
}

/* Loads denoted by passed nodeptr @place child node */
reiser4_node_t *reiser4_tree_child_node(reiser4_tree_t *tree,
					reiser4_place_t *place)
{
	reiser4_node_t *node;
	blk_t blk;
	
	aal_assert("umka-1889", tree != NULL);
	aal_assert("umka-1890", place != NULL);
	aal_assert("umka-1891", place->node != NULL);

	/* Initializing @place. */
	if (reiser4_place_fetch(place))
		return NULL;

	/* Checking if item is a branch of tree */
	if (!reiser4_item_branch(place->plug))
		return NULL;

	blk = reiser4_item_down_link(place);
	if (!(node = reiser4_tree_load_node(tree, place->node, blk))) {
		aal_error("Can't load child node %llu.", blk);
		return NULL;
	}

	return node;
}

static int reiser4_tree_neig_place(reiser4_tree_t *tree, 
				   reiser4_place_t *place,
				   uint32_t where) 
{
	int found = 0;
	uint32_t level = 0;
	
	/* Going up to the level where corresponding neighbour node may be
	   obtained by its nodeptr item. */
        while (place->node->p.node && found == 0) {
		aal_memcpy(place, &place->node->p, sizeof(*place));

		/* Checking position. Level is found if position is not first
		   (right neighbour) and is not last one (left neighbour). */
		found = where == DIR_LEFT ? 
			reiser4_place_gtfirst(place) :
			reiser4_place_ltlast(place);

                level++;
        }

	if (!found)
		return 0;
	
	/* Position correcting. We do not use place_inc() and place_dec() here,
	   because they are not accessible in minimal mode and we do not
	   want to make it accessible because here is one place only and they
	   are quite big. */
	place->pos.item += (where == DIR_LEFT ? -1 : 1);
	return level;
}

/* Finds both left and right neighbours and connects them into the tree. */
static reiser4_node_t *reiser4_tree_ltrt_node(reiser4_tree_t *tree,
					      reiser4_node_t *node, 
					      uint32_t where)
{
        reiser4_place_t place;
        uint32_t level;
                                                                                      
	aal_assert("umka-2213", tree != NULL);
	aal_assert("umka-2214", node != NULL);

	reiser4_place_assign(&place, node, 0, MAX_UINT32);
	
        if (!(level = reiser4_tree_neig_place(tree, &place, where)))
                return NULL;

	reiser4_node_lock(node);
	
        /* Going down to the level of @node. */
        while (level > 0) {
		
                if (!(place.node = reiser4_tree_child_node(tree, &place))) {
			reiser4_node_unlock(node);
			return NULL;
		}

		if (where == DIR_LEFT) {
			if (reiser4_place_last(&place)) {
				reiser4_node_unlock(node);
				return NULL;
			}
		} else {
			if (reiser4_place_first(&place)) {
				reiser4_node_unlock(node);
				return NULL;
			}
		}
		
                level--;
        }
	
	reiser4_node_unlock(node);
		
        /* Setting up sibling pointers. */
        if (where == DIR_LEFT) {
                node->left = place.node;
                place.node->right = node;
        } else {
                node->right = place.node;
                place.node->left = node;
        }
	
	return place.node;
}

static errno_t reiser4_tree_adjust_place(reiser4_tree_t *tree, 
					 reiser4_place_t *place, 
					 reiser4_place_t *next) 
{
	/* Check if we have to get right neighbour node. */
	if (place->pos.item >= reiser4_node_items(place->node)) {
		
		/* Load the right neighbour. */
		reiser4_tree_ltrt_node(tree, place->node, DIR_RIGHT);

		if (place->node->right) {
			/* The right neighbour exists. */
			reiser4_place_assign(next, place->node->right, 0, 0);
		} else {
			/* There is no right neighbour. Get the right neighbour
			   of the above level if there is one. */
			aal_memcpy(next, place, sizeof(*place));

			if (!reiser4_tree_neig_place(tree, next, DIR_RIGHT)) {
				/* Not found. */
				aal_memset(next, 0, sizeof(*next));
				return 0;
			}
		}
	} else {
		aal_memcpy(next, place, sizeof(*place));
	}
	
	/* Initializing @place. */
	return reiser4_place_fetch(next);
}

/* Moves @place by one item to right. If node is over, returns node next to
   passed @place. Needed for moving though the tree node by node, for instance
   in directory read code. */
errno_t reiser4_tree_next_place(reiser4_tree_t *tree, 
				reiser4_place_t *place,
				reiser4_place_t *next)
{
	reiser4_node_t *node;
	
	aal_assert("umka-867", tree != NULL);
	aal_assert("umka-868", place != NULL);
	aal_assert("umka-1491", next != NULL);

	aal_memcpy(next, place, sizeof(*place));
	next->pos.item++;
	next->pos.unit = 0;
	
	if (reiser4_tree_adjust_place(tree, next, next))
		return -EINVAL;

	if (!next->node) 
		return 0;
	
	node = next->node;
	reiser4_node_lock(node);
	
	/* If nodeptr item go down. */
	while (reiser4_item_branch(next->plug)) {
		blk_t blk;
		
		blk = reiser4_item_down_link(next);
		
		next->node = reiser4_tree_load_node(tree, next->node, blk);
		
		if (!next->node) {
			aal_error("Can't load a child node %llu of the node"
				  " (%llu).", blk, node->block->nr);
			goto error;
		}

		if (reiser4_place_first(next))
			goto error;

		if (reiser4_place_fetch(next))
			goto error;
	}
	
	reiser4_node_unlock(node);

	return 0;
 error:
	reiser4_node_unlock(node);
	return -EINVAL;
}

/* Get the key of the given @place. If @place is not valid, get the key of 
   the right neighbour. */
errno_t reiser4_tree_place_key(reiser4_tree_t *tree, 
			       reiser4_place_t *place,
			       reiser4_key_t *key) 
{
	reiser4_place_t next;
	
	aal_assert("vpf-1527", tree != NULL);
	aal_assert("vpf-1528", place != NULL);
	aal_assert("vpf-1529", key != NULL);
	
	aal_memcpy(&next, place, sizeof(*place));
	
	if (next.pos.item >= reiser4_node_items(next.node)) {
		if (!reiser4_tree_neig_place(tree, &next, DIR_RIGHT)) {
			key->plug = tree->key.plug;
			reiser4_key_maximal(key);
			return 0;
		}
	}

	if (reiser4_place_fetch(&next))
		return -EINVAL;
	
	return reiser4_item_get_key(&next, key);
}

#ifndef ENABLE_MINIMAL
/* Gets the key of the next item. */
errno_t reiser4_tree_next_key(reiser4_tree_t *tree, 
			      reiser4_place_t *place, 
			      reiser4_key_t *key) 
{
	reiser4_place_t temp;
	
	aal_assert("vpf-1427", tree != NULL);
	aal_assert("vpf-1427", place != NULL);
	aal_assert("vpf-1427", key != NULL);

	temp = *place;
	temp.pos.item++;
	temp.pos.unit = MAX_UINT32;

	return reiser4_tree_place_key(tree, &temp, key);
}

/* Requests block allocator for new block and creates empty node in it. */
reiser4_node_t *reiser4_tree_alloc_node(reiser4_tree_t *tree,
					uint8_t level)
{
	reiser4_plug_t *nplug;
	reiser4_node_t *node;
	uint32_t stamp;
	errno_t res;
	blk_t blk;
	rid_t node_pid;

	reiser4_format_t *format;
    
	aal_assert("umka-756", tree != NULL);
    
	/* Allocating fake block number. */
	blk = reiser4_fake_get();
	format = tree->fs->format;

	/* Setting up of the free blocks in format. */
	if ((res = reiser4_format_dec_free(format, 1)))
		return NULL;

	/* Grab node plugin */
	node_pid = reiser4_format_node_pid(format);
	if (!(nplug = reiser4_factory_ifind(NODE_PLUG_TYPE, node_pid))) {
		aal_error("Unknown node plugin.");
		return NULL;
	}
	
	/* Creating new node. */
	if (!(node = reiser4_node_create(tree, (reiser4_node_plug_t *)nplug,
					 blk, level))) {
		aal_error("Can't initialize new fake node.");
		return NULL;
	}

	/* Setting flush stamps to new node. */
	stamp = reiser4_format_get_stamp(format);
	reiser4_node_set_mstamp(node, stamp);
	node->tree = &tree->ent;

	return node;
}

/* Unload node and releasing it in block allocator */
errno_t reiser4_tree_release_node(reiser4_tree_t *tree,
				  reiser4_node_t *node)
{
	reiser4_alloc_t *alloc;
	reiser4_format_t *format;
	
	aal_assert("umka-1841", tree != NULL);
	aal_assert("umka-2255", node != NULL);

	alloc = tree->fs->alloc;
	format = tree->fs->format;
	reiser4_node_mkclean(node);

	/* Check if we're trying to releas a node with fake block number. If
	   not, free it in block allocator too. */
	if (!reiser4_fake_ack(node->block->nr)) {
		blk_t blk = node->block->nr;
		reiser4_alloc_release(alloc, blk, 1);
	}

	/* Setting up of the free blocks in format. */
	reiser4_format_inc_free(format, 1);

	/* Release node itself. */
	return reiser4_node_close(node);
}

/* Removes nodeptr that points to @node, disconnects it from tree and then
   releases @node itself. */
errno_t reiser4_tree_discard_node(reiser4_tree_t *tree,
				  reiser4_node_t *node)
{
	errno_t res;

	if ((res = reiser4_tree_detach_node(tree, node,
					    SF_DEFAULT)))
	{
		aal_error("Can't detach node %llu from tree.", 
			  node->block->nr);
		return res;
	}
	
	if ((res = reiser4_tree_release_node(tree, node))) {
		aal_error("Can't release node %llu.", node->block->nr);
		return res;
	}

	return res;
}

/* Helper function for freeing passed key instance tree's data hashtable entry
   is going to be removed. */
static void cb_blocks_keyrem_func(void *key) {
	reiser4_key_free((reiser4_key_t *)key);
}

/* Helper function for freeing hash value, that is, data block. */
static void cb_blocks_valrem_func(void *val) {
	aal_block_free((aal_block_t *)val);
}

/* Helper function for calculating 64-bit hash by passed key. This is used for
   tree's data hash. Note: as offset of extent blocks is divisible by blocksize
   (4096 by default) offset is shifted on 12 bits to the right to have neighbour 
   blocks in neighbour lists. */
static uint64_t cb_blocks_hash_func(void *key) {
	return (reiser4_key_get_objectid((reiser4_key_t *)key) +
		(reiser4_key_get_offset((reiser4_key_t *)key) >> 12));
}

/* Helper function for comparing two keys during tree's data hash lookups. */
static int cb_blocks_comp_func(void *key1, void *key2,
				     void *data)
{
	return reiser4_key_compfull((reiser4_key_t *)key1,
				    (reiser4_key_t *)key2);
}

/* Returns level in tree particular item should be inserted at. */
inline uint32_t reiser4_tree_target_level(reiser4_tree_t *tree,
					  reiser4_plug_t *plug)
{
	return (plug->id.group == EXTENT_ITEM) ?
		TWIG_LEVEL : LEAF_LEVEL;
}

#endif

/* Helpher function for freeing keys in @tree->nodes hash table during its
   destroying. */
static void cb_nodes_keyrem_func(void *key) {
	aal_free(key);
}

/* Return hash number from passed key value from @tree->nodes hashtable. */
static uint64_t cb_nodes_hash_func(void *key) {
	return *(uint64_t *)key;
}

/* Compares two passed keys of @tree->nodes hash table during lookup in it. */
static int cb_nodes_comp_func(void *key1, void *key2, void *data) {
	if (*(uint64_t *)key1 < *(uint64_t *)key2)
		return -1;

	if (*(uint64_t *)key1 > *(uint64_t *)key2)
		return 1;

	return 0;
}

/* Returns the key of the fake root parent */
errno_t reiser4_tree_root_key(reiser4_tree_t *tree,
			      reiser4_key_t *key)
{
	oid_t locality;
	oid_t objectid;
	
	aal_assert("umka-1949", tree != NULL);
	aal_assert("umka-1950", key != NULL);

	key->plug = (reiser4_key_plug_t *)tree->ent.tset[TSET_KEY];
	
#ifndef ENABLE_MINIMAL
	locality = reiser4_oid_root_locality(tree->fs->oid);
	objectid = reiser4_oid_root_objectid(tree->fs->oid);
#else
	locality = REISER4_ROOT_LOCALITY;
	objectid = REISER4_ROOT_OBJECTID;
#endif
	return objcall(key, build_generic, KEY_STATDATA_TYPE, 
		       locality, 0, objectid, 0);
}

#ifndef ENABLE_MINIMAL
# define TREE_NODES_TABLE_SIZE (512)
#else
# define TREE_NODES_TABLE_SIZE (32)
#endif
#define TREE_BLOCKS_TABLE_SIZE (512)

/* Initializes tree instance on passed filesystem and return it to caller. Then
   it may be used for modifying tree, making lookup, etc. */
reiser4_tree_t *reiser4_tree_init(reiser4_fs_t *fs) {
	reiser4_tree_t *tree;

	aal_assert("umka-737", fs != NULL);

	/* Allocating memory for tree instance */
	if (!(tree = aal_calloc(sizeof(*tree), 0)))
		return NULL;

	tree->fs = fs;
	tree->adjusting = 0;

	/* Initializing hash table for storing loaded formatted nodes in it. */
	if (!(tree->nodes = aal_hash_table_create(TREE_NODES_TABLE_SIZE,
						  cb_nodes_hash_func,
						  cb_nodes_comp_func,
						  cb_nodes_keyrem_func,
						  NULL)))
	{
		goto error_free_tree;
	}

#ifndef ENABLE_MINIMAL
	/* Initializing hash table for storing loaded unformatted blocks in
	   it. This uses all callbacks we described above for getting hash
	   values, lookup, etc. */
	if (!(tree->blocks = aal_hash_table_create(TREE_BLOCKS_TABLE_SIZE,
						   cb_blocks_hash_func,
						   cb_blocks_comp_func,
						   cb_blocks_keyrem_func,
						   cb_blocks_valrem_func)))
	{
		goto error_free_nodes;
	}

#endif
	/* Initializing the tset. */
	if (reiser4_tset_init(tree))
		goto error_free_data;

	/* Building tree root key. It is used in tree lookup, etc. */
	if (reiser4_tree_root_key(tree, &tree->key)) {
		aal_error("Can't build the tree root key.");
		goto error_free_data;
	}
    
	return tree;

 error_free_data:
#ifndef ENABLE_MINIMAL
	aal_hash_table_free(tree->blocks);
 error_free_nodes:
#endif
	aal_hash_table_free(tree->nodes);
 error_free_tree:
	aal_free(tree);
	return NULL;
}

/* Unloads all loaded tree nodes. */
errno_t reiser4_tree_collapse(reiser4_tree_t *tree) {
        aal_assert("umka-2265", tree != NULL);
                                                                                          
        if (!tree->root)
                return 0;
                                                                                          
        return reiser4_tree_walk_node(tree, tree->root, 
#ifndef ENABLE_MINIMAL
				      NULL, NULL,
#endif
                                      reiser4_tree_unload_node);
}

/* Closes specified tree without saving dirty nodes to device. It just thows out
   all loaded nodes without dealing with allocating etc. This may be used in
   minimal mode and/or just to free modified tree without a changes on
   device. */
void reiser4_tree_close(reiser4_tree_t *tree) {
	aal_assert("vpf-1316", tree != NULL);

	/* Close all remaining nodes. */
	reiser4_tree_collapse(tree);

	/* Releasing unformatted nodes hash table. */
#ifndef ENABLE_MINIMAL
	aal_hash_table_free(tree->blocks);
#endif

	/* Releasing fomatted nodes hash table. */
	aal_hash_table_free(tree->nodes);

	/* Freeing tree instance. */
	tree->fs->tree = NULL;
	aal_free(tree);
}
#ifndef ENABLE_MINIMAL
#if 0
static errno_t cb_flags_dup(reiser4_place_t *place, void *data) {
	reiser4_item_dup_flags(place, *(uint16_t *)data);
	return 0;
}

/* Allocates extent item at passed @place. */
static errno_t reiser4_tree_alloc_extent(reiser4_tree_t *tree,
					 reiser4_place_t *place)
{
	errno_t res;
	uint32_t units;
	uint16_t flags;
	ptr_hint_t ptr;
	uint32_t blksize;
	trans_hint_t hint;

	units = reiser4_item_units(place);
	blksize = reiser4_tree_get_blksize(tree);

	/* Prepare @hint. */
	hint.count = 1;
	hint.specific = &ptr;
	hint.plug = place->plug;
	hint.region_func = NULL;
	hint.place_func = cb_flags_dup;
	hint.data = &flags;

	/* We force balancing use these flags with disables left shift
	   in order to not affect to items/units left of insert point,
	   as we allocate items/units from left to right. */
	hint.shift_flags = (SF_DEFAULT & ~SF_ALLOW_LEFT);

	for (place->pos.unit = 0; place->pos.unit < units;
	     place->pos.unit++)
	{
		uint64_t width;
		uint64_t blocks;
		uint64_t offset;
		
		reiser4_key_t key;
		int first_time = 1;


		if (objcall(place, object->fetch_units, &hint) != 1)
			return -EIO;

		/* Check if we have accessed unallocated extent. */
		if (ptr.start != EXTENT_UNALLOC_UNIT)
			continue;

		/* Getting unit key. */
		objcall(place, balance->fetch_key, &key);

		/* Loop until all units get allocated. */
		for (blocks = 0, width = ptr.width; width > 0; width -= ptr.width) {
			blk_t blk;
			uint32_t i;
			aal_block_t *block;
			
			/* Trying to allocate @ptr.width blocks. */
			if (!(ptr.width = reiser4_alloc_allocate(tree->fs->alloc,
								 &ptr.start, width)))
			{
				return -ENOSPC;
			}

			if (first_time) {
				flags = reiser4_item_get_flags(place);

				/* Updating extent unit at @place->pos.unit. */
				if (objcall(place, object->update_units, 
					    &hint) != 1)
				{
					return -EIO;
				}

				first_time = 0;
			} else {
				errno_t res;
				uint32_t level;
				reiser4_place_t iplace;

				iplace = *place;
				iplace.pos.unit++;

				/* Insert new extent units. */
				aal_memcpy(&hint.offset, &key, sizeof(key));
				level = reiser4_node_get_level(iplace.node);
				
				if ((res = reiser4_tree_insert(tree, &iplace,
							       &hint, level)) < 0)
				{
					return res;
				}

                                /* Updating @place by insert point, as it might
				   be moved due to balancing. */
				aal_memcpy(place, &iplace, sizeof(iplace));

				/* Updating @units as it might be changed after
				   balancing during tree_insert(). */
				units = reiser4_item_units(place);
			}

			/* Moving data blocks to right places, saving them and
			   releasing from the cache. */
			for (blk = ptr.start, i = 0; i < ptr.width; i++, blk++) {
				/* Getting data block by @key */
				block = aal_hash_table_lookup(tree->blocks, &key);
				aal_assert("umka-2469", block != NULL);

				/* Moving block tro @blk */
				aal_block_move(block, tree->fs->device, blk);

				/* Saving block to device. */
				if ((res = aal_block_write(block))) {
					aal_error("Can't write block "
						  "%llu.", block->nr);
					return res;
				}

				/* Releasing cache entry. */
				aal_hash_table_remove(tree->blocks, &key);

				/* Updating the key to find next data block */
				offset = objcall(&key, get_offset);

				objcall(&key, set_offset, offset + blksize);
			}
			
			blocks += ptr.width;
		}
	}

	return 0;
}
#endif /* 0 */

static errno_t cb_node_adjust(reiser4_tree_t *tree, reiser4_node_t *node) {
	errno_t res;
	
	aal_assert("umka-2302", tree != NULL);
	aal_assert("umka-2303", node != NULL);
	aal_assert("umka-3075", reiser4_node_items(node) > 0);

	/* Requesting block allocator to allocate the real block number
	   for fake allocated node. */
	if (reiser4_fake_ack(node->block->nr)) {
		blk_t blk;
		
		if (!reiser4_alloc_allocate(tree->fs->alloc, &blk, 1))
			return -ENOSPC;

		if (reiser4_tree_root_node(tree, node))
			reiser4_tree_set_root(tree, blk);
		
		if (node->p.node) {
			if ((res = reiser4_item_update_link(&node->p, blk)))
				return res;
		}
		
		/* Rehashing node in @tree->nodes hash table. */
		reiser4_tree_rehash_node(tree, node, blk);
	}

	return 0;
}

#if 0
/* Runs through the node in @place and calls tree_adjust_node() for all
   children. */
static errno_t cb_nodeptr_adjust(reiser4_tree_t *tree, reiser4_place_t *place) {
	/* It is not good, that we reference here to particular item group. But,
	   we have to do so, considering, that this is up to tree to know about
	   items type in it. Probably this is why tree should be plugin too to
	   handle things like this in more flexible manner. */
	if (place->plug->p.id.group != EXTENT_ITEM) 
		return 0;
	
	/* Allocating unallocated extent item at @place. */
	return reiser4_tree_alloc_extent(tree, place);
}
#endif /* 0 */
#endif

static errno_t cb_node_unload(reiser4_tree_t *tree, reiser4_node_t *node) {
	/* If node is locked, that is not a leaf or it is used by someone, it
	   cannot be released, and thus, it does not make the sense to save it
	   to device too. */
	if (reiser4_node_locked(node))
		return 0;
	
#ifndef ENABLE_MINIMAL
	/* Okay, node is fully allocated now and ready to be saved to device if
	   it is dirty. */
	if (reiser4_node_isdirty(node) && reiser4_node_sync(node)) {
		aal_error("Can't write node %llu.", node->block->nr);
		return -EIO;
	}
#endif
	/* Unloading node from tree cache. */
	return reiser4_tree_unload_node(tree, node);
}


/* Entry point for adjsuting tree routines. */
errno_t reiser4_tree_adjust(reiser4_tree_t *tree) {
	aal_assert("umka-3034", tree != NULL);
	
	if (tree->root && !tree->adjusting) {
		errno_t res = 0;
		
		tree->adjusting = 1;

		/* Check for special case -- tree_adjust() is calling during
		   tree_growup(), when empty root is connected. */
		if (reiser4_node_items(tree->root))
#ifndef ENABLE_MINIMAL
			res = reiser4_tree_walk_node(tree, tree->root, 
						     cb_node_adjust,
						     NULL,
						     cb_node_unload);
#else
			res = reiser4_tree_walk_node(tree, tree->root, 
						     cb_node_unload);
#endif
		
		tree->adjusting = 0;
		
		return res;
	}

	return 0;
}

/* Walking though the tree cache and closing all nodes. */
errno_t reiser4_tree_walk_node(reiser4_tree_t *tree, 
			       reiser4_node_t *node,
#ifndef ENABLE_MINIMAL
			       walk_func_t pre_func, 
			       walk_on_func_t on_func,
#endif
			       walk_func_t post_func)
{
	uint32_t i;
	errno_t res;
	
	aal_assert("umka-1933", tree != NULL);
	aal_assert("umka-1934", node != NULL);

#ifndef ENABLE_MINIMAL
	if (pre_func && (res = pre_func(tree, node)))
		return res;
#endif
	
	for (i = 0; i < reiser4_node_items(node); i++) {
		blk_t blk;
		uint32_t j;

		reiser4_node_t *child;
		reiser4_place_t place;

		/* Initializing item at @i. */
		reiser4_place_assign(&place, node, i, MAX_UINT32);

		if ((res = reiser4_place_fetch(&place)))
			return res;

#ifndef ENABLE_MINIMAL
		if (on_func && (res = on_func(tree, &place)))
			return res;
#endif

		if (!reiser4_item_branch(place.plug))
			continue;

		reiser4_node_lock(node);
		for (j = 0; j < reiser4_item_units(&place); j++) {
			/* Getting node by its nodeptr. If it is loaded, we call
			   tree_adjust_node() recursively in order to allocate
			   children. */
			place.pos.unit = j;
			
			blk = reiser4_item_down_link(&place);

			if (!(child = reiser4_tree_lookup_node(tree, blk)))
				continue;

			/* Making recursive call to tree_walk_node(). */
			if ((res = reiser4_tree_walk_node(tree, child, 
#ifndef ENABLE_MINIMAL
							  pre_func, on_func, 
#endif
							  post_func)))
			{
				reiser4_node_unlock(node);
				return res;
			}
		}
		reiser4_node_unlock(node);
	}
	
	/* Calling @walk_func for @node. */
	return post_func ? post_func(tree, node) : 0;
}

#ifndef ENABLE_MINIMAL
/* Helper function for save one unformatted block to device. Used from
   tree_sync() to save all in-memory unfromatted blocks. */
static errno_t cb_save_block( void *entry, void *data) {
	aal_hash_node_t *node = (aal_hash_node_t *)entry;
	aal_block_t *block = (aal_block_t *)node->value;

	/* Save block if it is dirty. */
	if (block->dirty) {
		errno_t res;
		
		if ((res = aal_block_write(block)))
			return res;
	}

	return 0;
}

/* Packs one level at passed @node. Moves all items and units from right node
   to left neighbour node and so on until rightmost node is reached. */
static errno_t reiser4_tree_compress_level(reiser4_tree_t *tree,
					   reiser4_node_t *node)
{
	errno_t res;
	reiser4_node_t *right;
	
	aal_assert("umka-3009", tree != NULL);
	aal_assert("umka-3010", node != NULL);

	/* Loop until rightmost node is reached. */
	while ((right = reiser4_tree_ltrt_node(tree, node, DIR_RIGHT))) {
		uint32_t flags;
		reiser4_place_t bogus;

		bogus.node = right;

		/* Left shift and merge is allowed. As this function will be
		   used mostly in the case of out of space, we do not allow to
		   allocate new nodes during shift. */
		flags = (SF_ALLOW_LEFT | SF_ALLOW_MERGE);

		/* Shift items and units from @right to @node with @flags. */
		if ((res = reiser4_tree_shift(tree, &bogus, node, flags))) {
			aal_error("Can't shift node %llu into left.",
				  right->block->nr);
			return res;
		}

		/* Check if node get empty. If so we release it. */
		if (reiser4_node_items(right) == 0) {
			if (reiser4_node_locked(right)) {
				right->flags |= NF_HEARD_BANSHEE;
			} else {
				reiser4_node_lock(node);
			
				/* Releasing @right node from tree cache and
				   from tree structures (that is remove internal
				   nodeptr item in parent node if any). */

				if ((res = reiser4_tree_discard_node(tree, right))) {
					reiser4_node_unlock(node);
					return res;
				}

				reiser4_node_unlock(node);
			}
			
			/* Here we do not move compress point to node next to
			   @right, because @node may still have enough of space
			   to move some data to it and we prefer to do nothing
			   here. That is node data will be moved to on the next
			   cycle of this loop is still the same. */
		} else {
			/* Updating @node by @right in order to move control
			   flow to right neighbour node and so on until
			   rightmost one is reached. */
			node = right;
		}
	}
	
	return 0;
}

/* Makes tree compression to make tree more compact. It shifts items/units on
   all levels to left starting from leaf level. This may be used by fsck to make
   some additional space during reparing in some corner cases. For example, if
   some tail conversion is performed and filesystem is near to be full, we can
   run out of space and this function can releases something and in such a way
   lets fsck finish its job. */
errno_t reiser4_tree_compress(reiser4_tree_t *tree) {
	errno_t res;
	uint8_t level;
	reiser4_node_t *node;

	aal_assert("umka-3000", tree != NULL);

	if (!tree->root)
		return 0;

	node = tree->root;

	/* Loop for getting to first node of leaf level. */
	for (level = reiser4_tree_get_height(tree);
	     level >= LEAF_LEVEL; level--)
	{
		reiser4_place_t place;

		/* Are we on internal level at all? */
		if (level > LEAF_LEVEL) {
			/* Getting first nodeptr on level to get node by it and
			   in such a manner to move control flow to next level
			   of tree. */
			reiser4_place_assign(&place, node, 0, 0);

			/* Getting first node of the next level. */
			if (!(node = reiser4_tree_child_node(tree, &place))) {
				aal_error("Can't get first node on level %u.",
					  level);
				return -EINVAL;
			}
		}
	}

	/* Loop for packing a level starting from leftmost node on leaf
	   level. */
	for (level = LEAF_LEVEL;
	     level < reiser4_tree_get_height(tree); level++)
	{
		reiser4_node_t *parent = node->p.node;
		
		if ((res = reiser4_tree_compress_level(tree, node)))
			return res;
		
		if (!(node = parent))
			break;
	}

	return 0;
}

/* Saves all dirty nodes in tree to device tree lies on. */
errno_t reiser4_tree_sync(reiser4_tree_t *tree) {
	errno_t res;
	
	aal_assert("umka-2259", tree != NULL);

	if (!tree->root)
		return 0;

	/* Flushing formatted nodes starting from root node with memory pressure
	   flag set to 0, that is do not check memory presure, and save
	   everything. */
	if ((res = reiser4_tree_walk_node(tree, tree->root, 
					  cb_node_adjust,
					  NULL,
					  cb_node_unload)))
	{
		aal_error("Can't save formatted nodes to device.");
		return res;
	}

	/* Flushing unformatted blocks (extents data) attached to @tree->data
	   hash table. */
	if ((res = aal_hash_table_foreach(tree->blocks, cb_save_block, tree))) {
		aal_error("Can't save unformatted nodes to device.");
		return res;
	}
	
	return res;
}

/* Correct passed @place according to handle key collisions. */
lookup_t reiser4_tree_collision(reiser4_tree_t *tree, 
				reiser4_place_t *place,
				coll_hint_t *hint)
{
	uint32_t adjust = 0;
	uint32_t units, unit;
	lookup_t lookup = PRESENT;

	aal_assert("umka-3130", tree != NULL);
	aal_assert("vpf-1522", place != NULL);
	
	if (hint == NULL)
		return PRESENT;
	
	/* If type does not match, there is no collision found. */
	if (place->plug->p.id.group != hint->type)
		return PRESENT;

	/* Key collisions handling. Sequentional search by name. */	
	while (1) {
		units = reiser4_item_units(place);

		if (place->pos.unit != MAX_UINT32 && place->pos.unit >= units) {
			reiser4_place_t temp;
			
			/* Getting next item. */
			if ((reiser4_tree_next_place(tree, place, &temp)))
				return -EIO;
			
			/* Directory is over? */
			if (!temp.node || !plug_equal(place->plug, temp.plug)) {
				place->key.adjust = adjust;
				return ABSENT;
			}

			aal_memcpy(place, &temp, sizeof(*place));
		}
		
		unit = place->pos.unit == MAX_UINT32 ? 0 : place->pos.unit;
		
		if ((lookup = reiser4_item_collision(place, hint)) < 0)
			return lookup;
		
		adjust += place->pos.unit - unit;

		if (place->pos.unit < units)
			break;
	}
	
	place->key.adjust = adjust;
	
	return PRESENT;
}

/* Makes search of the leftmost item/unit with the same key as passed @key is
   starting from @place. This is needed to work with key collisions. */
static errno_t reiser4_tree_collision_start(reiser4_tree_t *tree,
					    reiser4_place_t *place,
					    reiser4_key_t *key)
{
        reiser4_place_t walk;
                                                                                     
        aal_assert("umka-2396", key != NULL);
        aal_assert("umka-2388", tree != NULL);
        aal_assert("umka-2389", place != NULL);
                                                                                     
        if (reiser4_place_fetch(place))
                return -EINVAL;
	
        aal_memcpy(&walk, place, sizeof(*place));
                                                                                     
        /* Main loop until leftmost node reached. */
        while (walk.node) {
                int32_t i;
                                                                                     
                /* Loop through the items of the current node */
                for (i = walk.pos.item - 1; i >= 0; i--) {
                        walk.pos.item = i;
                                                                                     
                        /* Fetching item info */
                        if (reiser4_place_fetch(&walk))
                                return -EINVAL;
                                                                                     
                        /* If items of different objects, get out here. */
                        if (reiser4_key_compshort(&walk.key, key))
                                return 0;
                        
			/* If the maxreal key does not match anymore, 
			   get out here. This clause is needed for the case 
			   of corruption when not directory item has a NAME 
			   minor. */
			if (walk.plug->balance->maxposs_key) {
				reiser4_key_t maxkey;

				reiser4_item_maxposs_key(&walk, &maxkey);
				
				if (reiser4_key_compfull(&maxkey, key) < 0)
					return 0;
			}
			
                        /* If item's lookup is implemented, we use it. Item key
                           comparing is used otherwise. */
                        if (walk.plug->balance->lookup) {
				lookup_hint_t lhint;

				lhint.key = key;
				
                                switch (objcall(&walk, balance->lookup, 
						&lhint, FIND_EXACT))
                                {
                                case PRESENT:
                                        aal_memcpy(place, &walk, sizeof(*place));
                                        break;
                                default:
                                        return 0;
                                }
                        } else if (!reiser4_key_compfull(&walk.key, key)) {
				aal_memcpy(place, &walk, sizeof(*place));
                        } else {
				return 0;
                        }
                }
                                                                                     
                /* Getting left neighbour node. */
                reiser4_node_lock(place->node);
                reiser4_tree_ltrt_node(tree, walk.node, DIR_LEFT);
                reiser4_node_unlock(place->node);
                                                                                     
                /* Initializing @walk by neighbour node and last item. */
                if ((walk.node = walk.node->left)) {
                        int32_t items = reiser4_node_items(walk.node);
                                                                                     
                        /* Here should be namely @items, not @items - 1, because
                           we will access @walk.item - 1 on the next cycle */
                        POS_INIT(&walk.pos, items, MAX_UINT32);
                }
        }
                                                                                     
        return 0;
}
#endif

#define restore_and_exit(res) \
	do {hint->key = saved; return res;} while (0)

/* Makes search in the tree by specified @key. Fills passed place by data of
   found item. That is body pointer, plugin, etc. */
lookup_t reiser4_tree_lookup(reiser4_tree_t *tree, lookup_hint_t *hint,
			     lookup_bias_t bias, reiser4_place_t *place)
{
	lookup_t res;
	reiser4_key_t *saved;
	reiser4_key_t wanted;

	aal_assert("umka-742", hint != NULL);
	aal_assert("umka-1760", tree != NULL);
	aal_assert("umka-2057", place != NULL);
	aal_assert("umka-3088", hint->key != NULL);

	/* We store @key in @wanted. All consequence code will use @wan. This is
	   needed, because @key might point to @place->key in @place and will be
	   corrupted during lookup. */
	aal_memcpy(&wanted, hint->key, sizeof(wanted));

	/* Setting hint->key to stored local key in order to keep not corrupted
	   if it points to @place->key and will be chnaged after @place is
	   modified. It will be restored after lookup is finished. */
	saved = hint->key;
	hint->key = &wanted;

	/* Zeroing place just of rcase it was not initialized before to prevent
	   having some garbage in it. */
	aal_memset(place, 0, sizeof(*place));

#ifndef ENABLE_MINIMAL
	/* Making sure that root exists. If not, getting out with @place
	   initialized by NULL root. */
	if (reiser4_tree_fresh(tree)) {
		reiser4_place_assign(place, NULL, 0, MAX_UINT32);
		return ABSENT;
	} else {
#endif
		if ((res = reiser4_tree_load_root(tree)) < 0)
			return res;
		
		reiser4_place_assign(place, tree->root, 0, MAX_UINT32);
#ifndef ENABLE_MINIMAL
	}
#endif

	while (1) {
		uint32_t clevel;
		lookup_bias_t cbias;
		blk_t blk;

		clevel = reiser4_node_get_level(place->node);
		cbias = (clevel > hint->level ? FIND_EXACT : bias);
		
		/* Looking up for key inside node. Result of lookuping will be
		   stored in &place->pos. */
		res = reiser4_node_lookup(place->node, hint,
					  cbias, &place->pos);

		/* Check if we should finish lookup because we reach stop level
		   or some error occurred during last node lookup. */
		if (clevel <= hint->level || res < 0) {
			if (res == PRESENT) {
#ifndef ENABLE_MINIMAL
				if (reiser4_tree_collision_start(tree, place,
								 &wanted))
				{
					restore_and_exit(-EIO);
				}

				/* Handle collisions. */
				if (hint->collision) {
					tree_entity_t *t;
					
					t = (tree_entity_t *)tree;
					
					res = hint->collision(t, place, 
							      hint->hint);
					
					if (res < 0)
						restore_and_exit(res);
				}
#endif
				/* Fetching item at @place if key is found */
				if (reiser4_place_fetch(place))
					restore_and_exit(-EIO);

			}
			
			restore_and_exit(res);
		}

		/* Initializing @place. This should be done before using any
		   item methods or access @place fields. */
		if (!reiser4_place_valid(place))
			restore_and_exit(ABSENT);
		
		if (reiser4_place_fetch(place))
			restore_and_exit(-EIO);
		
		/* Checking is item at @place is nodeptr one. If not, we correct
		   posision back. */
		if (!reiser4_item_branch(place->plug))
			restore_and_exit(res);
		
		/* Loading node by its nodeptr item at @place. */
		blk = reiser4_item_down_link(place);
		if (!(place->node = reiser4_tree_load_node(tree, place->node,
							   blk)))
		{
			aal_error("Can't load child node %llu.", blk);
			restore_and_exit(-EIO);
		}

		/* Zero the plug pointer. */
		place->plug = NULL;
	}
	
	restore_and_exit(ABSENT);
}

#ifndef ENABLE_MINIMAL
/* Returns 1 if passed @tree has minimal possible height and thus cannot be
   dried out. Othersize 0 is returned. */
bool_t reiser4_tree_minimal(reiser4_tree_t *tree) {
	return (reiser4_tree_get_height(tree) <= 2);
}

/* Returns 1 if root node contain one item, that is, tree is singular and should
   be dried out. Otherwise 0 is returned. */
bool_t reiser4_tree_singular(reiser4_tree_t *tree) {
	return (reiser4_node_items(tree->root) == 1);
}

/* Returns 1 if tree has not root node and 0 otherwise. Tree has not root just
   after format instance is created and tree is initialized on fs with it. And
   thus tree has not any nodes in it. */
bool_t reiser4_tree_fresh(reiser4_tree_t *tree) {
	aal_assert("umka-1930", tree != NULL);
	return (reiser4_tree_get_root(tree) == INVAL_BLK);
}

/* Updates key at passed @place by passed @key by means of using
   node_update_key() function in recursive maner. This function is used for
   update all internal left delimiting keys after balancing on underlying
   levels. */
errno_t reiser4_tree_update_keys(reiser4_tree_t *tree,
				 reiser4_place_t *place,
				 reiser4_key_t *key)
{
	errno_t res;
	reiser4_key_t pkey;
	
	aal_assert("umka-1892", tree != NULL);
	aal_assert("umka-1893", place != NULL);
	aal_assert("umka-1894", key != NULL);

	/* Small improvement. Do not update keys if this is not really
	   needed. */
	reiser4_item_get_key(place, &pkey);

	if (!reiser4_key_compfull(&pkey, key))
		return 0;
	
	aal_memcpy(&place->key, key, sizeof(*key));

	/* Check if we should update keys on higher levels of tree. */
	if (reiser4_place_leftmost(place) && place->node->p.node) {
		reiser4_place_t *parent = &place->node->p;
		
		if ((res = reiser4_tree_update_keys(tree, parent, key)))
			return res;
	}

	/* Actual key updating in @place->node. */
	return reiser4_node_update_key(place->node, &place->pos, key);
}

/* Returns 1 for attached node and 0 otherwise. */
bool_t reiser4_tree_attached_node(reiser4_tree_t *tree,
				  reiser4_node_t *node)
{
	aal_assert("umka-3128", tree != NULL);
	aal_assert("umka-3129", node != NULL);

	if (reiser4_tree_fresh(tree))
		return 0;

	if (reiser4_tree_root_node(tree, node))
		return 1;

	return (node->p.node != NULL && 
		(reiser4_tree_t *)node->tree == tree);
}

/* This function inserts new nodeptr item to the tree and in such way attaches
   passed @node to tree. It also connects passed @node into tree cache. */
errno_t reiser4_tree_attach_node(reiser4_tree_t *tree, reiser4_node_t *node,
				 reiser4_place_t *place, uint32_t flags)
{
	errno_t res;
	uint8_t level;
	ptr_hint_t ptr;
	trans_hint_t hint;

	aal_assert("umka-913", tree != NULL);
	aal_assert("umka-916", node != NULL);
	aal_assert("umka-3104", place != NULL);

	aal_memset(&hint, 0, sizeof(hint));
	
	hint.count = 1;
	hint.specific = &ptr;
	hint.shift_flags = flags;
	hint.plug = (reiser4_item_plug_t *)tree->ent.tset[TSET_NODEPTR];

	ptr.width = 1;
	ptr.start = node->block->nr;
	
	level = reiser4_node_get_level(node) + 1;
	reiser4_node_leftmost_key(node, &hint.offset);

	/* Inserting node ptr into tree. */
	if ((res = reiser4_tree_insert(tree, place, &hint, level)) < 0) {
		aal_error("Can't insert nodeptr item to the tree.");
		return res;
	}

	/* Connecting node to tree cache. */
	if ((res = reiser4_tree_connect_node(tree, place->node, node))) {
		aal_error("Can't connect node %llu to tree cache.",
			  node->block->nr);
		return res;
	}

	/* This is needed to update sibling pointers, as new attached node may
	   be inserted between two nodes, that has established sibling links and
	   they should be changed. */
	reiser4_tree_ltrt_node(tree, node, DIR_LEFT);
	reiser4_tree_ltrt_node(tree, node, DIR_RIGHT);
	
	return 0;
}

/* Removes passed @node from the on-disk tree and cache structures. That is
   removes nodeptr item from the tree and node instance itself from its parent
   children list. */
errno_t reiser4_tree_detach_node(reiser4_tree_t *tree,
				 reiser4_node_t *node,
				 uint32_t flags)
{
	errno_t res;
	reiser4_place_t parent;
	
	aal_assert("umka-1726", tree != NULL);
	aal_assert("umka-1727", node != NULL);

	/* Save parent pos, because it will be needed later and it is destroed
	   by tree_disconnect_node(). */
	parent = node->p;

	/* Disconnecting @node from tree. This should be done before removing
	   nodeptr item in parent, as parent may get empty and we will unable to
	   release it as it is locked by connect @node. */
	if ((res = reiser4_tree_disconnect_node(tree, node))) {
		aal_error("Can't disconnect node %llu "
			  "from tree during its detaching.",
			  node->block->nr);
		return res;
	}
	
        /* Disconnecting node from parent node if any. */
	if (!reiser4_tree_root_node(tree, node)) {
		trans_hint_t hint;
		
		hint.count = 1;
		hint.place_func = NULL;
		hint.region_func = NULL;
		hint.shift_flags = flags;

		/* Removing nodeptr item/unit at @parent. */
		return reiser4_tree_remove(tree, &parent, &hint);
	} else {
		/* Putting INVAL_BLK into root block number in super block to
		   let know that old root is detached. */
		reiser4_tree_set_root(tree, INVAL_BLK);
	}
	
	return 0;
}

/* This function forces tree to grow by one level and sets it up after the
   growing. This occurs when after next balancing root node needs to accept new
   nodeptr item, but has not free space enough.  */
errno_t reiser4_tree_growup(reiser4_tree_t *tree) {
	errno_t res;
	uint32_t height;
	reiser4_place_t aplace;
	reiser4_node_t *new_root;
	reiser4_node_t *old_root;

	aal_assert("umka-1701", tree != NULL);
	aal_assert("umka-1736", tree->root != NULL);
	
	height = reiser4_tree_get_height(tree) + 1;

	/* Allocating new root node. */
	if (!(new_root = reiser4_tree_alloc_node(tree, height)))
		return -ENOSPC;

	if ((res = reiser4_tree_load_root(tree)))
		goto error_free_new_root;
	
	old_root = tree->root;

	/* Detaching old root from tree first. */
	if ((res = reiser4_tree_detach_node(tree, old_root,
					    SF_DEFAULT)))
	{
		aal_error("Can't detach old root node %llu from "
			  "tree during tree growing up.",
			  old_root->block->nr);
		goto error_return_root;
	}
	
	/* Assign new root node, changing tree height and root node blk in
	   format used in fs instance tree belongs to. */
	if ((res = reiser4_tree_assign_root(tree, new_root))) {
		aal_error("Can't assign new root node "
			  "durring tree growing up.");
		goto error_free_new_root;
	}

	/* Attaching old root to tree back. Now it should be attached to new
	   root node, not to virtual super block. */
	reiser4_node_lock(new_root);

	/* Place old root will be attached. */
	reiser4_place_assign(&aplace, new_root, 0, MAX_UINT32);

	if ((res = reiser4_tree_attach_node(tree, old_root,
					    &aplace, SF_DEFAULT)))
	{
		aal_error("Can't attach node %llu to tree during"
			  "tree growing up.", old_root->block->nr);
		reiser4_node_unlock(new_root);
		goto error_return_root;
	}

	reiser4_node_unlock(new_root);

	return 0;

error_return_root:
	reiser4_tree_assign_root(tree, old_root);
error_free_new_root:
	reiser4_tree_release_node(tree, new_root);
	return res;
}

/* Decreases tree height by one level. This occurs when tree gets singular (root
   has one nodeptr item) after one of removals. */
errno_t reiser4_tree_dryout(reiser4_tree_t *tree) {
	errno_t res;
	reiser4_place_t place;
	reiser4_node_t *new_root;
	reiser4_node_t *old_root;

	aal_assert("umka-1731", tree != NULL);
	aal_assert("umka-1737", tree->root != NULL);

	if (reiser4_tree_minimal(tree))
		return -EINVAL;

	/* Rasing up the root node if it exists. */
	if ((res = reiser4_tree_load_root(tree)))
		return res;

	old_root = tree->root;
	
	/* Getting new root as the first child of the old root node. */
	reiser4_place_assign(&place, old_root, 0, 0);

	if (!(new_root = reiser4_tree_child_node(tree, &place))) {
		aal_error("Can't load new root during "
			  "drying tree out.");
		return -EINVAL;
	}

	/* Detaching new root from its parent (old_root). This will also release
	   parent node from tree, as it will be empty. */
	if ((res = reiser4_tree_detach_node(tree, new_root,
					    SF_DEFAULT)))
	{
		aal_error("Can't detach new root from "
			  "tree during tree drying out.");
		return res;
	}

	/* Assign new root node. Setting tree height to new root level and root
	   block number to new root block number. */
	if ((res = reiser4_tree_assign_root(tree, new_root))) {
		aal_error("Can't assign new root node "
			  "durring tree drying out.");
		return res;
	}

	return 0;
}

/* Tries to shift items and units from @place to passed @neig node. After that
   it's finished, place will contain new insert point, which may be used for
   inserting item/unit to it. */
errno_t reiser4_tree_shift(reiser4_tree_t *tree, reiser4_place_t *place,
			   reiser4_node_t *neig, uint32_t flags)
{
	errno_t res;
	reiser4_node_t *node;
	reiser4_node_t *right;

	shift_hint_t hint;
	reiser4_key_t lkey;
	
	uint8_t start;

	aal_assert("umka-1225", tree != NULL);
	aal_assert("umka-1226", place != NULL);
	aal_assert("umka-1227", neig != NULL);
    
	aal_memset(&hint, 0, sizeof(hint));

	/* Prepares shift hint. Initializing shift flags (shift direction, is it
	   allowed to create new nodes, etc) and insert point. */
	node = place->node;
	hint.control = flags;
	hint.pos = place->pos;

	/* Needed for the left shift. */
	start = reiser4_node_items(neig);
	start = start ? start - 1 : 0;
	
	/* Perform node shift from @node to @neig. */
	if ((res = reiser4_node_shift(node, neig, &hint)))
		return res;

	/* Check if insert point was moved to neighbour node. If so, assign
	   neightbour node to insert point coord. */
	if (hint.result & SF_MOVE_POINT)
		place->node = neig;

	/* Updating @place->pos by hint->pos if there is permission flag. */
	if (hint.control & SF_UPDATE_POINT)
		place->pos = hint.pos;

	right = (hint.control & SF_ALLOW_LEFT) ? node : neig;

	/* Check if we need update key in insert part of tree. That is if source
	   node is not empty and there was actually moved at least one item or
	   unit. */
	if (reiser4_node_items(right) > 0 && hint.update) {
		/* Check if node is connected to tree or it is not root and
		   updating left delimiting keys if it makes sense at all. */
		if (right->p.node != NULL) {
			reiser4_place_t parent;

			/* Getting leftmost key from @right. */
			reiser4_node_leftmost_key(right, &lkey);

			/* Recursive updating of all internal keys that supposed
			   to be updated. */
			aal_memcpy(&parent, &right->p, sizeof(parent));
				
			if ((res = reiser4_tree_update_keys(tree, &parent, &lkey)))
				return res;
		}
	}

	/* Updating @node and @neig children's parent position. */
	if (reiser4_node_get_level(node) > LEAF_LEVEL) {
		reiser4_node_t *left = (hint.control & SF_ALLOW_LEFT) ? neig : NULL;

		if (left && reiser4_node_items(left) > 0) {
			if ((res = reiser4_tree_update_node(tree, left, start, 
							    reiser4_node_items(left))))
			{
				return res;
			}
		}
	
		if (reiser4_node_items(right) > 0) {
			if ((res = reiser4_tree_update_node(tree, right, 0, 
							    reiser4_node_items(right))))
			{
				return res;
			}
		}
	}
	
	return 0;
}

/* Shifts data from passed @place to one of neighbour nodes basing on passed
   @flags. */
static errno_t tree_shift_todir(reiser4_tree_t *tree, reiser4_place_t *place,
				uint32_t flags, int direction)
{
	errno_t res;
	uint32_t shift_flags = 0;
	reiser4_node_t *neighbour;
	reiser4_node_t *old_node;

	if (direction == DIR_LEFT && (SF_ALLOW_LEFT & flags))
		shift_flags = SF_ALLOW_LEFT;

	if (direction == DIR_RIGHT && (SF_ALLOW_RIGHT & flags))
		shift_flags = SF_ALLOW_RIGHT;

	/* Setting up shift flags. */
	shift_flags |= SF_UPDATE_POINT;

	if (SF_ALLOW_MERGE & flags)
		shift_flags |= SF_ALLOW_MERGE;
	
	if (SF_HOLD_POS & flags)
		shift_flags |= SF_HOLD_POS;

	old_node = place->node;

	/* Getting neighbour. */
	neighbour = direction == DIR_LEFT ?
		place->node->left : place->node->right;

	aal_assert("umka-3096", neighbour != NULL);
	
	/* Shift items from @place to @left neighbour. */
	if ((res = reiser4_tree_shift(tree, place, neighbour, shift_flags)))
		return res;

	if (reiser4_node_items(old_node) == 0 &&
	    old_node != place->node)
	{
		if (reiser4_node_locked(old_node)) {
			old_node->flags |= NF_HEARD_BANSHEE;
		} else {
			reiser4_node_lock(place->node);
			
			if ((res = reiser4_tree_discard_node(tree, old_node))) {
				reiser4_node_unlock(place->node);
				return res;
			}

			reiser4_node_unlock(place->node);
		}
	}

	return 0;
}

/* This calculates if space in passed @needed is enough for passed @needed. */
static inline int32_t tree_calc_space(reiser4_place_t *place, 
				      uint32_t ioverh) 
{
	uint32_t overh = reiser4_node_overhead(place->node);
	
	return reiser4_node_space(place->node) - 
		(place->pos.unit == MAX_UINT32 ? (overh + ioverh) : 0);
}

/* Makes space in tree to insert @ilen bytes of data. Returns space in insert
   point, or negative value for errors. */
int32_t reiser4_tree_expand(reiser4_tree_t *tree, reiser4_place_t *place,
			    reiser4_place_t *parent, uint32_t ilen, 
			    uint32_t ioverh, uint32_t flags)
{
	int alloc;
	errno_t res;
	uint8_t level;
	int32_t enough;

	aal_assert("umka-929", tree != NULL);
	aal_assert("umka-766", place != NULL);
	
	aal_assert("umka-3064", place->node != NULL);
	aal_assert("vpf-1543", ilen + ioverh <= 
		   reiser4_node_maxspace(place->node));

	if (reiser4_tree_fresh(tree))
		return -EINVAL;

	/* Check if there is enough space in insert point node. If so -- do
	   nothing but exit. Here is also check if node is empty. Then we exit
	   too and return available space in it. */
	if ((enough = tree_calc_space(place, ioverh)) >= (int32_t)ilen)
		return enough;

	/* Shifting data into left neighbour if it exists and left shift
	   allowing flag is specified. */
	if ((SF_ALLOW_LEFT & flags) && 
	    reiser4_tree_ltrt_node(tree, place->node, DIR_LEFT)) 
	{
		if ((res = tree_shift_todir(tree, place, flags, DIR_LEFT)))
			return res;
		
		if ((enough = tree_calc_space(place, ioverh)) >= (int32_t)ilen)
			return enough;
	}

	/* Shifting data into right neighbour if it exists and right shift
	   allowing flag is specified. */
	if ((SF_ALLOW_RIGHT & flags) && 
	    reiser4_tree_ltrt_node(tree, place->node, DIR_RIGHT)) 
	{
		if ((res = tree_shift_todir(tree, place, flags, DIR_RIGHT)))
			return res;
		
		if ((enough = tree_calc_space(place, ioverh)) >= (int32_t)ilen)
			return enough;
	}

	/* Check if we allowed to allocate new nodes if there still not enough
	   of space for insert @ilen bytes of data to tree. */
	if (!(SF_ALLOW_ALLOC & flags))
		return tree_calc_space(place, ioverh);
	
	/* Here we still have not enough free space for inserting item/unit into
	   the tree. Allocating new nodes and trying to shift data into
	   them. There are possible two tries to allocate new node and shift
	   insert point to one of them. */
	for (alloc = 0; enough < (int32_t)ilen && alloc < 2; alloc++) {
		reiser4_place_t save;
		reiser4_node_t *node;
		uint32_t shift_flags;
		uint32_t left_items;
		reiser4_place_t aplace;

		/* Saving place as it will be usefull for us later */
		aal_memcpy(&save, place, sizeof(*place));

		/* Allocating new node of @level */
		level = reiser4_node_get_level(place->node);

		if (!(node = reiser4_tree_alloc_node(tree, level)))
			return -ENOSPC;

		/* reiser4_place_leftmost cannot be used here because unit == 
		   0 mean we should stay on this item -- writing to the extent
		   with holes or inserting a SD extention. */
		if (place->pos.unit == MAX_UINT32 && place->pos.item == 0) {
			/* Do not shift anything for the leftmost position.
			   Just insert the new node and move the insert point
			   there. */
			aal_memcpy(&aplace, &place->node->p, sizeof(aplace));
			place->node = node;
			place->pos.item = 0;
			place->pos.unit = MAX_UINT32;
		} else {
			/* Setting up shift flags. */
			shift_flags = (SF_ALLOW_RIGHT | SF_UPDATE_POINT);

			if (SF_ALLOW_MERGE & flags)
				shift_flags |= SF_ALLOW_MERGE;

			if (SF_HOLD_POS & flags)
				shift_flags |= SF_HOLD_POS;
			
			/* We will allow to move insert point to neighbour node 
			   if we at first iteration in this loop or if place 
			   points behind the last unit of last item in current 
			   node. */
			if (alloc > 0 || reiser4_place_rightmost(place))
				shift_flags |= SF_MOVE_POINT;

			/* Shift data from @place to @node. Updating @place by 
			   new insert point. */
			if ((res = reiser4_tree_shift(tree, place, node, 
						      shift_flags)))
			{
				return res;
			}

			/* Preparing new @node parent place. */
			tree_next_child_pos(save.node, &aplace);
		}
		
		left_items = reiser4_node_items(save.node);
		
		if (left_items == 0) {
			reiser4_node_lock(save.node);

			/* If evth was moved to the new allocated node, it will
			   be attached below. Do not pack the tree here, avoid
			   2 balancings. */
			shift_flags = SF_DEFAULT & ~SF_ALLOW_PACK;
			
			if ((res = reiser4_tree_detach_node(tree, save.node,
							    shift_flags)))
			{
				reiser4_node_unlock(save.node);
				return res;
			}

			reiser4_node_unlock(save.node);
			aplace.pos.item--;
		}
		
		if (reiser4_node_items(node) > 0) {
			reiser4_node_lock(save.node);
			
			/* Growing tree in the case we splitted the root
			   node. */
			if (reiser4_tree_root_node(tree, save.node)) {
				reiser4_node_t *old_root = tree->root;
				
				if ((res = reiser4_tree_growup(tree))) {
					reiser4_node_unlock(save.node);
					return res;
				}

				tree_next_child_pos(old_root, &aplace);
			}

			/* Attach new node to tree if it is not empty. */
			if ((res = reiser4_tree_attach_node(tree, node,
							    &aplace,
							    SF_DEFAULT)))
			{
				reiser4_node_unlock(save.node);
				return res;
			}

			reiser4_node_unlock(save.node);
			/* Update the parent to the just attached node's parent.
			   Needed as @save.node may be detached already. */
			aal_memcpy(parent, &node->p, sizeof(*parent));
		} else {
			/* As node was not attached here, it will be attached in
			   caller function, so we needd to update @parent by
			   coord of attach. */

			aal_memcpy(parent, &aplace, sizeof(aplace));
		}
		
		/* If we are still in the same node, but on not-esistent 
		   anymore item, set unit position to MAX_UINT32. */
		if (save.node == place->node && place->pos.item >= left_items) {
			aal_assert("vpf-1793", place->pos.unit == MAX_UINT32);
		}
	
		/* Checking if it is enough of space in @place. */
		enough = tree_calc_space(place, ioverh);

		if (enough < (int32_t)ilen && save.node != place->node) {
			aal_bug("vpf-1796", "When the insertion point is "
				"moved to the new node, it is the second "
				"node allocation and there must be enother "
				"space.");
		}

		/* Releasing new allocated @node if it is empty and insert point
		   is not inside it. */
		if (reiser4_node_items(node) == 0 && node != place->node)
			reiser4_tree_release_node(tree, node);
		
		/* Releasing save.@node if it is empty and insert point is not
		   inside it. */
		if (left_items == 0 && save.node != place->node) {
			aal_bug("vpf-1795", "The insert point cannot "
				"move to the new node with all items.");
		}
	}

	return enough;
}

/* Packs node in @place by means of using shift to left. */
errno_t reiser4_tree_shrink(reiser4_tree_t *tree, reiser4_place_t *place) {
	errno_t res;
	uint32_t flags;
	reiser4_node_t *left, *right;

	aal_assert("umka-1784", tree != NULL);
	aal_assert("umka-1783", place != NULL);

	/* Shift flags to be used in packing. */
	flags = (SF_ALLOW_LEFT | SF_ALLOW_MERGE);
	
	/* Packing node in order to keep the tree in well packed state
	   anyway. Here we will shift data from the target node to its left
	   neighbour node. */
	if ((left = reiser4_tree_ltrt_node(tree, place->node, DIR_LEFT))) {
		if ((res = reiser4_tree_shift(tree, place, left, flags))) {
			aal_error("Can't pack node %llu into left.",
				  place_blknr(place));
			return res;
		}
	}
		
	if (reiser4_node_items(place->node) > 0) {
		/* Shifting the data from the right neigbour node into the
		   target node. */
		if ((right = reiser4_tree_ltrt_node(tree, place->node,
						    DIR_RIGHT)))
		{
			reiser4_place_t bogus;

			reiser4_place_assign(&bogus, right, 0, MAX_UINT32);
	    
			if ((res = reiser4_tree_shift(tree, &bogus,
						      place->node, flags)))
			{
				aal_error("Can't pack node %llu into right.",
					  right->block->nr);
				return res;
			}

			/* Check if @bogus.node got empty. If so -- it will be
			   released from tree and tree cache. */
			if (reiser4_node_items(bogus.node) == 0) {
				if (reiser4_node_locked(bogus.node)) {
					bogus.node->flags |= NF_HEARD_BANSHEE;
				}else {
					reiser4_tree_discard_node(tree, bogus.node);
				}
			}
		}
	} else {
		/* Release node, because it got empty. */
		if (reiser4_node_locked(place->node)) {
			place->node->flags |= NF_HEARD_BANSHEE;
		} else {
			if ((res = reiser4_tree_discard_node(tree, place->node)))
				return res;
				
			place->node = NULL;
		}
	}

	/* Drying tree up in the case root node has only one item. */
	if (tree->root && reiser4_tree_singular(tree) &&
	    !reiser4_tree_minimal(tree))
	{
		if ((res = reiser4_tree_dryout(tree)))
			return res;
	}

	return 0;
}

/* Splits out the tree from passed @place up until passed @level is
   reached. This is used in fsck and in extents write code. */
static errno_t reiser4_tree_split(reiser4_tree_t *tree, 
				  reiser4_place_t *place,
				  uint8_t level)
{
	errno_t res;
	uint32_t flags;
	uint32_t clevel;
	reiser4_node_t *node;
	
	aal_assert("vpf-674", level > 0);
	aal_assert("vpf-672", tree != NULL);
	aal_assert("vpf-673", place != NULL);
	aal_assert("vpf-813", place->node != NULL);

	clevel = reiser4_node_get_level(place->node);
	aal_assert("vpf-680", clevel < level);

	/* Loop until desired @level is reached.*/
	while (clevel < level) {
		aal_assert("vpf-676", place->node->p.node != NULL);

		/* Check if @place points inside node. That is should we split
		   node or not. */
		if (!reiser4_place_leftmost(place) &&
		    !reiser4_place_rightmost(place))
		{
			reiser4_place_t aplace;
			
			/* We are not on the border, split @place->node. That is
			   allocate new right neighbour node and move all item
			   right to @place->pos to new allocated node. */
			if (!(node = reiser4_tree_alloc_node(tree, clevel))) {
				aal_error("Tree failed to allocate "
					  "a new node.");
				return -EINVAL;
			}

			/* Flags allowed to be used during shift. */
			flags = (SF_ALLOW_RIGHT | SF_UPDATE_POINT |
				 SF_ALLOW_MERGE);

			/* Perform shift from @place->node to @node. */
			if ((res = reiser4_tree_shift(tree, place, node,
						      flags)))
			{
				aal_error("Tree failed to shift into a "
					  "new allocated node.");
				goto error_free_node;
			}

			aal_assert("umka-3048", reiser4_node_items(node) > 0);
			
			reiser4_node_lock(place->node);

			/* Check if we should grow up the tree. */
			if (reiser4_tree_root_node(tree, place->node)) {
				reiser4_node_t *old_root = tree->root;
				
				if ((res = reiser4_tree_growup(tree))) {
					reiser4_node_unlock(place->node);
					goto error_free_node;
				}

				tree_next_child_pos(old_root, &aplace);
			} else
				tree_next_child_pos(place->node, &aplace);

			/* Attach new node to tree. */
			if ((res = reiser4_tree_attach_node(tree, node,
							    &aplace,
							    SF_DEFAULT)))
			{
				aal_error("Tree is failed to attach "
					  "node during split opeartion.");
				reiser4_node_unlock(place->node);
				goto error_free_node;
			}
			
			reiser4_node_unlock(place->node);

			/* Updating @place by parent coord from @place. */
			aal_memcpy(place, &node->p, sizeof(*place));
		} else {
			int rightmost = reiser4_place_rightmost(place);
			
			/* There is nothing to move out. We are on node border
			   (rightmost or leftmost). Here we should just go up by
			   one level and increment position if @place was at
			   rightmost position in the node. */
			aal_memcpy(place, &place->node->p, sizeof(*place));

			if (rightmost) {
				bool_t whole;
				
				whole = (place->pos.unit == MAX_UINT32);
				reiser4_place_inc(place, whole);
			}
		}

		clevel++;
	}
	
	return 0;
	
 error_free_node:
	reiser4_tree_release_node(tree, node);
	return res;
}

/* This grows tree until requested level reached. */
static inline errno_t tree_growup_level(reiser4_tree_t *tree,
					uint8_t level)
{
	if (level <= reiser4_tree_get_height(tree))
		return 0;
	
	if (reiser4_tree_fresh(tree))
		return -EINVAL;
		
	while (level > reiser4_tree_get_height(tree)) {
		errno_t res;
		
		if ((res = reiser4_tree_growup(tree)))
			return res;
	}
		
	return 0;
}

/* This function prepares the tree for insert. That is it allocated root and
   first leaf if needed, splits the tree, etc. */
static inline errno_t tree_prep_modify(reiser4_tree_t *tree,
				       reiser4_place_t *parent,
				       reiser4_place_t *place,
				       uint8_t level)
{
	reiser4_node_t *root;
	uint32_t height;
	errno_t res;

	/* Check if tree has at least one node. If so -- load root node. Tree
	   has not nodes just after it is created. Root node and first leaf will
	   be created on demand then. */
	if (!reiser4_tree_fresh(tree)) {
		aal_assert("umka-3124", place->node != NULL);
		
		reiser4_node_lock(place->node);
		
		if ((res = reiser4_tree_load_root(tree))) {
			reiser4_node_unlock(place->node);
			return res;
		}

		reiser4_node_unlock(place->node);
	}

	/* Checking if we have the tree with height less than requested
	   level. If so, we should grow the tree up to requested level. */
	if (level > reiser4_tree_get_height(tree)) {
		reiser4_node_lock(place->node);

		if ((res = tree_growup_level(tree, level))) {
			reiser4_node_unlock(place->node);
			return res;
		}

		reiser4_node_unlock(place->node);
	}

	if (!reiser4_tree_fresh(tree)) {
		if (level < reiser4_node_get_level(place->node)) {
			aal_memcpy(parent, place, sizeof(*place));
			
			/* Allocating node of requested level and assign place
			   for insert to it. This is the case, when we insert a
			   tail among extents. That is previous lookup stoped on
			   twig level and now we have to allocate a node of
			   requested level, insert tail to it and then attach
			   new node to tree. */
			if (!(place->node = reiser4_tree_alloc_node(tree, level)))
				return -ENOSPC;

			POS_INIT(&place->pos, 0, MAX_UINT32);
		} else {
			if (level > reiser4_node_get_level(place->node)) {
				/* Prepare the tree for insertion at the @level. 
				   Here is case when extent is going to inserted.
				   As lookup goes to the leaf level, we split 
				   tree from leaf level up to requested @level,
				   which is level new extent should be inserted. */
				if ((res = reiser4_tree_split(tree, place, level)))
					return res;
			}

			/* Here we do not need to update @parent, as there is no
			   new not attached nodes. */
		}
	} else {
		aal_assert("umka-3055", place->node == NULL);
		
		/* Preparing tree for insert first item (allocating root,
		   etc). */
		height = reiser4_tree_get_height(tree);

		if (!(root = reiser4_tree_alloc_node(tree, height)))
			return -ENOSPC;

		if ((res = reiser4_tree_assign_root(tree, root)))
			return res;

		if (level == reiser4_node_get_level(root)) {
			place->node = root;
		} else {
			if (!(place->node = reiser4_tree_alloc_node(tree, level)))
				return -ENOMEM;
		}
	
		POS_INIT(&place->pos, 0, MAX_UINT32);
		reiser4_place_init(parent, root, &place->pos);
	}
	
	return 0;
}

static inline errno_t tree_post_modify(reiser4_tree_t *tree, 
				       reiser4_place_t *place) 
{
	uint32_t items;
	errno_t res;

	/* Nothing to be done if no item is left in the node. */
	if (!(items = reiser4_node_items(place->node)))
		return 0;

	/* Initializing insert point place. */
	if ((res = reiser4_place_fetch(place)))
		return res;

	/* Parent keys will be updated if we inserted item/unit into leftmost
	   pos and if target node has parent. */
	if (reiser4_place_leftmost(place) && place->node->p.node) {
		/* We will not update keys if node is not attached to tree
		   yet. This will be done later on its attach. */
		reiser4_place_t *parent = &place->node->p;
		reiser4_key_t lkey;

		reiser4_item_get_key(place, &lkey);

		if ((res = reiser4_tree_update_keys(tree, parent, &lkey)))
			return res;
	}

	/* Update @place->node children's parent pointers. */
	if (reiser4_node_get_level(place->node) > LEAF_LEVEL) {
		if ((res = reiser4_tree_update_node(tree, place->node, 
						    place->pos.item, items)))
		{
			return res;
		}
	}
	
	return 0;
}

/* Main function for tree modifications. It is used for inserting data to tree
   (stat data items, directries) or writting (tails, extents). */
int64_t reiser4_tree_modify(reiser4_tree_t *tree, reiser4_place_t *place,
			    trans_hint_t *hint, uint8_t level,
			    estimate_func_t estimate_func,
			    modify_func_t modify_func)
{
	errno_t res;
	int32_t space;
	int32_t write;
	reiser4_place_t parent;

	aal_assert("umka-2673", tree != NULL);
	aal_assert("umka-2674", hint != NULL);
	aal_assert("umka-1645", hint->plug != NULL);
	aal_assert("umka-1644", place != NULL);
	
	aal_assert("umka-2676", modify_func != NULL);
	aal_assert("umka-2675", estimate_func != NULL);

	/* Preparing the tree modification. This may include splitting, 
	   allocating root, etc. */
	if ((res = tree_prep_modify(tree, &parent, place, level)))
		return res;

	/* Estimating item/unit to inserted/written to tree. */
	if ((res = estimate_func(place, hint)))
		return res;
	
	/* Preparing space in tree. */
	space = reiser4_tree_expand(tree, place, &parent, hint->len, 
				    hint->overhead, hint->shift_flags);
	
	/* Needed space is the length of the item + an item overhead on 
	   the item creation if needed. */
	if (place->pos.unit != MAX_UINT32)
		hint->overhead = 0;

	if (space < 0) {
		aal_error("Can't prepare space in tree.");
		return space;
	} else {
		/* Checking if we still have less space than needed. This is
		   ENOSPC case if we tried to insert data. */
		if (space < hint->len)
			return -ENOSPC;
	}

	reiser4_node_lock(place->node);
	
	/* Inserting/writing data to node. */
	if ((write = modify_func(place->node, &place->pos, hint)) < 0) {
		aal_error("Can't insert data to node %llu.",
			  place_blknr(place));
		return write;
	}
	
	reiser4_node_unlock(place->node);

	if ((res = tree_post_modify(tree, place)))
		return res;
	
        /* If make space function allocates new node, we should attach it to the
	   tree. Also, here we should handle the special case, when tree root
	   should be changed. */
	if (place->node != tree->root && !place->node->p.node) {
		/* Check if insert was on root node level. If so -- growing tree
		   up by one. */
		if (level == reiser4_tree_get_height(tree)) {
			reiser4_node_t *old_root = tree->root;
			
			reiser4_node_lock(place->node);

			if ((res = reiser4_tree_growup(tree))) {
				aal_error("Can't grow tree up during "
					  "modifying it.");
				reiser4_node_unlock(place->node);
				return res;
			}
			
			reiser4_node_unlock(place->node);

			/* Handling tree growth case. */
			tree_next_child_pos(old_root, &parent);
		}
		
		/* Attaching new node to the tree. */
		if ((res = reiser4_tree_attach_node(tree, place->node, &parent,
						    hint->shift_flags)))
		{
			aal_error("Can't attach node %llu to tree.",
				  place_blknr(place));
			return res;
		}
	}

	/* Calling @hint->place_func if a new item was created. */
	if (hint->place_func && place->pos.unit == MAX_UINT32) {
		if ((res = hint->place_func(place, hint->data)))
			return res;
	}

	return write;
}

/* Estimates how many bytes is needed to insert data described by @hint. */
static errno_t cb_tree_prep_insert(reiser4_place_t *place, trans_hint_t *hint) {
	aal_assert("umka-2440", hint != NULL);
	aal_assert("umka-2439", place != NULL);

	hint->len = 0;
	hint->overhead = 0;

	return plugcall(hint->plug->object, prep_insert, place, hint);
}

static errno_t cb_tree_insert(reiser4_node_t *node, pos_t *pos, 
			      trans_hint_t *hint) 
{
	return objcall(node, insert, pos, hint);
}

/* Inserts data to the tree. This function is used for inserting items which are
   not file body items, that is statdata, directory, etc. */
int64_t reiser4_tree_insert(reiser4_tree_t *tree, reiser4_place_t *place,
			    trans_hint_t *hint, uint8_t level)
{
	return reiser4_tree_modify(tree, place, hint, level, 
				   cb_tree_prep_insert, cb_tree_insert);
}

/* Estimates how many bytes is needed to write data described by @hint. */
static errno_t cb_tree_prep_write(reiser4_place_t *place, trans_hint_t *hint) {
	aal_assert("umka-3007", hint != NULL);
	aal_assert("umka-3008", place != NULL);

	hint->len = 0;
	hint->overhead = 0;

	return plugcall(hint->plug->object, prep_write, place, hint);
}

static errno_t cb_tree_write(reiser4_node_t *node, 
			     pos_t *pos, trans_hint_t *hint) 
{
	return objcall(node, write, pos, hint);
}

int64_t reiser4_tree_write(reiser4_tree_t *tree, reiser4_place_t *place,
			   trans_hint_t *hint, uint8_t level)
{
	return reiser4_tree_modify(tree, place, hint, level,
				   cb_tree_prep_write, cb_tree_write);
}

/* Removes item/unit at @place. */
errno_t reiser4_tree_remove(reiser4_tree_t *tree, 
			    reiser4_place_t *place,
			    trans_hint_t *hint)
{
	errno_t res;

	aal_assert("umka-2055", tree != NULL);
	aal_assert("umka-2056", place != NULL);
	aal_assert("umka-2392", hint != NULL);

	/* Removing iten/unit from the node. */
	if ((res = reiser4_node_remove(place->node, &place->pos, hint)))
		return res;

	if ((res = tree_post_modify(tree, place)))
		return res;
	
	/* Checking if the node became empty. If so, we release it, otherwise we
	   pack the tree about it. */
	if (reiser4_node_items(place->node) == 0) {
		if (reiser4_node_locked(place->node)) {
			place->node->flags |= NF_HEARD_BANSHEE;
		} else {
			if ((res = reiser4_tree_discard_node(tree, place->node)))
				return res;

			place->node = NULL;
		}
	} else {
		if (hint->shift_flags & SF_ALLOW_PACK) {
			if ((res = reiser4_tree_shrink(tree, place)))
				return res;
		}
	}

	/* Drying tree up in the case root node exists and tree is singular,
	   that is has only one item. Tree also should not be minimal of
	   height. Here root may be NULL due to nested call from
	   tree_dryout(). */
	if (tree->root && reiser4_tree_singular(tree) && 
	    !reiser4_tree_minimal(tree) && (hint->shift_flags & SF_ALLOW_PACK))
	{
		if ((res = reiser4_tree_dryout(tree)))
			return res;
	}
	
	return 0;
}

/* Traverses @node with passed callback functions as actions. */
errno_t reiser4_tree_trav_node(reiser4_tree_t *tree,
			       reiser4_node_t *node,
			       tree_open_func_t open_func,
			       node_func_t before_func,
			       place_func_t update_func,
			       node_func_t after_func,
			       void *data)
{
	errno_t res = 0;
	reiser4_place_t place;
	pos_t *pos = &place.pos;
 
	aal_assert("vpf-390", node != NULL);
	aal_assert("umka-1935", tree != NULL);
	
	if (open_func == NULL)
		open_func = (tree_open_func_t)reiser4_tree_child_node;

	/* Locking @node to make sure, that it will not be released while we are
	   working with it. Of course, it should be unlocked after we
	   finished. */
	reiser4_node_lock(node);

	if ((before_func && (res = before_func(node, data))))
		goto error_unlock_node;

	/* The loop though the items of current node */
	for (pos->item = 0; pos->item < reiser4_node_items(node);
	     pos->item++)
	{
		pos->unit = MAX_UINT32; 

		/* If there is a suspicion of a corruption, it must be checked
		   in before_func. All items must be opened here. */
		if (reiser4_place_open(&place, node, pos)) {
			aal_error("Node (%llu), item (%u): Can't open item "
				  "by place.", node->block->nr, pos->item);
			goto error_after_func;
		}

		if (!reiser4_item_branch(place.plug))
			continue;

		/* The loop though the units of the current item */
		for (pos->unit = 0; pos->unit < reiser4_item_units(&place);
		     pos->unit++)
		{
			reiser4_node_t *child = NULL;
			
			/* Opening the node by its pointer kept in @place */
			if ((child = open_func(tree, &place, data)) == INVAL_PTR)
				goto error_after_func;

			if (!child)
				goto update;

			/* Traversing the node */
			if ((res = reiser4_tree_trav_node(tree, child, open_func, 
							  before_func, update_func, 
							  after_func, data)) < 0)
			{
				goto error_after_func;
			}
			
		update:	
			if (update_func && (res = update_func(&place, data)))
				goto error_after_func;
		}
	}
	
	if (after_func)
		res = after_func(node, data);

	reiser4_tree_unlock_node(tree, node);
	return res;

 error_after_func:
	if (after_func)
		after_func(node, data);

 error_unlock_node:
	reiser4_tree_unlock_node(tree, node);
	return res;
}

/* Traverses tree with passed callback functions for each event. This is used
   for all tree traverse related operations like copy, measurements, etc. */
errno_t reiser4_tree_trav(reiser4_tree_t *tree,
			  tree_open_func_t open_func,
			  node_func_t before_func,
			  place_func_t update_func,
			  node_func_t after_func,
			  void *data)
{
	errno_t res;
	
	aal_assert("umka-1768", tree != NULL);

	if ((res = reiser4_tree_load_root(tree)))
		return res;
	
	return reiser4_tree_trav_node(tree, tree->root, open_func,
				      before_func, update_func,
				      after_func, data);
}

errno_t reiser4_tree_scan(reiser4_tree_t *tree, 
			  node_func_t pre_func, 
			  place_func_t func, 
			  void *data) 
{
        reiser4_key_t key, max;
        errno_t res;

        aal_assert("vpf-1423", tree != NULL);
        aal_assert("vpf-1424", func != NULL);

        if (reiser4_tree_fresh(tree))
                return -EINVAL;

        if ((res = reiser4_tree_load_root(tree)))
                return res;

        if (tree->root == NULL)
                return -EINVAL;

        /* Prepare the start and the end keys. */
        key.plug = max.plug = tree->key.plug;
        reiser4_key_minimal(&key);
        reiser4_key_maximal(&max);

        /* While not the end of the tree. */
        while (reiser4_key_compfull(&key, &max)) {
                reiser4_place_t place;
		lookup_hint_t hint;
                lookup_t lookup;
		pos_t *pos;

                /* Some items can be handled a few times due to key 
		   collisions. */
		hint.key = &key;
		hint.level = LEAF_LEVEL;
		hint.collision = NULL;

                /* Lookup the key. FIXME: it is possible to spped it up. */
                if ((lookup = reiser4_tree_lookup(tree, &hint, FIND_EXACT,
						  &place)) < 0)
		{
                        return lookup;
		}

		pos = &place.pos;

		if (pre_func) {
			if ((res = pre_func(place.node, data)) < 0)
				return res;
		
			/* If res != 0, lookup is needed. */
			if (res) continue;
		}
		
		/* Count may get change in func (e.g. after item fusing), so 
		   get it on every loop */
		while (1) {
			if (pos->item >= reiser4_node_items(place.node)) {
				/* All items are handled whithin this node, but 
				   lookup is not needed. To avoid infinite loop 
				   in the case of key collision, get the next item 
				   instead of another lookup call. */
				if ((res = reiser4_tree_next_place(tree, &place,
								   &place))) 
				{
					aal_error("Failed to get the next node.");
					return res;
				}

				if (!place.node)
					return 0;
			}
				
                        if ((res = reiser4_place_fetch(&place)))
                                return res;
			
			/* Go down to the child if branch. */
			if ((res = reiser4_item_branch(place.plug))) {
				if (!(place.node = 
				      reiser4_tree_child_node(tree, &place)))
				{
					return -EIO;
				}
		
				if (pre_func) {
					if ((res = pre_func(place.node, 
							    data)) < 0)
					{
						return res;
					}

					/* If res != 0, lookup is needed. */
					if (res) break;
				}

				place.pos.item = -1;
				continue;
			}
			
                        /* Get the key of the next item. */
                        if ((res = reiser4_tree_next_key(tree, &place, &key)))
                                return res;

                        /* Call func for the item. */
                        if ((res = func(&place, data)) < 0)
                                return res;

                        /* If res != 0, lookup is needed. */
                        if (res) break;

			pos->item++;
                }
        }

        return 0;
}

/* Makes copy of @src_tree to @dst_tree */
errno_t reiser4_tree_copy(reiser4_tree_t *src_tree,
			  reiser4_tree_t *dst_tree)
{
	aal_assert("umka-2304", src_tree != NULL);
	aal_assert("umka-2305", dst_tree != NULL);

	aal_error("Sorry, not implemented yet!");
	return -EINVAL;
}

/* Resizes @tree by @blocks */
errno_t reiser4_tree_resize(reiser4_tree_t *tree,
			    count_t blocks)
{
	aal_assert("umka-2323", tree != NULL);

	aal_error("Sorry, not implemented yet!");
	return -EINVAL;
}
#endif

/*
   Local variables:
   c-indentation-style: "K&R"
   mode-name: "LC"
   c-basic-offset: 8
   tab-width: 8
   fill-column: 80
   scroll-step: 1
   End:
*/
