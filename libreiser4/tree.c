/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tree.c -- reiser4 tree related code. */

#include <reiser4/reiser4.h>

#ifndef ENABLE_STAND_ALONE
static errno_t callback_tree_pack(reiser4_tree_t *tree,
				  reiser4_place_t *place,
				  void *data)
{
	aal_assert("umka-1897", tree != NULL);
	aal_assert("umka-1898", place != NULL);

	return reiser4_tree_shrink(tree, place);
}
#endif

/* Dealing with loading root node if it is not loaded yet */
errno_t reiser4_tree_lroot(reiser4_tree_t *tree) {
	blk_t root;
	
	aal_assert("umka-1870", tree != NULL);

	if (tree->root)
		return 0;

#ifndef ENABLE_STAND_ALONE
	if (reiser4_tree_fresh(tree))
		return -EINVAL;
#endif
	
	root = reiser4_format_get_root(tree->fs->format);
	
	if (!(tree->root = reiser4_tree_load(tree, NULL, root)))
		return -EINVAL;
    
	tree->root->tree = tree;
	
	return 0;
}

#ifndef ENABLE_STAND_ALONE
/* Assignes passed @node as new root */
static errno_t reiser4_tree_sroot(reiser4_tree_t *tree,
				  reiser4_node_t *node)
{
	uint32_t level;
	
	aal_assert("umka-1867", tree != NULL);
	aal_assert("umka-1868", node != NULL);

	tree->root = node;
	node->tree = tree;
	node->p.node = NULL;

	level = reiser4_node_get_level(node);

	reiser4_format_set_root(tree->fs->format,
				tree->root->number);
	
	reiser4_format_set_height(tree->fs->format,
				  level);

	return 0;
}

/* Dealing with allocating root node if it is not allocated yet */
static errno_t reiser4_tree_aroot(reiser4_tree_t *tree) {
	uint32_t height;
	reiser4_node_t *root;
	
	aal_assert("umka-1869", tree != NULL);
	
	if (tree->root)
		return 0;

	if (!reiser4_tree_fresh(tree))
		return -EINVAL;

	height = reiser4_tree_height(tree);
	
	if (!(root = reiser4_tree_alloc(tree, height)))
		return -ENOSPC;

	return reiser4_tree_sroot(tree, root);
}
#endif

/* Registers passed node in tree and connects left and right neighbour
   nodes. This function does not do any modifications. */
errno_t reiser4_tree_connect(
	reiser4_tree_t *tree,    /* tree instance */
	reiser4_node_t *parent,	 /* node child will be connected to */
	reiser4_node_t *node)	 /* child node to be attached */
{
	errno_t res = 0;
	
	aal_assert("umka-1857", tree != NULL);
	aal_assert("umka-2261", node != NULL);

	node->tree = tree;
	
	if (!parent) {
		/* This is the case when we connect root node, that is with no
		   parent. */
		tree->root = node;
	} else {
		/* Registering @child in @node children list */
		if ((res = reiser4_node_connect(parent, node))) {
			aal_exception_error("Can't connect node %llu to "
					    "tree.", node->number);
			return res;
		}
	}

#ifndef ENABLE_STAND_ALONE
	if (tree->traps.connect) {
		reiser4_place_t place;

		reiser4_place_init(&place, parent, &node->p.pos);
		
		if (place.node) {
			if ((res = reiser4_place_fetch(&place)))
				return res;
		}
			
		/* Placing the connect() callback calling between lock/unlock
		   braces in order to prevent its freeing by memory pressure
		   handler. */
		reiser4_node_lock(node);
		
		res = tree->traps.connect(tree, &place, node,
					  tree->traps.data);
		
		reiser4_node_unlock(node);
	}
#endif

	return res;
}

/* Remove specified child from the node children list. Updates all neighbour
   pointers and parent pointer.*/
errno_t reiser4_tree_disconnect(
	reiser4_tree_t *tree,    /* tree instance */
	reiser4_node_t *parent,	 /* node child will be detached from */
	reiser4_node_t *node)	 /* pointer to child to be deleted */
{
	aal_assert("umka-1858", tree != NULL);
	aal_assert("umka-563", node != NULL);

#ifndef ENABLE_STAND_ALONE
	if (tree->traps.disconnect) {
		errno_t res;

		if (node->p.node) {
			if ((res = reiser4_place_fetch(&node->p)))
				return res;
		}

		/* Placing callback calling into lock/unlock braces for
		   preventing it freeing by handler. */
		reiser4_node_lock(node);
		
		if ((res = tree->traps.disconnect(tree, &node->p, node,
						  tree->traps.data)))
		{
			reiser4_node_unlock(node);
			return res;
		}

		reiser4_node_unlock(node);
	}
#endif
	
	/* Disconnecting left and right neighbours */
	if (node->left) {
		node->left->right = NULL;
		node->left = NULL;
	}
	
	if (node->right) {
		node->right->left = NULL;
		node->right = NULL;
	}
	
	node->tree = NULL;
	
	if (!parent) {
		/* The case when we're disconnecting root node for some
		   reasons. And we will let to do so? Yes, why not? */
		tree->root = NULL;
	} else {
		/* Disconnecting generic node */
		reiser4_node_disconnect(parent, node);
	}
	

	return 0;
}

/* Loads node and connects it to @parent */
reiser4_node_t *reiser4_tree_load(reiser4_tree_t *tree,
				  reiser4_node_t *parent,
				  blk_t blk)
{
	reiser4_node_t *node = NULL;

	aal_assert("umka-1289", tree != NULL);

	/* Checking if node in the local cache of @parent */
	if (!parent || !(node = reiser4_node_child(parent, blk))) {
		rid_t pid;
		uint32_t blksize;

		if (tree->mpc_func && tree->mpc_func()) {
			if (parent)
				reiser4_node_lock(parent);

			if (tree->root)
				reiser4_tree_adjust(tree, tree->root, 1);

			if (parent)
				reiser4_node_unlock(parent);
		}
		
		blksize = reiser4_master_blksize(tree->fs->master);

#ifndef ENABLE_STAND_ALONE
		pid = reiser4_format_node_pid(tree->fs->format);
#else
		pid = reiser4_profile_value(tree->fs->profile, "node");
#endif

		/* Node is not loaded yet. Loading it and connecting to @parent
		   node cache. */
		if (!(node = reiser4_node_open(tree->fs->device,
					       blksize, blk, pid,
					       tree->key.plug)))
		{
			aal_exception_error("Can't open node "
					    "%llu.", blk);
			return NULL;
		}

		if (reiser4_tree_connect(tree, parent, node))
			goto error_free_node;
	}

	return node;

 error_free_node:
	reiser4_node_close(node);
	return NULL;
}

/* Unloading node from the tree cache */
errno_t reiser4_tree_unload(reiser4_tree_t *tree,
			    reiser4_node_t *node)
{
	aal_assert("umka-1840", tree != NULL);
	aal_assert("umka-1842", node != NULL);

	/* Disconnecting @node from its parent node and releasing passed
	   @node */
	reiser4_tree_disconnect(tree, node->p.node, node);
	return reiser4_node_close(node);
}

/* Loads denoted by passed nodeptr @place child node */
reiser4_node_t *reiser4_tree_child(reiser4_tree_t *tree,
				   reiser4_place_t *place)
{
	ptr_hint_t ptr;
	
	aal_assert("umka-1889", tree != NULL);
	aal_assert("umka-1890", place != NULL);
	aal_assert("umka-1891", place->node != NULL);

	if (reiser4_place_fetch(place))
		return NULL;

	/* Checking if item is a branch of tree */
	if (!reiser4_item_branch(place))
		return NULL;
			
	plug_call(place->plug->o.item_ops, read,
		  (place_t *)place, &ptr, 0, 1);

	return reiser4_tree_load(tree, place->node,
				 ptr.start);
}

/* Finds both left and right neighbours and connects them into the tree */
static reiser4_node_t *reiser4_tree_ltrt(reiser4_tree_t *tree,
					 reiser4_node_t *node,
					 aal_direction_t where)
{
        int found = 0;
        uint32_t level;
        reiser4_place_t place;
                                                                                      
	aal_assert("umka-2213", tree != NULL);
	aal_assert("umka-2214", node != NULL);

	level = 0;
	reiser4_place_assign(&place, node, 0, MAX_UINT32);
                                                                                      
        /* Going up to the level where corresponding neighbour node may be
	   obtained by its nodeptr item. */
        while (place.node->p.node && found == 0) {
		place = place.node->p;

		/* Checking position. Level is found if position is not first
		   (right neighbour) and is not last one (left neighbour). */
		if (where == D_LEFT) {
			found = reiser4_place_gtfirst(&place);
		} else {
			found = reiser4_place_ltlast(&place);
		}

                level++;
        }
                                                                                      
        if (found == 0)
                return NULL;

	/* Position correcting */
        place.pos.item += (where == D_LEFT ? -1 : 1);
                                                                                      
        /* Going down to the level of @node */
        while (level > 0) {
                if (!(place.node = reiser4_tree_child(tree, &place)))
                        return NULL;

		if (where == D_LEFT) {
			if (reiser4_place_last(&place))
				return NULL;
		} else {
			if (reiser4_place_first(&place))
				return NULL;
		}
		
                level--;
        }
                                                                                      
        /* Setting up neightbour links */
        if (where == D_LEFT) {
                node->left = place.node;
                place.node->right = node;
        } else {
                node->right = place.node;
                place.node->left = node;
        }

	return place.node;
}

/* Gets left or right neighbour nodes */
reiser4_node_t *reiser4_tree_neigh(reiser4_tree_t *tree,
				   reiser4_node_t *node,
				   aal_direction_t where)
{
	aal_assert("umka-2219", node != NULL);
	aal_assert("umka-1859", tree != NULL);

	/* Parent is not present. The root node */
	if (!node->p.node)
		return NULL;

	if ((where == D_LEFT && !node->left) ||
	    (where == D_RIGHT && !node->right))
	{
		/* Looking for neighbour on disk */
		reiser4_node_lock(node);
		reiser4_tree_ltrt(tree, node, where);
		reiser4_node_unlock(node);
	}
	
	return (where == D_LEFT) ? node->left :
		node->right;
}

#ifndef ENABLE_STAND_ALONE
/* Requests block allocator for new block and creates empty node in it */
reiser4_node_t *reiser4_tree_alloc(
	reiser4_tree_t *tree,	    /* tree for operating on */
	uint8_t level)	 	    /* level of new node */
{
	rid_t pid;
	uint32_t blocksize;
	uint32_t free, stamp;
	reiser4_node_t *node;
    
	aal_assert("umka-756", tree != NULL);
    
	/* Setting up of the free blocks in format */
	if (!(free = reiser4_format_get_free(tree->fs->format)))
		return NULL;
	
	if (tree->mpc_func && tree->mpc_func() && tree->root)
		reiser4_tree_adjust(tree, tree->root, 1);
	
	reiser4_format_set_free(tree->fs->format, free - 1);

	blocksize = reiser4_master_blksize(tree->fs->master);
	pid = reiser4_profile_value(tree->fs->profile, "node");

	/* Creating new node */
	if (!(node = reiser4_node_init(tree->fs->device, blocksize,
				       reiser4_fake_get(), pid,
				       tree->key.plug)))
	{
		aal_exception_error("Can't initialize node.");
		return NULL;
	}

	/* Forming node of @level */
	if (reiser4_node_form(node, level))
		goto error_free_node;

	/* Setting flush stamps to new node */
	stamp = reiser4_format_get_stamp(tree->fs->format);
	reiser4_node_set_mstamp(node, stamp);
	
	if (tree->root) {
		stamp = reiser4_node_get_fstamp(tree->root);
		reiser4_node_set_fstamp(node, stamp);
	}

	node->tree = tree;
	return node;

 error_free_node:
	reiser4_node_close(node);
	return NULL;
}

/* Unload node and releasing it in block allocator */
errno_t reiser4_tree_release(reiser4_tree_t *tree,
			     reiser4_node_t *node)
{
	blk_t free;
	reiser4_alloc_t *alloc;
	
	aal_assert("umka-1841", tree != NULL);
	aal_assert("umka-2255", node != NULL);

	alloc = tree->fs->alloc;
	reiser4_node_mkclean(node);

	/* Check if we're trying to releas a node with fake block number. If
	   not, free it in block allocator too. */
	if (!reiser4_fake_ack(node->number))
		reiser4_alloc_release(alloc, node->number, 1);

	/* Changing format field free_block_count */
	free = reiser4_alloc_free(alloc);
	
	reiser4_format_set_free(tree->fs->format,
				free + 1);
	
	return reiser4_tree_unload(tree, node);
}
#endif

/* Builds root key and stores it in passed @tree instance */
static errno_t reiser4_tree_key(reiser4_tree_t *tree) {
	rid_t pid;
	oid_t locality;
	oid_t objectid;
    
	aal_assert("umka-1090", tree != NULL);
	aal_assert("umka-1091", tree->fs != NULL);
	aal_assert("umka-1092", tree->fs->oid != NULL);

#ifndef ENABLE_STAND_ALONE
	pid = reiser4_profile_value(tree->fs->profile, "key");
#else
	pid = reiser4_format_key_pid(tree->fs->format);
#endif

	/* Finding needed key plugin by its identifier */
	if (!(tree->key.plug = libreiser4_factory_ifind(KEY_PLUG_TYPE,
							pid)))
	{
		aal_exception_error("Can't find key plugin by its id "
				    "0x%x.", pid);
		return -EINVAL;
	}
	
	return reiser4_fs_root_key(tree->fs, &tree->key);
}

/* Returns tree root block number */
blk_t reiser4_tree_root(reiser4_tree_t *tree) {
	aal_assert("umka-738", tree != NULL);

	if (!tree->fs || !tree->fs->format)
		return INVAL_BLK;

	return reiser4_format_get_root(tree->fs->format);
}

/* Extents hash table related functions */
static int callback_foreach_func(const void *entry, void *data) {
	aal_hash_node_t *node;

	node = (aal_hash_node_t *)entry;
	
	reiser4_key_free((reiser4_key_t *)node->key);
	aal_block_free((aal_block_t *)node->value);
	
	return 0;
}

static uint64_t callback_hash_func(const void *k) {
	reiser4_key_t *key;
	
	key = (reiser4_key_t *)k;
	
	return (reiser4_key_get_objectid(key) +
		reiser4_key_get_offset(key));
}

static int callback_comp_func(const void *k1,
			      const void *k2,
			      void *data)
{
	return reiser4_key_compare((reiser4_key_t *)k1,
				   (reiser4_key_t *)k2);
}

/* Opens the tree (that is, the tree cache) on specified filesystem */
reiser4_tree_t *reiser4_tree_init(reiser4_fs_t *fs,
				  mpc_func_t mpc_func)
{
	reiser4_tree_t *tree;

	aal_assert("umka-737", fs != NULL);

	/* Allocating memory for the tree instance */
	if (!(tree = aal_calloc(sizeof(*tree), 0)))
		return NULL;

	tree->fs = fs;
	tree->fs->tree = tree;
	tree->mpc_func = mpc_func;

	if (!(tree->data = aal_hash_table_alloc(callback_hash_func,
						callback_comp_func)))
	{
		goto error_free_tree;
	}

	/* Building the tree root key */
	if (reiser4_tree_key(tree)) {
		aal_exception_error("Can't build the tree "
				    "root key.");
		goto error_free_data;
	}
    
#ifndef ENABLE_STAND_ALONE
	reiser4_tree_pack_on(tree);
	tree->traps.pack = callback_tree_pack;
#endif
		
	return tree;

 error_free_data:
	aal_hash_table_free(tree->data);
 error_free_tree:
	aal_free(tree);
	return NULL;
}

/* Closes specified tree (frees all assosiated memory) */
void reiser4_tree_fini(reiser4_tree_t *tree) {
	aal_assert("umka-134", tree != NULL);

#ifndef ENABLE_STAND_ALONE
	/* Flushing tree cache */
	reiser4_tree_sync(tree);
#endif

	reiser4_tree_collapse(tree);
	tree->fs->tree = NULL;
	
	/* Freeing tree data (extents) */
	aal_hash_table_foreach(tree->data,
			       callback_foreach_func, NULL);

	aal_hash_table_free(tree->data);
	
	/* Freeing the tree */
	aal_free(tree);
}

/* Flushes some part of tree cache to device starting from passed @node */
errno_t reiser4_tree_adjust(reiser4_tree_t *tree,
			    reiser4_node_t *node,
			    bool_t check)
{
	errno_t res;
	aal_list_t *walk;
	reiser4_node_t *child;
	
	aal_assert("umka-2302", tree != NULL);
	aal_assert("umka-2303", node != NULL);

#ifndef ENABLE_STAND_ALONE
	/* We are dealing only with dirty nodes */
	if (reiser4_node_isdirty(node)) {
		blk_t number;

		/* Requesting block allocator to allocate the real block number
		   for fake allocated node. */
		if (reiser4_fake_ack(node->number)) {
			if (!reiser4_alloc_allocate(tree->fs->alloc,
						    &number, 1))
			{
				return -ENOSPC;
			}

			/* Assigning new block number to @node */
			reiser4_node_move(node, number);
		}

		/* Allocating all children nodes if we are on internal level */
		if (reiser4_node_get_level(node) > LEAF_LEVEL) {
			
			/* Allocating internal items */
			aal_list_foreach_forward(node->children, walk) {
				
				/* Checking if @child is fake one */
				child = (reiser4_node_t *)walk->data;

				if (!reiser4_fake_ack(child->number))
					continue;

				/* If @child is fake one it needs to be
				   allocated here and its nodeptr should be
				   updated. */
				if (!reiser4_alloc_allocate(tree->fs->alloc,
							    &number, 1))
				{
					return -ENOSPC;
				}

				/* Assigning node to new node blk */
				reiser4_node_move(child, number);

				/* Updating parent node pointer */
				if ((res = reiser4_node_upos(child)))
					return res;
			}
		}
	}
#endif

	/* Walking through the list of children and calling tree_adjust()
	   function for next level nodes if memory pressure event is still
	   actual. */
	if (node->children) {
		aal_list_foreach_forward(node->children, walk) {
			child = (reiser4_node_t *)walk->data;
			
			if ((res = reiser4_tree_adjust(tree, child, check)))
				return res;
		}
	}

#ifndef ENABLE_STAND_ALONE	
	/* Checking for root node and this is it, we update also root node and
	   free nodes fileds in format. */
	if (!node->p.node) {
		count_t free;
			
		/* Setting up format by new root and free blocks */
		reiser4_format_set_root(tree->fs->format,
					node->number);

		free = reiser4_alloc_free(tree->fs->alloc);
		reiser4_format_set_free(tree->fs->format, free);
	}
#endif
	
	/* Checking if we should flush the whole tree */
	if (check) {
		/* Checking if memory pressure is still exist */
		if (!tree->mpc_func || !tree->mpc_func())
			return 1;
	} else {
		aal_assert("umka-2318", node->counter == 0);
	}

	if (!reiser4_node_locked(node)) {
#ifndef ENABLE_STAND_ALONE
		/* Okay, node is allocated and ready to be saved to device */
		if (reiser4_node_isdirty(node) && reiser4_node_sync(node)) {
			aal_exception_error("Can't write node %llu.",
					    node->number);
			return -EIO;
		}

#endif
		reiser4_tree_unload(tree, node);
	}

	return 0;
}

/* Walking though the tree cache and closing all nodes */
errno_t reiser4_tree_walk(reiser4_tree_t *tree,
			  reiser4_node_t *node,
			  walk_func_t walk_func)
{
	errno_t res;
	
	aal_assert("umka-1933", tree != NULL);
	aal_assert("umka-1934", node != NULL);
	aal_assert("umka-2264", walk_func != NULL);

	/* Walking though the children list of @node */
	if (node->children) {
		reiser4_node_t *child;
		aal_list_t *walk, *next;

		for (walk = node->children; walk; walk = next) {
			next = walk->next;
			child = (reiser4_node_t *)walk->data;

			/* Making recursive call to tree_walk() */
			if ((res = reiser4_tree_walk(tree, child,
						     walk_func)))
			{
				return res;
			}
		}
	}

	/* Calling @walk_func for @node */
	return walk_func(tree, node);
}

#ifndef ENABLE_STAND_ALONE
/* Syncs whole tree cache */
errno_t reiser4_tree_sync(reiser4_tree_t *tree) {
	aal_assert("umka-2259", tree != NULL);

	if (!tree->root)
		return 0;

        /* Flushing whole tree. */
	return reiser4_tree_adjust(tree, tree->root, 0);
}

/* Returns TRUE if tree has not root node allocated */
bool_t reiser4_tree_fresh(reiser4_tree_t *tree) {
	aal_assert("umka-1930", tree != NULL);
	aal_assert("umka-2210", tree->fs != NULL);
	aal_assert("umka-2211", tree->fs->format != NULL);

	return (reiser4_format_get_root(tree->fs->format) == INVAL_BLK);
}
#endif

/* Unloads all tree nodes */
errno_t reiser4_tree_collapse(reiser4_tree_t *tree) {
	aal_assert("umka-2265", tree != NULL);

	if (!tree->root)
		return 0;

	return reiser4_tree_walk(tree, tree->root,
				 reiser4_tree_unload);
}

/* Returns current tree height */
uint8_t reiser4_tree_height(reiser4_tree_t *tree) {
	aal_assert("umka-1065", tree != NULL);
	aal_assert("umka-1284", tree->fs != NULL);
	aal_assert("umka-1285", tree->fs->format != NULL);

	return reiser4_format_get_height(tree->fs->format);
}

/* Makes search in the tree by specified key. Fills passed place by places of
   found item. */
lookup_t reiser4_tree_lookup(
	reiser4_tree_t *tree,	  /* tree to be grepped */
	reiser4_key_t *key,	  /* key to be find */
	uint8_t level,	          /* stop level for search */
	reiser4_place_t *place)	  /* place the found item to be stored */
{
	lookup_t res;
	reiser4_key_t wan;

	aal_assert("umka-742", key != NULL);
	aal_assert("umka-1760", tree != NULL);
	aal_assert("umka-2057", place != NULL);

	/* We store @key in @wan. All consewuence code will use @wan. This is
	   neede, because @key might point to @place->item.key in @place and
	   will be corrupted durring lookup. */
	reiser4_key_assign(&wan, key);

	/* Initializing place by root node */
	reiser4_place_assign(place, tree->root, 0, MAX_UINT32);

	/* Making sure that root exists. If not, getting out with @place
	   initialized by NULL root. */
	if (reiser4_tree_lroot(tree))
		return ABSENT;

	/* Reinitialziing @place by root node, as root exists and loaded. This
	   is needed for code bellow. */
	reiser4_place_assign(place, tree->root, 0, MAX_UINT32);

	/* Checking the case when wanted key is smaller than root one. This is
	   the case, when somebody is trying go up of the root by ".." entry in
	   root directory. If so, we initialize the key to be looked up by root
	   key. */
	if (reiser4_key_compare(&wan, &tree->key) < 0)
		reiser4_key_assign(&wan, &tree->key);
		    
	while (1) {
		int whole = 0;
		
		/* Looking up for key inside node. Result of lookuping will be
		   stored in &place->pos. */
		res = reiser4_node_lookup(place->node, &wan, &place->pos);

		/* Check if we should finish lookup because we reach stop level
		   or some error occured durring last node lookup. */
		if (reiser4_node_get_level(place->node) <= level ||
		    res == FAILED)
		{
			if (res == PRESENT) {
				/* Fetching item at @place if key is found */
				reiser4_place_fetch(place);
			}
			
			return res;
		}

		/* Position correcting for internal levels */
		if (res == ABSENT) {
			whole = (place->pos.unit == MAX_UINT32);
			reiser4_place_dec(place, whole);
		}
		
		/* Initializing @place->item. This should be done before using
		   any item methods. */
		if (reiser4_place_fetch(place))
			return FAILED;
		    
		/* Checking is item at @place is nodeptr one. If not, we correct
		   position back. */
		if (!reiser4_item_branch(place)) {
			if (res == ABSENT) {
				reiser4_place_inc(place, whole);
			}

			if (!reiser4_place_valid(place))
				return res;
			
			if (reiser4_place_fetch(place))
				return FAILED;
			
			if (!reiser4_item_branch(place))
				return res;
		} 

		/* Loading node by its nodeptr item at @place */
		if (!(place->node = reiser4_tree_child(tree, place)))
			return FAILED;
	}
    
	return ABSENT;
}

#ifndef ENABLE_STAND_ALONE
/* Returns TRUE if passed @tree has minimal possible height and thus cannot be
   dried out. */
static bool_t reiser4_tree_minimal(reiser4_tree_t *tree) {

	if (reiser4_tree_height(tree) <= 2)
		return TRUE;

	return FALSE;
}

/* Updates key at passed @place by passed @key by means of using
   reiser4_node_ukey functions in recursive maner. */
errno_t reiser4_tree_ukey(reiser4_tree_t *tree,
			  reiser4_place_t *place,
			  reiser4_key_t *key)
{
	errno_t res;
	
	aal_assert("umka-1892", tree != NULL);
	aal_assert("umka-1893", place != NULL);
	aal_assert("umka-1894", key != NULL);

	/* Getting into recursion if we should update leftmost key */
	if (reiser4_place_leftmost(place)) {
		
		if (place->node->p.node) {
			reiser4_place_t *p = &place->node->p;
			
			if ((res = reiser4_tree_ukey(tree, p, key)))
				return res;
		}
	}

	return reiser4_node_ukey(place->node, &place->pos, key);
}

/* This function inserts new nodeptr item to the tree and in such way it
   attaches passed @node to it. It also connects passed @node into tree
   cache. */
errno_t reiser4_tree_attach(
	reiser4_tree_t *tree,	    /* tree we will attach node to */
	reiser4_node_t *node)       /* child to attached */
{
	rid_t pid;
	errno_t res;
	uint8_t level;
	
	create_hint_t hint;
	reiser4_place_t place;
	ptr_hint_t nodeptr_hint;

	aal_assert("umka-913", tree != NULL);
	aal_assert("umka-916", node != NULL);
    
	/* Preparing nodeptr item hint */
	aal_memset(&hint, 0, sizeof(hint));

	/* Prepare nodeptr hint from passed @node */
	nodeptr_hint.width = 1;
	nodeptr_hint.start = node->number;

	hint.count = 1;
	hint.type_specific = &nodeptr_hint;

	reiser4_node_lkey(node, &hint.key);
	pid = reiser4_profile_value(tree->fs->profile, "nodeptr");

	if (!(hint.plug = libreiser4_factory_ifind(ITEM_PLUG_TYPE,
						   pid)))
	{
		aal_exception_error("Can't find item plugin by "
				    "its id 0x%x.", pid);
		return -EINVAL;
	}

	level = reiser4_node_get_level(node) + 1;

	/* Looking up for the insert point place */
	switch ((res = reiser4_tree_lookup(tree, &hint.key,
					   level, &place)))
	{
	case PRESENT:
		aal_exception_error("Can't attach node. Key already "
				    "exists in tree.");
		return -EINVAL;
	case FAILED:
		aal_exception_error("Lookup is failed durring attach "
				    "new node.");
		return -EINVAL;
	default:
		break;
	}

	/* Inserting node pointer into tree */
	if ((res = reiser4_tree_insert(tree, &place, level, &hint))) {
		aal_exception_error("Can't insert nodeptr item to "
				    "the tree.");
		return res;
	}

	/* Attaching node to insert point node. */
	if ((res = reiser4_tree_connect(tree, place.node, node))) {
		aal_exception_error("Can't attach the node %llu to "
				    "the tree.", node->number);
		return res;
	}

	reiser4_tree_neigh(tree, node, D_LEFT);
	reiser4_tree_neigh(tree, node, D_RIGHT);
	
	return 0;
}

/* Removes passed @node from the on-disk tree and cache structures */
errno_t reiser4_tree_detach(reiser4_tree_t *tree,
			    reiser4_node_t *node)
{
	aal_assert("umka-1726", tree != NULL);
	aal_assert("umka-1727", node != NULL);

	reiser4_tree_disconnect(tree, node->p.node, node);
	
	/* Removing item/unit from the parent node */
	return reiser4_tree_remove(tree, &node->p, 1);
}

/* This function forces tree to grow by one level and sets it up after the
   growing. */
errno_t reiser4_tree_growup(
	reiser4_tree_t *tree)	/* tree to be growed up */
{
	errno_t res;
	uint8_t height;
	reiser4_node_t *old_root;

	aal_assert("umka-1701", tree != NULL);
	aal_assert("umka-1736", tree->root != NULL);
	
	if ((res = reiser4_tree_lroot(tree)))
		return res;
	
	old_root = tree->root;
	height = reiser4_tree_height(tree);
    
	/* Allocating new root node */
	if (!(tree->root = reiser4_tree_alloc(tree, height + 1))) {
		res = -ENOSPC;
		goto error_back_root;
	}

	tree->root->tree = tree;

	/* Updating format-related fields */
    	reiser4_format_set_root(tree->fs->format,
				tree->root->number);

	reiser4_format_set_height(tree->fs->format,
				  height + 1);
	
	if ((res = reiser4_tree_attach(tree, old_root)))
		goto error_free_root;

	return 0;

 error_free_root:
	reiser4_tree_release(tree, tree->root);

 error_back_root:
	tree->root = old_root;
	return res;
}

/* Decreases tree height by one level */
errno_t reiser4_tree_dryout(reiser4_tree_t *tree) {
	errno_t res;
	reiser4_node_t *root;
	reiser4_node_t *child;
	reiser4_place_t place;

	aal_assert("umka-1731", tree != NULL);
	aal_assert("umka-1737", tree->root != NULL);

	if (reiser4_tree_minimal(tree))
		return -EINVAL;

	/* Rasing up the root node if it exists */
	if ((res = reiser4_tree_lroot(tree)))
		return res;

	root = tree->root;
	
	/* Check if we can dry tree out safely */
	if (reiser4_node_items(root) > 1)
		return -EINVAL;

	/* Getting new root as the first child of the old root node */
	reiser4_place_assign(&place, root, 0, 0);

	if (!(child = reiser4_tree_child(tree, &place))) {
		aal_exception_error("Can't load new root durring "
				    "drying tree out.");
		return -EINVAL;
	}

	aal_assert("umka-1929", root->counter > 1);
	
	/* Disconnect old root and its child from the tree */
	reiser4_tree_disconnect(tree, NULL, root);
	reiser4_tree_disconnect(tree, root, child);

	/* Assign new root */
	reiser4_tree_sroot(tree, child);
	
        /* Releasing old root node */
	reiser4_node_mkclean(root);
	reiser4_tree_release(tree, root);

	return 0;
}

/* Tries to shift items and units from @place to passed @neig node. After it is
   finished place will contain new insert point. */
errno_t reiser4_tree_shift(
	reiser4_tree_t *tree,	/* tree we will operate on */
	reiser4_place_t *place,	/* insert point place */
	reiser4_node_t *neig,	/* node items will be shifted to */
	uint32_t flags)	        /* some flags (direction, move ip or not, etc) */
{
	errno_t res;
	shift_hint_t hint;
	reiser4_key_t lkey;
	reiser4_node_t *node;

	aal_assert("umka-1225", tree != NULL);
	aal_assert("umka-1226", place != NULL);
	aal_assert("umka-1227", neig != NULL);
    
	aal_memset(&hint, 0, sizeof(hint));

	node = place->node;
	hint.control = flags;
	hint.pos = place->pos;

	if ((res = reiser4_node_shift(node, neig, &hint)))
		return res;

	place->pos = hint.pos;

	if (hint.result & SF_MOVIP)
		place->node = neig;

	/* Updating left delimiting keys in the tree */
	if (hint.control & SF_LEFT) {
		
		if (reiser4_node_items(node) > 0 &&
		    (hint.items > 0 || hint.units > 0))
		{
			if (node->p.node) {
				reiser4_place_t p;

				if ((res = reiser4_node_lkey(node, &lkey)))
					return res;

				reiser4_place_init(&p, node->p.node,
						   &node->p.pos);
				
				if ((res = reiser4_tree_ukey(tree, &p, &lkey)))
					return res;
			}
		}
	} else {
		if (hint.items > 0 || hint.units > 0) {

			if (neig->p.node) {
				reiser4_place_t p;
				
				if ((res = reiser4_node_lkey(neig, &lkey)))
					return res;
				
				reiser4_place_init(&p, neig->p.node,
						   &neig->p.pos);
				
				if ((res = reiser4_tree_ukey(tree, &p, &lkey)))
					return res;
			}
		}
	}

	return 0;
}

/* Default enough space condition check function */
static bool_t enough_by_space(reiser4_tree_t *tree,
			      reiser4_place_t *place,
			      uint32_t needed)
{
	if (needed <= reiser4_node_space(place->node))
		return TRUE;

	return FALSE;
}

/* Makes space in tree to insert @needed bytes of data (item/unit) */
errno_t reiser4_tree_expand(
	reiser4_tree_t *tree,	    /* tree pointer function operates on */
	reiser4_place_t *place,	    /* place of insertion point */
	uint32_t needed,	    /* amount of space that should be freed */
	uint32_t flags)
{
	int alloc;
	errno_t res;
	bool_t enough;

	reiser4_place_t old;
	reiser4_node_t *left;
	reiser4_node_t *right;

	aal_assert("umka-766", place != NULL);
	aal_assert("umka-929", tree != NULL);

	if ((enough = enough_by_space(tree, place, needed)))
		return 0;

	old = *place;
	
	/* Shifting data into left neighbour if it exists */
	if ((SF_LEFT & flags) &&
	    (left = reiser4_tree_neigh(tree, place->node, D_LEFT)))
	{
	    
		if ((res = reiser4_tree_shift(tree, place, left,
					      SF_LEFT | SF_UPTIP)))
		{
			return res;
		}
	
		if ((enough = enough_by_space(tree, place, needed)))
			return 0;
	}

	/* Shifting data into right neighbour if it exists */
	if ((SF_RIGHT & flags) &&
	    (right = reiser4_tree_neigh(tree, place->node, D_RIGHT)))
	{
	    
		if ((res = reiser4_tree_shift(tree, place, right,
					      SF_RIGHT | SF_UPTIP)))
		{
			return res;
		}
	
		if ((enough = enough_by_space(tree, place, needed)))
			return 0;
	}

	if (!(SF_ALLOC & flags))
		return -ENOSPC;
	
	/*
	  Here we still have not enough free space for inserting item/unit into
	  the tree. Allocating new node and trying to shift data into it.
	*/
	for (alloc = 0; !enough && (alloc < 2); alloc++) {
		uint8_t level;
		shift_flags_t flags;
		reiser4_place_t save;
		reiser4_node_t *node;

		level = reiser4_node_get_level(place->node);
	
		if (!(node = reiser4_tree_alloc(tree, level))) {
			aal_exception_error("Can't allocate new node. "
					    "No space left?");
			return -ENOSPC;
		}
		
		flags = SF_RIGHT | SF_UPTIP;

		if (alloc == 0)
			flags |= SF_MOVIP;

		save = *place;
		
		if ((res = reiser4_tree_shift(tree, place, node, flags)))
			return res;

		/* Releasing old node, because it has become empty as result of
		   data shifting. */
		if (reiser4_node_items(save.node) == 0) {

			if ((res = reiser4_tree_detach(tree, save.node)))
				return res;
			
			reiser4_node_mkclean(save.node);
			reiser4_tree_release(tree, save.node);
		}

		/* Attaching new allocated node into the tree, if it is not
		   empty */
		if (reiser4_node_items(node) > 0) {

			/* Growing the tree in the case we splitted the root
			   node. Root node has not parent. */
			if (!old.node->p.node)
				reiser4_tree_growup(tree);
			
			/* Attaching new node to the tree */
			if ((res = reiser4_tree_attach(tree, node))) {
				aal_exception_error("Can't attach new node to the "
						    "tree while making space.");
				
				reiser4_tree_release(tree, node);
				return res;
			}
		}
		
		enough = enough_by_space(tree, place, needed);
	}

	return enough ? 0 : -ENOSPC;
}

/* Packs node in @place by means of using shift into/from neighbours */
errno_t reiser4_tree_shrink(reiser4_tree_t *tree,
			    reiser4_place_t *place)
{
	errno_t res;
	reiser4_node_t *left, *right;

	aal_assert("umka-1784", tree != NULL);
	aal_assert("umka-1783", place != NULL);
	
	/* Packing node in order to keep the tree in well packed state
	   anyway. Here we will shift data from the target node to its left
	   neighbour node. */
	if ((left = reiser4_tree_neigh(tree, place->node, D_LEFT))) {
	    
		if ((res = reiser4_tree_shift(tree, place, left, SF_LEFT))) {
			aal_exception_error("Can't pack node %llu into left.",
					    place->node->number);
			return res;
		}
	}
		
	if (reiser4_node_items(place->node) > 0) {
		/* Shifting the data from the right neigbour node into the
		   target node. */
		if ((right = reiser4_tree_neigh(tree, place->node, D_RIGHT))) {
				
			reiser4_place_t bogus;
			bogus.node = right;
	    
			if ((res = reiser4_tree_shift(tree, &bogus,
						      place->node, SF_LEFT)))
			{
				aal_exception_error("Can't pack node %llu "
						    "into left.", right->number);
				return res;
			}

			if (reiser4_node_items(right) == 0) {
				reiser4_tree_detach(tree, right);
				reiser4_node_mkclean(right);
				reiser4_tree_release(tree, right);
			}
		}
	} else {
		reiser4_node_mkclean(place->node);
		reiser4_tree_detach(tree, place->node);
		
		reiser4_tree_release(tree, place->node);
		place->node = NULL;
	}

	/* Drying tree up in the case root node has only one item */
	if (reiser4_node_items(tree->root) == 1 && !reiser4_tree_minimal(tree)) {
		if ((res = reiser4_tree_dryout(tree)))
			return res;
	}

	return 0;
}

/* Splits out the tree from passed @place up to the passed @level */
static errno_t reiser4_tree_split(reiser4_tree_t *tree, 
				  reiser4_place_t *place, 
				  uint8_t level)
{
	errno_t res;
	uint8_t curr;
	
	pos_t pos = {0, 0};
	reiser4_node_t *node;
	
	aal_assert("vpf-672", tree != NULL);
	aal_assert("vpf-673", place != NULL);
	aal_assert("vpf-813", place->node != NULL);
	aal_assert("vpf-674", level > 0);

	curr = reiser4_node_get_level(place->node);
	aal_assert("vpf-680", curr < level);
	
	while (curr < level) {
		aal_assert("vpf-676", place->node->p.node != NULL);
		
		if (!(place->pos.item == 0 && place->pos.unit == 0) && 
		    !(place->pos.item == reiser4_node_items(place->node)) && 
		    !(place->pos.item + 1 == reiser4_node_items(place->node) &&
		      place->pos.unit == reiser4_item_units(place)))
		{
			/* We are not on the border, split */
			if ((node = reiser4_tree_alloc(tree, curr)) == NULL) {
				aal_exception_error("Tree failed to allocate "
						    "a new node.");
				return -EINVAL;
			}
    
			if ((res = reiser4_tree_shift(tree, place, node,
						      SF_RIGHT | SF_UPTIP)))
			{
				aal_exception_error("Tree failed to shift "
						    "into a newly "
						    "allocated node.");
				goto error_free_node;
			}

			if (!place->node->p.node)
				reiser4_tree_growup(tree);
			
			if ((res = reiser4_tree_attach(tree, node))) {
				reiser4_tree_release(tree, node);
				
				aal_exception_error("Tree is failed to attach "
						    "node durring split opeartion.");
				goto error_free_node;
			}
		
		} else
			node = place->node;
		
		reiser4_place_init(place, node->p.node,
				   &node->p.pos);

		reiser4_place_inc(place, place->pos.unit == MAX_UINT32);
		
		curr++;
	}
	
	return 0;
	
 error_free_node:
	reiser4_node_close(node);
	return res;
}

/* Inserts new item/unit described by item hint into the tree */
errno_t reiser4_tree_insert(
	reiser4_tree_t *tree,	    /* tree new item will be inserted in */
	reiser4_place_t *place,	    /* place item or unit inserted at */
	uint8_t level,              /* level item/unit will be inserted on */
	create_hint_t *hint)        /* item hint to be inserted */
{
	int mode;
	errno_t res;
	uint32_t needed;
	uint32_t maxspace;
	
	reiser4_key_t *key;
	reiser4_place_t old;

	aal_assert("umka-779", tree != NULL);
	aal_assert("umka-779", hint != NULL);
	
	aal_assert("umka-1644", place != NULL);
	aal_assert("umka-1645", hint->plug != NULL);

	/* Checking if tree is fresh one, thus, it does not have the root
	   node. If so, we allocate new node of the requested level, insert
	   item/unit into it and then attach it into the empty tree by means of
	   using reiser4_tree_attach() function. This function will take care
	   about another things which should be done for keeping reiser4 tree in
	   tact and namely alloate new root and insert one nodeptr item into
	   it. */
	if (reiser4_tree_fresh(tree)) {

		if (level == LEAF_LEVEL) {
			if ((res = reiser4_tree_aroot(tree)))
				return res;
		}
		
		if (!(place->node = reiser4_tree_alloc(tree, level)))
			return -EINVAL;
		
		POS_INIT(&place->pos, 0, MAX_UINT32);
		
		if ((res = reiser4_item_estimate(place, hint)))
			return res;
		
		if ((res = reiser4_node_insert(place->node, &place->pos, hint))) {
			reiser4_tree_release(tree, place->node);
			return res;
		}

		if ((res = reiser4_tree_attach(tree, place->node))) {
			reiser4_tree_release(tree, place->node);
			return res;
		}

		return 0;
	}
	
	if ((res = reiser4_tree_lroot(tree)))
		return res;

	/* Checking if we have the tree with height smaller than requested
	   level. If so, we should grow the tree up to requested level. */
	if (level > reiser4_tree_height(tree)) {
		while (level > reiser4_tree_height(tree))
			reiser4_tree_growup(tree);

		/* Getting new place item/unit will be inserted at after tree is
		   growed up. It is needed because we want insert item onto
		   level equal to the requested one passed by @level
		   variable. */
		if (reiser4_tree_lookup(tree, &hint->key, level,
					place) == FAILED)
		{
			aal_exception_error("Lookup failed after "
					    "tree growed up to "
					    "requested level %d.",
					    level);
			return -EINVAL;
		}
	}

	old = *place;

	if (level < reiser4_node_get_level(place->node)) {
		/* Allocating node of requested level and assign place for
		   insert to it. */
		if (!(place->node = reiser4_tree_alloc(tree, level)))
			return -ENOSPC;

		POS_INIT(&place->pos, 0, MAX_UINT32);
	} else if (level > reiser4_node_get_level(place->node)) {
		
		/* Prepare the tree for insertion at the level @level. */
		if ((res = reiser4_tree_split(tree, place, level)))
			return res;
	}
	    
	/* Estimating item/unit to inserted to tree */
	if ((res = reiser4_item_estimate(place, hint)))
		return res;
	
	if (tree->traps.pre_insert) {
		if ((res = tree->traps.pre_insert(tree, place, hint,
						  tree->traps.data)))
			return res;
	}

	maxspace = reiser4_node_maxspace(place->node);
	
	/* Checking if item hint to be inserted to tree has length more than max
	   possible space in a node. */
	if (hint->len > maxspace) {
		aal_exception_error("Item size %u is too big. Maximal possible "
				    "item can be %u bytes long.", hint->len,
				    maxspace);
		return -EINVAL;
	}

	/* Saving mode of insert (insert new item, paste units to the existent
	   one) before making space for new inset/unit. */
	mode = (place->pos.unit == MAX_UINT32);
	
	needed = hint->len + (place->pos.unit == MAX_UINT32 ? 
		reiser4_node_overhead(place->node) : 0);
	    
	if ((res = reiser4_tree_expand(tree, place, needed, SF_DEFAULT))) {
		aal_exception_error("Can't expand the tree for insertion.");
		return res;
	}
	
	/* As position after making space is generaly changing, we check is mode
	   of insert was changed or not. If so, we should perform estimate one
	   more time. That is because, estimated value depends on insert
	   mode. */
	if (mode != (place->pos.unit == MAX_UINT32)) {
		if ((res = reiser4_item_estimate(place, hint)))
			return res;
	}

	/* Inserting item/unit and updating parent keys */
	if ((res = reiser4_node_insert(place->node, &place->pos, hint))) {
		aal_exception_error("Can't insert an item/unit into the "
				    "node %llu.", place->node->number);
		return res;
	}

	/* Parent keys will be updated if we inserted item/unit into leftmost
	   pos and if target node has the parent. */
	if (reiser4_place_leftmost(place) &&
	    place->node->p.node)
	{
		reiser4_place_t p;

		reiser4_place_init(&p, place->node->p.node,
				   &place->node->p.pos);
		
		if ((res = reiser4_tree_ukey(tree, &p, &hint->key)))
			return res;
	}
	
	/* If make space function allocates new node, we should attach it to the
	   tree. Also, here we should handle the special case, when tree root
	   should be changed. */
	if (place->node != tree->root && !place->node->p.node) {
		
		aal_assert("vpf-889", old.node != NULL);
		
		/* Growing the tree */
		if (!old.node->p.node)
			reiser4_tree_growup(tree);
	
		/* Attaching new node to the tree */
		if ((res = reiser4_tree_attach(tree, place->node))) {
			aal_exception_error("Can't attach node %llu to "
					    "the tree.", place->node->number);
			reiser4_tree_release(tree, place->node);
			return res;
		}
	}

	/* Initializing insert point place */
	if ((res = reiser4_place_fetch(place)))
		return res;

	/* Calling post_insert hook installed in tree */
	if (tree->traps.post_insert) {
		if ((res = tree->traps.post_insert(tree, place, hint,
						   tree->traps.data)))
			return res;
	}
    
	return 0;
}

/* Cuts some amount of items or units from the tree. Updates internal keys and
   children list. */
errno_t reiser4_tree_cut(
	reiser4_tree_t *tree,       /* tree for working with */
	reiser4_place_t *start,     /* place of the start */
	reiser4_place_t *end)       /* place of the end */
{
	errno_t res;
	reiser4_node_t *node;

	aal_assert("umka-1018", tree != NULL);
	aal_assert("umka-1725", start != NULL);
	aal_assert("umka-1782", end != NULL);

	node = reiser4_tree_neigh(tree, start->node, D_RIGHT);
	
	while (node && node != end->node)
		node = reiser4_tree_neigh(tree, node, D_RIGHT);

	if (node != end->node) {
		aal_exception_error("End place is not reachable from the"
				    "start one during cutting the tree.");
		return -EINVAL;
	}

	if (start->node != end->node) {
		pos_t pos = {MAX_UINT32, MAX_UINT32};

		/* Removing start + 1 though end - 1 node from the tree */
		node = reiser4_tree_neigh(tree, start->node, D_RIGHT);
		
		while (node && node != end->node) {
			reiser4_node_t *right;

			right = node->right;
			
			reiser4_node_mkclean(node);
			reiser4_tree_detach(tree, node);
			reiser4_tree_release(tree, node);
			
			node = right;
		}

		/* Removing items/units from the start node */
		pos.item = reiser4_node_items(start->node);

		if ((res = reiser4_node_cut(start->node, &start->pos, &pos)))
			return res;

		if (reiser4_place_leftmost(start) &&
		    start->node->p.node)
		{
			reiser4_place_t p;
			reiser4_key_t lkey;

			if ((res = reiser4_node_lkey(start->node, &lkey)))
				return res;
			
			reiser4_place_init(&p, start->node->p.node,
					   &start->node->p.pos);

			if ((res = reiser4_tree_ukey(tree, &p, &lkey)))
				return res;
		}
		
		if (reiser4_node_items(start->node) == 0) {
			reiser4_node_mkclean(start->node);
			reiser4_tree_detach(tree, start->node);
			
			reiser4_tree_release(tree, start->node);
			start->node = NULL;
		}
		
		/* Removing from the end node */
		pos.item = 0;
		
		if ((res = reiser4_node_cut(end->node, &pos, &end->pos)))
			return res;

		if (reiser4_place_leftmost(end) &&
		    end->node->p.node)
		{
			reiser4_place_t p;
			reiser4_key_t lkey;

			if ((res = reiser4_node_lkey(end->node, &lkey)))
				return res;
			
			reiser4_place_init(&p, end->node->p.node,
					   &end->node->p.pos);

			if ((res = reiser4_tree_ukey(tree, &p, &lkey)))
				return res;
		}
		
		if (reiser4_node_items(end->node) == 0) {
			reiser4_node_mkclean(end->node);
			reiser4_tree_detach(tree, end->node);
			
			reiser4_tree_release(tree, end->node);
			end->node = NULL;
		}

		/* Packing the tree at @start */
		if (start->node) {
			if ((tree->flags & TF_PACK) && tree->traps.pack) {
				errno_t res;
			
				if ((res = tree->traps.pack(tree, start,
							    tree->traps.data)))
					return res;
			}
		}

		/* Packing the tree at @end */
		if (end->node) {
			if ((tree->flags & TF_PACK) && tree->traps.pack) {
				errno_t res;
			
				if ((res = tree->traps.pack(tree, end,
							    tree->traps.data)))
					return res;
			}
		}
	} else {
		if ((res = reiser4_node_cut(start->node, &start->pos, &end->pos)))
			return res;

		if (reiser4_place_leftmost(start) &&
		    start->node->p.node)
		{
			reiser4_place_t p;
			reiser4_key_t lkey;

			if ((res = reiser4_node_lkey(start->node, &lkey)))
				return res;
			
			reiser4_place_init(&p, start->node->p.node,
					   &start->node->p.pos);

			if ((res = reiser4_tree_ukey(tree, &p, &lkey)))
				return res;
		}
		
		if (reiser4_node_items(start->node) > 0) {
			if (start->node) {
				if ((tree->flags & TF_PACK) && tree->traps.pack) {
					errno_t res;
			
					if ((res = tree->traps.pack(tree, start,
								    tree->traps.data)))
						return res;
				}
			}
		} else {
			reiser4_node_mkclean(start->node);
			reiser4_tree_detach(tree, start->node);
			
			reiser4_tree_release(tree, start->node);
			start->node = NULL;
		}

	}

	/* Drying tree up in the case root node has only one item */
	if (reiser4_node_items(tree->root) == 1 && !reiser4_tree_minimal(tree)) {
		if ((res = reiser4_tree_dryout(tree)))
			return res;
	}

	return 0;
}

/* Installs new pack handler. If it is NULL, default one will be used */
void reiser4_tree_pack_set(reiser4_tree_t *tree,
			   pack_func_t func)
{
	aal_assert("umka-1896", tree != NULL);

	tree->traps.pack = (func != NULL) ? func :
		callback_tree_pack;
}


/* Switches on/off flag, which displays whenever tree should pack itself after
   remove operations or not. It is needed because all operations like this
   should be under control. */
void reiser4_tree_pack_on(reiser4_tree_t *tree) {
	aal_assert("umka-1881", tree != NULL);
	tree->flags |= TF_PACK;
}

void reiser4_tree_pack_off(reiser4_tree_t *tree) {
	aal_assert("umka-1882", tree != NULL);
	tree->flags &= ~TF_PACK;
}

/* Removes item/unit at passed @place. This functions also perform so called
   "local packing". This is shift as many as possible items and units from the
   node pointed by @place into its left neighbour node and the same shift from
   the right neighbour into target node. This behavior may be controlled by
   tree's control flags (tree->flags) or by functions tree_pack_on() and
   tree_pack_off(). */
errno_t reiser4_tree_remove(
	reiser4_tree_t *tree,	  /* tree item will be removed from */
	reiser4_place_t *place,   /* place the item will be removed at */
	uint32_t count)
{
	errno_t res;

	aal_assert("umka-2055", tree != NULL);
	aal_assert("umka-2056", place != NULL);
	
	/* Calling "pre_remove" handler if it is defined */
	if (tree->traps.pre_remove) {
		if ((res = tree->traps.pre_remove(tree, place,
						  tree->traps.data)))
			return res;
	}

	/* Removing iten/unit from the node */
	if ((res = reiser4_node_remove(place->node, &place->pos, count)))
		return res;

	/* Updating left deleimiting key in all parent nodes */
	if (reiser4_place_leftmost(place) &&
	    place->node->p.node)
	{

		/* If node became empty it will be detached from the tree, so
		   updating is not needed and impossible, because it has no
		   items. */
		if (reiser4_node_items(place->node) > 0) {
			reiser4_place_t p;
			reiser4_key_t lkey;

			/* Updating parent keys */
			reiser4_node_lkey(place->node, &lkey);
				
			reiser4_place_init(&p, place->node->p.node,
					   &place->node->p.pos);

			if ((res = reiser4_tree_ukey(tree, &p, &lkey)))
				return res;
		}
	}
	
	/* Calling "post_remove" handler if it is defined */
	if (tree->traps.post_remove) {
		if ((res = tree->traps.post_remove(tree, place,
						   tree->traps.data)))
			return res;
	}

	/* Checking if the node became empty. If so, we release it, otherwise we
	   pack the tree about it. */
	if (reiser4_node_items(place->node) > 0) {
		if ((tree->flags & TF_PACK) && tree->traps.pack) {
			if ((res = tree->traps.pack(tree, place, tree->traps.data)))
				return res;
		}
	} else {
		/* Detaching node from the tree, because it became empty */
		reiser4_node_mkclean(place->node);
		reiser4_tree_detach(tree, place->node);

		/* Freeing node and updating place node component in order to
		   let user know that node do not exist any longer. */
		reiser4_tree_release(tree, place->node);
		place->node = NULL;
	}

	/* Drying tree up in the case root node has only one item */
	if (reiser4_node_items(tree->root) == 1 && !reiser4_tree_minimal(tree)) {
		if ((res = reiser4_tree_dryout(tree)))
			return res;
	}
	
	return 0;
}

errno_t reiser4_tree_down(
	reiser4_tree_t *tree,		/* tree for traversing it */
	reiser4_node_t *node,		/* node to be traversed */
	tree_open_func_t open_func,	/* callback for node opening */
	tree_edge_func_t before_func,	/* begin callback */
	tree_update_func_t update_func,	/* per child callback */
	tree_edge_func_t after_func,	/* end callback */
	void *data)			/* caller specific data */
{
	errno_t res = 0;
	reiser4_place_t place;
	pos_t *pos = &place.pos;
 
	aal_assert("vpf-390", node != NULL);
	aal_assert("umka-1935", tree != NULL);
	
	if (open_func == NULL)
		open_func = (tree_open_func_t)reiser4_tree_child;
	
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
			aal_exception_error("Can't open item by place. Node "
					    "%llu, item %u.", node->number,
					    pos->item);
			goto error_after_func;
		}

		if (!reiser4_item_branch(&place))
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
				continue;

			/* Traversing the node */
			if ((res = reiser4_tree_down(tree, child, open_func, 
						     before_func, update_func, 
						     after_func, data)) < 0)
			{
				goto error_after_func;
			}
			
			if (update_func && (res = update_func(tree, &place, data)))
				goto error_after_func;
		}
	}
	
	if (after_func)
		res = after_func(tree, node, data);

	reiser4_node_unlock(node);
	return res;

 error_after_func:
	if (after_func)
		res = after_func(tree, node, data);

 error_unlock_node:
	reiser4_node_unlock(node);
	return res;
}

/* Traverses tree with passed callback functions for each event (node open,
   etc). This is is used for all tree traverse related operations like copy,
   measurements, etc. */
errno_t reiser4_tree_traverse(
	reiser4_tree_t *tree,		/* node to be traversed */
	tree_open_func_t open_func,	/* callback for node opening */
	tree_edge_func_t before_func,	/* start callback */
	tree_update_func_t update_func,	/* after child callback */
	tree_edge_func_t after_func,	/* end callback */
	void *data)			/* caller specific data */
{
	errno_t res;
	
	aal_assert("umka-1768", tree != NULL);

	if ((res = reiser4_tree_lroot(tree)))
		return res;
	
	return reiser4_tree_down(tree, tree->root, open_func,
				 before_func, update_func,
				 after_func, data);
}

/* Makes copy of @node from @src_tree to @dst_tree */
reiser4_node_t *reiser4_tree_clone(reiser4_tree_t *src_tree,
				   reiser4_node_t *src_node,
				   reiser4_tree_t *dst_tree)
{
	rid_t pid;
	uint32_t level;
	
	reiser4_node_t *dst_node;
	aal_device_t *dst_device;
	
	dst_device = dst_tree->fs->device;
	pid = src_node->entity->plug->id.id;
	
	if (!(dst_node = reiser4_node_init(dst_device, src_node->size,
					   reiser4_fake_get(), pid,
					   src_tree->key.plug)))
	{
		aal_exception_error("Can't initialize destination "
				    "node durring tree copy.");
		return NULL;
	}

	level = reiser4_node_get_level(src_node);
	reiser4_node_form(dst_node, level);

	if (reiser4_node_clone(src_node, dst_node))
		goto error_free_dst_node;

	return dst_node;
	
 error_free_dst_node:
	reiser4_node_close(dst_node);
	return NULL;
}

struct move_hint {
	aal_list_t *path;
	reiser4_tree_t *tree;
};

typedef struct move_hint move_hint_t;

static errno_t copy_down(reiser4_tree_t *src_tree,
			 reiser4_node_t *src_node,
			 void *data)
{
	pos_t pos;
	errno_t res = 0;
	move_hint_t *hint;

	aal_list_t *last;
	reiser4_key_t lkey;
	aal_list_t *current;
	reiser4_node_t *parent;
	reiser4_node_t *dst_node;
	reiser4_tree_t *dst_tree;

	hint = (move_hint_t *)data;

	if (!(dst_tree = hint->tree))
		return -EINVAL;

	if (!(dst_node = reiser4_tree_clone(src_tree, src_node,
					    dst_tree)))
	{
		aal_exception_error("Can't clone node %llu.",
				    src_node->number);
		return -EINVAL;
	}

	last = aal_list_last(hint->path);
	parent = last ?	(reiser4_node_t *)last->data : NULL;
	
	dst_node->flags |= NF_FOREIGN;
	
	if ((res = reiser4_tree_connect(dst_tree, parent, dst_node))) {
		aal_exception_error("Can't connect node %llu to "
				    "target tree.", src_node->number);
		goto error_free_dst_node;
	}

	if ((res = reiser4_node_upos(dst_node)))
		return res;
	
	/* FIXME-UMKA: Here also should be extents handling */
	
	if (reiser4_node_get_level(src_node) > LEAF_LEVEL) {
		current = aal_list_append(hint->path, dst_node);
	
		if (!current->prev)
			hint->path = current;
	}
	
	reiser4_node_mkdirty(dst_node);
	
	return 0;

 error_free_dst_node:
	reiser4_node_close(dst_node);
	return res;
}

/* Cutting path by one from the end */
static errno_t copy_up(reiser4_tree_t *src_tree,
		       reiser4_node_t *src_node,
		       void *data)
{
	aal_list_t *next;
	aal_list_t *last;
	move_hint_t *hint;

	if (reiser4_node_get_level(src_node) > LEAF_LEVEL) {
		hint = (move_hint_t *)data;
		last = aal_list_last(hint->path);
		next = aal_list_remove(last, last->data);

		if (!next || !next->prev)
			hint->path = next;
	}
	
	return 0;
}

/* Makes copy of @src_tree to @dst_tree */
errno_t reiser4_tree_copy(reiser4_tree_t *src_tree,
			  reiser4_tree_t *dst_tree)
{
	move_hint_t hint;
	
	aal_assert("umka-2304", src_tree != NULL);
	aal_assert("umka-2305", dst_tree != NULL);

	hint.path = NULL;
	hint.tree = dst_tree;

	return reiser4_tree_traverse(src_tree, NULL, copy_down,
				     NULL, copy_up, &hint);
}

/* Resizes @tree by @blocks */
errno_t reiser4_tree_resize(reiser4_tree_t *tree,
			    count_t blocks)
{
	aal_assert("umka-2323", tree != NULL);
	return -EINVAL;
}
#endif
