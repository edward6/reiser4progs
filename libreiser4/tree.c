/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tree.c -- reiser4 tree code. */

#include <reiser4/libreiser4.h>

/* Fetches data from passed @tree to passed @hint */
int64_t reiser4_tree_fetch(reiser4_tree_t *tree,
			   reiser4_place_t *place,
			   trans_hint_t *hint)
{
	return plug_call(place->plug->o.item_ops->object,
			 fetch_units, place, hint);
}

/* Return current fs blksize, which may be used in tree. */
uint32_t reiser4_tree_get_blksize(reiser4_tree_t *tree) {
	aal_assert("umka-2579", tree != NULL);
	aal_assert("umka-2580", tree->fs != NULL);
	aal_assert("umka-2581", tree->fs->master != NULL);

	return reiser4_master_get_blksize(tree->fs->master);
}

aal_device_t *reiser4_tree_get_device(reiser4_tree_t *tree) {
	aal_assert("umka-2582", tree != NULL);
	aal_assert("umka-2583", tree->fs != NULL);

	return tree->fs->device;
}

/* Returns TRUE if passed @node is tree root node. */
static bool_t reiser4_tree_root_node(reiser4_tree_t *tree,
				     reiser4_node_t *node)
{
	aal_assert("umka-2482", tree != NULL);
	aal_assert("umka-2483", node != NULL);
	
	return reiser4_tree_get_root(tree) == node_blocknr(node);
}

#ifndef ENABLE_STAND_ALONE
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
		if (reiser4_tree_root_node(tree, node) ||
		    node->p.node != NULL)
		{
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

	return reiser4_format_get_root(tree->fs->format);
}

/* Returns tree height stored in format. */
uint8_t reiser4_tree_get_height(reiser4_tree_t *tree) {
	aal_assert("umka-2411", tree != NULL);
	aal_assert("umka-2418", tree->fs != NULL);
	aal_assert("umka-2419", tree->fs->format != NULL);

	return reiser4_format_get_height(tree->fs->format);
}

#ifndef ENABLE_STAND_ALONE
/* As @node already lies in @tree->nodes hash table and it is going to change
   its block number, we have to update its hash entry in @tree->nodes. This
   function does that job and also moves @node to @new_blk location. */
static errno_t reiser4_tree_rehash_node(reiser4_tree_t *tree,
					reiser4_node_t *node, blk_t new_blk)
{
	blk_t old_blk;
	blk_t *set_blk;

	aal_assert("umka-3043", tree != NULL);
	aal_assert("umka-3044", node != NULL);
	aal_assert("umka-3045", reiser4_node_items(node) > 0);
	
	old_blk = node_blocknr(node);
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

	*blk = node_blocknr(node);

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

	blk = node_blocknr(node);
	return aal_hash_table_remove(tree->nodes, &blk);
}

#ifndef ENABLE_STAND_ALONE
/* Acknowledles, that passed @place has nodeptr that points onto passed
   @node. This is needed for tree_realize_node() function. */
static int reiser4_tree_node_ack(reiser4_node_t *node, reiser4_place_t *place) {
	if (!(place->pos.item < reiser4_node_items(place->node)))
		return 0;
	       
	if (reiser4_place_fetch(place))
		return 0;

	if (!reiser4_item_branch(place->plug))
		return 0;

	return reiser4_item_down_link(place) == node_blocknr(node);
}
#endif

/* Updates @node->p by position in parent node. */
errno_t reiser4_tree_realize_node(reiser4_tree_t *tree, reiser4_node_t *node) {
#ifndef ENABLE_STAND_ALONE
	uint32_t i;
#endif

	lookup_hint_t hint;
        reiser4_key_t lkey;
	reiser4_place_t *parent;
    
	aal_assert("umka-869", node != NULL);
	aal_assert("umka-1941", node->p.node != NULL);
	aal_assert("umka-3038", reiser4_node_items(node) > 0);

	parent = &node->p;

	/* Checking if we are in position already */
#ifndef ENABLE_STAND_ALONE
	if (reiser4_tree_node_ack(node, parent))
		goto out_correct_place;
#endif

        reiser4_node_leftmost_key(node, &lkey);

	hint.key = &lkey;

	/* Getting position by means of using node lookup. */
        if (reiser4_node_lookup(parent->node, &hint, FIND_EXACT,
				&parent->pos) == PRESENT)
	{
#ifndef ENABLE_STAND_ALONE
		if (reiser4_tree_node_ack(node, parent))
			goto out_correct_place;
#endif
	}

	/* Getting position by means of linear traverse. */
#ifndef ENABLE_STAND_ALONE
	for (i = 0; i < reiser4_node_items(parent->node); i++) {
		blk_t blk;
		uint32_t j;
		lookup_t res;
			
		parent->pos.item = i;

		if ((res = reiser4_place_fetch(parent)))
			return res;

		if (!reiser4_item_branch(parent->plug))
			continue;

		for (j = 0; j < reiser4_item_units(parent); j++) {
			parent->pos.unit = j;
				
			blk = reiser4_item_down_link(parent);
				
			if (blk == node_blocknr(node))
				goto out_correct_place;
		}
	}

	return -EINVAL;

out_correct_place:
#endif
	if (reiser4_item_units(parent) == 1)
		parent->pos.unit = MAX_UINT32;

	return 0;
}

/* Loads root node and put it to @tree->nodes hash table. */
errno_t reiser4_tree_load_root(reiser4_tree_t *tree) {
	blk_t root_blk;
	
	aal_assert("umka-1870", tree != NULL);

	/* Check if root is loaded. */
	if (tree->root)
		return 0;

#ifndef ENABLE_STAND_ALONE
	/* Check if tree contains some nodes at all. It does not contain them
	   just after creation. The is root blk in format is set to INVAL_BLK.*/
	if (reiser4_tree_fresh(tree))
		return -EINVAL;
#endif

	/* Getting root node and loading it. */
	root_blk = reiser4_tree_get_root(tree);
	
	if (!(tree->root = reiser4_tree_load_node(tree, NULL, root_blk))) {
		aal_error("Can't load root node %llu.", root_blk);
		return -EIO;
	}

	tree->root->tree = tree;
	return 0;
}

#if 0
static errno_t callback_count_children(reiser4_place_t *place, void *data) {
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
	reiser4_node_trav(node, callback_count_children, &count);
	return count;
}
#endif

#ifndef ENABLE_STAND_ALONE
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
	node->tree = tree;
	node->p.node = NULL;

	if (reiser4_tree_connect_node(tree, NULL, node))
		return -EINVAL;
	
	/* Updating tree height. */
	level = reiser4_node_get_level(node);
	reiser4_tree_set_height(tree, level);

	/* Updating root block number. */
	blk = node_blocknr(tree->root);
	reiser4_tree_set_root(tree, blk);

	return 0;
}
#endif

/* Registers passed node in tree and connects left and right neighbour
   nodes. This function does not do any tree modifications. */
errno_t reiser4_tree_connect_node(reiser4_tree_t *tree,
				  reiser4_node_t *parent, 
				  reiser4_node_t *node)
{
	aal_assert("umka-1857", tree != NULL);
	aal_assert("umka-2261", node != NULL);

	node->tree = tree;

	if (reiser4_tree_root_node(tree, node)) {
		/* This is the case when we connect root node, that is with no
		   parent. */
		tree->root = node;
	} else if (parent) {
		/* Assigning parent, locking it asit is referenced by
		   @node->p.node and updating @node->p.pos. */
		node->p.node = parent;

		if (reiser4_tree_realize_node(tree, node))
			return -EINVAL;

		reiser4_node_lock(parent);
	}
	
	if (reiser4_tree_hash_node(tree, node))
		return -EINVAL;

	/* Check for memory pressure event. If memory pressure is uppon us, we
	   call memory cleaning function. For now we call tree_adjust() in order
	   to release not locked nodes. */
	if (tree->mpc_func && tree->mpc_func(tree)) {
		/* Adjusting the tree as memory pressure is here. */
		reiser4_node_lock(node);
		
		if (reiser4_tree_adjust(tree)) {
			aal_error("Can't adjust tree during connect "
				  "node %llu.", node_blocknr(node));
			reiser4_node_unlock(node);
			if (parent) {
				reiser4_node_unlock(parent);
			}
			
			reiser4_tree_unhash_node(tree, node);
			return -EINVAL;
		}
		
		reiser4_node_unlock(node);
	}

	return 0;
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

#ifndef ENABLE_STAND_ALONE
/* Updates all internal node loaded children positions in parent. */
static errno_t reiser4_tree_update_node(reiser4_tree_t *tree,
					reiser4_node_t *node)
{
	uint32_t i;
	errno_t res;

	aal_assert("umka-3035", tree != NULL);
	aal_assert("umka-3036", node != NULL);
	aal_assert("umka-3037", reiser4_node_items(node) > 0);
	
	for (i = 0; i < reiser4_node_items(node); i++) {
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
			
			/* Update @child parent. */
			child->p.node = node;

			if (reiser4_node_items(node) == 0) {
				if (node->flags & NF_HEARD_BANSHEE)
					continue;

				aal_bug("umka-3060", "Node %llu is empty but "
					"not marked as 'heard banshee'.",
					node_blocknr(node));
			}

			/* Updating position in parent node. */
			if ((res = reiser4_tree_realize_node(tree, child)))
				return res;
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
		if (!(node = reiser4_node_open(tree, blk))) {
			aal_error("Can't open node %llu.", blk);
			return NULL;
		}

		/* Connect loaded node to cache. */
		if (reiser4_tree_connect_node(tree, parent, node)) {
			aal_error("Can't connect node %llu "
				  "to tree cache.", node_blocknr(node));
			goto error_free_node;
		}
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

#ifndef ENABLE_STAND_ALONE
	/* Check if node is dirty. */
	if (reiser4_node_isdirty(node)) {
		aal_warn("Unloading dirty node %llu.",
			 node_blocknr(node));
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
	return reiser4_tree_load_node(tree, place->node, blk);
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
	   because they are not accessible in stand alone mode and we do not
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

/* Gets left or right neighbour nodes. */
reiser4_node_t *reiser4_tree_neig_node(reiser4_tree_t *tree,
				       reiser4_node_t *node, 
				       uint32_t where)
{
	aal_assert("umka-2219", node != NULL);
	aal_assert("umka-1859", tree != NULL);

	/* Parent is not present. This is root node -- no neighbors.  */
	if (!node->p.node)
		return NULL;

	if (where == DIR_LEFT && node->left)
		return node->left;

	if (where == DIR_RIGHT && node->right)
		return node->right;

	return reiser4_tree_ltrt_node(tree, node, where);
}

static errno_t reiser4_tree_adjust_place(reiser4_tree_t *tree, 
					 reiser4_place_t *place, 
					 reiser4_place_t *next) 
{
	/* Check if we have to get right neighbour node. */
	if (place->pos.item >= reiser4_node_items(place->node)) {
		/* Load the right neighbour. */
		reiser4_tree_neig_node(tree, place->node, DIR_RIGHT);

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
	
	if (reiser4_tree_adjust_place(tree, next, next))
		return -EINVAL;

	if (!next->node) 
		return 0;
	
	node = next->node;
	reiser4_node_lock(node);
	
	/* If nodeptr item go down. */
	while (reiser4_item_branch(next->plug)) {
		if (!(next->node = reiser4_tree_child_node(tree, next)))
			goto error;

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
	
	reiser4_item_get_key(&next, key);

	return 0;
}

#ifndef ENABLE_STAND_ALONE
/* Requests block allocator for new block and creates empty node in it. */
reiser4_node_t *reiser4_tree_alloc_node(reiser4_tree_t *tree,
					uint8_t level)
{
	blk_t blk;
	rid_t pid;
	
	reiser4_node_t *node;
	uint32_t stamp;
	
	uint64_t free_blocks;
	reiser4_format_t *format;
    
	aal_assert("umka-756", tree != NULL);
    
	/* Allocating fake block number. */
	blk = reiser4_fake_get();
	format = tree->fs->format;
	pid = reiser4_param_value("node");

	/* Setting up of the free blocks in format. */
	if (!(free_blocks = reiser4_format_get_free(format)))
		return NULL;

	reiser4_format_set_free(format, free_blocks - 1);

	/* Creating new node. */
	if (!(node = reiser4_node_create(tree, blk, pid, level))) {
		aal_error("Can't initialize new fake node.");
		return NULL;
	}

	/* Setting flush stamps to new node. */
	stamp = reiser4_format_get_stamp(format);
	reiser4_node_set_mstamp(node, stamp);

	return node;
}

/* Unload node and releasing it in block allocator */
errno_t reiser4_tree_release_node(reiser4_tree_t *tree,
				  reiser4_node_t *node)
{
	uint64_t free_blocks;
	reiser4_alloc_t *alloc;
	reiser4_format_t *format;
	
	aal_assert("umka-1841", tree != NULL);
	aal_assert("umka-2255", node != NULL);

	alloc = tree->fs->alloc;
	format = tree->fs->format;
	reiser4_node_mkclean(node);

	/* Check if we're trying to releas a node with fake block number. If
	   not, free it in block allocator too. */
	if (!reiser4_fake_ack(node_blocknr(node))) {
		blk_t blk = node_blocknr(node);
		reiser4_alloc_release(alloc, blk, 1);
	}

	/* Setting up of the free blocks in format. */
	free_blocks = reiser4_format_get_free(format);
	reiser4_format_set_free(format, free_blocks + 1);

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
		aal_error("Can't detach node %llu from "
			  "tree.", node_blocknr(node));
		return res;
	}
	
	if ((res = reiser4_tree_release_node(tree, node))) {
		aal_error("Can't release node %llu.",
			  node_blocknr(node));
		return res;
	}

	return res;
}

/* Helper function for freeing passed key instance tree's data hashtable entry
   is going to be removed. */
static void callback_blocks_keyrem_func(void *key) {
	reiser4_key_free((reiser4_key_t *)key);
}

/* Helper function for freeing hash value, that is, data block. */
static void callback_blocks_valrem_func(void *val) {
	aal_block_free((aal_block_t *)val);
}

/* Helper function for calculating 64-bit hash by passed key. This is used for
   tree's data hash. */
static uint64_t callback_blocks_hash_func(void *key) {
	return (reiser4_key_get_objectid((reiser4_key_t *)key) +
		reiser4_key_get_offset((reiser4_key_t *)key));
}

/* Helper function for comparing two keys during tree's data hash lookups. */
static int callback_blocks_comp_func(void *key1, void *key2,
				     void *data)
{
	return reiser4_key_compfull((reiser4_key_t *)key1,
				    (reiser4_key_t *)key2);
}
#endif

/* Helpher function for freeing keys in @tree->nodes hash table during its
   destroying. */
static void callback_nodes_keyrem_func(void *key) {
	aal_free(key);
}

/* Return hash number from passed key value from @tree->nodes hashtable. */
static uint64_t callback_nodes_hash_func(void *key) {
	return *(uint64_t *)key;
}

/* Compares two passed keys of @tree->nodes hash table during lookup in it. */
static int callback_nodes_comp_func(void *key1, void *key2,
				    void *data)
{
	if (*(uint64_t *)key1 < *(uint64_t *)key2)
		return -1;

	if (*(uint64_t *)key1 > *(uint64_t *)key2)
		return 1;

	return 0;
}

/* Builds root key and stores it in passed @tree instance. */
static errno_t reiser4_tree_key(reiser4_tree_t *tree) {
	rid_t pid;
    
	aal_assert("umka-1090", tree != NULL);
	aal_assert("umka-1091", tree->fs != NULL);
	aal_assert("umka-1092", tree->fs->oid != NULL);

	pid = reiser4_format_key_pid(tree->fs->format);

	/* Finding needed key plugin by its identifier. */
	if (!(tree->key.plug = reiser4_factory_ifind(KEY_PLUG_TYPE,
						     pid)))
	{
		aal_error("Can't find key plugin by its "
			  "id 0x%x.", pid);
		return -EINVAL;
	}

	return reiser4_fs_root_key(tree->fs, &tree->key);
}

/* Returns level in tree particular item should be inserted at. */
inline uint32_t reiser4_tree_target_level(reiser4_tree_t *tree,
					  reiser4_plug_t *plug)
{
	return (plug->id.group == TAIL_ITEM) ?
		LEAF_LEVEL : TWIG_LEVEL;
}

#define TREE_NODES_TABLE_SIZE (512)
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
	tree->fs->tree = tree;

#ifndef ENABLE_STAND_ALONE
	tree->bottom = TWIG_LEVEL;
#endif
	
	/* Initializing hash table for storing loaded formatted nodes in it. */
	if (!(tree->nodes = aal_hash_table_create(TREE_NODES_TABLE_SIZE,
						  callback_nodes_hash_func,
						  callback_nodes_comp_func,
						  callback_nodes_keyrem_func,
						  NULL)))
	{
		goto error_free_tree;
	}

#ifndef ENABLE_STAND_ALONE
	/* Initializing hash table for storing loaded unformatted blocks in
	   it. This uses all callbacks we described above for getting hash
	   values, lookup, etc. */
	if (!(tree->blocks = aal_hash_table_create(TREE_BLOCKS_TABLE_SIZE,
						   callback_blocks_hash_func,
						   callback_blocks_comp_func,
						   callback_blocks_keyrem_func,
						   callback_blocks_valrem_func)))
	{
		goto error_free_nodes;
	}
#endif

	/* Building tree root key. It is used in tree lookup, etc. */
	if (reiser4_tree_key(tree)) {
		aal_error("Can't build the tree root key.");
		goto error_free_data;
	}
    
	return tree;

 error_free_data:
#ifndef ENABLE_STAND_ALONE
	aal_hash_table_free(tree->blocks);
error_free_nodes:
#endif
	aal_hash_table_free(tree->nodes);
 error_free_tree:
	aal_free(tree);
	return NULL;
}

/* Closes specified tree. */
void reiser4_tree_fini(reiser4_tree_t *tree) {
	aal_assert("umka-134", tree != NULL);

	/* Allocates everything is needed to allocated and saves dirty nodes to
	   device. Unloads saved nodes from tree. */
#ifndef ENABLE_STAND_ALONE
	reiser4_tree_sync(tree);
#endif

	/* Releasing all loaded formatted nodes and tree itself. */
	reiser4_tree_close(tree);
}

/* Unloads all loaded tree nodes. */
errno_t reiser4_tree_collapse(reiser4_tree_t *tree) {
        aal_assert("umka-2265", tree != NULL);
                                                                                          
        if (!tree->root)
                return 0;
                                                                                          
        return reiser4_tree_walk_node(tree, tree->root,
                                      reiser4_tree_unload_node);
}

/* Closes specified tree without saving dirty nodes to device. It just thows out
   all loaded nodes without dealing with allocating etc. This may be used in
   stand alone mode and/or just to free modified tree without a changes on
   device. */
void reiser4_tree_close(reiser4_tree_t *tree) {
	aal_assert("vpf-1316", tree != NULL);

	/* Close all remaining nodes. */
	reiser4_tree_collapse(tree);

	/* Releasing unformatted nodes hash table. */
#ifndef ENABLE_STAND_ALONE
	aal_hash_table_free(tree->blocks);
#endif

	/* Releasing fomatted nodes hash table. */
	aal_hash_table_free(tree->nodes);

	/* Freeing tree instance. */
	tree->fs->tree = NULL;
	aal_free(tree);
}

#ifndef ENABLE_STAND_ALONE
/* Allocates nodeptr item at passed @place. */
static errno_t reiser4_tree_alloc_nodeptr(reiser4_tree_t *tree,
					  reiser4_place_t *place)
{
	uint32_t units;
	reiser4_node_t *node;

	units = reiser4_item_units(place);

	for (place->pos.unit = 0; place->pos.unit < units;
	     place->pos.unit++)
	{
		blk_t blk = reiser4_item_down_link(place);

		if (!reiser4_fake_ack(blk))
			continue;

		/* Checking for loaded node. If it is, then we move it new
		   allocated node blk. Though it is possible to have not
		   allocated nodeptr item as its node is not yet in hash table
		   of formatted nodes. That is because it is in process
		   yet. This is possible if tree_attach_node() causes yet
		   another tree_attach_node() on higher levels. */
		if (!(node = reiser4_tree_lookup_node(tree, blk)))
			continue;
		
		/* If @child is fake one it needs to be allocated here and its
		   nodeptr should be updated. */
		if (!reiser4_alloc_allocate(tree->fs->alloc, &blk, 1))
			return -ENOSPC;

		if (reiser4_item_update_link(place, blk))
			return -EIO;

		/* Rehashing node in @tree->nodes hash table. */
		reiser4_tree_rehash_node(tree, node, blk);
	}

	return 0;
}

static errno_t callback_flags_dup(reiser4_place_t *place, void *data) {
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
	hint.place_func = callback_flags_dup;
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


		if (plug_call(place->plug->o.item_ops->object,
			      fetch_units, place, &hint) != 1)
		{
			return -EIO;
		}

		/* Check if we have accessed unallocated extent. */
		if (ptr.start != EXTENT_UNALLOC_UNIT)
			continue;

		/* Getting unit key. */
		plug_call(place->plug->o.item_ops->balance,
			  fetch_key, place, &key);

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
				if (plug_call(place->plug->o.item_ops->object,
					      update_units, place, &hint) != 1)
				{
					return -EIO;
				}

				first_time = 0;
			} else {
				reiser4_place_t iplace;
				uint32_t level;
				errno_t res;

				iplace = *place;
				iplace.pos.unit++;

				/* Insert new extent units. */
				reiser4_key_assign(&hint.offset, &key);
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
				offset = plug_call(key.plug->o.key_ops,
						   get_offset, &key);

				plug_call(key.plug->o.key_ops, set_offset,
					  &key, offset + blksize);
			}
			
			blocks += ptr.width;
		}
	}

	return 0;
}
#endif

/* Entry point for adjsuting tree routines. */
errno_t reiser4_tree_adjust(reiser4_tree_t *tree) {
	aal_assert("umka-3034", tree != NULL);
	
	if (tree->root && !tree->adjusting) {
		errno_t res = 0;
		
		tree->adjusting = 1;

		/* Check for special case -- tree_adjust() is calling during
		   tree_growup(), when empty root is connected. */
		if (reiser4_node_items(tree->root))
			res = reiser4_tree_adjust_node(tree, tree->root);
		
		tree->adjusting = 0;
		
		return res;
	}

	return 0;
}

#ifndef ENABLE_STAND_ALONE
/* Runs through the node in @place and calls tree_adjust_node() for all
   children. */
static errno_t callback_tree_adjust(reiser4_place_t *place, void *data) {
	blk_t blk;
	uint32_t j;
	errno_t res;
	reiser4_tree_t *tree;
	reiser4_node_t *child;

	/* It is not good, that we reference here to particular item group. But,
	   we have to do so, considering, that this is up to tree to know about
	   items type in it. Probably this is why tree should be plugin too to
	   handle things like this in more flexible manner. */

	tree = (reiser4_tree_t *)place->node->tree;

	if (place->plug->id.group == EXTENT_ITEM) {
		/* Allocating unallocated extent item at @place. */
		if ((res = reiser4_tree_alloc_extent(tree, place)))
			return res;
	}
	
	/* Extents are handled above, nodeptrs below, nothing else needs to 
	   be handled. */
	if (!reiser4_item_branch(place->plug))
		return 0;

	/* Allocating unallocated nodeptr item at @place. */
	if ((res = reiser4_tree_alloc_nodeptr(tree, place)))
		return res;

	for (j = 0; j < reiser4_item_units(place); j++) {
		/* Getting child node by its nodeptr. If child is loaded, 
		   we call tree_adjust_node() on it recursively in order 
		   to allocate it and its items. */
		place->pos.unit = j;

		blk = reiser4_item_down_link(place);

		if (!(child = reiser4_tree_lookup_node(tree, blk)))
			continue;

		if ((res = reiser4_tree_adjust_node(tree, child)))
			return res;
	}

	return 0;
}
#endif

/* Flushes some part of tree cache (recursively) to device starting from passed
   @node. This function is used for allocating part of tree and flusing it to
   device on memory pressure event or on tree_sync() call. */
errno_t reiser4_tree_adjust_node(reiser4_tree_t *tree, reiser4_node_t *node) {
#ifndef ENABLE_STAND_ALONE
	errno_t res;
	count_t free_blocks;
#endif
	
	aal_assert("umka-2302", tree != NULL);
	aal_assert("umka-2303", node != NULL);
	aal_assert("umka-3075", reiser4_node_items(node) > 0);

#ifndef ENABLE_STAND_ALONE
	/* Requesting block allocator to allocate the real block number
	   for fake allocated node. */
	if (reiser4_fake_ack(node_blocknr(node))) {
		blk_t allocnr;
		
		if (!reiser4_alloc_allocate(tree->fs->alloc,
					    &allocnr, 1))
		{
			return -ENOSPC;
		}

		if (reiser4_tree_root_node(tree, node))
			reiser4_tree_set_root(tree, allocnr);
		
		/* Rehashing node in @tree->nodes hash table. */
		reiser4_tree_rehash_node(tree, node, allocnr);
		
		if (!reiser4_tree_root_node(tree, node)) {
			if ((res = reiser4_node_update_ptr(node)))
				return res;
		}
	}

	/* Allocating all children nodes if we are up on @tree->bottom. */
	if (reiser4_node_get_level(node) >= tree->bottom) {
		reiser4_node_lock(node);
		res = reiser4_node_trav(node, callback_tree_adjust, NULL);
		reiser4_node_unlock(node);
		
		if (res) return res;
	}

	/* Updating free space counter in format. */
	free_blocks = reiser4_alloc_free(tree->fs->alloc);
	reiser4_format_set_free(tree->fs->format, free_blocks);
#endif

	/* If node is locked, that is not a leaf or it is used by someone, it
	   cannot be released, and thus, it does not make the sense to save it
	   to device too. */
	if (reiser4_node_locked(node))
		return 0;
	
#ifndef ENABLE_STAND_ALONE
	/* Okay, node is fully allocated now and ready to be saved to device if
	   it is dirty. */
	if (reiser4_node_isdirty(node) && reiser4_node_sync(node)) {
		aal_error("Can't write node %llu.", node_blocknr(node));
		return -EIO;
	}
#endif
	/* Unloading node from tree cache. */
	return reiser4_tree_unload_node(tree, node);
}

/* Walking though the tree cache and closing all nodes. */
errno_t reiser4_tree_walk_node(reiser4_tree_t *tree, reiser4_node_t *node,
			       walk_func_t walk_func)
{
	uint32_t i;
	errno_t res;
	
	aal_assert("umka-1933", tree != NULL);
	aal_assert("umka-1934", node != NULL);
	aal_assert("umka-2264", walk_func != NULL);

	for (i = 0; i < reiser4_node_items(node); i++) {
		blk_t blk;
		uint32_t j;

		reiser4_node_t *child;
		reiser4_place_t place;

		/* Initializing item at @i. */
		reiser4_place_assign(&place, node,
				     i, MAX_UINT32);

		if ((res = reiser4_place_fetch(&place)))
			return res;

		if (!reiser4_item_branch(place.plug))
			continue;

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
							  walk_func)))
			{
				return res;
			}
		}
	}
	
	/* Calling @walk_func for @node. */
	return walk_func(tree, node);
}

#ifndef ENABLE_STAND_ALONE
/* Helper function for save one unformatted block to device. Used from
   tree_sync() to save all in-memory unfromatted blocks. */
static errno_t callback_save_block( void *entry, void *data) {
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
	while ((right = reiser4_tree_neig_node(tree, node, DIR_RIGHT))) {
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
				  node_blocknr(right));
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
	if ((res = reiser4_tree_adjust_node(tree, tree->root))) {
		aal_error("Can't save formatted nodes to device.");
		return res;
	}

	/* Flushing unformatted blocks (extents data) attached to @tree->data
	   hash table. */
	if ((res = aal_hash_table_foreach(tree->blocks,
					  callback_save_block, tree)))
	{
		aal_error("Can't save unformatted nodes to device.");
		return res;
	}
	
	return res;
}
#endif

/* Correct passed @place according to handle key collisions. */
lookup_t reiser4_collision_handler(reiser4_place_t *place,
				   lookup_hint_t *hint,
				   lookup_bias_t bias,
				   lookup_t lookup)
{
#ifndef ENABLE_STAND_ALONE
	char *name;
	entry_hint_t entry;
	trans_hint_t trans;
	reiser4_tree_t *tree;
#endif

	aal_assert("vpf-1523", hint != NULL);
	aal_assert("vpf-1522", place != NULL);
	aal_assert("vpf-1524", hint->data != NULL);
	
	if (lookup != PRESENT)
		return lookup;
	
#ifndef ENABLE_STAND_ALONE
	if (place->pos.unit == MAX_UINT32)
		place->pos.unit = 0;

	/* Only direntry items have collisions. */
	if (place->plug->id.group != DIRENTRY_ITEM)
		return PRESENT;

	/* Key collisions handling. Sequentional search by name. */
	trans.count = 1;
	trans.specific = &entry;
	trans.place_func = NULL;
	trans.region_func = NULL;
	trans.shift_flags = SF_DEFAULT;
	
	name = hint->data;
	place->key.adjust = 0;
	tree = (reiser4_tree_t *)place->node->tree;
	
	while (1) {
		int32_t comp;
		uint32_t units;
		reiser4_place_t temp;

		/* Check if item is over. */
		units = plug_call(place->plug->o.item_ops->balance,
				  units, place);

		if (place->pos.unit >= units) {
			/* Getting next item. */
			if ((reiser4_tree_next_place(tree, place, &temp)))
				return -EIO;
			
			/* Directory is over? */
			if (!temp.node || !plug_equal(place->plug, temp.plug))
				return ABSENT;

			aal_memcpy(place, &temp, sizeof(*place));
		}

		/* Fetching current unit (entry) into @temp entry hint.*/
		aal_memcpy(&entry.place, place, sizeof(*place));

		if (reiser4_tree_fetch(tree, place, &trans) < 0)
			return -EIO;

		/* Checking if we found needed name. */
		comp = aal_strcmp(entry.name, name);

		/* If current name is less then we need, we increase collisions
		   counter -- @adjust. */
		if (comp < 0) {
			place->pos.unit++;
			place->key.adjust++;
		} else if (comp > 0) {
			/* The current name is greather then the given one. */
			return ABSENT;
		} else {
			/* We have found entry we need. */
			break;
		}
	}
#endif
	return PRESENT;
}

#ifndef ENABLE_STAND_ALONE
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
                                                                                     
                        /* If item's lookup is implemented, we use it. Item key
                           comparing is used otherwise. */
                        if (walk.plug->o.item_ops->balance->lookup) {
				lookup_hint_t lhint;

				lhint.key = key;
				
                                switch (plug_call(walk.plug->o.item_ops->balance,
                                                  lookup, &walk, &lhint, FIND_EXACT))
                                {
                                case PRESENT:
                                        aal_memcpy(place, &walk,
                                                   sizeof(*place));
                                        break;
                                default:
                                        return 0;
                                }
                        } else {
                                if (!reiser4_key_compfull(&walk.key, key)) {
                                        aal_memcpy(place, &walk, sizeof(*place));
                                } else {
                                        return 0;
				}
                        }
                }
                                                                                     
                /* Getting left neighbour node. */
                reiser4_node_lock(place->node);
                reiser4_tree_neig_node(tree, walk.node, DIR_LEFT);
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

#define restore_and_exit(dst, src, res)		\
	do {dst = src; return res;} while (0)

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
	reiser4_key_assign(&wanted, hint->key);

	/* Setting hint->key to stored local key in order to keep not corrupted
	   if it points to @place->key and will be chnaged after @place is
	   modified. It will be restored after lookup is finished. */
	saved = hint->key;
	hint->key = &wanted;

	/* Zeroing place just of rcase it was not initialized before to prevent
	   having some garbage in it. */
	aal_memset(place, 0, sizeof(*place));

#ifndef ENABLE_STAND_ALONE
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
#ifndef ENABLE_STAND_ALONE
	}
#endif

	/* Checking the case when wanted key is smaller than root one. This is
	   the case, when somebody is trying go up of the root by ".." entry in
	   root directory. If so, we initialize the key to be looked up by root
	   key. */
	if (reiser4_key_compfull(&wanted, &tree->key) < 0)
		reiser4_key_assign(&wanted, &tree->key);
		    
	while (1) {
		uint32_t clevel;
		lookup_bias_t cbias;

		clevel = reiser4_node_get_level(place->node);
		cbias = (clevel > hint->level ? FIND_EXACT : bias);
		
		/* Looking up for key inside node. Result of lookuping will be
		   stored in &place->pos. */
		res = reiser4_node_lookup(place->node, hint,
					  cbias, &place->pos);

		/* Check if we should finish lookup because we reach stop level
		   or some error occured during last node lookup. */
		if (clevel <= hint->level || res < 0) {
			if (res == PRESENT) {
#ifndef ENABLE_STAND_ALONE
				if (reiser4_tree_collision_start(tree, place,
								 &wanted))
				{
					restore_and_exit(hint->key, saved, -EIO);
				}
#endif
				/* Fetching item at @place if key is found */
				if (reiser4_place_fetch(place))
					restore_and_exit(hint->key, saved, -EIO);
			}
			
			goto correct;
		}

		/* Initializing @place. This should be done before using any
		   item methods or access @place fields. */
		if (!reiser4_place_valid(place)) {
			res = ABSENT;
			goto correct;
		}
		
		if (reiser4_place_fetch(place))
			restore_and_exit(hint->key, saved, -EIO);

		/* Checking is item at @place is nodeptr one. If not, we correct
		   posision back. */
		if (!reiser4_item_branch(place->plug))
			goto correct;

		/* Loading node by its nodeptr item at @place. */
		if (!(place->node = reiser4_tree_child_node(tree, place)))
			restore_and_exit(hint->key, saved, -EIO);
	}
	
	res = ABSENT;
	
 correct:
#ifndef ENABLE_STAND_ALONE
	/* Correcting found pos if the corresponding callback is specified. */
	if (hint->correct_func)
		res = hint->correct_func(place, hint, bias, res);
#endif

	hint->key = saved;
	return res;
}

/* Reads data from the @tree from @place to passed @hint */
int64_t reiser4_tree_read(reiser4_tree_t *tree, reiser4_place_t *place,
			  trans_hint_t *hint)
{
	return plug_call(place->plug->o.item_ops->object,
			 read_units, place, hint);
}

#ifndef ENABLE_STAND_ALONE
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
errno_t reiser4_tree_update_key(reiser4_tree_t *tree, reiser4_place_t *place,
				reiser4_key_t *key)
{
	errno_t res;
	
	aal_assert("umka-1892", tree != NULL);
	aal_assert("umka-1893", place != NULL);
	aal_assert("umka-1894", key != NULL);

	/* Getting into recursion if we should update leftmost key. */
	if (reiser4_place_leftmost(place)) {
		
		if (place->node->p.node) {
			reiser4_place_t *p = &place->node->p;
			
			if ((res = reiser4_tree_update_key(tree, p, key)))
				return res;
		}
	}

	/* Update key in parent node. */
	return reiser4_node_update_key(place->node, &place->pos, key);
}

/* This function inserts new nodeptr item to the tree and in such way attaches
   passed @node to tree. It also connects passed @node into tree cache. */
errno_t reiser4_tree_attach_node(reiser4_tree_t *tree, reiser4_node_t *node,
				 reiser4_place_t *place, uint32_t flags)
{
	rid_t pid;
	errno_t res;
	uint8_t level;
	
	ptr_hint_t ptr;
	trans_hint_t thint;

	aal_assert("umka-913", tree != NULL);
	aal_assert("umka-916", node != NULL);
	aal_assert("umka-3104", place != NULL);
    
	/* Preparing nodeptr item hint. */
	thint.count = 1;
	thint.specific = &ptr;
	thint.place_func = NULL;
	thint.region_func = NULL;
	thint.shift_flags = flags;

	ptr.width = 1;
	ptr.start = node_blocknr(node);
	pid = reiser4_param_value("nodeptr");

	level = reiser4_node_get_level(node) + 1;
	reiser4_node_leftmost_key(node, &thint.offset);

	if (!(thint.plug = reiser4_factory_ifind(ITEM_PLUG_TYPE, pid))) {
		aal_error("Can't find item plugin by its id 0x%x.", pid);
		return -EINVAL;
	}

	/* Inserting node ptr into tree. */
	if ((res = reiser4_tree_insert(tree, place, &thint, level)) < 0) {
		aal_error("Can't insert nodeptr item to the tree.");
		return res;
	}

	/* Connecting node to tree cache. */
	if ((res = reiser4_tree_connect_node(tree, place->node, node))) {
		aal_error("Can't connect node %llu to tree cache.",
			  node_blocknr(node));
		return res;
	}

	/* This is needed to update sibling links, as new attached node may be
	   inserted between two nodes, that has established sibling links and
	   they should be changed. */
	reiser4_tree_neig_node(tree, node, DIR_LEFT);
	reiser4_tree_neig_node(tree, node, DIR_RIGHT);
	
	return 0;
}

/* Removes passed @node from the on-disk tree and cache structures. That is
   removes nodeptr item from the tree and node instance itself from its parent
   children list. */
errno_t reiser4_tree_detach_node(reiser4_tree_t *tree,
				 reiser4_node_t *node, uint32_t flags)
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
			  node_blocknr(node));
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
   growing. This occures when after next balancing root node needs to accept new
   nodeptr item, but has not free space enough.  */
errno_t reiser4_tree_growup(reiser4_tree_t *tree) {
	errno_t res;
	uint32_t height;
	reiser4_place_t place;
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
			  node_blocknr(old_root));
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
	reiser4_place_assign(&place, new_root, 0, MAX_UINT32);

	if ((res = reiser4_tree_attach_node(tree, old_root,
					    &place, SF_DEFAULT)))
	{
		aal_error("Can't attach node %llu to tree during"
			  "tree growing up.", node_blocknr(old_root));
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
	reiser4_node_t *update;

	shift_hint_t hint;
	reiser4_key_t lkey;

	aal_assert("umka-1225", tree != NULL);
	aal_assert("umka-1226", place != NULL);
	aal_assert("umka-1227", neig != NULL);
    
	aal_memset(&hint, 0, sizeof(hint));

	/* Prepares shift hint. Initializing shift flags (shift direction, is it
	   allowed to create new nodes, etc) and insert point. */
	node = place->node;
	hint.control = flags;
	hint.pos = place->pos;

	/* Perform node shift from @node to @neig. */
	if ((res = reiser4_node_shift(node, neig, &hint)))
		return res;

	/* Check if insert point was moved to neighbour node. If so, assign
	   neightbour node to insert point coord. */
	if (hint.control & SF_UPDATE_POINT && hint.result & SF_MOVE_POINT)
		place->node = neig;

	/* Updating @place->pos by hint->pos if there is permission flag. */
	if (hint.control & SF_UPDATE_POINT)
		place->pos = hint.pos;

	update = (hint.control & SF_ALLOW_LEFT) ? node : neig;

	/* Check if we need update key in insert part of tree. That is if source
	   node is not empty and there was actually moved at least one item or
	   unit. */
	if (reiser4_node_items(update) > 0 && hint.update) {
		/* Check if node is connected to tree or it is not root and
		   updating left delimiting keys if it makes sense at all. */
		if (update->p.node) {
			reiser4_place_t p;

			/* Getting leftmost key from @update. */
			reiser4_node_leftmost_key(update, &lkey);

			/* Recursive updating of all internal keys that supposed
			   to be updated. */
			reiser4_place_init(&p, update->p.node,
					   &update->p.pos);
				
			reiser4_key_assign(&update->p.key, &lkey);
				
			if ((res = reiser4_tree_update_key(tree, &p, &lkey)))
				return res;
		}
	}

	/* Updating @node and @neig children's parent position. */
	if (reiser4_node_get_level(node) > LEAF_LEVEL) {
		if (reiser4_node_items(node) > 0) {
			if ((res = reiser4_tree_update_node(tree, node)))
				return res;
		}
	
		if (reiser4_node_items(neig) > 0) {
			if ((res = reiser4_tree_update_node(tree, neig)))
				return res;
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
	uint32_t shift_flags;
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
		
	if (SF_MOVE_POINT & flags)
		shift_flags |= SF_MOVE_POINT;

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
static int32_t tree_calc_space(reiser4_tree_t *tree, reiser4_place_t *place,
			       uint32_t needed)
{
	int32_t enough;

	if ((enough = reiser4_node_space(place->node) - needed) >= 0) {
		enough = reiser4_node_space(place->node);
		
		if (place->pos.unit == MAX_UINT32)
			enough -= reiser4_node_overhead(place->node);
	}
	
	return enough;
}

/* Makes space in tree to insert @needed bytes of data. Returns space in insert
   point, or negative value for errors. */
int32_t reiser4_tree_expand(reiser4_tree_t *tree, reiser4_place_t *place,
			    uint32_t needed, uint32_t flags)
{
	int alloc;
	errno_t res;
	uint8_t level;
	int32_t enough;
	uint32_t overhead;

	aal_assert("umka-929", tree != NULL);
	aal_assert("umka-766", place != NULL);

	aal_assert("umka-3064", place->node != NULL);

	if (reiser4_tree_fresh(tree))
		return -EINVAL;

	overhead = reiser4_node_overhead(place->node);

	/* Adding node overhead to @needed. */
	if (place->pos.unit == MAX_UINT32)
		needed += overhead;

	/* Check if there is enough space in insert point node. If so -- do
	   nothing but exit. Here is also check if node is empty. Then we exit
	   too and return available space in it. */
	if ((enough = reiser4_node_space(place->node) - needed) >= 0 ||
	    reiser4_node_items(place->node) == 0)
	{
		enough = reiser4_node_space(place->node);
		
		if (place->pos.unit == MAX_UINT32)
			enough -= overhead;

		return enough;
	}

	/* Shifting data into left neighbour if it exists and left shift
	   allowing flag is specified. */
	if ((SF_ALLOW_LEFT & flags) && 
	    reiser4_tree_neig_node(tree, place->node, DIR_LEFT)) 
	{
		if ((res = tree_shift_todir(tree, place, flags, DIR_LEFT)))
			return res;

		if ((enough = tree_calc_space(tree, place, needed)) > 0)
			return enough;
	}

	/* Shifting data into right neighbour if it exists and right shift
	   allowing flag is specified. */
	if ((SF_ALLOW_RIGHT & flags) && 
	    reiser4_tree_neig_node(tree, place->node, DIR_RIGHT)) 
	{
		if ((res = tree_shift_todir(tree, place, flags, DIR_RIGHT)))
			return res;

		if ((enough = tree_calc_space(tree, place, needed)) > 0)
			return enough;
	}

	/* Check if we allowed to allocate new nodes if there still not enough
	   of space for insert @needed bytes of data to tree. */
	if (!(SF_ALLOW_ALLOC & flags)) {
		enough = reiser4_node_space(place->node);
		
		if (place->pos.unit == MAX_UINT32)
			enough -= overhead;

		return enough;
	}
	
	/* Here we still have not enough free space for inserting item/unit into
	   the tree. Allocating new nodes and trying to shift data into
	   them. There are possible two tries to allocate new node and shift
	   insert point to one of them. */
	for (alloc = 0; enough < 0 && alloc < 2; alloc++) {
		reiser4_place_t save;
		reiser4_node_t *node;
		uint32_t shift_flags;
		reiser4_place_t aplace;

		/* Saving place as it will be usefull for us later */
		save = *place;

		/* Allocating new node of @level */
		level = reiser4_node_get_level(place->node);

		if (!(node = reiser4_tree_alloc_node(tree, level)))
			return -ENOSPC;
		
		/* Setting up shift flags. */
		shift_flags = (SF_ALLOW_RIGHT | SF_UPDATE_POINT);

		if (SF_ALLOW_MERGE & flags)
			shift_flags |= SF_ALLOW_MERGE;
		
		/* We will allow to move insert point to neighbour node if we at
		   first iteration in this loop or if place points behind the
		   last unit of last item in current node. */
		if (alloc > 0 || reiser4_place_rightmost(place))
			shift_flags |= SF_MOVE_POINT;

		/* Shift data from @place to @node. Updating @place by new
		   insert point. */
		if ((res = reiser4_tree_shift(tree, place, node, shift_flags)))
			return res;

		/* Preparing new @node parent place. */
		reiser4_place_dup(&aplace, &save.node->p);
			
		if (reiser4_node_items(save.node) == 0) {
			reiser4_node_lock(place->node);
			
			if ((res = reiser4_tree_detach_node(tree, save.node,
							    SF_DEFAULT)))
			{
				reiser4_node_unlock(place->node);
				return res;
			}

			reiser4_node_unlock(place->node);
		} else {
			aplace.pos.item++;
		}
		
		if (reiser4_node_items(node) > 0) {
			reiser4_node_lock(place->node);
			
			/* Growing tree in the case we splitted the root
			   node. */
			if (reiser4_tree_root_node(tree, save.node)) {
				if ((res = reiser4_tree_growup(tree))) {
					reiser4_node_unlock(place->node);
					return res;
				}
				
				/* Updating new root parent after tree grow. */
				reiser4_place_dup(&aplace, &save.node->p);
				aplace.pos.item++;
			}

			reiser4_node_lock(save.node);

			/* Attach new node to tree if it is not empty. */
			if ((res = reiser4_tree_attach_node(tree, node,
							    &aplace,
							    SF_DEFAULT)))
			{
				reiser4_node_unlock(save.node);
				reiser4_node_unlock(place->node);
				return res;
			}

			reiser4_node_unlock(save.node);
			reiser4_node_unlock(place->node);
		}
		
		/* Checking if it is enough of space in @place. */
		enough = (reiser4_node_space(place->node) - needed);

		/* If it is not enough of space and insert point was actually
		   moved to neighbour node, we set @place to @save and give it
		   yet another try to make space. */
		if (enough < 0 && save.node != place->node) {
			*place = save;
			enough = (reiser4_node_space(place->node) - needed);
		}

		/* Releasing new allocated @node if it is empty and insert point
		   is not inside it. */
		if (reiser4_node_items(node) == 0 && node != place->node)
			reiser4_tree_release_node(tree, node);
		
		/* Releasing save.@node if it is empty and insert point is not
		   inside it. */
		if (reiser4_node_items(save.node) == 0 && save.node != place->node)
			reiser4_tree_release_node(tree, save.node);
	}

	/* Return value of free space in insert point node. */
	enough = reiser4_node_space(place->node);
		
	if (place->pos.unit == MAX_UINT32)
		enough -= overhead;
	
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
	if ((left = reiser4_tree_neig_node(tree, place->node, DIR_LEFT))) {
		if ((res = reiser4_tree_shift(tree, place, left, flags))) {
			aal_error("Can't pack node %llu into left.",
				  node_blocknr(place->node));
			return res;
		}
	}
		
	if (reiser4_node_items(place->node) > 0) {
		/* Shifting the data from the right neigbour node into the
		   target node. */
		if ((right = reiser4_tree_neig_node(tree, place->node,
						    DIR_RIGHT)))
		{
			reiser4_place_t bogus;

			reiser4_place_assign(&bogus, right, 0, MAX_UINT32);
	    
			if ((res = reiser4_tree_shift(tree, &bogus,
						      place->node, flags)))
			{
				aal_error("Can't pack node %llu into left.",
					  node_blocknr(right));
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
			reiser4_node_t *old_node;
			
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

			old_node = place->node;
			
			reiser4_place_dup(&aplace, &old_node->p);
			
			/* Perform shift from @place->node to @node. */
			if ((res = reiser4_tree_shift(tree, place, node,
						      flags)))
			{
				aal_error("Tree failed to shift into a "
					  "new allocated node.");
				goto error_free_node;
			}

			aal_assert("umka-3048", reiser4_node_items(node) > 0);
			
			reiser4_node_lock(old_node);

			/* Check if we should grow up the tree. */
			if (reiser4_tree_root_node(tree, place->node)) {
				if ((res = reiser4_tree_growup(tree))) {
					reiser4_node_unlock(old_node);
					goto error_free_node;
				}
				reiser4_place_dup(&aplace, &old_node->p);
			}

			aplace.pos.item++;
		
			/* Attach new node to tree. */
			if ((res = reiser4_tree_attach_node(tree, node,
							    &aplace,
							    SF_DEFAULT)))
			{
				aal_error("Tree is failed to attach "
					  "node during split opeartion.");
				reiser4_node_unlock(old_node);
				goto error_free_node;
			}
			
			reiser4_node_unlock(old_node);

			/* Updating @place by parent coord from @place. */
			reiser4_place_init(place, node->p.node, &node->p.pos);
		} else {
			int inc;
			
			node = place->node;
			inc = reiser4_place_rightmost(place);
			
			/* There is nothing to move out. We are on node border
			   (rightmost or leftmost). Here we should just go up by
			   one level and increment position if @place was at
			   rightmost position in node. */
			reiser4_place_init(place, node->p.node, &node->p.pos);

			if (inc) {
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

/* Estimates how many bytes is needed to insert data described by @hint. */
static errno_t callback_prep_insert(reiser4_place_t *place, 
				    trans_hint_t *hint) 
{
	aal_assert("umka-2440", hint != NULL);
	aal_assert("umka-2439", place != NULL);

	hint->len = 0;
	hint->overhead = 0;

	return plug_call(hint->plug->o.item_ops->object,
			 prep_insert, place, hint);
}

/* Estimates how many bytes is needed to write data described by @hint. */
static errno_t callback_prep_write(reiser4_place_t *place, 
				   trans_hint_t *hint) 
{
	aal_assert("umka-3007", hint != NULL);
	aal_assert("umka-3008", place != NULL);

	hint->len = 0;
	hint->overhead = 0;

	return plug_call(hint->plug->o.item_ops->object,
			 prep_write, place, hint);
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
static inline errno_t tree_prepare_insert(reiser4_tree_t *tree,
					  reiser4_place_t *place,
					  uint8_t level)
{
	errno_t res;
	uint32_t height;
	reiser4_node_t *root;

	if (!reiser4_tree_fresh(tree)) {
		if (level < reiser4_node_get_level(place->node)) {
			/* Allocating node of requested level and assign place
			   for insert to it. This is the case, when we insert a
			   tail among extents. That is previous lookup stoped on
			   twig level and now we have to allocate a node of
			   requested level, insert tail to it and then attach
			   new node to tree. */
			if (!(place->node = reiser4_tree_alloc_node(tree, level)))
				return -ENOSPC;

			POS_INIT(&place->pos, 0, MAX_UINT32);
		} else if (level > reiser4_node_get_level(place->node)) {
			/* Prepare the tree for insertion at the @level. Here is
			   case when extent is going to inserted. As lookup goes
			   to the leaf level, we split tree from leaf level up
			   to requested @level, which is level new extent should
			   be inserted. */
			if ((res = reiser4_tree_split(tree, place, level)))
				return res;
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
	bool_t mode;
	errno_t res;
	int32_t space;
	int32_t write;
	uint32_t needed;
	reiser4_place_t aplace;

	aal_assert("umka-2673", tree != NULL);
	aal_assert("umka-2674", hint != NULL);
	
	aal_assert("umka-2676", modify_func != NULL);
	aal_assert("umka-2675", estimate_func != NULL);

	/* Check if tree has at least one node. If so -- load root node. Tree
	   has not nodes just after it is created. Root node and first leaf will
	   be created on demand then. */
	if (!reiser4_tree_fresh(tree)) {
		if ((res = reiser4_tree_load_root(tree)))
			return res;
	}

	/* Checking if we have the tree with height less than requested
	   level. If so, we should grow the tree up to requested level. */
	if (level > reiser4_tree_get_height(tree)) {
		if (reiser4_tree_fresh(tree))
			return -EINVAL;
		
		reiser4_node_lock(place->node);

		if ((res = tree_growup_level(tree, level))) {
			reiser4_node_unlock(place->node);
			return res;
		}

		reiser4_node_unlock(place->node);

		/* Assigning insert point into new root as it was requested to
		   be insert point. */
		reiser4_place_assign(place, tree->root, 0, MAX_UINT32);
	}

	/* Preparing position new allocated node (if any) will be attached
	   later. */
	if (place->node != NULL) {
		if (level < reiser4_node_get_level(place->node)) {
			reiser4_place_dup(&aplace, place);
		} else {
			reiser4_place_dup(&aplace, &place->node->p);
			aplace.pos.item++;
		}
	} else {
		reiser4_place_assign(&aplace, NULL, 0, MAX_UINT32);
	}

	/* Preparing things for insert. This may be splitting the tree,
	   allocating root, etc. */
	if ((res = tree_prepare_insert(tree, place, level)))
		return res;

	if (aplace.node == NULL) {
		reiser4_place_assign(&aplace, tree->root,
				     0, MAX_UINT32);
	}

	/* Estimating item/unit to inserted/written to tree. */
	if ((res = estimate_func(place, hint)))
		return res;
	
	/* Needed space to be prepared in tree. */
	needed = hint->len + hint->overhead;
	mode = (place->pos.unit == MAX_UINT32);

	/* Preparing space in tree. */
	if ((space = reiser4_tree_expand(tree, place, needed,
					 hint->shift_flags)) < 0)
	{
		aal_error("Can't prepare space in tree.");
		return space;
	}

	/* Checking if we still have less space than needed. This is ENOSPC case
	   if we tried to insert data. */
	if ((uint32_t)space < needed)
		return -ENOSPC;

	/* Making yet another estimate if insert mode is changed after making
	   space. That is if we wanted to insert new unit into existent item,
	   but insert point was moved to new empty node and thus, we need to
	   insert new item. As item may has an overhead like directory one has,
	   we should take it to acount. */
	if (mode != (place->pos.unit == MAX_UINT32))
		hint->overhead = reiser4_item_overhead(hint->plug);

	/* Inserting/writing data to node. */
	if ((write = reiser4_node_modify(place->node, &place->pos,
					 hint, modify_func)) < 0)
	{
		aal_error("Can't insert data to node %llu.",
			  node_blocknr(place->node));
		return write;
	}

	/* Parent keys will be updated if we inserted item/unit into leftmost
	   pos and if target node has parent. */
	if (reiser4_place_leftmost(place) && place->node != tree->root) {
		reiser4_place_t *p = &place->node->p;
		
		if (p->node != NULL) {
			reiser4_key_assign(&place->node->p.key,
					   &hint->offset);
			
			if ((res = reiser4_tree_update_key(tree, p,
							   &hint->offset)))
			{
				return res;
			}
		}
	}
	
	if (reiser4_node_get_level(place->node) > LEAF_LEVEL) {
		if ((res = reiser4_tree_update_node(tree, place->node)))
			return res;
	}

        /* If make space function allocates new node, we should attach it to the
	   tree. Also, here we should handle the special case, when tree root
	   should be changed. */
	if (place->node != tree->root && !place->node->p.node) {
		/* Check if insert was on root node level. If so -- growing tree
		   up by one. */
		if (level == reiser4_tree_get_height(tree)) {
			reiser4_node_lock(place->node);

			if ((res = reiser4_tree_growup(tree))) {
				aal_error("Can't grow tree up during "
					  "modify it.");
				reiser4_node_unlock(place->node);
				return res;
			}
			
			reiser4_node_unlock(place->node);
		}

		/* Attaching new node to the tree. */
		if ((res = reiser4_tree_attach_node(tree, place->node, &aplace,
						    hint->shift_flags)))
		{
			aal_error("Can't attach node %llu to tree.",
				  node_blocknr(place->node));
			return res;
		}
	}
	
	/* Initializing insert point place. */
	if ((res = reiser4_place_fetch(place)))
		return res;

	/* Calling @hint->place_func if a new item was created. */
	if (hint->place_func && place->pos.unit == MAX_UINT32) {
		if ((res = hint->place_func(place, hint->data)))
			return res;
	}

	return write;
}

/* Inserts data to the tree. This function is used for inserting items which are
   not file body items, that is statdata, directory, etc. */
int64_t reiser4_tree_insert(reiser4_tree_t *tree, reiser4_place_t *place,
			    trans_hint_t *hint, uint8_t level)
{
	aal_assert("umka-779", tree != NULL);
	aal_assert("umka-779", hint != NULL);
	
	aal_assert("umka-1644", place != NULL);
	aal_assert("umka-1645", hint->plug != NULL);

	return reiser4_tree_modify(tree, place, hint, level, 
				   callback_prep_insert,
				   callback_node_insert);
}

/* Writes data to the tree. used for puting tail and extents to tree. */
int64_t reiser4_tree_write(reiser4_tree_t *tree, reiser4_place_t *place,
			   trans_hint_t *hint, uint8_t level)
{
	aal_assert("umka-2441", tree != NULL);
	aal_assert("umka-2442", hint != NULL);
	
	aal_assert("umka-2443", place != NULL);
	aal_assert("umka-2444", hint->plug != NULL);

	return reiser4_tree_modify(tree, place, hint, level,
				   callback_prep_write,
				   callback_node_write);
}

/* Removes item/unit at @place. */
errno_t reiser4_tree_remove(reiser4_tree_t *tree, reiser4_place_t *place,
			    trans_hint_t *hint)
{
	errno_t res;

	aal_assert("umka-2055", tree != NULL);
	aal_assert("umka-2056", place != NULL);
	aal_assert("umka-2392", hint != NULL);

	if (hint->count == 0)
		return -EINVAL;
	
	/* Removing iten/unit from the node. */
	if ((res = reiser4_node_remove(place->node, &place->pos, hint)))
		return res;

	/* Updating left deleimiting key in all parent nodes. */
	if (reiser4_place_leftmost(place) &&
	    place->node->p.node)
	{
		/* If node became empty it will be detached from the tree, so
		   updating is not needed and impossible, because it has no
		   items. */
		if (reiser4_node_items(place->node) > 0) {
			reiser4_place_t p;
			reiser4_key_t lkey;

			/* Updating parent keys. */
			reiser4_node_leftmost_key(place->node, &lkey);
				
			reiser4_place_init(&p, place->node->p.node,
					   &place->node->p.pos);

			reiser4_key_assign(&place->node->p.key, &lkey);
			
			if ((res = reiser4_tree_update_key(tree, &p, &lkey)))
				return res;
		}
	}
	
	if (reiser4_node_get_level(place->node) > LEAF_LEVEL &&
	    reiser4_node_items(place->node) > 0)
	{
		if ((res = reiser4_tree_update_node(tree, place->node)))
			return res;
	}

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
		if ((res = reiser4_tree_shrink(tree, place)))
			return res;
	}

	/* Drying tree up in the case root node exists and tree is singular,
	   that is has only one item. Tree also should not be minimal of
	   height. Here root may be NULL due to nested call from
	   tree_dryout(). */
	if (tree->root && reiser4_tree_singular(tree) &&
	    !reiser4_tree_minimal(tree))
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
			       tree_edge_func_t before_func,
			       tree_update_func_t update_func,
			       tree_edge_func_t after_func,
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

	if ((before_func && (res = before_func(tree, node, data))))
		goto error_unlock_node;

	/* The loop though the items of current node */
	for (pos->item = 0; pos->item < reiser4_node_items(node);
	     pos->item++)
	{
		pos->unit = MAX_UINT32; 

		/* If there is a suspicion of a corruption, it must be checked
		   in before_func. All items must be opened here. */
		if (reiser4_place_open(&place, node, pos)) {
			aal_error("Can't open item by place. Node "
				  "%llu, item %u.", node_blocknr(node),
				  pos->item);
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
			if (update_func && (res = update_func(tree, &place, data)))
				goto error_after_func;
		}
	}
	
	if (after_func)
		res = after_func(tree, node, data);

	reiser4_tree_unlock_node(tree, node);
	return res;

 error_after_func:
	if (after_func)
		res = after_func(tree, node, data);

 error_unlock_node:
	reiser4_tree_unlock_node(tree, node);
	return res;
}

/* Traverses tree with passed callback functions for each event. This is used
   for all tree traverse related operations like copy, measurements, etc. */
errno_t reiser4_tree_trav(reiser4_tree_t *tree,
			  tree_open_func_t open_func,
			  tree_edge_func_t before_func,
			  tree_update_func_t update_func,
			  tree_edge_func_t after_func,
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
