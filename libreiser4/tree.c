/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tree.c -- reiser4 tree related code. */

#include <reiser4/reiser4.h>

#ifndef ENABLE_STAND_ALONE
/* Tree packing callback. Used for packing the tree, after delete items. This is
   needed for keeping tree in well packed state. */
static errno_t callback_tree_pack(reiser4_tree_t *tree,
				  place_t *place, void *data)
{
	aal_assert("umka-1897", tree != NULL);
	aal_assert("umka-1898", place != NULL);

	return reiser4_tree_shrink(tree, place);
}

/* Updates root block number in format by passed @blk. Takes care about correct
   block number in loaded root node if any. */
void reiser4_tree_set_root(reiser4_tree_t *tree, blk_t blk) {
	aal_assert("umka-2409", tree != NULL);
	aal_assert("umka-2412", tree->fs != NULL);
	aal_assert("umka-2413", tree->fs->format != NULL);
	
	reiser4_format_set_root(tree->fs->format, blk);
}

/* Updates height in format by passed @height. */
void reiser4_tree_set_height(reiser4_tree_t *tree, uint8_t height) {
	aal_assert("umka-2410", tree != NULL);
	aal_assert("umka-2416", tree->fs != NULL);
	aal_assert("umka-2417", tree->fs->format != NULL);
	
	reiser4_format_set_height(tree->fs->format, height);
}
#endif

/* Fetches data from the @tree to passed @hint */
int64_t reiser4_tree_fetch(reiser4_tree_t *tree, place_t *place,
			   trans_hint_t *hint)
{
	return plug_call(place->plug->o.item_ops->object,
			 fetch_units, place, hint);
}

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

#ifndef ENABLE_STAND_ALONE
static errno_t reiser4_tree_rehash_node(reiser4_tree_t *tree,
					node_t *node, blk_t new_blk)
{
	blk_t old_blk;
	blk_t *set_blk;

	old_blk = node_blocknr(node);
	reiser4_node_move(node, new_blk);

	if (!(set_blk = aal_calloc(sizeof(*set_blk), 0)))
		return -ENOMEM;

	*set_blk = new_blk;
	
	if (aal_hash_table_remove(tree->nodes, &old_blk))
		return -EINVAL;

	return aal_hash_table_insert(tree->nodes, set_blk, node);
}
#endif

/* Puts @node to @tree->nodes hash table. */
static errno_t reiser4_tree_hash_node(reiser4_tree_t *tree,
				      node_t *node)
{
	blk_t *blk;
	
	/* Registering @node in @tree->nodes hash table. */
	if (!(blk = aal_calloc(sizeof(*blk), 0)))
		return -ENOMEM;

	*blk = node_blocknr(node);

	return aal_hash_table_insert(tree->nodes, blk, node);
}

/* Removes @node from @tree->nodes hash table. */
static errno_t reiser4_tree_unhash_node(reiser4_tree_t *tree,
					node_t *node)
{
	blk_t blk = node_blocknr(node);
	return aal_hash_table_remove(tree->nodes, &blk);
}

/* Returns TRUE if passed @node is tree root node. */
static bool_t reiser4_tree_root_node(reiser4_tree_t *tree,
				     node_t *node)
{
	aal_assert("umka-2482", tree != NULL);
	aal_assert("umka-2483", node != NULL);
	
	return reiser4_tree_get_root(tree) == node_blocknr(node);
}

/* Dealing with loading root node if it is not loaded yet. */
errno_t reiser4_tree_load_root(reiser4_tree_t *tree) {
	blk_t root_blk;
	
	aal_assert("umka-1870", tree != NULL);

	/* Check if root is loaded. */
	if (tree->root)
		return 0;

	/* Checking if tree contains some nodes at all. It does not contain them
	   just after creation. The is root blk in format is set to
	   INVAL_BLK. */
	if (reiser4_tree_fresh(tree))
		return -EINVAL;

	/* Getting root node and loading it. */
	root_blk = reiser4_tree_get_root(tree);
	
	if (!(tree->root = reiser4_tree_load_node(tree, NULL, root_blk))) {
		aal_exception_error("Can't load root node %llu.",
				    root_blk);
		return -EIO;
	}

	tree->root->tree = tree;
	
	return 0;
}

#ifndef ENABLE_STAND_ALONE
/* Assignes passed @node to root. Takes care about root block number and tree
   height in format. */
static errno_t reiser4_tree_assign_root(reiser4_tree_t *tree,
					node_t *node)
{
	blk_t root_blk;
	uint32_t level;
	
	aal_assert("umka-1867", tree != NULL);
	aal_assert("umka-1868", node != NULL);

	/* Establishing connection between node and tree. */
	tree->root = node;
	node->tree = tree;

	/* Zeroing out parent pointer, as root has not parent. */
	node->p.node = NULL;

	/* Updating tree height. */
	level = reiser4_node_get_level(node);
	reiser4_tree_set_height(tree, level);

	/* Updating root block number. */
	root_blk = node_blocknr(tree->root);
	reiser4_tree_set_root(tree, root_blk);

	return reiser4_tree_connect_node(tree, NULL, node);
}

/* Dealing with allocating root node if it is not allocated yet */
static errno_t reiser4_tree_alloc_root(reiser4_tree_t *tree) {
	node_t *root;
	uint32_t height;
	
	aal_assert("umka-1869", tree != NULL);

	/* Root exists already? */
	if (tree->root)
		return 0;

	/* Tree is fresh? */
	if (!reiser4_tree_fresh(tree))
		return -EINVAL;

	/* Allocating node with level of tree height. */
	height = reiser4_tree_get_height(tree);

	if (!(root = reiser4_tree_alloc_node(tree, height)))
		return -ENOSPC;

	/* Assign new node to root. */
	return reiser4_tree_assign_root(tree, root);
}
#endif

/* Registers passed node in tree and connects left and right neighbour
   nodes. This function does not do any tree modifications. */
errno_t reiser4_tree_connect_node(reiser4_tree_t *tree,
				  node_t *parent, node_t *node)
{
	aal_assert("umka-1857", tree != NULL);
	aal_assert("umka-2261", node != NULL);

	node->tree = tree;

	if (reiser4_tree_root_node(tree, node)) {
		/* This is the case when we connect root node, that is with no
		   parent. */
		tree->root = node;
	} else {
		aal_assert("umka-3003", parent != NULL);

		/* Assigning parent, locking it asit is refferenced by
		   @node->p.node and updating @node->p.pos. */
		node->p.node = parent;

		if (reiser4_node_realize(node))
			return -EINVAL;

		reiser4_node_lock(parent);
	}
	
	return reiser4_tree_hash_node(tree, node);
}

/* Remove specified child from the node children list. Updates all neighbour
   pointers and parent pointer.*/
errno_t reiser4_tree_disconnect_node(reiser4_tree_t *tree,
				     node_t *node)
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

	if (reiser4_tree_root_node(tree, node)) {
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
					node_t *node)
{
	uint32_t i;
	errno_t res;
	
	for (i = 0; i < reiser4_node_items(node); i++) {
		blk_t blk;
		uint32_t j;

		node_t *child;
		place_t place;

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

			/* Getting loaded child node. If it is not loaded, we
			   don't have to update its parent pos. */
			if (!(child = reiser4_tree_lookup_node(tree, blk)))
				continue;

			/* Update @child parent pos. */
			child->p.node = node;
			
			if ((res = reiser4_node_realize(child)))
				return res;
		}
	}

	return 0;
}
#endif

node_t *reiser4_tree_lookup_node(reiser4_tree_t *tree, blk_t blk) {
	aal_assert("umka-3002", tree != NULL);
	return aal_hash_table_lookup(tree->nodes, &blk);
}

/* Loads node from @blk and connects it to @parent. */
node_t *reiser4_tree_load_node(reiser4_tree_t *tree,
			       node_t *parent, blk_t blk)
{
	node_t *node = NULL;

	aal_assert("umka-1289", tree != NULL);

	/* Checking if node in the local cache of @parent. */
	if (!(node = reiser4_tree_lookup_node(tree, blk))) {
		aal_assert("umka-3004", !reiser4_fake_ack(blk));
		
		/* Check for memory pressure event. If memory pressure is uppon
		   us, we call memory cleaning function. For now we call
		   tree_adjust() in order to release not locked nodes. */
		if (tree->mpc_func && tree->mpc_func()) {
			/* Adjusting the tree. It will be finished as soon as
			   memory pressure condition will gone. */
			if (tree->root) {
				reiser4_tree_adjust_node(tree, tree->root);
			} else {
				aal_exception_warn("Tree seem to be empty, but "
						   "there is memory pressure event "
						   "occured.");
			}
		}
		
		/* Node is not loaded yet. Loading it and connecting to @parent
		   node cache. */
		if (!(node = reiser4_node_open(tree, blk))) {
			aal_exception_error("Can't open node %llu.", blk);
			return NULL;
		}

		/* Connect loaded node to cache. */
		if (reiser4_tree_connect_node(tree, parent, node)) {
			aal_exception_error("Can't connect node %llu "
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
errno_t reiser4_tree_unload_node(reiser4_tree_t *tree, node_t *node) {
	errno_t res;
	
	aal_assert("umka-1840", tree != NULL);
	aal_assert("umka-1842", node != NULL);

#ifndef ENABLE_STAND_ALONE
	/* Check if node is dirty. */
	if (reiser4_node_isdirty(node)) {
		aal_exception_warn("Unloading dirty node %llu.",
				   node_blocknr(node));
	}
#endif

	/* Disconnecting @node from its parent node. */
	if (node->tree) {
		if ((res = reiser4_tree_disconnect_node(tree, node))) {
			aal_exception_error("Can't disconnect node %llu "
					    "from tree cache.",
					    node_blocknr(node));
			return res;
		}
	}

	/* Releasing node instance. */
	return reiser4_node_close(node);
}

/* Loads denoted by passed nodeptr @place child node */
node_t *reiser4_tree_child_node(reiser4_tree_t *tree,
				place_t *place)
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

/* Finds both left and right neighbours and connects them into the tree. */
static node_t *reiser4_tree_ltrt_node(reiser4_tree_t *tree,
				      node_t *node, uint32_t where)
{
        int found = 0;
        place_t place;
        uint32_t level;
                                                                                      
	aal_assert("umka-2213", tree != NULL);
	aal_assert("umka-2214", node != NULL);

	level = 0;

	reiser4_node_lock(node);
	reiser4_place_assign(&place, node, 0, MAX_UINT32);
                                                                                      
        /* Going up to the level where corresponding neighbour node may be
	   obtained by its nodeptr item. */
        while (place.node->p.node && found == 0) {
		aal_memcpy(&place, &place.node->p, sizeof(place));

		/* Checking position. Level is found if position is not first
		   (right neighbour) and is not last one (left neighbour). */
		if (where == DIR_LEFT) {
			found = reiser4_place_gtfirst(&place);
		} else {
			found = reiser4_place_ltlast(&place);
		}

                level++;
        }
                                                                                      
        if (found == 0)
                goto error_unlock_node;

	/* Position correcting */
        place.pos.item += (where == DIR_LEFT ? -1 : 1);
                                                                                      
        /* Going down to the level of @node */
        while (level > 0) {
                if (!(place.node = reiser4_tree_child_node(tree, &place)))
			goto error_unlock_node;

		if (where == DIR_LEFT) {
			if (reiser4_place_last(&place))
				goto error_unlock_node;
		} else {
			if (reiser4_place_first(&place))
				goto error_unlock_node;
		}
		
                level--;
        }
                                                                                      
	reiser4_node_unlock(node);
	
        /* Setting up neightbour links */
        if (where == DIR_LEFT) {
                node->left = place.node;
                place.node->right = node;
        } else {
                node->right = place.node;
                place.node->left = node;
        }
	
	return place.node;

error_unlock_node:
	reiser4_node_unlock(node);
	return NULL;
}

/* Moves @place by one item to right. If node is over, returns node next to
   passed @place. Needed for moving though the tree node by node, for instance
   in directory read code. */
errno_t reiser4_tree_next_node(reiser4_tree_t *tree, 
			       place_t *place, place_t *next)
{
	aal_assert("umka-867", tree != NULL);
	aal_assert("umka-868", place != NULL);
	aal_assert("umka-1491", next != NULL);

	/* Check if we have to get right neoghbour node. */
	if (place->pos.item >= reiser4_node_items(place->node) - 1) {
		reiser4_tree_neigh_node(tree, place->node, DIR_RIGHT);

		/* There is no right neighbour. */
		if (!place->node->right) {
			aal_memset(next, 0, sizeof(*next));
			return 0;
		}

		/* Assigning new coord to @place. */
		reiser4_place_assign(next, place->node->right, 0, 0);
	} else {
		/* Assigning new coord to @place. */
		reiser4_place_assign(next, place->node,
				     place->pos.item + 1, 0);
	}

	/* Initializing @place. */
	return reiser4_place_fetch(next);
}

/* Gets left or right neighbour nodes. */
node_t *reiser4_tree_neigh_node(reiser4_tree_t *tree,
				node_t *node, uint32_t where)
{
	aal_assert("umka-2219", node != NULL);
	aal_assert("umka-1859", tree != NULL);

	/* Parent is not present. The root node */
	if (!node->p.node)
		return NULL;

	if (where == DIR_LEFT && node->left)
		return node->left;

	if (where == DIR_RIGHT && node->right)
		return node->right;

	return reiser4_tree_ltrt_node(tree, node, where);
}

#ifndef ENABLE_STAND_ALONE
/* Requests block allocator for new block and creates empty node in it. */
node_t *reiser4_tree_alloc_node(reiser4_tree_t *tree,
				uint8_t level)
{
	rid_t pid;
	blk_t fake_blk;
	uint32_t stamp;
	uint64_t free_blocks;
	node_t *node;
    
	aal_assert("umka-756", tree != NULL);
    
	/* Setting up of the free blocks in format */
	if (!(free_blocks = reiser4_format_get_free(tree->fs->format)))
		return NULL;

	/* Check for memory pressure event. */
	if (tree->mpc_func && tree->mpc_func() && tree->root) {
		/* Memory pressure is here, trying to release nodes. */
		if (reiser4_tree_adjust_node(tree, tree->root)) {
			aal_exception_warn("Error when adjusting "
					   "tree during allocating "
					   "new node.");
		}
	}

	/* Allocating fake block number. */
	fake_blk = reiser4_fake_get();
	pid = reiser4_param_value("node");

	reiser4_format_set_free(tree->fs->format, free_blocks - 1);

	/* Creating new node. */
	if (!(node = reiser4_node_create(tree, fake_blk, pid, level))) {
		aal_exception_error("Can't initialize new fake node.");
		return NULL;
	}

	/* Setting flush stamps to new node */
	stamp = reiser4_format_get_stamp(tree->fs->format);
	reiser4_node_set_mstamp(node, stamp);
	
	if (tree->root) {
		stamp = reiser4_node_get_fstamp(tree->root);
		reiser4_node_set_fstamp(node, stamp);
	}

	node->tree = tree;
	return node;
}

/* Unload node and releasing it in block allocator */
errno_t reiser4_tree_release_node(reiser4_tree_t *tree,
				  node_t *node)
{
	reiser4_alloc_t *alloc;
	
	aal_assert("umka-1841", tree != NULL);
	aal_assert("umka-2255", node != NULL);

	alloc = tree->fs->alloc;
	reiser4_node_mkclean(node);

	/* Check if we're trying to releas a node with fake block number. If
	   not, free it in block allocator too. */
	if (!reiser4_fake_ack(node_blocknr(node))) {
		blk_t blk = node_blocknr(node);
		reiser4_alloc_release(alloc, blk, 1);
	}

	/* Updating free blocks in super block */
	reiser4_format_set_free(tree->fs->format,
				reiser4_alloc_free(alloc) + 1);
	
	return reiser4_tree_unload_node(tree, node);
}

/* Releases node from tree previously detaching it if @detach is 1. This
   function is just for making bunch of actions which occured often enough. */
static void reiser4_tree_discard_node(reiser4_tree_t *tree,
				      node_t *node, int detach)
{
	aal_assert("umka-2671", tree != NULL);
	aal_assert("umka-2672", node != NULL);

	if (detach)
		reiser4_tree_detach_node(tree, node);
	
	reiser4_node_mkclean(node);
	reiser4_tree_release_node(tree, node);
}

/* Helper function for freeing passed key instance tree's data hashtable entry
   is going to be removed. */
static void callback_data_keyrem_func(void *key) {
	reiser4_key_free((reiser4_key_t *)key);
}

/* Helper function for freeing hash value, that is, data block. */
static void callback_data_valrem_func(void *val) {
	aal_block_free((aal_block_t *)val);
}

/* Helper function for calculating 64-bit hash by passed key. This is used for
   tree's data hash. */
static uint64_t callback_data_hash_func(void *key) {
	return (reiser4_key_get_objectid((reiser4_key_t *)key) +
		reiser4_key_get_offset((reiser4_key_t *)key));
}

/* Helper function for comparing two keys during tree's data hash lookups. */
static int callback_data_comp_func(void *key1, void *key2,
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
		aal_exception_error("Can't find key plugin by its "
				    "id 0x%x.", pid);
		return -EINVAL;
	}

	return reiser4_fs_root_key(tree->fs, &tree->key);
}

/* Initializes tree instance on passed filesystem and return it to caller. Then
   it may be used for modifying tree, making lookup, etc. */
reiser4_tree_t *reiser4_tree_init(reiser4_fs_t *fs) {
	reiser4_tree_t *tree;

	aal_assert("umka-737", fs != NULL);

	/* Allocating memory for tree instance */
	if (!(tree = aal_calloc(sizeof(*tree), 0)))
		return NULL;

	tree->fs = fs;
	tree->fs->tree = tree;

#ifndef ENABLE_STAND_ALONE
	tree->bottom = TWIG_LEVEL;
#endif
	
	/* Initializing hash table for storing loaded formatted nodes in it. */
	if (!(tree->nodes = aal_hash_table_alloc(callback_nodes_hash_func,
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
	if (!(tree->data = aal_hash_table_alloc(callback_data_hash_func,
						callback_data_comp_func,
						callback_data_keyrem_func,
						callback_data_valrem_func)))
	{
		goto error_free_nodes;
	}
#endif

	/* Building tree root key. It is used in tree lookup, etc. */
	if (reiser4_tree_key(tree)) {
		aal_exception_error("Can't build the tree "
				    "root key.");
		goto error_free_data;
	}
    
#ifndef ENABLE_STAND_ALONE
	/* Initializing packing trap. */
	reiser4_tree_pack_on(tree);
	tree->traps.pack = callback_tree_pack;
#endif
		
	return tree;

 error_free_data:
#ifndef ENABLE_STAND_ALONE
	aal_hash_table_free(tree->data);
error_free_nodes:
#endif
	aal_hash_table_free(tree->nodes);
 error_free_tree:
	aal_free(tree);
	return NULL;
}

/* Closes specified tree (frees all assosiated with tree memory). */
void reiser4_tree_fini(reiser4_tree_t *tree) {
	aal_assert("umka-134", tree != NULL);

#ifndef ENABLE_STAND_ALONE
	/* Flushing tree cache */
	reiser4_tree_sync(tree);
#endif

	/* Releasing all loaded formetted nodes and tree itself */
	reiser4_tree_close(tree);
}

/* Closes specified tree (frees all assosiated memory). */
void reiser4_tree_close(reiser4_tree_t *tree) {
	aal_assert("vpf-1316", tree != NULL);

	/* Releasing loaded formatted nodes. */
	reiser4_tree_collapse(tree);

	/* Releasing unformatted nodes hash table. */
#ifndef ENABLE_STAND_ALONE
	aal_hash_table_free(tree->data);
#endif
	
	/* Releasing fomatted nodes hash table. */
	aal_hash_table_free(tree->nodes);

	/* Detaching tree instance from fs. */
	tree->fs->tree = NULL;
	
	/* Freeing tree instance. */
	aal_free(tree);
}

#ifndef ENABLE_STAND_ALONE
/* Allocates nodeptr item at passed @place. */
static errno_t reiser4_tree_alloc_nodeptr(reiser4_tree_t *tree,
					  place_t *place)
{
	node_t *node;
	uint32_t units;
	
	units = plug_call(place->plug->o.item_ops->balance,
			  units, place);

	for (place->pos.unit = 0; place->pos.unit < units;
	     place->pos.unit++)
	{
		blk_t blk = reiser4_item_down_link(place);

		if (!reiser4_fake_ack(blk))
			continue;

		/* Checking for loaded node. If it is, then we move it new
		   allocated node blk. */
		if (!(node = reiser4_tree_lookup_node(tree, blk))) {
			aal_exception_error("Can't find node by its "
					    "nodeptr %llu.", blk);
			return -EINVAL;
		}
		
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

/* Allocates extent item at passed @place. */
static errno_t reiser4_tree_alloc_extent(reiser4_tree_t *tree,
					 place_t *place)
{
	errno_t res;
	uint32_t units;
	ptr_hint_t ptr;
	uint32_t blksize;
	trans_hint_t hint;

	units = plug_call(place->plug->o.item_ops->balance,
			  units, place);

	blksize = reiser4_tree_get_blksize(tree);
	
	for (place->pos.unit = 0; place->pos.unit < units;
	     place->pos.unit++)
	{
		uint64_t width;
		uint64_t blocks;
		uint64_t offset;
		key_entity_t key;

		/* Fetching extent infomation. */
		hint.count = 1;
		hint.specific = &ptr;

		if (plug_call(place->plug->o.item_ops->object,
			      fetch_units, place, &hint) != 1)
		{
			return -EIO;
		}

		/* Check if we have accessed unallocated extent */
		if (ptr.start != EXTENT_UNALLOC_UNIT)
			continue;

		/* Getting unit key */
		plug_call(place->plug->o.item_ops->balance,
			  fetch_key, place, &key);

		/* Checking if some data assigned to this unit. */
		if (!aal_hash_table_lookup(tree->data, &key))
			continue;

		/* Loop until all units get allocated */
		for (blocks = 0, width = ptr.width;
		     width > 0; width -= ptr.width)
		{
			blk_t blk;
			uint32_t i;
			int first = 1;
			aal_block_t *block;
			
			/* Trying to allocate @ptr.width blocks. */
			if (!(ptr.width = reiser4_alloc_allocate(tree->fs->alloc,
								 &ptr.start, width)))
			{
				return -ENOSPC;
			}

			if (first) {
				/* Updating extent item data */
				if (plug_call(place->plug->o.item_ops->object,
					      update_units, place, &hint) != 1)
				{
					return -EIO;
				}
			} else {
				errno_t res;
				uint32_t level;
				place_t iplace;

				iplace = *place;
				iplace.pos.unit++;

				/* Insert new extent units */
				level = reiser4_node_get_level(iplace.node);
				
				if ((res = reiser4_tree_insert(tree, &iplace,
							       &hint, level)) < 0)
				{
					return res;
				}

                                /* Updating @place by insert point, as it might
				   be moved due to balancing. */
				*place = iplace;
				place->pos.unit--;

				/* Updating key by allocated blocks in order to
				   keep it in correspondence to right data
				   block. */
				offset = plug_call(key.plug->o.key_ops,
						   get_offset, &key);

				plug_call(key.plug->o.key_ops, set_offset,
					  &key, offset + (blocks * blksize));

				units++;
			}

			/* Moving data blocks to right places, saving them and
			   releasing from the cache. */
			for (blk = ptr.start, i = 0;
			     i < ptr.width; i++, blk++)
			{
				/* Getting data block by @key */
				block = aal_hash_table_lookup(tree->data, &key);
				aal_assert("umka-2469", block != NULL);

				/* Moving block tro @blk */
				aal_block_move(block, tree->fs->device, blk);

				/* Saving block to device. */
				if ((res = aal_block_write(block))) {
					aal_exception_error("Can't write block "
							    "%llu.", block->nr);
					return res;
				}

				/* Releasing cache entry */
				aal_hash_table_remove(tree->data, &key);

				/* Updating the key to find next data block */
				offset = plug_call(key.plug->o.key_ops,
						   get_offset, &key);

				plug_call(key.plug->o.key_ops, set_offset,
					  &key, offset + blksize);
			}
			
			first = 0;
			blocks += ptr.width;
		}
	}
	
	return 0;
}
#endif

/* Flushes some part of tree cache (recursively) to device starting from passed
   @node. This function is used for allocating part of tree and flusing it to
   device on memory pressure event or on tree_sync() call. */
errno_t reiser4_tree_adjust_node(reiser4_tree_t *tree,
				 node_t *node)
{
	aal_assert("umka-2302", tree != NULL);
	aal_assert("umka-2303", node != NULL);

#ifndef ENABLE_STAND_ALONE
	/* We are dealing only with dirty nodes. */
	if (reiser4_node_isdirty(node)) {
		uint32_t i;
		errno_t res;
		blk_t allocnr;

		/* Requesting block allocator to allocate the real block number
		   for fake allocated node. */
		if (reiser4_fake_ack(node_blocknr(node))) {
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

		/* Allocating all children nodes if we are up on
		   @tree->bottom. */
		if (reiser4_node_get_level(node) >= tree->bottom) {
			/* Going though the all items in node and allocating
			   them if needed. */
			for (i = 0; i < reiser4_node_items(node); i++) {
				place_t place;

				/* Initializing item at @i. */
				reiser4_place_assign(&place, node,
						     i, MAX_UINT32);

				if ((res = reiser4_place_fetch(&place)))
					return res;

				/* It is not good, that we refference here to
				   particular item group. But, we have to do so,
				   considering, that this is up tree to know
				   about items type in it. Probably this is why
				   tree should be plugin too to handle things
				   like this in more flexible manner. */
				if (place.plug->id.group == NODEPTR_ITEM) {
					blk_t blk;
					uint32_t j;
					node_t *child;
					
					/* Allocating unallocated nodeptr item
					   at @place. */
					if ((res = reiser4_tree_alloc_nodeptr(tree, &place)))
						return res;

					for (j = 0; j < reiser4_item_units(&place); j++) {
						/* Getting child node by its
						   nodeptr. If child is loaded,
						   we call tree_adjust_node()
						   onit recursively in order to
						   allocate it and its items. */
						place.pos.unit = j;
			
						blk = reiser4_item_down_link(&place);

						if (!(child = reiser4_tree_lookup_node(tree, blk)))
							continue;

						if ((res = reiser4_tree_adjust_node(tree, child)))
							return res;
					}
				} else if (place.plug->id.group == EXTENT_ITEM) {
					/* Allocating unallocated extent item at
					   @place. */
					if ((res = reiser4_tree_alloc_extent(tree, &place)))
						return res;
				}
			}
		}
	}

        /* Updating free space counter in format. */
	{
		count_t free_blocks;
			
		free_blocks = reiser4_alloc_free(tree->fs->alloc);
		reiser4_format_set_free(tree->fs->format, free_blocks);
	}
#endif

	/* If node is locked, that is not a leaf or it is used by someone, it
	   cannot be released, and thus, it does not make sense to save it to
	   device. */
	if (!reiser4_node_locked(node)) {
#ifndef ENABLE_STAND_ALONE
		/* Okay, node is allocated and ready to be saved to device. */
		if (reiser4_node_isdirty(node) && reiser4_node_sync(node)) {
			aal_exception_error("Can't write node %llu.",
					    node_blocknr(node));
			return -EIO;
		}

#endif
		/* Unloading node from tree cache. */
		reiser4_tree_unload_node(tree, node);
	}

	return 0;
}

/* Walking though the tree cache and closing all nodes. */
errno_t reiser4_tree_walk_node(reiser4_tree_t *tree, node_t *node,
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

		node_t *child;
		place_t place;

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

/* Pack tree to make it more compact. Needed for fsck to pack tree after each
   pass and in the case of lack of disk space. */
errno_t reiser4_tree_pack(reiser4_tree_t *tree) {
	aal_assert("umka-3000", tree != NULL);
	return 0;
}

/* Saves all dirty nodes in tree to device tree lies on.. */
errno_t reiser4_tree_sync(reiser4_tree_t *tree) {
	errno_t res;
	
	aal_assert("umka-2259", tree != NULL);

	if (!tree->root)
		return 0;

        /* Flushing formatted nodes starting from root node with memory pressure
	   flag set to 0, that is do not check memory presure, and save
	   everything. */
	if ((res = reiser4_tree_adjust_node(tree, tree->root))) {
		aal_exception_error("Can't save formatted nodes "
				    "to device.");
		return res;
	}

	/* Flushing unformatted blocks (extents data) attached to @tree->data
	   hash table. */
	if ((res = aal_hash_table_foreach(tree->data,
					  callback_save_block, tree)))
	{
		aal_exception_error("Can't save unformatted nodes "
				    "to device.");
		return res;
	}
	
	return res;
}
#endif

/* Returns 1 if tree has not root node and 0 otherwise. Tree has not root just
   after format instance is created and tree is initialized on fs with it. And
   thus tree has not any nodes in it. */
bool_t reiser4_tree_fresh(reiser4_tree_t *tree) {
	aal_assert("umka-1930", tree != NULL);
	return (reiser4_tree_get_root(tree) == INVAL_BLK);
}

/* Unloads all tree nodes from memory. Used in tree_collapse(). */
errno_t reiser4_tree_collapse(reiser4_tree_t *tree) {
	aal_assert("umka-2265", tree != NULL);

	if (!tree->root)
		return 0;

	return reiser4_tree_walk_node(tree, tree->root,
				      reiser4_tree_unload_node);
}

#ifndef ENABLE_STAND_ALONE
/* Makes search of the leftmost item/unit with the same key as passed @key is
   starting from @place. This is needed to work with key collisions. */
static errno_t reiser4_tree_leftmost(reiser4_tree_t *tree,
				     place_t *place, reiser4_key_t *key)
{
	place_t walk;
	
	aal_assert("umka-2396", key != NULL);
	aal_assert("umka-2388", tree != NULL);
	aal_assert("umka-2389", place != NULL);

	if (reiser4_place_fetch(place))
		return -EINVAL;

	aal_memcpy(&walk, place, sizeof(*place));
			
	/* Main loop until leftmost node reached */
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
				switch (plug_call(walk.plug->o.item_ops->balance,
						  lookup, &walk, key, FIND_EXACT))
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

		/* Getting left neighbour node */
		reiser4_tree_neigh_node(tree, walk.node, DIR_LEFT);

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

/* Makes search in the tree by specified @key. Fills passed place by data of
   found item. That is body pointer, plugin, etc. */
lookup_t reiser4_tree_lookup(reiser4_tree_t *tree, reiser4_key_t *key,
			     uint8_t level, bias_t bias, place_t *place)
{
	lookup_t res;
	reiser4_key_t wan;

	aal_assert("umka-742", key != NULL);
	aal_assert("umka-1760", tree != NULL);
	aal_assert("umka-2057", place != NULL);

	/* We store @key in @wan. All consequence code will use @wan. This is
	   needed, because @key might point to @place->item.key in @place and
	   will be corrupted during lookup. */
	reiser4_key_assign(&wan, key);

	/* Making sure that root exists. If not, getting out with @place
	   initialized by NULL root. */
	if (reiser4_tree_fresh(tree)) {
		reiser4_place_assign(place, NULL,
				     0, MAX_UINT32);
		return ABSENT;
	} else {
		if ((res = reiser4_tree_load_root(tree)) < 0)
			return res;
		
		reiser4_place_assign(place, tree->root,
				     0, MAX_UINT32);
	}

	/* Checking the case when wanted key is smaller than root one. This is
	   the case, when somebody is trying go up of the root by ".." entry in
	   root directory. If so, we initialize the key to be looked up by root
	   key. */
	if (reiser4_key_compfull(&wan, &tree->key) < 0)
		reiser4_key_assign(&wan, &tree->key);
		    
	while (1) {
		uint32_t curr_level = reiser4_node_get_level(place->node);
		bias_t curr_bias = (curr_level > level ? FIND_EXACT : bias);
		
		/* Looking up for key inside node. Result of lookuping will be
		   stored in &place->pos. */
		res = reiser4_node_lookup(place->node, &wan,
					  curr_bias, &place->pos);

		/* Check if we should finish lookup because we reach stop level
		   or some error occured during last node lookup. */
		if (curr_level <= level || res < 0) {
			if (res == PRESENT) {
#ifndef ENABLE_STAND_ALONE
				/* If collision handling is allwoed, we will
				   find leftmost coord with the same key. This
				   is needed for correct key collisions
				   handling. */
				if (reiser4_tree_leftmost(tree, place, &wan)) {
					aal_exception_error("Can't find leftmost "
							    "position during lookup.");
					return -EIO;
				}
#endif	
				/* Fetching item at @place if key is found */
				reiser4_place_fetch(place);
			}
			
			return res;
		}

		/* Initializing @place. This should be done before using any
		   item methods or access @place fields. */
		if (reiser4_place_valid(place)) {
			if (reiser4_place_fetch(place))
				return -EIO;

			/* Checking is item at @place is nodeptr one. If not, we
			   correct posision back. */
			if (!reiser4_item_branch(place->plug))
				return res;

			/* Loading node by its nodeptr item at @place */
			if (!(place->node = reiser4_tree_child_node(tree, place)))
				return -EIO;
		} else {
			return ABSENT;
		}
	}
    
	return ABSENT;
}

/* Reads data from the @tree from @place to passed @hint */
int64_t reiser4_tree_read(reiser4_tree_t *tree, place_t *place,
			  trans_hint_t *hint)
{
	return plug_call(place->plug->o.item_ops->object,
			 read_units, place, hint);
}

/* Reads reads some number of bytes from @tree to @hint. This function is used
   in tail conversion and for reading data from the files. */
int64_t reiser4_tree_read_flow(reiser4_tree_t *tree,
			       trans_hint_t *hint)
{
	char *buff;
	errno_t res;
	int64_t total;
	uint64_t size;
	reiser4_key_t key;

	buff = hint->specific;
	reiser4_key_assign(&key, &hint->offset);
	
	for (total = 0, size = hint->count; size > 0; ) {
		int32_t read;
		place_t place;

		/* Looking for the place to read. */
		if ((res = reiser4_tree_lookup(tree, &hint->offset,
					       LEAF_LEVEL, FIND_EXACT,
					       &place)) < 0)
		{
			return res;
		}

		/* Data does not found. This may mean, that we have hole in tree
		   between keys. */
		if (res == ABSENT) {
			uint64_t hole_size;
			uint64_t next_offset;
			uint64_t look_offset;
			
			/* Here we suppose, that @place points to next item,
			   just behind the hole. */
			if ((res = reiser4_place_fetch(&place)))
				return res;

			next_offset = reiser4_key_get_offset(&place.key);
			look_offset = reiser4_key_get_offset(&hint->offset);

			hole_size = next_offset - look_offset;
			read = (hole_size > size ? size : hole_size);

			/* Making holes in buffer */
			aal_memset(hint->specific, 0, read);
		} else {
			/* Prepare hint for read */
			hint->tree = tree;
			hint->count = size;
		
			/* Read data from the tree */
			if ((read = reiser4_tree_read(tree, &place, hint)) < 0) {
				return read;
			} else {
				if (read == 0)
					break;
			}
		}

		size -= read;
		total += read;

		/* Updating key and data buffer pointer */
		hint->specific += read;
		reiser4_key_inc_offset(&hint->offset, read);
	}

	hint->specific = buff;
	reiser4_key_assign(&hint->offset, &key);
	
	return total;
}

#ifndef ENABLE_STAND_ALONE
/* Returns 1 if passed @tree has minimal possible height and thus cannot be
   dried out. Othersize 0 is returned. */
static bool_t reiser4_tree_minimal(reiser4_tree_t *tree) {
	return (reiser4_tree_get_height(tree) <= 2);
}

/* Returns 1 if root node contain one item, that is, tree is singular and should
   be dried out. Otherwise 0 is returned. */
static bool_t reiser4_tree_singular(reiser4_tree_t *tree) {
	return (reiser4_node_items(tree->root) == 1);
}

/* Updates key at passed @place by passed @key by means of using
   node_update_key() functions in recursive maner. This function is used for
   update all internal left delimiting keys after balancing on underlying
   levels. */
errno_t reiser4_tree_update_key(reiser4_tree_t *tree, place_t *place,
				reiser4_key_t *key)
{
	errno_t res;
	
	aal_assert("umka-1892", tree != NULL);
	aal_assert("umka-1893", place != NULL);
	aal_assert("umka-1894", key != NULL);

	/* Getting into recursion if we should update leftmost key. */
	if (reiser4_place_leftmost(place)) {
		
		if (place->node->p.node) {
			place_t *p = &place->node->p;
			
			if ((res = reiser4_tree_update_key(tree, p, key)))
				return res;
		}
	}

	/* Update key in parent node. */
	return reiser4_node_update_key(place->node, &place->pos, key);
}

/* This function inserts new nodeptr item to the tree and in such way attaches
   passed @node to tree. It also connects passed @node into tree cache. */
errno_t reiser4_tree_attach_node(reiser4_tree_t *tree, node_t *node) {
	rid_t pid;
	errno_t res;
	uint8_t level;
	
	place_t place;
	trans_hint_t hint;
	ptr_hint_t nodeptr_hint;

	aal_assert("umka-913", tree != NULL);
	aal_assert("umka-916", node != NULL);
    
	/* Preparing nodeptr item hint */
	aal_memset(&hint, 0, sizeof(hint));

	hint.count = 1;
	hint.specific = &nodeptr_hint;

	/* Prepare nodeptr hint. */
	nodeptr_hint.width = 1;
	nodeptr_hint.start = node_blocknr(node);

	pid = reiser4_param_value("nodeptr");
	reiser4_node_leftmost_key(node, &hint.offset);

	if (!(hint.plug = reiser4_factory_ifind(ITEM_PLUG_TYPE,
						pid)))
	{
		aal_exception_error("Can't find item plugin by "
				    "its id 0x%x.", pid);
		return -EINVAL;
	}

	level = reiser4_node_get_level(node) + 1;

	/* Looking up for the insert point place */
	if ((res = reiser4_tree_lookup(tree, &hint.offset, level,
				       FIND_CONV, &place)) < 0)
	{
		/* Lookup is failed. Tree is corrupted? */
		return res;
	}

	/* Inserting node pointer item into tree. Here we do not analize @res
	   returned by tree_lookup(), because both remaining values ABSENT and
	   PRESENT are possible. First one for the case when we insert item with
	   key that exists in tree (key collision) and second case is useful
	   case, when key is not in tree. */
	if ((res = reiser4_tree_insert(tree, &place, &hint, level)) < 0) {
		aal_exception_error("Can't insert nodeptr item "
				    "to the tree.");
		return res;
	}

	/* Connecting node to tree cache. */
	if ((res = reiser4_tree_connect_node(tree, place.node, node))) {
		aal_exception_error("Can't connect node %llu to "
				    "tree cache.", node_blocknr(node));
		return res;
	}

	/* Getting left and right neighbours. */
	reiser4_tree_neigh_node(tree, node, DIR_LEFT);
	reiser4_tree_neigh_node(tree, node, DIR_RIGHT);
	
	return 0;
}

/* Removes passed @node from the on-disk tree and cache structures. That is
   removes nodeptr item from the tree and node instance itself from its parent
   children list. */
errno_t reiser4_tree_detach_node(reiser4_tree_t *tree,
				 node_t *node)
{
	place_t p;
	errno_t res;
	trans_hint_t hint;
	
	aal_assert("umka-1726", tree != NULL);
	aal_assert("umka-1727", node != NULL);

        /* Disconnecting node from tree cache */
	p = node->p;
	hint.count = 1;

	if (node->tree) {
		if ((res = reiser4_tree_disconnect_node(tree, node))) {
			aal_exception_error("Can't disconnect node %llu "
					    "from tree cache.",
					    node_blocknr(node));
			return res;
		}
	}
	
	/* Removing nodeptr item/unit from @p->node. */
	return reiser4_tree_remove(tree, &p, &hint);
}

/* This function forces tree to grow by one level and sets it up after the
   growing. This occures when after next balancing root node needs to accept new
   nodeptr item, but has not free space enough.  */
errno_t reiser4_tree_growup(
	reiser4_tree_t *tree)	/* tree to be growed up */
{
	errno_t res;
	node_t *new_root;
	node_t *old_root;
	uint32_t new_height;

	aal_assert("umka-1701", tree != NULL);
	aal_assert("umka-1736", tree->root != NULL);
	
	if ((res = reiser4_tree_load_root(tree)))
		return res;
	
	old_root = tree->root;
	new_height = reiser4_tree_get_height(tree) + 1;
    
	/* Allocating new root node */
	if (!(new_root = reiser4_tree_alloc_node(tree, new_height)))
		return -ENOSPC;

	/* Assign new root node, changing tree height and root node
	   blk in format. */
	if ((res = reiser4_tree_assign_root(tree, new_root)))
		return res;

	old_root->p.node = new_root;

	/* Attaching old root node to tree. */
	if ((res = reiser4_tree_attach_node(tree, old_root)))
		goto error_return_root;

	return 0;

 error_return_root:
	reiser4_tree_release_node(tree, new_root);
	reiser4_tree_assign_root(tree, old_root);
	
	return res;
}

/* Decreases tree height by one level. This occurs when tree gets singular (root
   has one nodeptr item) after one of removals. */
errno_t reiser4_tree_dryout(reiser4_tree_t *tree) {
	errno_t res;
	place_t place;
	node_t *new_root;
	node_t *old_root;

	aal_assert("umka-1731", tree != NULL);
	aal_assert("umka-1737", tree->root != NULL);

	if (reiser4_tree_minimal(tree))
		return -EINVAL;

	/* Rasing up the root node if it exists. */
	if ((res = reiser4_tree_load_root(tree)))
		return res;

	old_root = tree->root;
	
	/* Check if we can dry tree out safely. */
	if (reiser4_node_items(old_root) > 1)
		return -EINVAL;

	/* Getting new root as the first child of the old root node */
	reiser4_place_assign(&place, old_root, 0, 0);

	if (!(new_root = reiser4_tree_child_node(tree, &place))) {
		aal_exception_error("Can't load new root during "
				    "drying tree out.");
		return -EINVAL;
	}

	/* Disconnect old root and its child from the tree */
	reiser4_tree_disconnect_node(tree, old_root);
	reiser4_tree_disconnect_node(tree, new_root);

	/* Assign new root. Setting tree height to new root level and root block
	   number to new root block number. */
	reiser4_tree_assign_root(tree, new_root);
	
        /* Releasing old root node */
	reiser4_tree_discard_node(tree, old_root, 0);
	
	return 0;
}

/* Tries to shift items and units from @place to passed @neig node. After that
   it's finished, place will contain new insert point, which may be used for
   inserting item/unit to it. */
errno_t reiser4_tree_shift(reiser4_tree_t *tree, place_t *place,
			   node_t *neig, uint32_t flags)
{
	errno_t res;
	node_t *node;

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

	/* Updating @node and @neig children's parent position. */
	if ((res = reiser4_tree_update_node(tree, node)))
		return res;
	
	if ((res = reiser4_tree_update_node(tree, neig)))
		return res;
	
	/* Updating insert point by pos returned from node_shift(). */
	place->pos = hint.pos;

	/* Check if insert point was moved to neighbour node. If so, assign
	   neightbour node to insert point coord. */
	if (hint.result & SF_MOVE_POINT)
		place->node = neig;

	/* Updating left delimiting keys in the tree */
	if (hint.control & SF_LEFT_SHIFT) {

		/* Check if we need update key in insert part of tree. That is
		   if source node is not empty and there was actually moved at
		   least one item or unit. */
		if (reiser4_node_items(node) > 0 &&
		    (hint.items > 0 || hint.units > 0))
		{

			/* Check if node is connected to tree or it is not root
			   and updating left delimiting keys makes sense at
			   all. */
			if (node->p.node) {
				place_t p;

				/* Getting leftmost key from @node.  */
				reiser4_node_leftmost_key(node, &lkey);

				/* Recursive updating of all internal keys that
				   supposed to be updated. */
				reiser4_place_init(&p, node->p.node,
						   &node->p.pos);
				
				reiser4_key_assign(&node->p.key, &lkey);
				
				if ((res = reiser4_tree_update_key(tree, &p,
								   &lkey)))
				{
					return res;
				}
			}
		}
	} else {
		/* The same checks and insernal keys update actions as for left
		   shift. Diference is that, we will get leftmost key from
		   neighbour node, becaause its leftmost key will be changed as
		   soon as at least one item or unit is shifted to it. */
		if (hint.items > 0 || hint.units > 0) {

			if (neig->p.node) {
				place_t p;
				
				reiser4_node_leftmost_key(neig, &lkey);
				
				reiser4_place_init(&p, neig->p.node,
						   &neig->p.pos);
				
				reiser4_key_assign(&neig->p.key, &lkey);
				
				if ((res = reiser4_tree_update_key(tree, &p,
								   &lkey)))
				{
					return res;
				}
			}
		}
	}

	return 0;
}

/* Takes care about @left and @right nodes after shifting data to right node if
   it was new allocated one. */
static errno_t reiser4_tree_care(reiser4_tree_t *tree,
				 node_t *left, node_t *right)
{
	errno_t res;
	
	if (reiser4_tree_root_node(tree, left)) {
		/* Growing the tree in the case we splitted the root
		   node. Root node has not parent. */
		if ((res = reiser4_tree_growup(tree)))
			return res;
	} else {
		/* Releasing old node, because it got empty as result of data
		   shifting. */
		if (reiser4_node_items(left) == 0)
			reiser4_tree_discard_node(tree, left, 1);
	}

	/* Attaching new allocated node into the tree, if it is not
	   empty */
	if (reiser4_node_items(right) > 0) {
		/* Attaching new node to the tree */
		if ((res = reiser4_tree_attach_node(tree, right)))
			return res;
	}
	
	return 0;
}

/* Makes space in tree to insert @needed bytes of data. Returns space in insert
   point, or negative value for errors. */
int32_t reiser4_tree_expand(reiser4_tree_t *tree, place_t *place,
			    uint32_t needed, uint32_t flags)
{
	int alloc;
	errno_t res;
	int32_t enough;
	uint32_t overhead;

	node_t *left;
	node_t *right;

	aal_assert("umka-766", place != NULL);
	aal_assert("umka-929", tree != NULL);

	/* Check if tree is fresh. If so, allocating new node with level of tree
	   height and assigning it to passed @place. This may happen if this
	   function will be called by user on empty tree. */
	if (reiser4_tree_fresh(tree)) {
		uint8_t level = reiser4_tree_get_height(tree);
		
		if (!(place->node = reiser4_tree_alloc_node(tree, level)))
			return -ENOSPC;

		POS_INIT(&place->pos, 0, MAX_UINT32);
		
		return reiser4_node_space(place->node) -
			reiser4_node_overhead(place->node);
	}

	overhead = reiser4_node_overhead(place->node);
	
	/* Adding node overhead to @needed. */
	if (place->pos.unit == MAX_UINT32)
		needed += overhead;

	/* Check if there is enough of space in insert point node. If so -- do
	   nothing, but exit. */
	if ((enough = reiser4_node_space(place->node) - needed) > 0) {
		enough = reiser4_node_space(place->node);
		
		if (place->pos.unit == MAX_UINT32)
			enough -= overhead;

		return enough;
	}

	/* Shifting data into left neighbour if it exists and left shift
	   allowing flag is specified. */
	if ((SF_LEFT_SHIFT & flags) &&
	    (left = reiser4_tree_neigh_node(tree, place->node, DIR_LEFT)))
	{
		uint32_t left_flags = (SF_LEFT_SHIFT | SF_UPDATE_POINT);

		if (SF_ALLOW_MERGE & flags)
			left_flags |= SF_ALLOW_MERGE;
		
		/* Shift items from @place to @left neighbour. */
		if ((res = reiser4_tree_shift(tree, place, left, left_flags)))
			return res;

		/* Check fo result of shift -- space in node. */
		if ((enough = reiser4_node_space(place->node) - needed) > 0) {
			enough = reiser4_node_space(place->node);
		
			if (place->pos.unit == MAX_UINT32)
				enough -= overhead;

			return enough;
		}
	}

	/* Shifting data into right neighbour if it exists and right shift
	   allowing flag is specified. */
	if ((SF_RIGHT_SHIFT & flags) &&
	    (right = reiser4_tree_neigh_node(tree, place->node, DIR_RIGHT)))
	{
		uint32_t right_flags = (SF_RIGHT_SHIFT | SF_UPDATE_POINT);
		
		if (SF_ALLOW_MERGE & flags)
			right_flags |= SF_ALLOW_MERGE;
		
		/* Shift items from @place to @right neighbour. */
		if ((res = reiser4_tree_shift(tree, place, right, right_flags)))
			return res;

		/* Check if node has enough of space and fucntion should do
		   nothing but exit with success return code. */
		if ((enough = reiser4_node_space(place->node) - needed) > 0) {
			enough = reiser4_node_space(place->node);
		
			if (place->pos.unit == MAX_UINT32)
				enough -= overhead;

			return enough;
		}
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
	   it. There are possible two tries to allocate new node and shift
	   insert point to it. */
	for (alloc = 0; enough < 0 && alloc < 2; alloc++) {
		place_t save;
		node_t *node;
		uint8_t level;
		uint32_t alloc_flags;

		/* Saving place as it will be usefull for us later */
		save = *place;

		/* Allocating new node of @level */
		level = reiser4_node_get_level(place->node);
	
		if (!(node = reiser4_tree_alloc_node(tree, level)))
			return -ENOSPC;

		/* Setting up shift flags */
		alloc_flags = (SF_RIGHT_SHIFT | SF_UPDATE_POINT |
			       SF_ALLOW_MERGE);

		if (SF_ALLOW_MERGE & flags)
			alloc_flags |= SF_ALLOW_MERGE;
		
		/* We will allow to move insert point to neighbour node if we
		   are at first iteration in this loop or if place points behind
		   the last unit of last item in current node. */
		if (alloc == 0 || !reiser4_place_ltlast(place))
			alloc_flags |= SF_MOVE_POINT;

		/* Shift data from @place to @node. Updating @place by new
		   insert point. */
		if ((res = reiser4_tree_shift(tree, place, node, alloc_flags)))
			return res;

		/* Taking care about new allocated @node and possible gets free
		   @save.node (attaching, detaching from the tree, etc.). */
		if ((res = reiser4_tree_care(tree, save.node, node))) {
			reiser4_tree_discard_node(tree, node, 0);
			return res;
		}

		/* Checking if it is enough of space in @place */
		enough = (reiser4_node_space(place->node) - needed);

		/* If it is not enopugh the space and insert point was actually
		   moved to neighbour node, we set @place to @save and give it
		   yet another try to make space.*/
		if (enough < 0 && place->node != save.node) {
			*place = save;
			enough = (reiser4_node_space(place->node) - needed);
		}
	}

	/* Return value of free space in insert point node. */
	enough = reiser4_node_space(place->node);
		
	if (place->pos.unit == MAX_UINT32)
		enough -= overhead;
	
	return enough;
}

/* Packs node in @place by means of using shift into/from neighbours */
errno_t reiser4_tree_shrink(reiser4_tree_t *tree, place_t *place) {
	errno_t res;
	uint32_t flags;
	node_t *left, *right;

	aal_assert("umka-1784", tree != NULL);
	aal_assert("umka-1783", place != NULL);

	/* Shift flags to be used in packing. */
	flags = (SF_LEFT_SHIFT | SF_ALLOW_MERGE);
	
	/* Packing node in order to keep the tree in well packed state
	   anyway. Here we will shift data from the target node to its left
	   neighbour node. */
	if ((left = reiser4_tree_neigh_node(tree, place->node, DIR_LEFT))) {
		if ((res = reiser4_tree_shift(tree, place, left, flags))) {
			aal_exception_error("Can't pack node %llu into left.",
					    node_blocknr(place->node));
			return res;
		}
	}
		
	if (reiser4_node_items(place->node) > 0) {
		/* Shifting the data from the right neigbour node into the
		   target node. */
		if ((right = reiser4_tree_neigh_node(tree, place->node,
						     DIR_RIGHT)))
		{
			place_t bogus;

			bogus.node = right;
	    
			if ((res = reiser4_tree_shift(tree, &bogus,
						      place->node,
						      flags)))
			{
				aal_exception_error("Can't pack node "
						    "%llu into left.",
						    node_blocknr(right));
				return res;
			}

			/* Check if node got enmpty. If so then we release
			   it. */
			if (reiser4_node_items(right) == 0)
				reiser4_tree_discard_node(tree, right, 1);
		}
	} else {
		/* Release node, because it got empty. */
		reiser4_tree_discard_node(tree, place->node, 1);
	}

	/* Drying tree up in the case root node has only one item */
	if (reiser4_tree_singular(tree) && !reiser4_tree_minimal(tree)) {
		if ((res = reiser4_tree_dryout(tree)))
			return res;
	}

	return 0;
}

/* Splits out the tree from passed @place up until passed @level is
   reached. This is used in fsck and in extents write code. */
static errno_t reiser4_tree_split(reiser4_tree_t *tree, 
				  place_t *place, uint8_t level)
{
	errno_t res;
	node_t *node;
	uint32_t flags;
	uint32_t curr_level;
	
	aal_assert("vpf-674", level > 0);
	aal_assert("vpf-672", tree != NULL);
	aal_assert("vpf-673", place != NULL);
	aal_assert("vpf-813", place->node != NULL);

	curr_level = reiser4_node_get_level(place->node);
	aal_assert("vpf-680", curr_level < level);

	/* Loop until desired @level is reached.*/
	while (curr_level < level) {
		aal_assert("vpf-676", place->node->p.node != NULL);

		/* Check if @place points inside node. That is should we split
		   node or not. */
		if (!reiser4_place_leftmost(place) &&
		    !reiser4_place_rightmost(place))
		{
			/* We are not on the border, split @place->node. That is
			   allocate new right neighbour node and move all item
			   right to @place->pos to new allocated node. */
			if (!(node = reiser4_tree_alloc_node(tree, curr_level))) {
				aal_exception_error("Tree failed to allocate "
						    "a new node.");
				return -EINVAL;
			}

			flags = (SF_RIGHT_SHIFT | SF_UPDATE_POINT |
				 SF_ALLOW_MERGE);
			
			/* Perform shift. */
			if ((res = reiser4_tree_shift(tree, place, node,
						      flags)))
			{
				aal_exception_error("Tree failed to shift "
						    "into a newly "
						    "allocated node.");
				goto error_free_node;
			}

			/* Check if we should grow up the tree */
			if (reiser4_tree_root_node(tree, place->node)) {
				if ((res = reiser4_tree_growup(tree)))
					return res;
			}

			/* Attach new node to tree. */
			if ((res = reiser4_tree_attach_node(tree, node))) {
				reiser4_tree_discard_node(tree, node, 0);
				aal_exception_error("Tree is failed to attach "
						    "node during split opeartion.");
				goto error_free_node;
			}

			/* Updating @place by parent coord from @place. */
			reiser4_place_init(place, node->p.node, &node->p.pos);
		} else {
			node = place->node;

			/* There is nothing to move out. We are on node border
			   (rightmost or leftmost). Here we should just go up by
			   one level and increment position if @place was at
			   rightmost position in node. */
			if (reiser4_place_rightmost(place)) {
				bool_t whole;
				
				reiser4_place_init(place, node->p.node,
						   &node->p.pos);

				/* Increment position. */
				whole = (place->pos.unit == MAX_UINT32);
				reiser4_place_inc(place, whole);
			} else {
				reiser4_place_init(place, node->p.node,
						   &node->p.pos);
			}
		}

		curr_level++;
	}
	
	return 0;
	
 error_free_node:
	reiser4_node_close(node);
	return res;
}

/* Installs new pack handler. If it is NULL, default one will be used. Pack
   handler is a fucntion, which is called when tree need to be packed after some
   actions like remove. */
void reiser4_tree_pack_set(reiser4_tree_t *tree,
			   pack_func_t func)
{
	aal_assert("umka-1896", tree != NULL);
	tree->traps.pack = func ? func : callback_tree_pack;
}

/* Switches on/off pack flag, which displays whether tree should pack itself
   after remove operations. */
void reiser4_tree_pack_on(reiser4_tree_t *tree) {
	aal_assert("umka-1881", tree != NULL);
	tree->flags |= TF_PACK;
}

void reiser4_tree_pack_off(reiser4_tree_t *tree) {
	aal_assert("umka-1882", tree != NULL);
	tree->flags &= ~TF_PACK;
}

/* Releases passed region in block allocator. This is used in tail during tree
   trunacte. */
static errno_t callback_region_func(void *entity, uint64_t start,
				    uint64_t width, void *data)
{
	reiser4_tree_t *tree = (reiser4_tree_t *)data;
	return reiser4_alloc_release(tree->fs->alloc, start, width);
}

/* Writes flow described by @hint to tree. Takes care about keys in index part
   of tree, root updatings, etc. Returns number of bytes actually written. */
int64_t reiser4_tree_write_flow(reiser4_tree_t *tree,
				trans_hint_t *hint)
{
	char *buff;
	errno_t res;
	uint64_t size;
	uint64_t bytes;
	uint64_t total;
	reiser4_key_t key;

	buff = hint->specific;
	reiser4_key_assign(&key, &hint->offset);

	/* Loop until desired number of bytes is written. */
	for (total = bytes = 0, size = hint->count; size > 0;) {
		int32_t write;
		uint32_t level;
		place_t place;

		hint->count = size;

		/* Looking for place to write. */
		if ((res = reiser4_tree_lookup(tree, &hint->offset,
					       LEAF_LEVEL, FIND_CONV,
					       &place)) < 0)
		{
			return res;
		}

		/* Making decission if we should write data to leaf level or to
		   twig. Probably this may be improved somehow. */
		if (hint->plug->id.group == TAIL_ITEM) {
			level = LEAF_LEVEL;
		} else {
			level = TWIG_LEVEL;
		}
		
		/* Writing data to tree. */
		if ((write = reiser4_tree_write(tree, &place,
						hint, level)) < 0)
		{
			return write;
		} else {
			if (write == 0)
				break;
		}

		/* Updating counters */
		size -= write;
		total += write;
		bytes += hint->bytes;
		
		/* Updating key and buffer pointer */
		if (hint->specific) {
			hint->specific += write;
		}
		
		reiser4_key_inc_offset(&hint->offset, write);
	}

	hint->bytes = bytes;
	hint->specific = buff;
	reiser4_key_assign(&hint->offset, &key);
	
	return total;
}

/* Truncates item pointed by @hint->offset key by value stored in
   @hint->count. This is used during tail conversion and in object plugins
   truncate() code path. */
int64_t reiser4_tree_trunc_flow(reiser4_tree_t *tree,
				trans_hint_t *hint)
{
	errno_t res;
	int64_t trunc;
	uint32_t size;
	uint64_t total;
	key_entity_t key;

	aal_assert("umka-2475", tree != NULL);
	aal_assert("umka-2476", hint != NULL);

	reiser4_key_assign(&key, &hint->offset);

	/* Setting up region func to release region callback. It is needed for
	   releasing extent blocks. */
	hint->region_func = callback_region_func;

	for (total = 0, size = hint->count; size > 0;
	     size -= trunc, total += trunc)
	{
		place_t place;
		
		if ((res = reiser4_tree_lookup(tree, &hint->offset,
					       LEAF_LEVEL, FIND_EXACT,
					       &place)) < 0)
		{
			return res;
		}

		/* Nothing found by @hint->offset. This means, that tree has a
		   hole between keys. We will handle this, as it is needed for
		   fsck. */
		if (res == ABSENT) {
			uint64_t hole_size;
			uint64_t next_offset;
			uint64_t look_offset;

			/* Emulating truncating unexistent item. */
			if ((res = reiser4_place_fetch(&place)))
				return res;

			next_offset = reiser4_key_get_offset(&place.key);
			look_offset = reiser4_key_get_offset(&hint->offset);

			hole_size = next_offset - look_offset;
			trunc = (hole_size > size ? size : hole_size);

			reiser4_key_inc_offset(&hint->offset, trunc);
			continue;
		}

		hint->count = size;

		/* Calling node truncate method. */
		if ((trunc = reiser4_node_trunc(place.node, &place.pos,
						hint)) < 0)
		{
			return trunc;
		}
		
		/* Updating left delimiting keys in all parent nodes */
		if (reiser4_place_leftmost(&place) &&
		    place.node->p.node)
		{
			/* If node became empty it will be detached from the
			   tree, so updating is not needed and impossible,
			   because it has no items. */
			if (reiser4_node_items(place.node) > 0) {
				place_t p;
				reiser4_key_t lkey;

				/* Updating parent keys */
				reiser4_node_leftmost_key(place.node, &lkey);
				
				reiser4_place_init(&p, place.node->p.node,
						   &place.node->p.pos);

				reiser4_key_assign(&place.node->p.key, &lkey);

				if ((res = reiser4_tree_update_key(tree, &p,
								   &lkey)))
				{
					return res;
				}
			}
		}
	
		/* Checking if the node got empty. If so, we release it. */
		if (reiser4_node_items(place.node) > 0) {
			if ((tree->flags & TF_PACK) && tree->traps.pack) {
				if ((res = tree->traps.pack(tree, &place,
							    tree->traps.data)))
				{
					return res;
				}
			}
		} else {
			/* Release @place.node, as it gets empty.  */
			reiser4_tree_discard_node(tree, place.node, 1);
		}

		/* Drying tree up in the case root node has only one item */
		if (reiser4_tree_singular(tree) && !reiser4_tree_minimal(tree))	{
			if ((res = reiser4_tree_dryout(tree)))
				return res;
		}

		reiser4_key_inc_offset(&hint->offset, trunc);
	}

	reiser4_key_assign(&hint->offset, &key);
	return total;
}

/* Converts file body at @hint->offset from tail to extent or from extent to
   tail. Main tail convertion function. It uses tree_read_flow(),
   tree_truc_flow() and tree_write_flow(). */
errno_t reiser4_tree_conv_flow(reiser4_tree_t *tree,
			       conv_hint_t *hint)
{
	char *buff;
	errno_t res;
	int64_t conv;
	uint64_t size;
	trans_hint_t trans;
	
	aal_assert("umka-2406", tree != NULL);
	aal_assert("umka-2407", hint != NULL);
	aal_assert("umka-2481", hint->plug != NULL);

	reiser4_key_assign(&trans.offset, &hint->offset);

	/* Check if convertion chunk is zero. If so -- use filesystem block
	   size. */
	if (hint->chunk == 0)
		hint->chunk = reiser4_tree_get_blksize(tree);
	
	/* Loop until @size bytes is converted. */
	for (size = hint->count, hint->bytes = 0;
	     size > 0; size -= conv)
	{
		/* Each convertion tick may be divided onto tree stages:

		   (1) Read convert chunk (@hint->chunk bytes long now) to
		   @trans hint.

		   (2) Truncate data in tree we have just read described by
		   @trans hint.

		   (3) Write data back to tree with target item plugin used for
		   writing (tail plugin if we convert extents to tails and
		   extent plugin is used otherwise).
		*/
		
		/* Preparing buffer to read data to it. */
		trans.count = hint->chunk;

		if (trans.count > size)
			trans.count = size;

		if (!(buff = aal_calloc(trans.count, 0)))
			return -ENOMEM;

		trans.specific = buff;

		/* First stage -- reading data from tree. */
		if ((conv = reiser4_tree_read_flow(tree, &trans)) < 0) {
			res = conv;
			goto error_free_buff;
		}
		
		/* Second statge -- removing data from the tree. */
		trans.data = tree;
		trans.count = conv;

		if ((conv = reiser4_tree_trunc_flow(tree, &trans)) < 0) {
			res = conv;
			goto error_free_buff;
		}

		trans.count = conv;
		trans.plug = hint->plug;
		
		/* Third stage -- writing data back to tree with new item plugin
		   used.*/
		if ((conv = reiser4_tree_write_flow(tree, &trans)) < 0) {
			res = conv;
			goto error_free_buff;
		}

		hint->bytes += trans.bytes;
		reiser4_key_inc_offset(&trans.offset, conv);

		aal_free(buff);
	}

	return 0;
	
 error_free_buff:
	aal_free(buff);
	return res;
}

/* Estimates how many bytes is needed to insert data described by @hint. */
static errno_t callback_prep_insert(place_t *place, 
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
static errno_t callback_prep_write(place_t *place, 
				   trans_hint_t *hint) 
{
	aal_assert("umka-3007", hint != NULL);
	aal_assert("umka-3008", place != NULL);

	hint->len = 0;
	hint->overhead = 0;

	return plug_call(hint->plug->o.item_ops->object,
			 prep_write, place, hint);
}

/* Main function for tree modifications. It is used for inserting data to tree
   (stat data items, directries) or writting (tails, extents). */
int64_t reiser4_tree_modify(reiser4_tree_t *tree, place_t *place,
			    trans_hint_t *hint, uint8_t level,
			    estimate_func_t estimate_func,
			    modify_func_t modify_func)
{
	bool_t mode;
	errno_t res;
	place_t old;

	int32_t space;
	int32_t write;
	uint32_t needed;

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
		
		while (level > reiser4_tree_get_height(tree)) {
			if (reiser4_tree_growup(tree))
				return -EINVAL;
		}

		/* Getting new place item/unit will be inserted at after tree is
		   growed up. It is needed because we want to insert item into
		   the node of the given @level bu after tree_growup() and thus
		   rebalancing we need to get correct position where to insert
		   item. */
		if ((res = reiser4_tree_lookup(tree, &hint->offset, level,
					       FIND_CONV, place) < 0))
		{
			aal_exception_error("Lookup failed after "
					    "tree growed up to "
					    "requested level %d.",
					    level);
			return res;
		}
	}

	/* Handling the case when tree is empty (just after tree is initialized
	   by tree_init() function). */
	if (!reiser4_tree_fresh(tree)) {
		old = *place;
		
		if (level < reiser4_node_get_level(place->node)) {
			/* Allocating node of requested level and assign place
			   for insert to it. */
			if (!(place->node = reiser4_tree_alloc_node(tree, level)))
				return -ENOSPC;

			POS_INIT(&place->pos, 0, MAX_UINT32);
		} else if (level > reiser4_node_get_level(place->node)) {
			/* Prepare the tree for insertion at the level
			   @level. */
			if ((res = reiser4_tree_split(tree, place, level)))
				return res;
		}
	} else {
		old.node = NULL;

		/* Allocating root node and assign insert point to it */
		if ((res = reiser4_tree_alloc_root(tree)))
			return res;

		if (level == reiser4_tree_get_height(tree)) {
			reiser4_place_assign(place, tree->root,
					     0, MAX_UINT32);
		} else {
			if (!(place->node = reiser4_tree_alloc_node(tree, level)))
				return -ENOMEM;
			
			POS_INIT(&place->pos, 0, MAX_UINT32);
		}
	}
	
	/* Estimating item/unit to inserted/written to tree. */
	if ((res = estimate_func(place, hint)))
		return res;
	
	/* Needed space to be prepared in tree */
	needed = hint->len + hint->overhead;
	mode = (place->pos.unit == MAX_UINT32);

	/* Preparing space in tree. */
	if ((space = reiser4_tree_expand(tree, place, needed, SF_DEFAULT)) < 0) {
		aal_exception_error("Can't prepare space in tree.");
		return space;
	}

	/* Checking if we still have less space than needed. This is ENOSPC case
	   if we tried to insert data. And normal case for writtig data, because
	   we can write at least one byte. */
	if ((uint32_t)space < needed) {

		/* Check if we insert file body items. If so, we can insert only
		   part of passed stream. Error will be returned otherwise. */
		if (hint->plug->id.group != TAIL_ITEM &&
		    hint->plug->id.group != EXTENT_ITEM)
		{
			return -ENOSPC;
		}
		
		/* Check is we have spece at all. */
		if (!(hint->len = hint->count = space))
			return -ENOSPC;
	}

	/* Making yet another estimate if insert mode is changed after making
	   space. That is if we wanted to insert new unit into existent item,
	   but insert point was moved to new empty node and thus, we need to
	   insert new item. As item may has an overhead like directory one has,
	   we should take it to acount. */
	if (mode != (place->pos.unit == MAX_UINT32)) {
		if ((res = estimate_func(place, hint)))
			return res;
	}

	/* Inserting/writing data to node. */
	if ((write = reiser4_node_modify(place->node, &place->pos,
					 hint, modify_func)) < 0)
	{
		aal_exception_error("Can't insert data to node %llu.",
				    node_blocknr(place->node));
		return write;
	}

	if ((res = reiser4_tree_update_node(tree, place->node)))
		return res;
	
	/* Parent keys will be updated if we inserted item/unit into leftmost
	   pos and if target node has parent. */
	if (reiser4_place_leftmost(place) && place->node != tree->root) {
		place_t *parent = &place->node->p;
		
		if (parent->node) {
			reiser4_key_assign(&place->node->p.key,
					   &hint->offset);
			
			if ((res = reiser4_tree_update_key(tree, parent,
							   &hint->offset)))
			{
				return res;
			}
		}
	}
	
	/* If make space function allocates new node, we should attach it to the
	   tree. Also, here we should handle the special case, when tree root
	   should be changed. */
	if (place->node != tree->root && !place->node->p.node) {
		if (old.node && reiser4_tree_root_node(tree, old.node)) {
			if (reiser4_tree_growup(tree))
				return -EINVAL;
		}
		
		/* Attaching new node to the tree */
		if ((res = reiser4_tree_attach_node(tree, place->node)))
			return res;
	}
	
	/* Initializing insert point place. */
	if ((res = reiser4_place_fetch(place)))
		return res;

	return write;
}

/* Inserts data to the tree. This function is used for inserting items which are
 * not file body items, that is statdata, directory, etc. */
int64_t reiser4_tree_insert(reiser4_tree_t *tree, place_t *place,
			    trans_hint_t *hint, uint8_t level)
{
	aal_assert("umka-779", tree != NULL);
	aal_assert("umka-779", hint != NULL);
	
	aal_assert("umka-1644", place != NULL);
	aal_assert("umka-1645", hint->plug != NULL);

	return reiser4_tree_modify(tree, place, hint, level, 
				   callback_prep_insert,
				   reiser4_node_insert);
}

/* Writes data to the tree. used for puting tail and extents to tree. */
int64_t reiser4_tree_write(reiser4_tree_t *tree, place_t *place,
			   trans_hint_t *hint, uint8_t level)
{
	aal_assert("umka-2441", tree != NULL);
	aal_assert("umka-2442", hint != NULL);
	
	aal_assert("umka-2443", place != NULL);
	aal_assert("umka-2444", hint->plug != NULL);

	return reiser4_tree_modify(tree, place, hint, level,
				   callback_prep_write,
				   reiser4_node_write);
}

/* Removes item/unit at passed @place and performs so called local packing. This
   is shift data from target node to its left neighbour if any and then shift
   data from right neighbour to target node. This operation will make tree
   packing better after removals. */
errno_t reiser4_tree_remove(reiser4_tree_t *tree, place_t *place,
			    trans_hint_t *hint)
{
	errno_t res;

	aal_assert("umka-2055", tree != NULL);
	aal_assert("umka-2056", place != NULL);
	aal_assert("umka-2392", hint != NULL);

	if (hint->count == 0)
		return -EINVAL;
	
	/* Removing iten/unit from the node. */
	if ((res = reiser4_node_remove(place->node,
				       &place->pos, hint)))
	{
		return res;
	}

	if ((res = reiser4_tree_update_node(tree, place->node)))
		return res;

	/* Updating left deleimiting key in all parent nodes. */
	if (reiser4_place_leftmost(place) &&
	    place->node->p.node)
	{
		/* If node became empty it will be detached from the tree, so
		   updating is not needed and impossible, because it has no
		   items. */
		if (reiser4_node_items(place->node) > 0) {
			place_t p;
			reiser4_key_t lkey;

			/* Updating parent keys */
			reiser4_node_leftmost_key(place->node, &lkey);
				
			reiser4_place_init(&p, place->node->p.node,
					   &place->node->p.pos);

			reiser4_key_assign(&place->node->p.key, &lkey);
			
			if ((res = reiser4_tree_update_key(tree, &p, &lkey)))
				return res;
		}
	}
	
	/* Checking if the node became empty. If so, we release it, otherwise we
	   pack the tree about it. */
	if (reiser4_node_items(place->node) > 0) {
		if ((tree->flags & TF_PACK) && tree->traps.pack) {
			if ((res = tree->traps.pack(tree, place, tree->traps.data)))
				return res;
		}
	} else {
		/* Releasing node from the tree, as it gets empty. */
		reiser4_tree_discard_node(tree, place->node, 1);
	}

	/* Drying tree up in the case root node has only one item */
	if (reiser4_tree_singular(tree) && !reiser4_tree_minimal(tree)) {
		if ((res = reiser4_tree_dryout(tree)))
			return res;
	}
	
	return 0;
}

/* Traverses @node with passed callback functions as actions. */
errno_t reiser4_tree_trav_node(reiser4_tree_t *tree, node_t *node,
			       tree_open_func_t open_func,
			       tree_edge_func_t before_func,
			       tree_update_func_t update_func,
			       tree_edge_func_t after_func,
			       void *data)
{
	place_t place;
	errno_t res = 0;
	pos_t *pos = &place.pos;
 
	aal_assert("vpf-390", node != NULL);
	aal_assert("umka-1935", tree != NULL);
	
	if (open_func == NULL) {
		open_func = (tree_open_func_t)reiser4_tree_child_node;
	}
	
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
			node_t *child = NULL;
			
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
	
	if (after_func) {
		res = after_func(tree, node, data);
	}

	reiser4_node_unlock(node);
	return res;

 error_after_func:
	if (after_func) {
		res = after_func(tree, node, data);
	}
 error_unlock_node:
	reiser4_node_unlock(node);
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
errno_t reiser4_tree_clone(reiser4_tree_t *src_tree,
			   reiser4_tree_t *dst_tree)
{
	aal_assert("umka-2304", src_tree != NULL);
	aal_assert("umka-2305", dst_tree != NULL);
	
	return -EINVAL;
}

/* Resizes @tree by @blocks */
errno_t reiser4_tree_resize(reiser4_tree_t *tree,
			    count_t blocks)
{
	aal_assert("umka-2323", tree != NULL);
	return -EINVAL;
}
#endif
