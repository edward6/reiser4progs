/*
  tree.c -- reiser4 tree cache code.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/reiser4.h>

static int callback_node_free(void *data) {
	reiser4_node_t *node = (reiser4_node_t *)data;
	
	if (reiser4_node_locked(node) || node->children || !node->parent)
		return 0;

	return reiser4_node_close(node) == 0;
}

#ifndef ENABLE_ALONE

static int callback_node_sync(void *data) {
	reiser4_node_t *node = (reiser4_node_t *)data;
	return reiser4_node_sync(node) == 0;
}

#endif

static aal_list_t *callback_get_next(void *data) {
	return ((reiser4_node_t *)data)->lru_link.next;
}

static void callback_set_next(void *data, aal_list_t *next) {
	((reiser4_node_t *)data)->lru_link.next = next;
}

static aal_list_t *callback_get_prev(void *data) {
	return ((reiser4_node_t *)data)->lru_link.prev;
}

static void callback_set_prev(void *data, aal_list_t *prev) {
	((reiser4_node_t *)data)->lru_link.prev = prev;
}
		
static lru_ops_t lru_ops = {
	.free     = callback_node_free,
	
#ifndef ENABLE_ALONE
	.sync     = callback_node_sync,
#else
	.sync     = NULL,
#endif

	.get_next = callback_get_next,
	.set_next = callback_set_next,
	.get_prev = callback_get_prev,
	.set_prev = callback_set_prev
};

/*
  Registers passed node in tree and connects left and right neighbour
  nodes. This function do not do any modifications.
*/
errno_t reiser4_tree_connect(
	reiser4_tree_t *tree,    /* tree instance */
	reiser4_node_t *parent,	 /* node child will be connected to */
	reiser4_node_t *node)	 /* child node to be attached */
{
	aal_assert("umka-1857", tree != NULL);
	aal_assert("umka-561", parent != NULL);
	aal_assert("umka-564", node != NULL);

	/* Registering @child in @node children list */
	if (reiser4_node_connect(parent, node))
		return -1;

	/*
	  Getting neighbours. This should be done after reiser4_node_connect is
	  done and parent assigned.
	*/
	node->left = reiser4_tree_neighbour(tree, node, D_LEFT);
	node->right = reiser4_tree_neighbour(tree, node, D_RIGHT);

	if (tree->traps.connect) {
		errno_t res;
		reiser4_coord_t coord;
			
		if (reiser4_coord_open(&coord, parent, &node->pos))
			return -1;
			
		if ((res = tree->traps.connect(tree, &coord, node,
					       tree->traps.data)))
			return res;
	}
	
	return 0;
}

/*
  Remove specified child from the node children list. Updates all neighbour
  pointers and parent pointer.
*/
errno_t reiser4_tree_disconnect(
	reiser4_tree_t *tree,    /* tree instance */
	reiser4_node_t *parent,	 /* node child will be detached from */
	reiser4_node_t *node)	 /* pointer to child to be deleted */
{
	aal_list_t *next;
	
	aal_assert("umka-1858", tree != NULL);
	aal_assert("umka-562", parent != NULL);
	aal_assert("umka-563", node != NULL);

	if (tree->traps.disconnect) {
		errno_t res;
		reiser4_coord_t coord;
		reiser4_node_t *parent;

		if ((parent = node->parent)) {
			if (reiser4_coord_open(&coord, parent, &node->pos))
				return -1;
			
			if ((res = tree->traps.disconnect(tree, &coord, node,
							  tree->traps.data)))
				return res;
		}
	}
	
	if (node->left) {
		node->left->right = NULL;
		node->left = NULL;
	}
	
	if (node->right) {
		node->right->left = NULL;
		node->right = NULL;
	}

	return reiser4_node_disconnect(parent, node);
}

/* Loads node and connects it into the tree */
reiser4_node_t *reiser4_tree_load(reiser4_tree_t *tree,
				  reiser4_node_t *parent,
				  blk_t blk)
{
	aal_device_t *device;
	reiser4_node_t *node = NULL;

	aal_assert("umka-1289", tree != NULL);
    
	device = tree->fs->device;

	if (!parent || !(node = reiser4_node_cbp(parent, blk))) {
		
		if (!(node = reiser4_node_open(device, blk))) {
			aal_exception_error("Can't read block %llu. %s.",
					    blk, device->error);
			return NULL;
		}
		
		reiser4_node_mkclean(node);
		node->tree = tree;
		
		if (parent && reiser4_tree_connect(tree, parent, node))
			goto error_free_node;
	}
	
	return node;

 error_free_node:
	reiser4_node_close(node);
	return NULL;
}

/* Unloads passed @node from the tree */
errno_t reiser4_tree_unload(reiser4_tree_t *tree,
			    reiser4_node_t *node)
{
	aal_assert("umka-1840", tree != NULL);
	aal_assert("umka-1842", node != NULL);
	
	if (node->parent) {
		if (reiser4_tree_disconnect(tree, node->parent, node))
			return -1;
	}
	
	return reiser4_node_close(node);
}

/* Finds both left and right neighbours and connects them into the tree */
reiser4_node_t *reiser4_tree_neighbour(reiser4_tree_t *tree,
				       reiser4_node_t *node,
				       aal_direction_t where)
{
	int found = 0;
	uint32_t orig;
	uint32_t level;

	rpos_t pos;
	reiser4_node_t *old;
	reiser4_node_t *child;
	reiser4_coord_t coord;
	reiser4_ptr_hint_t ptr;

	old = node;
	level = orig = reiser4_node_get_level(node);

	while (node->parent && !found) {
		
		if (reiser4_node_pos(node, &pos))
			return NULL;

		found = (where == D_LEFT ? (pos.item > 0) :
			 (pos.item < reiser4_node_items(node->parent) - 1));

		level++;
		node = node->parent;
	}

	if (!found)
		return NULL;
	
	pos.item += (where == D_LEFT ? -1 : 1);
	
	while (level > orig) {
		if (reiser4_coord_open(&coord, node, &pos))
			return NULL;

		/* Checking if item is a branch of tree */
		if (!reiser4_item_branch(&coord))
			return node;
			
		plugin_call(coord.item.plugin->item_ops, read,
			    &coord.item, &ptr, 0, 1);

		/* Checking item for validness */
		if (ptr.ptr == INVAL_BLK)
			return NULL;

		if (!(child = reiser4_tree_load(tree, node, ptr.ptr)))
			return NULL;

		level--;
		node = child;

		pos.item = (where == D_LEFT ?
			    reiser4_node_items(node) - 1 : 0);
	}

	if (where == D_LEFT) {
		old->left = node;
		node->right = old;
	} else {
		old->right = node;
		node->left = old;
	}
	
	return node;
}

/* 
  This function raises up to the tree the left neighbour node. This is used by
  reiser4_tree_expand function.
*/
reiser4_node_t *reiser4_tree_left(reiser4_tree_t *tree,
				  reiser4_node_t *node)
{
	aal_assert("umka-1859", tree != NULL);
	aal_assert("umka-776", node != NULL);

	/* Parent is not present. The root node. */
	if (!node->parent)
		return NULL;

	if (!node->left) {
		aal_assert("umka-1629", node->tree != NULL);
		node->left = reiser4_tree_neighbour(tree, node, D_LEFT);
	}

	return node->left;
}

/* The same as previous function, but for right neighbour. */
reiser4_node_t *reiser4_tree_right(reiser4_tree_t *tree,
				   reiser4_node_t *node)
{
	aal_assert("umka-1860", tree != NULL);
	aal_assert("umka-1510", node != NULL);

	if (!node->parent)
		return NULL;
    
	if (!node->right) {
		aal_assert("umka-1630", node->tree != NULL);
		node->right = reiser4_tree_neighbour(tree, node, D_RIGHT);
	}
    
	return node->right;
}

#ifndef ENABLE_ALONE

/* Requests block allocator for new block and creates empty node in it */
reiser4_node_t *reiser4_tree_alloc(
	reiser4_tree_t *tree,	    /* tree for operating on */
	uint8_t level)	 	    /* level of new node */
{
	blk_t blk;
	rpid_t pid;

	uint32_t free, stamp;
	reiser4_node_t *node;
	aal_device_t *device;
    
	aal_assert("umka-756", tree != NULL);
    
	/* Allocating the block */
	if (!reiser4_alloc_allocate_region(tree->fs->alloc, &blk, 1)) {
		aal_exception_error("Can't allocate block for new node. "
				    "No space left?");
		return NULL;
	}

	device = tree->fs->device;
	pid = tree->fs->profile->node;
    
	/* Creating new node */
	if (!(node = reiser4_node_create(device, blk, pid, level)))
		return NULL;

	stamp = reiser4_format_get_stamp(tree->fs->format);
	reiser4_node_set_mstamp(node, stamp);
	
	/* Setting up of the free blocks in format */
	free = reiser4_alloc_unused(tree->fs->alloc);
	reiser4_format_set_free(tree->fs->format, free);

	if (tree->root) {
		stamp = reiser4_node_get_fstamp(tree->root);
		reiser4_node_set_fstamp(node, stamp);
	}

	reiser4_node_mkdirty(node);
	node->tree = tree;
	
	return node;
    
 error_free_node:
	reiser4_node_close(node);
	return NULL;
}

errno_t reiser4_tree_release(reiser4_tree_t *tree,
			     reiser4_node_t *node)
{
	blk_t free;
	
	aal_assert("umka-1841", tree != NULL);
	aal_assert("umka-917", node != NULL);

    	/* Sets up the free blocks in block allocator */
	free = reiser4_alloc_unused(tree->fs->alloc);
	reiser4_alloc_release_region(tree->fs->alloc, node->blk, 1);
	reiser4_format_set_free(tree->fs->format, free);
    
	return reiser4_tree_unload(tree, node);
}

#endif

/*
  Builds the tree root key. It is used for lookups and other as init key. This
  method id needed because of root key in reiser3 and reiser4 has a diffrent
  locality and object id values.
*/
static errno_t reiser4_tree_key(
	reiser4_tree_t *tree,	/* tree to be used */
	rpid_t pid)	        /* key plugin in use */
{
	reiser4_oid_t *oid;
	roid_t objectid, locality;
	reiser4_plugin_t *plugin;
    
	aal_assert("umka-1090", tree != NULL);
	aal_assert("umka-1091", tree->fs != NULL);
	aal_assert("umka-1092", tree->fs->oid != NULL);
    
	oid = tree->fs->oid;
    
	/* Finding needed key plugin by its identifier */
	if (!(plugin = libreiser4_factory_ifind(KEY_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find key plugin by its "
				    "id 0x%x.", pid);
		return -1;
	}
    
	/* Getting root directory attributes from oid allocator */
	locality = plugin_call(oid->entity->plugin->oid_ops, root_locality,);
	objectid = plugin_call(oid->entity->plugin->oid_ops, root_objectid,);

	/* Initializing the key by found plugin */
	tree->key.plugin = plugin;

	/* Building the key */
	reiser4_key_build_generic(&tree->key, KEY_STATDATA_TYPE,
				  locality, objectid, 0);

	return 0;
}

/* Returns tree root block number */
blk_t reiser4_tree_root(reiser4_tree_t *tree) {
	aal_assert("umka-738", tree != NULL);

	if (tree->root)
		return tree->root->blk;

	return INVAL_BLK;
}

void reiser4_tree_fini(reiser4_tree_t *tree) {
	aal_assert("umka-1531", tree != NULL);
	aal_lru_free(tree->lru);
}

/* Opens the tree (that is, the tree cache) on specified filesystem */
reiser4_tree_t *reiser4_tree_init(reiser4_fs_t *fs) {
	reiser4_tree_t *tree;

	aal_assert("umka-737", fs != NULL);

	/* Allocating memory for the tree instance */
	if (!(tree = aal_calloc(sizeof(*tree), 0)))
		return NULL;
    
	tree->fs = fs;
	tree->fs->tree = tree;

	/* Building the tree root key */
	if (reiser4_tree_key(tree, KEY_REISER40_ID)) {
		aal_exception_error("Can't build the tree root key.");
		goto error_free_tree;
	}
    
	if (!(tree->lru = aal_lru_create(&lru_ops))) {
		aal_exception_error("Can't initialize tree cache lru list.");
		goto error_free_tree;
	}
	
	return tree;

 error_free_tree:
	aal_free(tree);
	return NULL;
}

#ifndef ENABLE_ALONE

/* Syncs whole tree cache */
errno_t reiser4_tree_sync(reiser4_tree_t *tree) {
	aal_assert("umka-560", tree != NULL);

	if (!tree->root)
		return 0;
	
	return reiser4_node_sync(tree->root);
}

#endif

/* Closes specified tree (frees all assosiated memory) */
void reiser4_tree_close(reiser4_tree_t *tree) {
	aal_assert("umka-134", tree != NULL);

	tree->fs->tree = NULL;
	
	/*
	  Freeing tree cache by means of calling node_close for root node. It
	  will call itself recursively.
	*/
	if (tree->root)
		reiser4_node_close(tree->root);

	reiser4_tree_fini(tree);
	aal_free(tree);
}

uint8_t reiser4_tree_height(reiser4_tree_t *tree) {
	aal_assert("umka-1065", tree != NULL);
	aal_assert("umka-1284", tree->fs != NULL);
	aal_assert("umka-1285", tree->fs->format != NULL);

	return reiser4_format_get_height(tree->fs->format);
}

/* Dealing with loading root node if it is not loaded yet */
static errno_t reiser4_tree_ldroot(reiser4_tree_t *tree) {
	blk_t root;
	
	aal_assert("umka-1870", tree != NULL);
	
	if (tree->root)
		return 0;

	if ((root = reiser4_format_get_root(tree->fs->format)) == INVAL_BLK)
		return -1;
	
	if (!(tree->root = reiser4_tree_load(tree, NULL, root)))
		return -1;
    
	tree->root->tree = tree;
	return 0;
}

/* Dealing with allocating root node if it is not allocated yet */
static errno_t reiser4_tree_alroot(reiser4_tree_t *tree) {
	aal_assert("umka-1869", tree != NULL);
	
	if (tree->root)
		return 0;

	if (reiser4_format_get_root(tree->fs->format) != INVAL_BLK)
		return -1;
	
	if (!(tree->root = reiser4_tree_alloc(tree, LEAF_LEVEL + 1)))
		return -1;
	
	reiser4_format_set_root(tree->fs->format, tree->root->blk);
	tree->root->tree = tree;
	
	return 0;
}

static errno_t reiser4_tree_asroot(reiser4_tree_t *tree,
				   reiser4_node_t *node)
{
	uint32_t level;
	
	aal_assert("umka-1867", tree != NULL);
	aal_assert("umka-1868", node != NULL);

	tree->root = node;
	node->tree = tree;

	level = reiser4_node_get_level(node);
	reiser4_format_set_height(tree->fs->format, level);
	reiser4_format_set_root(tree->fs->format, tree->root->blk);

	return 0;
}

/* 
   Makes search in the tree by specified key. Fills passed coord by coords of 
   found item. 
*/
int reiser4_tree_lookup(
	reiser4_tree_t *tree,	/* tree to be grepped */
	reiser4_key_t *key,	/* key to be find */
	uint8_t level,	        /* stop level for search */
	reiser4_coord_t *coord)	/* coord found item to be stored */
{
	int result, deep;

	reiser4_coord_t fake;
	reiser4_ptr_hint_t ptr;
	rpos_t pos = {0, ~0ul};
	reiser4_node_t *parent = NULL;

	aal_assert("umka-1760", tree != NULL);
	aal_assert("umka-742", key != NULL);

	if (!coord)
		coord = &fake;
    
	reiser4_coord_init(coord, tree->root, &pos);
	
	if (reiser4_format_get_root(tree->fs->format) == INVAL_BLK)
		return ABSENT;
	
	deep = reiser4_tree_height(tree);

	/* Making sure that root is exist */
	if (reiser4_tree_ldroot(tree))
		return FAILED;
    
	reiser4_coord_init(coord, tree->root, &pos);
	
	/* 
	  Check for the case when wanted key smaller than root key. This is the
	  case, when somebody is trying to go up of the root by ".." entry of
	  root directory.
	*/
	if (reiser4_key_compare(key, &tree->key) < 0)
		*key = tree->key;
		    
	while (1) {
		item_entity_t *item;
		reiser4_node_t *node = coord->node;
	
		if (reiser4_node_items(node) == 0)
			return ABSENT;

		/* 
		  Looking up for key inside node. Result of lookuping will be
		  stored in &coord->pos.
		*/
		if ((result = reiser4_node_lookup(node, key, &coord->pos)) == -1)
			return -1;

		/* Check if we should finish lookup because we reach stop level */
		if (deep <= level) {

			if (result == 1)
				reiser4_coord_realize(coord);
			
			return result;
		}
		
		if (result == 0 && coord->pos.item > 0)
			coord->pos.item--;
				
		if (reiser4_coord_realize(coord)) {
			aal_exception_error("Can't open item by its coord. Node "
					    "%llu, item %u.", coord->node->blk,
					    coord->pos.item);
			return FAILED;
		}

		if (!reiser4_item_branch(coord)) {

			if (result == 1)
				reiser4_coord_realize(coord);
			
			return result;
		}
		
		item = &coord->item;
		
		/* Getting the node pointer from internal item */
		plugin_call(item->plugin->item_ops, read, item, &ptr,
			    coord->pos.unit, 1);
		
		if (ptr.ptr == INVAL_BLK) {
			aal_exception_error("Can't get pointer from nodeptr item %u, "
					    "node %llu.", coord->pos.item,
					    coord->node->blk);
			return FAILED;
		}
	
		deep--;
		
		parent = coord->node;
	
		/* 
		   Check whether specified node already in cache. If so, we use
		   node from the cache.
		*/
		reiser4_node_lock(parent);

		/* Loading node by ptr */
		if (!(coord->node = reiser4_tree_load(tree, parent, ptr.ptr))) {
			reiser4_node_unlock(parent);
			return FAILED;
		}

		reiser4_node_unlock(parent);
	}
    
	return 0;
    
 error_free_node:
	reiser4_node_close(coord->node);
	return FAILED;
}

#ifndef ENABLE_ALONE

/*
  This function inserts new nodeptr item to the tree and in such way it attaches
  passed @node to it. It also connects passed @node into tree cache.
*/
errno_t reiser4_tree_attach(
	reiser4_tree_t *tree,	    /* tree we will attach node to */
	reiser4_node_t *node)       /* child to attached */
{
	rpid_t pid;
	errno_t res;
	uint8_t level;
	
	reiser4_coord_t coord;
	reiser4_ptr_hint_t ptr;
	reiser4_item_hint_t hint;

	aal_assert("umka-913", tree != NULL);
	aal_assert("umka-916", node != NULL);
    
	/* Preparing nodeptr item hint */
	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&ptr, 0, sizeof(ptr));

	/* Prepare nodeptr hint from opassed @node */
	hint.count = 1;
	hint.hint = &ptr;

	ptr.width = 1;
	ptr.ptr = node->blk;

	reiser4_node_lkey(node, &hint.key);

	pid = tree->fs->profile->item.nodeptr;

	if (!(hint.plugin = libreiser4_factory_ifind(ITEM_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find item plugin by its id 0x%x.", pid);
		return -1;
	}

	level = reiser4_node_get_level(node) + 1;

	/*
	  Checking if tree is fresh one, thus, it does not have the root node.
	  If so, we are taking care about it here.
	*/
	if (reiser4_format_get_root(tree->fs->format) == INVAL_BLK) {

		if (reiser4_node_get_level(node) == LEAF_LEVEL) {
			if (reiser4_tree_alroot(tree))
				return -1;

			coord.node = tree->root;
			POS_INIT(&coord.pos, 0, ~0ul);
		
			if (reiser4_node_insert(coord.node, &coord.pos, &hint))
				return -1;
		
			reiser4_node_set_level(coord.node, level);

			/*
			  Attaching node to insert point node. We should attach formatted nodes
			  only.
			*/
			if (reiser4_tree_connect(tree, coord.node, node)) {
				aal_exception_error("Can't attach the node %llu to the tree.", 
						    node->blk);
				return -1;
			}
		} else {
			if (reiser4_tree_asroot(tree, node))
				return -1;
		}
		
		return 0;
	} else {
		if (reiser4_tree_ldroot(tree))
			return -1;

		/*
		  Checking if we have the tree with height smaller than node we are
		  going to attach in it. If so, we should grow the tree by requested
		  level.
		*/
		while (level > reiser4_tree_height(tree))
			reiser4_tree_grow(tree);
		
	}
	
	/* Looking up for the insert point coord */
	if ((res = reiser4_tree_lookup(tree, &hint.key, level, &coord))) {
		aal_exception_error("Can't find left delimiting key of "
				    "node to be attached.");
		return res;
	}

	/* Inserting node pointer into tree */
	if ((res = reiser4_tree_insert(tree, &coord, &hint))) {
		aal_exception_error("Can't insert nodeptr item to the tree.");
		return res;
	}

	/*
	  Attaching node to insert point node. We should attach formatted nodes
	  only.
	*/
	if (reiser4_tree_connect(tree, coord.node, node)) {
		aal_exception_error("Can't attach the node %llu to the tree.", 
				    node->blk);
		return -1;
	}

	return 0;
}

/* Removes passed @node from the on-disk tree and cache structures */
errno_t reiser4_tree_detach(reiser4_tree_t *tree,
			    reiser4_node_t *node)
{
	errno_t res;
	reiser4_coord_t coord;
	reiser4_node_t *parent;
	
	aal_assert("umka-1726", tree != NULL);
	aal_assert("umka-1727", node != NULL);

	if (!(parent = node->parent))
		return 0;

	reiser4_coord_init(&coord, parent, &node->pos);
	
	/* Removing item/unit from the parent node */
	if (reiser4_tree_remove(tree, &coord, 1))
		return -1;

	reiser4_tree_disconnect(tree, parent, node);
			
	return 0;
}

/*
  This function forces tree to grow by one level and sets it up after the
  growing.
*/
errno_t reiser4_tree_grow(
	reiser4_tree_t *tree)	/* tree to be growed up */
{
	uint8_t height;
	reiser4_node_t *old_root;

	aal_assert("umka-1701", tree != NULL);
	aal_assert("umka-1736", tree->root != NULL);
	
	if (reiser4_format_get_root(tree->fs->format) == INVAL_BLK)
		return -1;

	if (reiser4_tree_ldroot(tree))
		return -1;
	
	if (!(old_root = tree->root))
		return -1;
	
	height = reiser4_tree_height(tree);
    
	/* Allocating new root node */
	if (!(tree->root = reiser4_tree_alloc(tree, height + 1))) {
		aal_exception_error("Can't allocate new root node.");
		goto error_return_root;
	}

	tree->root->tree = tree;

	/* Updating format-related fields */
    	reiser4_format_set_root(tree->fs->format,
				tree->root->blk);

	reiser4_format_set_height(tree->fs->format,
				  height + 1);
	
	if (reiser4_tree_attach(tree, old_root)) {
		aal_exception_error("Can't attach old root to the tree.");
		goto error_free_root;
	}

	return 0;

 error_free_root:
	reiser4_tree_release(tree, tree->root);

 error_return_root:
	tree->root = old_root;
	return -1;
}

/* Decreases tree height by one level */
errno_t reiser4_tree_dryup(reiser4_tree_t *tree) {
	uint32_t height;
	aal_list_t *children;
	reiser4_node_t *old_root;

	aal_assert("umka-1731", tree != NULL);
	aal_assert("umka-1737", tree->root != NULL);

	if (reiser4_format_get_root(tree->fs->format) == INVAL_BLK)
		return -1;
	
	height = reiser4_tree_height(tree);

	if (height - 1 == LEAF_LEVEL)
		return -1;

	if (reiser4_tree_ldroot(tree))
		return -1;
	
	/* Replacing old root ndoe by new root */
	if (!(old_root = tree->root))
		return -1;

	children = old_root->children;
	aal_assert("umka-1735", children != NULL);

	/*
	  FIXME-UMKA: Here first root child chould be obtained in other way,
	  means of using reiser4_tree_load function. That is because, this
	  function may be called when tree cache is empty and thus children is
	  empty too.
	*/
	
	/* Check if we can dry tree safely */
	if (reiser4_node_items(old_root) > 1)
		return -1;

	tree->root = (reiser4_node_t *)children->data;
	reiser4_tree_disconnect(tree, old_root, tree->root);

	/* Releasing old root */
	reiser4_node_mkclean(old_root);
	reiser4_tree_release(tree, old_root);

	/* Setting up format tree-related fields */
    	reiser4_format_set_root(tree->fs->format,
				tree->root->blk);

	reiser4_format_set_height(tree->fs->format,
				  height - 1);

	return 0;
}

/*
  Tried to shift items and units from @coord to passed @neig node. After it is
  finished coord will contain new insert point.
*/
errno_t reiser4_tree_shift(
	reiser4_tree_t *tree,	/* tree we will operate on */
	reiser4_coord_t *coord,	/* insert point coord */
	reiser4_node_t *neig,	/* node items will be shifted to */
	uint32_t flags)	        /* some flags (direction, move ip or not, etc) */
{
	rpos_t pos;
	uint32_t i, items;
	reiser4_key_t lkey;
	
	shift_hint_t hint;
	reiser4_node_t *node;

	aal_assert("umka-1225", tree != NULL);
	aal_assert("umka-1226", coord != NULL);
	aal_assert("umka-1227", neig != NULL);
    
	aal_memset(&hint, 0, sizeof(hint));

	node = coord->node;
	hint.control = flags;
	hint.pos = coord->pos;
	
	/*
	  Saving node position in parent. It will be used bellow for updating
	  left delemiting key.
	*/
	if (hint.control & SF_LEFT) {
		if (node->parent) {
			if (reiser4_node_pos(node, &pos)) {
				aal_exception_error("Can't find node %llu in "
						    "its parent node.", node->blk);
				return -1;
			}
		}
	} else {
		if (neig->parent) {
			if (reiser4_node_pos(neig, &pos)) {
				aal_exception_error("Can't find node %llu in "
						    "its parent node.", neig->blk);
				return -1;
			}
		}
	}
	
	if (reiser4_node_shift(node, neig, &hint))
		return -1;

	coord->pos = hint.pos;

	if (hint.result & SF_MOVIP)
		coord->node = neig;

	if (hint.items > 0 || hint.units > 0) {
		reiser4_node_mkdirty(node);
		reiser4_node_mkdirty(neig);
	}
	
	/* Updating left delimiting keys in the tree */
	if (hint.control & SF_LEFT) {
		
		if (reiser4_node_items(node) != 0 &&
		    (hint.items > 0 || hint.units > 0))
		{
			if (node->parent) {
				if (reiser4_node_lkey(node, &lkey))
					return -1;
				
				if (reiser4_node_ukey(node->parent, &pos, &lkey))
					return -1;
			}
		}
	} else {
		if (hint.items > 0 || hint.units > 0) {

			if (neig->parent) {
				if (reiser4_node_lkey(neig, &lkey))
					return -1;
				
				if (reiser4_node_ukey(neig->parent, &pos, &lkey))
					return -1;
			}
		}
	}

	/* We do not need update children lists if we are on leaf level */
	if (reiser4_node_get_level(node) <= LEAF_LEVEL)
		return 0;

	/* Updating children lists in node and its neighbour */
	items = reiser4_node_items(neig);
	
	for (i = 0; i < hint.items; i++) {
		uint32_t units;
		reiser4_coord_t coord;

		POS_INIT(&pos, (hint.control & SF_LEFT) ?
			 items - i - 1 : i, ~0ul);

		if (reiser4_coord_open(&coord, neig, &pos))
			return -1;

		units = reiser4_item_units(&coord);
		
		if (!reiser4_item_branch(&coord))
			continue;

		for (pos.unit = 0; pos.unit < units; pos.unit++) {
			reiser4_node_t *child;
			reiser4_ptr_hint_t ptr;
			
			plugin_call(coord.item.plugin->item_ops,
				    read, &coord.item, &ptr, pos.unit, 1);
			
			if (!(child = reiser4_node_cbp(node, ptr.ptr)))
				continue;

			reiser4_node_disconnect(node, child);

			if (reiser4_node_connect(neig, child))
				return -1;
		}

	}
	
	return 0;
}

errno_t reiser4_tree_expand(
	reiser4_tree_t *tree,	    /* tree pointer function operates on */
	reiser4_coord_t *coord,	    /* coord of insertion point */
	uint32_t needed)	    /* amount of space that should be freed */
{
	int alloc;
	int32_t not_enough;
	uint32_t max_space;

	reiser4_coord_t old;
	reiser4_node_t *left;
	reiser4_node_t *right;

	aal_assert("umka-766", coord != NULL);
	aal_assert("umka-929", tree != NULL);

	if (needed == 0)
		return 0;
    
	max_space = reiser4_node_maxspace(coord->node);
	
	/* 
	   Checking if item hint to be inserted to tree has length more than 
	   max possible space in a node.
	*/
	if (needed > max_space) {
		aal_exception_error("Item size is too big. Maximal possible "
				    "item can be %u bytes long.", max_space);
		return -1;
	}
    
	if ((not_enough = needed  - reiser4_node_space(coord->node)) <= 0)
		return 0;

	old = *coord;
	
	/* Shifting data into left neighbour if it exists */
	if ((left = reiser4_tree_left(tree, coord->node))) {
	    
		if (reiser4_tree_shift(tree, coord, left, SF_LEFT | SF_UPTIP))
			return -1;
	
		if ((not_enough = needed - reiser4_node_space(coord->node)) <= 0)
			return 0;
	}

	/* Shifting data into right neighbour if it exists */
	if ((right = reiser4_tree_right(tree, coord->node))) {
	    
		if (reiser4_tree_shift(tree, coord, right, SF_RIGHT | SF_UPTIP))
			return -1;
	
		if ((not_enough = needed - reiser4_node_space(coord->node)) <= 0)
			return 0;
	}
    
	/*
	  Here we still have not enough free space for inserting item/unit into
	  the tree. Allocating new node and trying to shift data into it.
	*/
	for (alloc = 0; (not_enough > 0) && (alloc < 2); alloc++) {
		uint8_t level;
		shift_flags_t flags;
		reiser4_coord_t save;
		reiser4_node_t *node;
	
		level = reiser4_node_get_level(coord->node);
	
		if (!(node = reiser4_tree_alloc(tree, level)))
			return -1;
		
		flags = SF_RIGHT | SF_UPTIP;

		if (alloc == 0)
			flags |= SF_MOVIP;

		save = *coord;
		
		if (reiser4_tree_shift(tree, coord, node, flags))
			return -1;	

		/*
		  Releasing old node, because it has become empty as result of
		  data shifting.
		*/
		if (reiser4_node_items(save.node) == 0) {

			if (reiser4_tree_detach(tree, save.node))
				return -1;
			
			reiser4_node_mkclean(save.node);
			reiser4_tree_release(tree, save.node);
		}
		
		/* Attaching new allocated node into the tree, if it is not empty */
		if (reiser4_node_items(node) > 0) {

			/*
			  Growing the tree in the case we splitted the root
			  node. Root node has not parent.
			*/
			if (!old.node->parent)
				reiser4_tree_grow(tree);
			
			/* Attaching new node to the tree */
			if (reiser4_tree_attach(tree, node)) {
				aal_exception_error("Can't attach new node to the "
						    "tree while making space.");
				
				reiser4_tree_release(tree, node);
				return -1;
			}
		}
	
		not_enough = needed - reiser4_node_space(coord->node);
	}

	return -(not_enough > 0);
}

/* Packs node in @coord by means of using shift into/from neighbours */
errno_t reiser4_tree_shrink(reiser4_tree_t *tree,
			    reiser4_coord_t *coord)
{
	reiser4_node_t *left, *right;

	aal_assert("umka-1784", tree != NULL);
	aal_assert("umka-1783", coord != NULL);
	
	/*
	  Packing node in order to keep the tree in well packed state
	  anyway. Here we will shift data from the target node to its left
	  neighbour node.
	*/
	if ((left = reiser4_tree_left(tree, coord->node))) {
	    
		if (reiser4_tree_shift(tree, coord, left, SF_LEFT)) {
			aal_exception_error("Can't pack node %llu into left.",
					    coord->node->blk);
			return -1;
		}
	}
		
	if (reiser4_node_items(coord->node) > 0) {
		/*
		  Shifting the data from the right neigbour node into the target
		  node.
		*/
		if ((right = reiser4_tree_right(tree, coord->node))) {
				
			reiser4_coord_t bogus;
			reiser4_coord_assign(&bogus, right);
	    
			if (reiser4_tree_shift(tree, &bogus,
					       coord->node, SF_LEFT))
			{
				aal_exception_error("Can't pack node %llu "
						    "into left.", right->blk);
				return -1;
			}

			if (reiser4_node_items(right) == 0) {
				reiser4_tree_detach(tree, right);
				reiser4_node_mkclean(right);
				reiser4_tree_release(tree, right);
			}
		}
	} else {
		reiser4_node_mkclean(coord->node);
		reiser4_tree_detach(tree, coord->node);
		reiser4_tree_release(tree, coord->node);

		coord->node = NULL;
	}

	/* Drying tree up in the case root node has only one item */
	if (reiser4_node_items(tree->root) == 1) {
		if (reiser4_tree_dryup(tree))
			return -1;
	}

	return 0;
}

errno_t reiser4_tree_split(reiser4_tree_t *tree, 
			   reiser4_coord_t *coord, 
			   uint8_t level) 
{
	uint8_t cur_level;
	rpos_t pos = {0, 0};
	reiser4_node_t *node;
	
	aal_assert("vpf-672", tree != NULL);
	aal_assert("vpf-673", coord != NULL);
	aal_assert("vpf-674", level > 0);

	cur_level = reiser4_node_get_level(coord->node);
	aal_assert("vpf-680", cur_level <= level);
	
	if (reiser4_coord_realize(coord))
		return -1;

	while (cur_level <= level) {
		aal_assert("vpf-676", coord->node->parent != NULL);
		
		if (coord->pos.item != 0 || coord->pos.unit != 0 || 
		    coord->pos.item != reiser4_node_items(coord->node) || 
		    coord->pos.unit != reiser4_item_units(coord))
		{
			/* We are not on the border, split. */
			if ((node = reiser4_tree_alloc(tree, cur_level)) == NULL) {
				aal_exception_error("Tree failed to allocate "
						    "a new node.");
				return -1;
			}
    
			if (reiser4_tree_shift(tree, coord, node, SF_RIGHT)) {
				aal_exception_error("Tree failed to shift "
						    "into a newly "
						    "allocated node.");
				goto error_free_node;
			}
		
			if (reiser4_tree_attach(tree, node)) {
				aal_exception_error("Tree failed to attach "
						    "a newly allocated "
						    "node to the tree.");
				goto error_free_node;
			}
		
		} else {
			node = coord->node;
			
			if (reiser4_node_pos(node, NULL))
				return -1;		
		}
		
		if (reiser4_coord_open(coord, node->parent, &node->pos))
			return -1;

		cur_level--;
	}
	
	return 0;
	
 error_free_node:
	reiser4_node_close(node);
	return -1;
}

/* Returns level in the tree, passed item @hint can lie on */
static void reiser4_tree_level(reiser4_tree_t *tree,
			       reiser4_item_hint_t *hint,
			       uint8_t *top, uint8_t *bottom)
{
	aal_assert("umka-1871", tree != NULL);
	aal_assert("umka-1872", hint != NULL);

	switch (hint->plugin->h.group) {
	case EXTENT_ITEM:
		*top = TWIG_LEVEL;
		*bottom = TWIG_LEVEL;
		break;
	case NODEPTR_ITEM:
		*top = TMAX_LEVEL;
		*bottom = TWIG_LEVEL;
		break;
	default:
		*top = LEAF_LEVEL;
		*bottom = LEAF_LEVEL;
	}
}

/* Inserts new item described by item hint into the tree */
errno_t reiser4_tree_insert(
	reiser4_tree_t *tree,	    /* tree new item will be inserted in */
	reiser4_coord_t *coord,	    /* coord item or unit inserted at */
	reiser4_item_hint_t *hint)  /* item hint to be inserted */
{
	int mode;
	uint32_t needed;	
	reiser4_key_t *key;
	reiser4_coord_t old;

	aal_assert("umka-779", tree != NULL);
	aal_assert("umka-779", hint != NULL);
	
	aal_assert("umka-1644", coord != NULL);
	aal_assert("umka-1645", hint->plugin != NULL);

	/*
	  Initializing hint context and enviromnent fields. This should be done
	  before estimate is called, because it may use these fields.
	*/
	if (coord->node)
		hint->con.blk = coord->node->blk;
	
	hint->con.device = tree->fs->device;
	
	hint->env.oid = tree->fs->oid->entity;
	hint->env.alloc = tree->fs->alloc->entity;
	
	/* Estimating item in order to insert it into found node */
	if (reiser4_item_estimate(coord, hint))
		return -1;

	/*
	  Checking if tree is fresh one, thus, it does not have the root node.
	  If so, we are taking care about it here.
	*/
	if (reiser4_format_get_root(tree->fs->format) == INVAL_BLK) {
		uint8_t top, bottom;

		if (reiser4_tree_alroot(tree))
			return -1;

		reiser4_tree_level(tree, hint, &top, &bottom);

		if (top == LEAF_LEVEL && bottom == LEAF_LEVEL) {
			if (!(coord->node = reiser4_tree_alloc(tree, LEAF_LEVEL)))
				return -1;
		
			POS_INIT(&coord->pos, 0, ~0ul);
		
			if (reiser4_node_insert(coord->node, &coord->pos, hint)) {
				reiser4_tree_release(tree, coord->node);
				return -1;
			}

			if (reiser4_tree_attach(tree, coord->node)) {
				reiser4_tree_release(tree, coord->node);
				return -1;
			}
		} else {
			coord->node = tree->root;
			POS_INIT(&coord->pos, 0, ~0ul);
			
			if (reiser4_node_insert(coord->node, &coord->pos, hint))
				return -1;
		}

		return 0;
	} else {
		if (reiser4_tree_ldroot(tree))
			return -1;
	}

	/* Needed space is estimated space plugs item overhead */
	needed = hint->len + (coord->pos.unit == ~0ul ? 
			      reiser4_node_overhead(coord->node) : 0);
	
	old = *coord;
		
	if (tree->traps.pre_insert) {
		bool_t res;
		
		if ((res = tree->traps.pre_insert(tree, coord, hint,
						  tree->traps.data)))
			return res;
	}

	/*
	  Saving mode of insert (insert new item, paste units to the existent
	  one) before making space for new inset/unit.
	*/
	mode = (coord->pos.unit == ~0ul);
	
	if (reiser4_tree_expand(tree, coord, needed)) {
		aal_exception_error("Can't prepare space for insert "
				    "one more item/unit.");
		return -1;
	}

	/*
	  As position after making space is generaly changing, we check is mode
	  of insert was changed or not. If so, we should perform estimate one
	  more time. That is because, estimated value depends on insert mode.
	*/
	if (mode != (coord->pos.unit == ~0ul)) {
		if (reiser4_item_estimate(coord, hint))
			return -1;
	}

	/* Inserting item/unit and updating parent keys */
	{
		int update;
		rpos_t ppos;

		if ((update = reiser4_coord_utmost(coord))) {
			if (coord->node->parent) {
				if (reiser4_node_pos(coord->node, &ppos))
					return -1;
			}
		}

		if (reiser4_node_insert(coord->node, &coord->pos, hint)) {
			aal_exception_error("Can't insert an %s into the node %llu.", 
					    (coord->pos.unit == ~0ul ? "item" : "unit"),
					    coord->node->blk);
			return -1;
		}

		if (update && coord->node->parent) {
			if (reiser4_node_ukey(coord->node->parent, &ppos, &hint->key))
				return -1;
		}
	}
	
	/* 
	   If make space function allocates new node, we should attach it to the
	   tree. Also, here we should handle the special case, when tree root
	   should be changed.
	*/
	if (coord->node != tree->root && !coord->node->parent) {

		/* Growing the tree */
		if (!old.node->parent)
			reiser4_tree_grow(tree);
	
		/* Attaching new node to the tree */
		if (reiser4_tree_attach(tree, coord->node)) {
			aal_exception_error("Can't attach node %llu to the tree.",
					    coord->node->blk);
			reiser4_tree_release(tree, coord->node);
			return -1;
		}
	}

	/* Initializing insert point coord */
	if (reiser4_coord_realize(coord))
		return -1;

	if (reiser4_item_get_key(coord, NULL))
		return -1;

	/* Calling post_insert hook installed in tree */
	if (tree->traps.post_insert) {
		bool_t res;
		
		if ((res = tree->traps.post_insert(tree, coord, hint,
						   tree->traps.data)))
			return res;
	}
    
	return 0;
}

/* 
   The method should insert/overwrite the specified src coord to the dst coord
   from count items/units started at src coord.
*/
errno_t reiser4_tree_write(
	reiser4_tree_t *tree,	    /* tree insertion is performing into. */
	reiser4_coord_t *dst,       /* coord found by lookup */
	reiser4_coord_t *src,       /* coord to be inserted. */
	uint32_t count)		    /* number of units to be inserted. */
{
	aal_assert("vpf-683", tree != NULL);
	aal_assert("vpf-684", dst != NULL);
	aal_assert("vpf-685", src != NULL);
	
	return 0;
}

/*
  Cuts some amount of items or units from the tree. Updates internal keys and
  children list.
*/
errno_t reiser4_tree_cut(
	reiser4_tree_t *tree,       /* tree for working with */
	reiser4_coord_t *start,     /* coord of the start */
	reiser4_coord_t *end)       /* coord of the end */
{
	reiser4_node_t *neig;
	uint8_t end_level;
	uint8_t start_level;

	aal_assert("umka-1018", tree != NULL);
	aal_assert("umka-1725", start != NULL);
	aal_assert("umka-1782", end != NULL);

	/*
	  Getting nodes neighbour links to be connected. Also we check here is
	  start node and end node lie on the same level and can they be accessed
	  node by node durring neighbour scan.
	*/
	end_level = reiser4_node_get_level(end->node);
	start_level = reiser4_node_get_level(start->node);
	
	if (start_level != end_level) {
		aal_exception_error("Invalid start and end node levels.");
		return -1;
	}
	
	neig = reiser4_tree_right(tree, start->node);
	
	while (neig && neig != end->node)
		neig = reiser4_tree_right(tree, neig);

	if (neig != end->node) {
		aal_exception_error("End node is not reachable from the start corod.");
		return -1;
	}

	if (start->node != end->node) {
		rpos_t pos = { ~0ul, ~0ul };

		/* Removing start + 1 though end - 1 node from the tree */
		neig = neig->left;
		
		while (neig && neig != start->node) {
			if (neig->parent) {

				/*
				  As we removing from the right to left, node's
				  pos will point correct position in parent node.
				*/
				
				if (reiser4_node_remove(neig->parent, &neig->pos, 1))
					return -1;

				reiser4_node_mkclean(neig);
				reiser4_tree_detach(tree, neig);
				reiser4_tree_release(tree, neig);
			}
						
			neig = neig->left;
		}

		/* Removing items/units from the start node */
		pos.item = reiser4_node_items(start->node);
			
		if (reiser4_node_cut(start->node, &start->pos, &pos))
			return -1;
		
		if (reiser4_node_items(start->node) > 0) {
			if (reiser4_tree_shrink(tree, start))
				return -1;
		} else {
			reiser4_node_mkclean(start->node);
			reiser4_tree_detach(tree, start->node);
			reiser4_tree_release(tree, start->node);
		}
		
		/* Removing from the end node */
		pos.item = 0;
		
		if (reiser4_node_cut(end->node, &pos, &end->pos))
			return -1;

		if (reiser4_node_items(end->node) > 0) {
			if (reiser4_tree_shrink(tree, end))
				return -1;
		} else {
			reiser4_node_mkclean(end->node);
			reiser4_tree_detach(tree, end->node);
			reiser4_tree_release(tree, end->node);
		}

	} else {
		if (reiser4_node_cut(start->node, &start->pos, &end->pos))
			return -1;

		if (reiser4_node_items(start->node) > 0) {
			if (reiser4_tree_shrink(tree, start))
				return -1;
		} else {
			reiser4_node_mkclean(start->node);
			reiser4_tree_detach(tree, start->node);
			reiser4_tree_release(tree, start->node);
		}

	}

	/* Drying tree up in the case root node has only one item */
	if (reiser4_node_items(tree->root) == 1) {
		if (reiser4_tree_dryup(tree))
			return -1;
	}

	return 0;
}

/* Removes item by specified key */
errno_t reiser4_tree_remove(
	reiser4_tree_t *tree,	  /* tree item will be removed from */
	reiser4_coord_t *coord,   /* coord item will be removed at */
	uint32_t count)
{
	errno_t res;

	/* Calling "pre_remove" handler if it is defined */
	if (tree->traps.pre_remove) {
		if ((res = tree->traps.pre_remove(tree, coord,
						  tree->traps.data)))
			return res;
	}

	/* Removing item/unit and updating parent keys */
	{
		rpos_t ppos;
		int update = reiser4_coord_utmost(coord);
	
		if (update && coord->node->parent) {
			if (reiser4_node_pos(coord->node, &ppos))
				return -1;
		}
		
		/* Removing iten/unit from the node */
		if (reiser4_node_remove(coord->node, &coord->pos, count))
			return -1;

		/* Updating left deleimiting key in all parent nodes */
		if (update && coord->node->parent) {
			if (reiser4_node_items(coord->node) > 0) {

				reiser4_key_t lkey;
				reiser4_node_lkey(coord->node, &lkey);
				
				if (reiser4_node_ukey(coord->node->parent,
						      &ppos, &lkey))
					return -1;
			}
		}
	}
	
	/*
	  Checking if the node became empty. If so, we release it, otherwise we
	  pack the tree about it.
	*/
	if (reiser4_node_items(coord->node) > 0) {
		if (reiser4_tree_shrink(tree, coord))
			return -1;
	} else {
		reiser4_node_mkclean(coord->node);
		reiser4_tree_detach(tree, coord->node);
		reiser4_tree_release(tree, coord->node);
	}

	/* Drying tree up in the case root node has only one item */
	if (reiser4_node_items(tree->root) == 1) {
		if (reiser4_tree_dryup(tree))
			return -1;
	}
	
	/* Calling "post_remove" handler if it is defined */
	if (tree->traps.post_remove) {
		if ((res = tree->traps.post_remove(tree, coord,
						   tree->traps.data)))
			return res;
	}

	return 0;
}

errno_t reiser4_tree_traverse(
	reiser4_tree_t *tree,		     /* node which should be traversed */
	traverse_hint_t *hint,		     /* hint for traverse and for callback methods */
	traverse_open_func_t open_func,	     /* callback for node opening */
	traverse_edge_func_t before_func,    /* callback to be called at the beginning */
	traverse_setup_func_t setup_func,    /* callback to be called before a child  */
	traverse_setup_func_t update_func,   /* callback to be called after a child */
	traverse_edge_func_t after_func)     /* callback to be called at the end */
{
	aal_assert("umka-1768", tree != NULL);

	if (reiser4_tree_ldroot(tree))
		return -1;
	
	return reiser4_node_traverse(tree->root, hint, open_func, before_func,
				     setup_func, update_func, after_func);
}

#endif
