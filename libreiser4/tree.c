/*
  tree.c -- reiser4 tree related code.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

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

static int callback_node_free(void *data) {
	reiser4_node_t *node = (reiser4_node_t *)data;
	
	if (reiser4_node_locked(node))
		return 0;

	return reiser4_tree_unload(node->tree, node) == 0;
}

static int callback_node_sync(void *data) {
#ifndef ENABLE_STAND_ALONE
	reiser4_node_t *node = (reiser4_node_t *)data;
	return reiser4_node_sync(node) == 0;
#endif
	return 0;
}

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
	.free      = callback_node_free,
	.sync      = callback_node_sync,
	.get_next  = callback_get_next,
	.set_next  = callback_set_next,
	.get_prev  = callback_get_prev,
	.set_prev  = callback_set_prev
};

/* Dealing with loading root node if it is not loaded yet */
static errno_t reiser4_tree_load_root(reiser4_tree_t *tree) {
	blk_t root;
	
	aal_assert("umka-1870", tree != NULL);
	
	if (tree->root)
		return 0;

	if (reiser4_tree_fresh(tree))
		return -EINVAL;
	
	root = reiser4_format_get_root(tree->fs->format);
	
	if (!(tree->root = reiser4_tree_load(tree, NULL, root)))
		return -EINVAL;
    
	tree->root->tree = tree;
	
	return 0;
}

#ifndef ENABLE_STAND_ALONE

/* Assignes passed @node as new root */
static errno_t reiser4_tree_assign_root(reiser4_tree_t *tree,
					reiser4_node_t *node)
{
	uint32_t level;
	
	aal_assert("umka-1867", tree != NULL);
	aal_assert("umka-1868", node != NULL);

	tree->root = node;
	node->tree = tree;
	node->parent.node = NULL;

	level = reiser4_node_get_level(node);

	reiser4_format_set_root(tree->fs->format,
				tree->root->blk);
	
	reiser4_format_set_height(tree->fs->format,
				  level);

	return 0;
}

/* Dealing with allocating root node if it is not allocated yet */
static errno_t reiser4_tree_alloc_root(reiser4_tree_t *tree) {
	reiser4_node_t *root;
	
	aal_assert("umka-1869", tree != NULL);
	
	if (tree->root)
		return 0;

	if (!reiser4_tree_fresh(tree))
		return -EINVAL;
	
	if (!(root = reiser4_tree_alloc(tree, reiser4_tree_height(tree))))
		return -ENOSPC;

	return reiser4_tree_assign_root(tree, root);
}

#endif

/*
  Registers passed node in tree and connects left and right neighbour
  nodes. This function do not do any modifications.
*/
errno_t reiser4_tree_connect(
	reiser4_tree_t *tree,    /* tree instance */
	reiser4_node_t *parent,	 /* node child will be connected to */
	reiser4_node_t *node)	 /* child node to be attached */
{
	errno_t res;
	
	aal_assert("umka-1857", tree != NULL);
	aal_assert("umka-561", parent != NULL);
	aal_assert("umka-564", node != NULL);

	/* Registering @child in @node children list */
	if ((res = reiser4_node_connect(parent, node)))
		return res;

	reiser4_node_lock(parent);
	node->tree = tree;

	if (tree->traps.connect) {
		reiser4_place_t place;
			
		if ((res = reiser4_place_open(&place, parent,
					      &node->parent.pos)))
			return res;
			
		/*
		  Placing the callback calling into lock/unlock braces for
		  preventing its freeing by handler.
		*/
		reiser4_node_lock(node);
		
		res = tree->traps.connect(tree, &place, node,
					  tree->traps.data);
		
		reiser4_node_unlock(node);
	}
	
	/* Attaching new node into tree's lru list */
	if ((res = aal_lru_attach(tree->lru, (void *)node)))
		return res;
	
	return res;
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
	errno_t res;
	aal_list_t *next;
	
	aal_assert("umka-1858", tree != NULL);
	aal_assert("umka-563", node != NULL);

#ifndef ENABLE_STAND_ALONE
	if (tree->traps.disconnect) {
		reiser4_place_t place;

		if (place.node && parent) {
			reiser4_place_init(&place, parent,
					   &node->parent.pos);

			if ((res = reiser4_place_realize(&place)))
				return res;
		} else {
			reiser4_place_assign(&place, NULL,
					     0, ~0ul);
		}

		/*
		  Placing callback calling into lock/unlock braces for
		  preventing it freeing by handler.
		*/
		reiser4_node_lock(node);
		
		res = tree->traps.disconnect(tree, &place, node,
					     tree->traps.data);
		
		reiser4_node_unlock(node);

		if (res != 0)
			return res;

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

	/* Detaching node from the global tree LRU list */
	if ((res = aal_lru_detach(tree->lru, (void *)node)))
		return res;
	
	/*
	  If parent is not exist, then we consider the @node is root and do not
	  do any unlock and disconnect from the parent.
	*/
	if (!parent) {
		node->parent.node = NULL;
		return 0;
	}

	reiser4_node_unlock(parent);
	
	return reiser4_node_disconnect(parent, node);
}

reiser4_node_t *reiser4_tree_load(reiser4_tree_t *tree,
				  reiser4_node_t *parent,
				  blk_t blk)
{
	aal_device_t *device;
	reiser4_node_t *node = NULL;

	aal_assert("umka-1289", tree != NULL);
    
	device = tree->fs->device;

	/* Checking if we want load the root node */
	if (blk == reiser4_tree_root(tree) && !tree->root) {

		/* Attaching root node to tree->root */
		if (!(tree->root = reiser4_node_open(device, blk))) {
			aal_exception_error("Can't open root node "
					    "%llu.", blk);
			return NULL;
		}

		return tree->root;
	}

	if (!parent || !(node = reiser4_node_cbp(parent, blk))) {

		if (!(node = reiser4_node_open(device, blk))) {
			aal_exception_error("Can't open node %llu.", blk);
			return NULL;
		}
		
		node->tree = tree;
		
		if (parent && reiser4_tree_connect(tree, parent, node))
			goto error_free_node;
	} else {
		/*
		  Touching node in LRU list in odrer to let it know that we
		  access it and in such maner move to the head of list.
		*/
		if (aal_lru_touch(tree->lru, (void *)node))
			return NULL;
	}

	return node;

 error_free_node:
	reiser4_node_close(node);
	return NULL;
}

errno_t reiser4_tree_unload(reiser4_tree_t *tree,
			    reiser4_node_t *node)
{
	errno_t res;
	reiser4_node_t *parent;
	
	aal_assert("umka-1840", tree != NULL);
	aal_assert("umka-1842", node != NULL);
	
	if ((parent = node->parent.node)) {
		if ((res = reiser4_tree_disconnect(tree, parent, node)))
			return res;
	}

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

	if (reiser4_place_realize(place))
		return NULL;

	/* Checking if item is a branch of tree */
	if (!reiser4_item_branch(place))
		return NULL;
			
	plugin_call(place->item.plugin->item_ops,
		    read, &place->item, &ptr, 0, 1);

	if (!VALID_BLK(ptr.start))
		return NULL;
	
	return reiser4_tree_load(tree, place->node,
				 ptr.start);
}

/* Finds both left and right neighbours and connects them into the tree */
reiser4_node_t *reiser4_tree_neighbour(reiser4_tree_t *tree,
				       reiser4_node_t *node,
				       aal_direction_t where)
{
	int found = 0;
	uint32_t orig;
	uint32_t level;

	pos_t pos;
	reiser4_node_t *old;
	reiser4_place_t place;

	old = node;
	level = orig = reiser4_node_get_level(node);

	/*
	  Going up to the level where corresponding neighbour node may be
	  obtained by its nodeptr item.
	*/
	while (node->parent.node && !found) {
		if (reiser4_node_pbc(node, &pos))
			return NULL;

		found = (where == D_LEFT) ? (pos.item > 0) :
			(pos.item < reiser4_node_items(node->parent.node) - 1);

		node = node->parent.node;
		level++;
	}

	if (!found)
		return NULL;
	
	pos.item += (where == D_LEFT ? -1 : 1);

	/* Going down to the level of @node */
	while (level > orig) {
		reiser4_place_init(&place, node, &pos);
		
		if (!(node = reiser4_tree_child(tree, &place)))
			return NULL;

		pos.item = (where == D_LEFT) ?
			reiser4_node_items(node) - 1 : 0;

		level--;
	}

	/* Setting up neightbour links */
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
	if (!node->parent.node)
		return NULL;

	reiser4_node_lock(node);
	
	if (!node->left) {
		aal_assert("umka-1629", node->tree != NULL);

		if ((node->left = reiser4_tree_neighbour(tree, node, D_LEFT)))
			node->left->right = node;
	}

	reiser4_node_unlock(node);
	
	return node->left;
}

/* The same as previous function, but for right neighbour. */
reiser4_node_t *reiser4_tree_right(reiser4_tree_t *tree,
				   reiser4_node_t *node)
{
	aal_assert("umka-1860", tree != NULL);
	aal_assert("umka-1510", node != NULL);

	if (!node->parent.node)
		return NULL;
    
	reiser4_node_lock(node);
	
	if (!node->right) {
		aal_assert("umka-1630", node->tree != NULL);

		if ((node->right = reiser4_tree_neighbour(tree, node, D_RIGHT)))
			node->right->left = node;
	}

	reiser4_node_unlock(node);
	
	return node->right;
}

#ifndef ENABLE_STAND_ALONE

/* Requests block allocator for new block and creates empty node in it */
reiser4_node_t *reiser4_tree_alloc(
	reiser4_tree_t *tree,	    /* tree for operating on */
	uint8_t level)	 	    /* level of new node */
{
	blk_t blk;
	rid_t pid;

	uint32_t free, stamp;
	reiser4_node_t *node;
	aal_device_t *device;
    
	aal_assert("umka-756", tree != NULL);
    
	/* Allocating the block */
	if (!reiser4_alloc_allocate(tree->fs->alloc, &blk, 1)) {
		aal_exception_error("Can't allocate block for new node. "
				    "No space left?");
		return NULL;
	}

	device = tree->fs->device;

	/* Getting node plugin id from the profile */
	pid = reiser4_profile_value(tree->fs->profile, "node");
    
	/* Creating new node */
	if (!(node = reiser4_node_init(device, blk, pid)))
		return NULL;

	if (reiser4_node_form(node, level))
		goto error_free_node;

	stamp = reiser4_format_get_stamp(tree->fs->format);
	reiser4_node_set_mstamp(node, stamp);
	
	/* Setting up of the free blocks in format */
	free = reiser4_alloc_free(tree->fs->alloc);
	reiser4_format_set_free(tree->fs->format, free);

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

errno_t reiser4_tree_release(reiser4_tree_t *tree,
			     reiser4_node_t *node)
{
	blk_t free;
	
	aal_assert("umka-1841", tree != NULL);
	aal_assert("umka-917", node != NULL);

	reiser4_node_mkclean(node);
	
	free = reiser4_alloc_free(tree->fs->alloc);

	reiser4_alloc_release(tree->fs->alloc,
			      node->blk, 1);
	
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
	rid_t pid)	        /* key plugin in use */
{
	reiser4_oid_t *oid;
	oid_t objectid, locality;
	reiser4_plugin_t *plugin;
    
	aal_assert("umka-1090", tree != NULL);
	aal_assert("umka-1091", tree->fs != NULL);
	aal_assert("umka-1092", tree->fs->oid != NULL);
    
	oid = tree->fs->oid;
    
	/* Finding needed key plugin by its identifier */
	if (!(plugin = libreiser4_factory_ifind(KEY_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find key plugin by its "
				    "id 0x%x.", pid);
		return -EINVAL;
	}
    
	/* Getting root directory attributes from oid allocator */
	locality = plugin_call(oid->entity->plugin->oid_ops,
			       root_locality,);
	
	objectid = plugin_call(oid->entity->plugin->oid_ops,
			       root_objectid,);

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

	if (!tree->fs || !tree->fs->format)
		return INVAL_BLK;

	return reiser4_format_get_root(tree->fs->format);
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
    
	if (!(tree->lru = aal_lru_create(&lru_ops)))
		goto error_free_tree;

#ifndef ENABLE_STAND_ALONE
	reiser4_tree_pack_on(tree);
	tree->traps.pack = callback_tree_pack;
#endif
		
	return tree;

 error_free_tree:
	aal_free(tree);
	return NULL;
}

#ifndef ENABLE_STAND_ALONE

/* Saves passed @nodes and its children onto device */
static errno_t reiser4_tree_flush(reiser4_tree_t *tree,
				  reiser4_node_t *node)
{
	errno_t res;
	
	aal_assert("umka-1927", tree != NULL);
	aal_assert("umka-1928", node != NULL);
    
	/* Synchronizing passed @node */
	if (reiser4_node_isdirty(node)) {
		
		if ((res = reiser4_node_sync(node))) {
			aal_exception_error("Can't synchronize node %llu "
					    "to device. %s.", node->blk,
					    node->device->error);
			return res;
		}
	}
	
	/*
	  Walking through the list of children and calling reiser4_node_sync
	  function for each element.
	*/
	if (node->children) {
		aal_list_t *walk;
		reiser4_node_t *child;
	
		aal_list_foreach_forward(node->children, walk) {
			child = (reiser4_node_t *)walk->data;
			
			if ((res = reiser4_tree_flush(tree, child)))
				return res;
		}
	}

	return 0;
}

/* Syncs whole tree cache */
errno_t reiser4_tree_sync(reiser4_tree_t *tree) {
	aal_assert("umka-560", tree != NULL);

	if (!tree->root)
		return 0;

	return reiser4_tree_flush(tree, tree->root);
}

#endif

void reiser4_tree_collapse(reiser4_tree_t *tree,
			   reiser4_node_t *node)
{
	aal_assert("umka-1933", tree != NULL);
	aal_assert("umka-1934", node != NULL);
	
	/* Closing children */
	if (node->children) {
		aal_list_t *walk;

		for (walk = node->children; walk; ) {
			aal_list_t *next = aal_list_next(walk);
			reiser4_node_t *child = (reiser4_node_t *)walk->data;
			
			reiser4_tree_collapse(tree, child);
			walk = next;
		}

		aal_list_free(node->children);
		node->children = NULL;
	}

	/* Throwing out @node from the cache and releasing it */
	reiser4_tree_unload(tree, node);
}

/* Closes specified tree (frees all assosiated memory) */
void reiser4_tree_close(reiser4_tree_t *tree) {
	aal_assert("umka-134", tree != NULL);

	tree->fs->tree = NULL;
	
	/* Freeing tree cache by means of calling recursive node destroy */
	if (tree->root)
		reiser4_tree_collapse(tree, tree->root);

	/* Freeing the tree */
	reiser4_tree_fini(tree);
	aal_free(tree);
}

uint8_t reiser4_tree_height(reiser4_tree_t *tree) {
	aal_assert("umka-1065", tree != NULL);
	aal_assert("umka-1284", tree->fs != NULL);
	aal_assert("umka-1285", tree->fs->format != NULL);

	return reiser4_format_get_height(tree->fs->format);
}

bool_t reiser4_tree_fresh(reiser4_tree_t *tree) {
	aal_assert("umka-1930", tree != NULL);

	if (reiser4_format_get_root(tree->fs->format) == INVAL_BLK)
		return TRUE;

	return FALSE;
}

/* 
  Makes search in the tree by specified key. Fills passed place by places of
  found item.
*/
lookup_t reiser4_tree_lookup(
	reiser4_tree_t *tree,	/* tree to be grepped */
	reiser4_key_t *key,	/* key to be find */
	uint8_t level,	        /* stop level for search */
	reiser4_place_t *place)	/* place the found item to be stored */
{
	lookup_t res;
	uint8_t curr_level;
	pos_t pos = {0, ~0ul};

	aal_assert("umka-742", key != NULL);
	aal_assert("umka-1760", tree != NULL);
	aal_assert("umka-2057", place != NULL);

	reiser4_place_init(place, tree->root, &pos);

	if (reiser4_tree_fresh(tree))
		return LP_ABSENT;
	
	/* Making sure that root exists */
	if (reiser4_tree_load_root(tree))
		return LP_FAILED;
    
	reiser4_place_init(place, tree->root, &pos);
	
	/* 
	  Checking the case when wanted key is smaller than root one. This is
	  the case, when somebody is trying go up of the root by ".." entry in
	  root directory. If so, we initialize the key to be looked up by root
	  key.
	*/
	if (reiser4_key_compare(key, &tree->key) < 0)
		reiser4_key_assign(key, &tree->key);
		    
	while (1) {
		reiser4_node_t *node = place->node;
	
		/* 
		  Looking up for key inside node. Result of lookuping will be
		  stored in &place->pos.
		*/
		res = reiser4_node_lookup(node, key, &place->pos);

		curr_level = reiser4_node_get_level(node);
		
		/* Check if we should finish lookup because we reach stop level */
		if (curr_level <= level || res == LP_FAILED) {

			if (res == LP_PRESENT)
				reiser4_place_realize(place);
			
			return res;
		}

		/* Position correcting for internal levels */
		if (res == LP_ABSENT && place->pos.item > 0)
			place->pos.item--;

		/* Initializing item at @place */
		if (reiser4_place_realize(place)) {
			aal_exception_error("Can't open item by its place. Node "
					    "%llu, item %u.", place->node->blk,
					    place->pos.item);
			return LP_FAILED;
		}

		/* Checking is item at @place is nodeptr one */
		if (!reiser4_item_branch(place))
			return res;

		/* Loading node by nodeptr item @place points to */
		if (!(place->node = reiser4_tree_child(tree, place))) {
			aal_exception_error("Can't load node by its nodeptr.");
			return LP_FAILED;
		}
	}
    
	return LP_ABSENT;
}

#ifndef ENABLE_STAND_ALONE

/*
  Returns TRUE if passed @tree has minimal possible height nd thus cannot be
  dried out.
*/
static bool_t reiser4_tree_minimal(reiser4_tree_t *tree) {

	if (reiser4_tree_height(tree) <= 2)
		return TRUE;

	return FALSE;
}

/*
  Updates key at passed @place by passed @key by means of using
  reiser4_node_ukey functions in recursive maner.
*/
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
		
		if (place->node->parent.node) {
			reiser4_place_t p;

			reiser4_place_init(&p, place->node->parent.node,
					   &place->pos);
			
			if ((res = reiser4_node_pbc(place->node, &p.pos)))
				return res;
	    
			if ((res = reiser4_tree_ukey(tree, &p, key)))
				return res;
		}
	}

	return reiser4_node_ukey(place->node, &place->pos, key);
}

/*
  This function inserts new nodeptr item to the tree and in such way it attaches
  passed @node to it. It also connects passed @node into tree cache.
*/
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
	aal_memset(&nodeptr_hint, 0, sizeof(nodeptr_hint));

	/* Prepare nodeptr hint from opassed @node */
	nodeptr_hint.width = 1;
	nodeptr_hint.start = node->blk;

	hint.count = 1;
	hint.flags = HF_FORMATD;
	hint.type_specific = &nodeptr_hint;

	reiser4_node_lkey(node, &hint.key);

	pid = reiser4_profile_value(tree->fs->profile, "nodeptr");

	if (!(hint.plugin = libreiser4_factory_ifind(
		      ITEM_PLUGIN_TYPE, pid)))
	{
		aal_exception_error("Can't find item plugin by "
				    "its id 0x%x.", pid);
		return -EINVAL;
	}

	level = reiser4_node_get_level(node) + 1;

	/* Looking up for the insert point place */
	if ((res = reiser4_tree_lookup(tree, &hint.key, level,
				       &place)) != LP_ABSENT)
	{
		aal_exception_error("Can't find position "
				    "node to be attached.");
		return res;
	}

	/* Inserting node pointer into tree */
	if ((res = reiser4_tree_insert(tree, &place, level, &hint))) {
		aal_exception_error("Can't insert nodeptr item to "
				    "the tree.");
		return res;
	}

	/*
	  Attaching node to insert point node. We should attach formatted nodes
	  only.
	*/
	if ((res = reiser4_tree_connect(tree, place.node, node))) {
		aal_exception_error("Can't attach the node %llu to "
				    "the tree.", node->blk);
		return res;
	}

	reiser4_tree_right(tree, node);
	reiser4_tree_left(tree, node);
	
	return 0;
}

/* Removes passed @node from the on-disk tree and cache structures */
errno_t reiser4_tree_detach(reiser4_tree_t *tree,
			    reiser4_node_t *node)
{
	errno_t res;
	reiser4_place_t place;
	reiser4_node_t *parent;
	
	aal_assert("umka-1726", tree != NULL);
	aal_assert("umka-1727", node != NULL);

	if (!(parent = node->parent.node))
		return 0;

	reiser4_tree_disconnect(tree, parent, node);
	reiser4_place_init(&place, parent, &node->parent.pos);
	
	/* Removing item/unit from the parent node */
	return reiser4_tree_remove(tree, &place, 1);
}

/*
  This function forces tree to grow by one level and sets it up after the
  growing.
*/
errno_t reiser4_tree_growup(
	reiser4_tree_t *tree)	/* tree to be growed up */
{
	errno_t res;
	uint8_t height;
	reiser4_node_t *old_root;

	aal_assert("umka-1701", tree != NULL);
	aal_assert("umka-1736", tree->root != NULL);
	
	if ((res = reiser4_tree_fresh(tree)))
		return res;

	if ((res = reiser4_tree_load_root(tree)))
		return res;
	
	if (!(old_root = tree->root))
		return -EINVAL;
	
	height = reiser4_tree_height(tree);
    
	/* Allocating new root node */
	if (!(tree->root = reiser4_tree_alloc(tree, height + 1))) {
		res = -ENOSPC;
		goto error_old_root;
	}

	tree->root->tree = tree;

	/* Updating format-related fields */
    	reiser4_format_set_root(tree->fs->format,
				tree->root->blk);

	reiser4_format_set_height(tree->fs->format,
				  height + 1);
	
	if (reiser4_tree_attach(tree, old_root)) {
		res = -EINVAL;
		goto error_free_root;
	}

	return 0;

 error_free_root:
	reiser4_tree_release(tree, tree->root);

 error_old_root:
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

	if (reiser4_tree_fresh(tree))
		return -EINVAL;
	
	if (reiser4_tree_minimal(tree))
		return -EINVAL;

	/* Rasing up the root node if it exists */
	if ((res = reiser4_tree_load_root(tree)))
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
	reiser4_tree_assign_root(tree, child);
	
        /* Releasing old root node */
	reiser4_node_mkclean(root);
	reiser4_tree_release(tree, root);

	return 0;
}

/*
  Tries to shift items and units from @place to passed @neig node. After it is
  finished place will contain new insert point.
*/
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
			if (node->parent.node) {
				reiser4_place_t p;

				if ((res = reiser4_node_lkey(node, &lkey)))
					return res;

				reiser4_place_init(&p, node->parent.node,
						   &node->parent.pos);
				
				if ((res = reiser4_tree_ukey(tree, &p, &lkey)))
					return res;
			}
		}
	} else {
		if (hint.items > 0 || hint.units > 0) {

			if (neig->parent.node) {
				reiser4_place_t p;
				
				if ((res = reiser4_node_lkey(neig, &lkey)))
					return res;
				
				reiser4_place_init(&p, neig->parent.node,
						   &neig->parent.pos);
				
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

/* Enough space condition check function for extents on twig level */
static bool_t enough_by_place(reiser4_tree_t *tree,
			      reiser4_place_t *place,
			      uint32_t needed)
{
	return reiser4_place_leftmost(place) ||
		reiser4_place_rightmost(place);
}

/* Makes space in tree to insert @needed bytes of data (item/unit) */
errno_t reiser4_tree_expand(
	reiser4_tree_t *tree,	    /* tree pointer function operates on */
	reiser4_place_t *place,	    /* place of insertion point */
	enough_func_t enough_func,  /* enough condition check func */
	uint32_t needed,	    /* amount of space that should be freed */
	uint32_t flags)
{
	int alloc;
	errno_t res;
	bool_t enough;
	uint32_t max_space;

	reiser4_place_t old;
	reiser4_node_t *left;
	reiser4_node_t *right;

	aal_assert("umka-766", place != NULL);
	aal_assert("umka-929", tree != NULL);

	if (!enough_func)
		enough_func = enough_by_space;

	if (needed == 0)
		return 0;
    
	max_space = reiser4_node_maxspace(place->node);
	
	/* 
	  Checking if item hint to be inserted to tree has length more than max
	  possible space in a node.
	*/
	if (needed > max_space) {
		aal_exception_error("Item size is too big. Maximal possible "
				    "item can be %u bytes long.", max_space);
		return -EINVAL;
	}

	if ((enough = enough_func(tree, place, needed)))
		return 0;

	old = *place;
	
	/* Shifting data into left neighbour if it exists */
	if ((SF_LEFT & flags) && (left = reiser4_tree_left(tree, place->node))) {
	    
		if ((res = reiser4_tree_shift(tree, place, left,
					      SF_LEFT | SF_UPTIP)))
			return res;
	
		if ((enough = enough_func(tree, place, needed)))
			return 0;
	}

	/* Shifting data into right neighbour if it exists */
	if ((SF_RIGHT & flags) && (right = reiser4_tree_right(tree, place->node))) {
	    
		if ((res = reiser4_tree_shift(tree, place, right,
					      SF_RIGHT | SF_UPTIP)))
			return res;
	
		if ((enough = enough_func(tree, place, needed)))
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

		aal_assert("umka-2197", reiser4_node_items(place->node) > 0);
		
		level = reiser4_node_get_level(place->node);
	
		if (!(node = reiser4_tree_alloc(tree, level)))
			return -ENOSPC;
		
		flags = SF_RIGHT | SF_UPTIP;

		if (alloc == 0)
			flags |= SF_MOVIP;

		save = *place;
		
		if ((res = reiser4_tree_shift(tree, place, node, flags)))
			return res;

		/*
		  Releasing old node, because it has become empty as result of
		  data shifting.
		*/
		if (reiser4_node_items(save.node) == 0) {

			if ((res = reiser4_tree_detach(tree, save.node)))
				return res;
			
			reiser4_node_mkclean(save.node);
			reiser4_tree_release(tree, save.node);
		}
		
		/* Attaching new allocated node into the tree, if it is not empty */
		if (reiser4_node_items(node) > 0) {

			/*
			  Growing the tree in the case we splitted the root
			  node. Root node has not parent.
			*/
			if (!old.node->parent.node)
				reiser4_tree_growup(tree);
			
			/* Attaching new node to the tree */
			if ((res = reiser4_tree_attach(tree, node))) {
				aal_exception_error("Can't attach new node to the "
						    "tree while making space.");
				
				reiser4_tree_release(tree, node);
				return res;
			}
		}
		
		enough = enough_func(tree, place, needed);
	}

	return enough ? 0 : -ENOSPC;
}

/* Prepares space in tree before inserting new item/unit inot it */
static errno_t reiser4_tree_mkspace(
	reiser4_tree_t *tree,       /* tree we will dealing with */
	reiser4_place_t *place,     /* target place */
	reiser4_plugin_t *plugin,   /* item plugin to be insert */
	uint32_t len)               /* estimated item len to be insert */
{
	uint32_t needed;
	
	aal_assert("umka-2194", tree != NULL);
	aal_assert("umka-2195", place != NULL);
	aal_assert("umka-2196", plugin != NULL);

	if (len == 0)
		return 0;
	
	/* Needed space is estimated space plugs item overhead */
	needed = len + (place->pos.unit == ~0ul ? 
			reiser4_node_overhead(place->node) : 0);
		
	/*
	  Handling the case when of insert onto level higher then leaf one and
	  inserted item contains more than one unit. In this case we need split
	  the tree out, in order to keep it consistent. The fear example is
	  extent item which is going to be inserted on twig level.
	*/
	if (reiser4_node_get_level(place->node) > LEAF_LEVEL &&
	    reiser4_item_data(plugin))
	{
		errno_t res;
		
		/*
		  Using @leaves_enough_func for checking enough space
		  condition.
		*/
		if ((res = reiser4_tree_expand(tree, place, enough_by_place,
					       needed, SF_DEFAULT)))
			return res;

		*place = place->node->parent;

		/*
		  Using @leaves_enough_func for checking enough space
		  condition.
		*/
		return reiser4_tree_expand(tree, place, enough_by_space,
					   needed, SF_DEFAULT);
	} else {

		/*
		  Using @leaves_enough_func for checking enough space
		  condition.
		*/
		return reiser4_tree_expand(tree, place, enough_by_space,
					   needed, SF_DEFAULT);
	}
}

/* Packs node in @place by means of using shift into/from neighbours */
errno_t reiser4_tree_shrink(reiser4_tree_t *tree,
			    reiser4_place_t *place)
{
	errno_t res;
	reiser4_node_t *left, *right;

	aal_assert("umka-1784", tree != NULL);
	aal_assert("umka-1783", place != NULL);
	
	/*
	  Packing node in order to keep the tree in well packed state
	  anyway. Here we will shift data from the target node to its left
	  neighbour node.
	*/
	if ((left = reiser4_tree_left(tree, place->node))) {
	    
		if ((res = reiser4_tree_shift(tree, place, left, SF_LEFT))) {
			aal_exception_error("Can't pack node %llu into left.",
					    place->node->blk);
			return res;
		}
	}
		
	if (reiser4_node_items(place->node) > 0) {
		/*
		  Shifting the data from the right neigbour node into the target
		  node.
		*/
		if ((right = reiser4_tree_right(tree, place->node))) {
				
			reiser4_place_t bogus;
			bogus.node = right;
	    
			if ((res = reiser4_tree_shift(tree, &bogus,
						      place->node, SF_LEFT)))
			{
				aal_exception_error("Can't pack node %llu "
						    "into left.", right->blk);
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
errno_t reiser4_tree_split(reiser4_tree_t *tree, 
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
	aal_assert("vpf-680", curr <= level);
	
	while (curr <= level) {
		aal_assert("vpf-676", place->node->parent.node != NULL);
		
		if (!(place->pos.item == 0 && place->pos.unit == 0) || 
		    !(place->pos.item == reiser4_node_items(place->node)) || 
		    !(place->pos.item + 1 == reiser4_node_items(place->node) &&
		      place->pos.unit == reiser4_item_units(place)))
		{
			/* We are not on the border, split. */
			if ((node = reiser4_tree_alloc(tree, curr)) == NULL) {
				aal_exception_error("Tree failed to allocate "
						    "a new node.");
				return -EINVAL;
			}
    
			if ((res = reiser4_tree_shift(tree, place, node,
						      SF_RIGHT)))
			{
				aal_exception_error("Tree failed to shift "
						    "into a newly "
						    "allocated node.");
				goto error_free_node;
			}
		
			if ((res = reiser4_tree_attach(tree, node))) {
				aal_exception_error("Tree failed to attach "
						    "a newly allocated "
						    "node to the tree.");
				goto error_free_node;
			}
		
		} else {
			node = place->node;
			
			if ((res = reiser4_node_pbc(node, NULL)))
				return res;
		}
		
		reiser4_place_init(place, node->parent.node, &node->parent.pos);
		curr--;
	}
	
	return 0;
	
 error_free_node:
	reiser4_node_close(node);
	return res;
}

/* This will be removed soon */
errno_t reiser4_tree_write(reiser4_tree_t *tree,
			   reiser4_place_t *src,
			   reiser4_place_t *dst,
			   uint32_t count)
{
	return -1;
}

/*
  Overwrites items/units pointed by @dst from @src one, from @start key though
  the @end one.
*/
errno_t reiser4_tree_overwrite(reiser4_tree_t *tree,
			       reiser4_place_t *dst,
			       reiser4_place_t *src,
			       reiser4_key_t *start,
			       reiser4_key_t *end)
{
	errno_t res;
	copy_hint_t hint;
	
	aal_assert("umka-2161", dst != NULL);
	aal_assert("umka-2162", src != NULL);
	aal_assert("umka-2164", end != NULL);
	aal_assert("umka-2160", tree != NULL);
	aal_assert("umka-2163", start != NULL);

	if (reiser4_tree_fresh(tree)) {
		aal_exception_error("Can't write item/units to "
				    "empty tree.");
		return -EINVAL;
	}
	
	if ((res = reiser4_item_feel(src, start, end, &hint)))
		return res;

	aal_assert("umka-2122", hint.len > 0);
	
	if ((res = reiser4_node_copy(dst->node, &dst->pos,
				     src->node, &src->pos,
				     start, end, &hint)))
	{
		aal_exception_error("Can't copy an item/unit from node "
				    "%llu to %llu one.", src->node->blk,
				    dst->node->blk);
		return res;
	}

	if (reiser4_place_leftmost(dst) && dst->node->parent.node) {
		reiser4_place_t p;

		reiser4_place_init(&p, dst->node->parent.node,
				   &dst->node->parent.pos);
		
		if ((res = reiser4_tree_ukey(tree, &p, &src->item.key)))
			return res;
	}
	
	return 0;
}

/*
  Makes copy of item at passed @src place or some amount of its units to the
  passed @dst from @start key though the @end one.
*/
errno_t reiser4_tree_copy(reiser4_tree_t *tree,
			  reiser4_place_t *dst,
			  reiser4_place_t *src,
			  reiser4_key_t *start,
			  reiser4_key_t *end)
{
	errno_t res;
	copy_hint_t hint;
	reiser4_place_t old;
	   
	aal_assert("umka-2116", dst != NULL);
	aal_assert("umka-2117", src != NULL);
	aal_assert("umka-2119", end != NULL);
	aal_assert("umka-2115", tree != NULL);
	aal_assert("umka-2118", start != NULL);

	if (reiser4_tree_fresh(tree)) {
		aal_exception_error("Can't copy item/units to "
				    "empty tree.");
		return -EINVAL;
	}
	
	if ((res = reiser4_item_feel(src, start, end, &hint)))
		return res;

	aal_assert("umka-2122", hint.len > 0);
	
	old = *dst;
	
	if ((res = reiser4_tree_mkspace(tree, dst, hint.plugin,
					hint.len)))
	{
		aal_exception_error("Can't prepare space for "
				    "copy one more item/unit.");
		return res;
	}

	if ((res = reiser4_node_copy(dst->node, &dst->pos, src->node,
				     &src->pos, start, end, &hint)))
	{
		aal_exception_error("Can't copy an item/unit from node "
				    "%llu to %llu one.", src->node->blk,
				    dst->node->blk);
		return res;
	}

	if (reiser4_place_leftmost(dst) && dst->node->parent.node) {
		reiser4_place_t p;

		reiser4_place_init(&p, dst->node->parent.node,
				   &dst->node->parent.pos);
		
		if ((res = reiser4_tree_ukey(tree, &p, &hint.key)))
			return res;
	}
	
	if (dst->node != tree->root && !dst->node->parent.node) {
		
		if (!old.node->parent.node)
			reiser4_tree_growup(tree);
	
		if ((res = reiser4_tree_attach(tree, dst->node))) {
			aal_exception_error("Can't attach node %llu to "
					    "the tree.", dst->node->blk);
			reiser4_tree_release(tree, dst->node);
			return res;
		}
	}
    
	return 0;
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
	
	reiser4_key_t *key;
	reiser4_place_t old;

	aal_assert("umka-779", tree != NULL);
	aal_assert("umka-779", hint != NULL);
	
	aal_assert("umka-1644", place != NULL);
	aal_assert("umka-1645", hint->plugin != NULL);

	/*
	  Checking if tree is fresh one, thus, it does not have the root
	  node. If so, we allocate new node of the requested level, insert
	  item/unit into it and then attach it into the empty tree by means of
	  using reiser4_tree_attach function. This function will take care about
	  another things which should be done for keeping reiser4 tree in tact
	  and namely alloate new root and insert one nodeptr item into it.
	*/
	if (reiser4_tree_fresh(tree)) {

		if (level == LEAF_LEVEL) {
			if ((res = reiser4_tree_alloc_root(tree)))
				return res;
		}
		
		if (!(place->node = reiser4_tree_alloc(tree, level)))
			return -EINVAL;
		
		POS_INIT(&place->pos, 0, ~0ul);
		
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
	} else {
		if ((res = reiser4_tree_load_root(tree)))
			return res;

		/*
		  Checking if we have the tree with height smaller than
		  requested level. If so, we should grow the tree up to
		  requested level.
		*/
		if (level > reiser4_tree_height(tree)) {

			while (level > reiser4_tree_height(tree))
				reiser4_tree_growup(tree);

			/*
			  Getting new place item/unit will be inserted at after
			  tree is growed up. It is needed because we want insert
			  item onto level equal to the requested one passed by
			  @level variable.
			*/
			if (reiser4_tree_lookup(tree, &hint->key, level,
						place) == LP_FAILED)
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

			/*
			  Allocating node of requested level and assign place
			  for insert to it.
			*/
			if (!(place->node = reiser4_tree_alloc(tree, level)))
				return -ENOSPC;

			POS_INIT(&place->pos, 0, ~0ul);
		}
	}

	/* Estimating item/unit to inserted to tree */
	if ((res = reiser4_item_estimate(place, hint)))
		return res;
	
		
	if (tree->traps.pre_insert) {
		if ((res = tree->traps.pre_insert(tree, place, hint,
						  tree->traps.data)))
			return res;
	}

	/*
	  Saving mode of insert (insert new item, paste units to the existent
	  one) before making space for new inset/unit.
	*/
	mode = (place->pos.unit == ~0ul);

	/* Making space in tree in order to insert new item/unit into it */
	if ((res = reiser4_tree_mkspace(tree, place, hint->plugin, hint->len))) {
		aal_exception_error("Can't prepare space for insert "
				    "one more item/unit.");
		return res;
	}

	/*
	  As position after making space is generaly changing, we check is mode
	  of insert was changed or not. If so, we should perform estimate one
	  more time. That is because, estimated value depends on insert mode.
	*/
	if (mode != (place->pos.unit == ~0ul)) {
		if ((res = reiser4_item_estimate(place, hint)))
			return res;
	}

	/* Inserting item/unit and updating parent keys */
	if ((res = reiser4_node_insert(place->node, &place->pos, hint))) {
		aal_exception_error("Can't insert an item/unit into the "
				    "node %llu.", place->node->blk);
		return res;
	}

	/*
	  Parent keys will be updated if we inserted item/unit into leftmost pos
	  and if target node has the parent.
	*/
	if (reiser4_place_leftmost(place) &&
	    place->node->parent.node)
	{
		reiser4_place_t p;

		reiser4_place_init(&p, place->node->parent.node,
				   &place->node->parent.pos);
		
		if ((res = reiser4_tree_ukey(tree, &p, &hint->key)))
			return res;
	}
	
	/* 
	  If make space function allocates new node, we should attach it to the
	  tree. Also, here we should handle the special case, when tree root
	  should be changed.
	*/
	if (place->node != tree->root && !place->node->parent.node) {
		
		aal_assert("vpf-889", old.node != NULL);
		
		/* Growing the tree */
		if (!old.node->parent.node)
			reiser4_tree_growup(tree);
	
		/* Attaching new node to the tree */
		if ((res = reiser4_tree_attach(tree, place->node))) {
			aal_exception_error("Can't attach node %llu to "
					    "the tree.", place->node->blk);
			reiser4_tree_release(tree, place->node);
			return res;
		}
	}

	/* Initializing insert point place */
	if ((res = reiser4_place_realize(place)))
		return res;

	if ((res = reiser4_item_realize(place)))
		return res;

	/* Calling post_insert hook installed in tree */
	if (tree->traps.post_insert) {
		if ((res = tree->traps.post_insert(tree, place, hint,
						   tree->traps.data)))
			return res;
	}
    
	return 0;
}

/*
  Cuts some amount of items or units from the tree. Updates internal keys and
  children list.
*/
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

	node = reiser4_tree_right(tree, start->node);
	
	while (node && node != end->node)
		node = reiser4_tree_right(tree, node);

	if (node != end->node) {
		aal_exception_error("End place is not reachable from the"
				    "start one durring cutting the tree.");
		return -EINVAL;
	}

	if (start->node != end->node) {
		pos_t pos = {~0ul, ~0ul};

		/* Removing start + 1 though end - 1 node from the tree */
		node = reiser4_tree_right(tree, start->node);
		
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
		    start->node->parent.node)
		{
			reiser4_place_t p;
			reiser4_key_t lkey;

			if ((res = reiser4_node_lkey(start->node, &lkey)))
				return res;
			
			reiser4_place_init(&p, start->node->parent.node,
					   &start->node->parent.pos);

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
		    end->node->parent.node)
		{
			reiser4_place_t p;
			reiser4_key_t lkey;

			if ((res = reiser4_node_lkey(end->node, &lkey)))
				return res;
			
			reiser4_place_init(&p, end->node->parent.node,
					   &end->node->parent.pos);

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
		    start->node->parent.node)
		{
			reiser4_place_t p;
			reiser4_key_t lkey;

			if ((res = reiser4_node_lkey(start->node, &lkey)))
				return res;
			
			reiser4_place_init(&p, start->node->parent.node,
					   &start->node->parent.pos);

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

/* Installs ne wpack handler. If it is NULL, default one will be used */
void reiser4_tree_pack_handler(reiser4_tree_t *tree,
			       pack_func_t func)
{
	aal_assert("umka-1896", tree != NULL);

	tree->traps.pack = (func != NULL) ? func :
		callback_tree_pack;
}


/*
  Switches on/off flag, which displays should tree pack itself after remove
  operations or not. It is needed because all operations like this should be
  under control.
*/
void reiser4_tree_pack_on(reiser4_tree_t *tree) {
	aal_assert("umka-1881", tree != NULL);
	tree->flags |= TF_PACK;
}

void reiser4_tree_pack_off(reiser4_tree_t *tree) {
	aal_assert("umka-1882", tree != NULL);
	tree->flags &= ~TF_PACK;
}

/*
  Removes item/unit at passd @place. This functions also perform so called
  "local packing". This is shift as many as possible items and units from the
  node pointed by @place into its left neighbour node and the same shift from
  the right neighbour into target node. This behavior may be controlled by
  tree's control flags (tree->flags) or by functions tree_pack_on() and
  tree_pack_off().
*/
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
	    place->node->parent.node)
	{

		/*
		  If node became empty it will be detached from the tree, so,
		  updating is not needed and impossible, becauseit has not
		  items.
		*/
		if (reiser4_node_items(place->node) > 0) {
			reiser4_place_t p;
			reiser4_key_t lkey;

			/* Updating parent keys */
			reiser4_node_lkey(place->node, &lkey);
				
			reiser4_place_init(&p, place->node->parent.node,
					   &place->node->parent.pos);

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

	/*
	  Checking if the node became empty. If so, we release it, otherwise we
	  pack the tree about it.
	*/
	if (reiser4_node_items(place->node) > 0) {
		if ((tree->flags & TF_PACK) && tree->traps.pack) {
			if ((res = tree->traps.pack(tree, place, tree->traps.data)))
				return res;
		}
	} else {
		/* Detaching node from the tree, because it became empty */
		reiser4_node_mkclean(place->node);
		reiser4_tree_detach(tree, place->node);

		/*
		  Freeing node and updating place node component in order to let
		  user know that node do not exist longer.
		*/
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
	reiser4_tree_t *tree,                /* tree for traversing it */
	reiser4_node_t *node,		     /* node which should be traversed */
	traverse_hint_t *hint,		     /* hint for traverse and for callback methods */
	traverse_open_func_t open_func,	     /* callback for node opening */
	traverse_edge_func_t before_func,    /* callback to be called at the beginning */
	traverse_setup_func_t setup_func,    /* callback to be called before a child  */
	traverse_setup_func_t update_func,   /* callback to be called after a child */
	traverse_edge_func_t after_func)     /* callback to be called at the end */
{
	errno_t res = 0;
	reiser4_place_t place;
	pos_t *pos = &place.pos;
	reiser4_node_t *child = NULL;
 
	aal_assert("vpf-418", hint != NULL);
	aal_assert("vpf-390", node != NULL);
	aal_assert("umka-1935", tree != NULL);

	reiser4_node_lock(node);

	if ((before_func && (res = before_func(node, hint->data))))
		goto error_unlock_node;

	/* The loop though the items of current node */
	for (pos->item = 0; pos->item < reiser4_node_items(node); pos->item++) {
		pos->unit = ~0ul; 

		/*
		  If there is a suspicion in a corruption, it must be checked in
		  before_func. All items must be opened here.
		*/
		if (reiser4_place_open(&place, node, pos)) {
			aal_exception_error("Can't open item by place. Node "
					    "%llu, item %u.", node->blk,
					    pos->item);
			goto error_after_func;
		}

		if (!reiser4_item_branch(&place))
			continue;

		/* The loop though the units of the current item */
		for (pos->unit = 0; pos->unit < reiser4_item_units(&place); pos->unit++) {
			ptr_hint_t ptr;
			
			/* Fetching node ptr */
			plugin_call(place.item.plugin->item_ops, read,
				    &place.item, &ptr, pos->unit, 1);

			if (setup_func && (res = setup_func(&place, hint->data))) {
				if (res < 0)
					goto error_after_func;
				else
					continue;
			}

			/*
			  Trying to get child node pointed by @ptr from the
			  parent cache. If it is successful we will traverse
			  it. If we are unable to find it in parent cache, it
			  means that it is not loaded yet and w load it and
			  connect to the tree explicitly.
			*/
			if (!(child = reiser4_node_cbp(node, ptr.start))) {
						
				if (!open_func)
					goto update;

				/* Opening the node by its @ptr */
				if ((res = open_func(&child, ptr.start, hint->data)))
					goto error_update_func;

				if (!child)
					goto update;

				/* Connect opened node to the tree */
				if (reiser4_tree_connect(tree, node, child))
					goto error_free_child;

				/* Traversing the node */
				if ((res = reiser4_tree_down(tree, child,
							     hint, open_func,
							     before_func,
							     setup_func,
							     update_func,
							     after_func)) < 0)
					goto error_free_child;

				/*
				  Here we're trying to unload loaded earlier
				  @child if it is not used and corresponding
				  control flag is specified.
				*/
				if (!reiser4_node_locked(child) && hint->cleanup)
					reiser4_tree_unload(tree, child);
			} else {
				if ((res = reiser4_tree_down(tree, child,
							     hint, open_func,
							     before_func,
							     setup_func,
							     update_func,
							     after_func)) < 0)
					goto error_after_func;

			}
			
		update:
			if (update_func && (res = update_func(&place, hint->data)))
				goto error_after_func;
		}
	}
	
	if (after_func)
		res = after_func(node, hint->data);

	reiser4_node_unlock(node);
	
	return res;

 error_free_child:
	if (!reiser4_node_locked(child) && hint->cleanup)
		reiser4_tree_unload(tree, child);

 error_update_func:
	if (update_func)
		res = update_func(&place, hint->data);
    
 error_after_func:
	if (after_func)
		res = after_func(node, hint->data);

 error_unlock_node:
	reiser4_node_unlock(node);
	return res;
}

errno_t reiser4_tree_traverse(
	reiser4_tree_t *tree,		     /* node which should be traversed */
	traverse_hint_t *hint,		     /* hint for for callback methods */
	traverse_open_func_t open_func,	     /* callback for node opening */
	traverse_edge_func_t before_func,    /* callback to be called at the start */
	traverse_setup_func_t setup_func,    /* callback to be called before a child  */
	traverse_setup_func_t update_func,   /* callback to be called after a child */
	traverse_edge_func_t after_func)     /* callback to be called at the end */
{
	errno_t res;
	
	aal_assert("umka-1768", tree != NULL);

	if ((res = reiser4_tree_load_root(tree)))
		return res;
	
	return reiser4_tree_down(tree, tree->root, hint, open_func,
				 before_func, setup_func, update_func,
				 after_func);
}

#endif
