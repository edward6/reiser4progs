/*
  tree.c -- reiser4 tree cache code.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
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

static int callback_node_sync(void *data) {
	reiser4_node_t *node = (reiser4_node_t *)data;
	return reiser4_node_sync(node) == 0;
}

static aal_list_t *callback_get_next(void *data) {
	return ((reiser4_node_t *)data)->lru.next;
}

static void callback_set_next(void *data, aal_list_t *next) {
	((reiser4_node_t *)data)->lru.next = next;
}

static aal_list_t *callback_get_prev(void *data) {
	return ((reiser4_node_t *)data)->lru.prev;
}

static void callback_set_prev(void *data, aal_list_t *prev) {
	((reiser4_node_t *)data)->lru.prev = prev;
}
		
static lru_ops_t lru_ops = {
	.free     = callback_node_free,
	.sync     = callback_node_sync,

	.get_next = callback_get_next,
	.set_next = callback_set_next,
	.get_prev = callback_get_prev,
	.set_prev = callback_set_prev
};

#ifndef ENABLE_COMPACT

/* Requests block allocator for new block and creates empty node in it */
reiser4_node_t *reiser4_tree_allocate(
	reiser4_tree_t *tree,	    /* tree for operating on */
	uint8_t level)	 	    /* level of new node */
{
	blk_t blk;
	rpid_t pid;

	uint32_t free, stamp;
	reiser4_node_t *node;
	aal_device_t *device;
    
	aal_assert("umka-756", tree != NULL, return NULL);
    
	/* Allocating the block */
	if (!reiser4_alloc_allocate_region(tree->fs->alloc, &blk, 1)) {
		aal_exception_error("Can't allocate block for new node. "
				    "No space left?");
		return NULL;
	}

	device = tree->fs->device;
	pid = tree->root->entity->plugin->h.id;
    
	/* Creating new node */
	if (!(node = reiser4_node_create(device, blk, pid, level)))
		return NULL;

	reiser4_node_set_make_stamp(node, 
		reiser4_format_get_make_stamp(tree->fs->format));
	
	/* Setting up of the free blocks in format */
	free = reiser4_alloc_free(tree->fs->alloc);
	reiser4_format_set_free(tree->fs->format, free);

	stamp = reiser4_node_get_flush_stamp(tree->root);
	reiser4_node_set_flush_stamp(node, stamp);

	reiser4_node_mkdirty(node);
	node->tree = tree;
	
	return node;
    
 error_free_node:
	reiser4_node_close(node);
	return NULL;
}

void reiser4_tree_release(reiser4_tree_t *tree, reiser4_node_t *node) {
	blk_t free;
	
	aal_assert("umka-917", node != NULL, return);

	free = reiser4_alloc_free(tree->fs->alloc);
	
    	/* Sets up the free blocks in block allocator */
	reiser4_alloc_release_region(tree->fs->alloc, node->blk, 1);
	reiser4_format_set_free(tree->fs->format, free);
    
	reiser4_node_close(node);
}

#endif

/* Loads node and attaches it into the tree */
reiser4_node_t *reiser4_tree_load(reiser4_tree_t *tree,
				  reiser4_node_t *parent,
				  blk_t blk)
{
	aal_device_t *device;
	reiser4_node_t *node = NULL;

	aal_assert("umka-1289", tree != NULL, return NULL);
    
	device = tree->fs->device;

	if (parent)
		node = reiser4_node_cbp(parent, blk);

	if (!node) {
		if (!(node = reiser4_node_open(device, blk))) {
			aal_exception_error("Can't read block %llu. %s.",
					    blk, device->error);
			return NULL;
		}
		
		node->tree = tree;
		reiser4_node_mkclean(node);

		if (parent && reiser4_node_attach(parent, node))
			return NULL;
	}
	
	return node;
}

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
    
	aal_assert("umka-1090", tree != NULL, return -1);
	aal_assert("umka-1091", tree->fs != NULL, return -1);
	aal_assert("umka-1092", tree->fs->oid != NULL, return -1);
    
	oid = tree->fs->oid;
    
	/* Finding needed key plugin by its identifier */
	if (!(plugin = libreiser4_factory_ifind(KEY_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find key plugin by its id 0x%x.", pid);
		return -1;
	}
    
	/* Getting root directory attributes from oid allocator */
	locality = plugin_call(return -1,
			       oid->entity->plugin->oid_ops, root_locality,);

	objectid = plugin_call(return -1,
			       oid->entity->plugin->oid_ops, root_objectid,);

	/* Initializing the key by found plugin */
	tree->key.plugin = plugin;

	/* Building the key */
	reiser4_key_build_generic(&tree->key, KEY_STATDATA_TYPE,
				  locality, objectid, 0);

	return 0;
}

/* Returns tree root block number */
blk_t reiser4_tree_root(reiser4_tree_t *tree) {
	aal_assert("umka-738", tree != NULL, return INVAL_BLK);
	return tree->root->blk;
}

#ifndef ENABLE_COMPACT

static errno_t callback_tree_mpressure(void *data, int result) {
	reiser4_tree_t *tree = (reiser4_tree_t *)data;
	
	if (!result)
		return 0;

	aal_mpressure_disable(tree->mpressure);

	if (aal_lru_adjust(tree->lru))
		return -1;
	
	aal_mpressure_enable(tree->mpressure);
	
	return 0;
}

#endif

static errno_t reiser4_tree_init(reiser4_tree_t *tree) {

	if (!(tree->lru = aal_lru_create(&lru_ops))) {
		aal_exception_error("Can't initialize tree cache lru list.");
		return -1;
	}
	
#ifndef ENABLE_COMPACT
	tree->mpressure = aal_mpressure_handler_create(callback_tree_mpressure,
						      "tree cache", (void *)tree);
#else
	tree->mpressure = NULL;
#endif

	/*
	  FIXME-UMKA: here should not be hardcoded plugin ids. Probably we
	  should get the mfrom the fs instance.
	*/
	tree->profile.key = KEY_REISER40_ID;
	tree->profile.nodeptr = ITEM_NODEPTR40_ID;
	
	return 0;
}

void reiser4_tree_fini(reiser4_tree_t *tree) {
	aal_assert("umka-1531", tree != NULL, return);

	if (tree->mpressure)
		aal_mpressure_handler_free(tree->mpressure);

	aal_lru_free(tree->lru);
}

/* Opens the tree (that is, the tree cache) on specified filesystem */
reiser4_tree_t *reiser4_tree_open(reiser4_fs_t *fs) {
	blk_t tree_root;
	reiser4_tree_t *tree;

	aal_assert("umka-737", fs != NULL, return NULL);

	/* Allocating memory for the tree instance */
	if (!(tree = aal_calloc(sizeof(*tree), 0)))
		return NULL;
    
	tree->fs = fs;

	/* Building the tree root key */
	if (reiser4_tree_key(tree, KEY_REISER40_ID)) {
		aal_exception_error("Can't build the tree root key.");
		goto error_free_tree;
	}
    
	/* Opening root node */
	if ((tree_root = reiser4_format_get_root(fs->format)) == INVAL_BLK)
		goto error_free_tree;
	
	if (!(tree->root = reiser4_tree_load(tree, NULL, tree_root)))
		goto error_free_tree;
    
	tree->root->tree = tree;
	reiser4_tree_init(tree);
    
	return tree;

 error_free_tree:
	aal_free(tree);
	return NULL;
}

#ifndef ENABLE_COMPACT

/* Creates new balanced tree on specified filesystem */
reiser4_tree_t *reiser4_tree_create(
	reiser4_fs_t *fs,		    /* filesystem new tree will be created on */
	reiser4_profile_t *profile)	    /* profile to be used */
{
	blk_t blk;
	uint8_t level;
	reiser4_tree_t *tree;

	aal_assert("umka-741", fs != NULL, return NULL);
	aal_assert("umka-749", profile != NULL, return NULL);

	/* Allocating memory needed for tree instance */
	if (!(tree = aal_calloc(sizeof(*tree), 0)))
		return NULL;

	tree->fs = fs;
    
	/* Building the tree root key */
	if (reiser4_tree_key(tree, profile->key)) {
		aal_exception_error("Can't build the tree root key.");
		goto error_free_tree;
	}
    
	/* Getting free block from block allocator for place root block in it */
	if (!reiser4_alloc_allocate_region(fs->alloc, &blk, 1)) {
		aal_exception_error("Can't allocate block for the root node.");
		goto error_free_tree;
	}

	level = reiser4_format_get_height(fs->format);
    
	/* Creating root node */
	if (!(tree->root = reiser4_node_create(fs->device, blk,
					       profile->node, level)))
	{
		aal_exception_error("Can't create root node.");
		goto error_free_tree;
	}

	/* Setting up of the root block */
	reiser4_format_set_root(fs->format, tree->root->blk);
    
	/* Setting up of the free blocks */
	reiser4_format_set_free(fs->format, reiser4_alloc_free(fs->alloc));

	tree->root->tree = tree;
	reiser4_tree_init(tree);

	return tree;

 error_free_tree:
	aal_free(tree);
	return NULL;
}

/* 
   Saves whole cached tree and removes all nodes except root node from the
   cache.
*/
errno_t reiser4_tree_flush(reiser4_tree_t *tree) {
	aal_list_t *list;
    
	aal_assert("umka-573", tree != NULL, return -1);

	reiser4_tree_sync(tree);
    
	list = tree->root->children ? 
		aal_list_first(tree->root->children) : NULL;
    
	if (list) {
		aal_list_t *walk;
	
		aal_list_foreach_forward(walk, list)
			reiser4_node_close((reiser4_node_t *)walk->data);
	
		tree->root->children = NULL;
	}

	return 0;
}

/* Syncs whole tree cache */
errno_t reiser4_tree_sync(reiser4_tree_t *tree) {
	aal_assert("umka-560", tree != NULL, return -1);
	return reiser4_node_sync(tree->root);
}

#endif

/* Closes specified tree (frees all assosiated memory) */
void reiser4_tree_close(reiser4_tree_t *tree) {
	aal_assert("umka-134", tree != NULL, return);

	/* Freeing tree cashe and tree itself*/
	reiser4_node_close(tree->root);

	reiser4_tree_fini(tree);
	aal_free(tree);
}

uint8_t reiser4_tree_height(reiser4_tree_t *tree) {
	aal_assert("umka-1065", tree != NULL, return 0);
	aal_assert("umka-1284", tree->fs != NULL, return 0);
	aal_assert("umka-1285", tree->fs->format != NULL, return 0);

	return reiser4_format_get_height(tree->fs->format);
}

/* 
   Makes search in the tree by specified key. Fills passed coord by coords of 
   found item. 
*/
int reiser4_tree_lookup(
	reiser4_tree_t *tree,	/* tree to be grepped */
	reiser4_key_t *key,	/* key to be find */
	uint8_t level,	        /* stop level for search */
	reiser4_coord_t *coord)	/* coord of found item */
{
	int result, deep;

	reiser4_coord_t fake;
	reiser4_ptr_hint_t ptr;
	reiser4_pos_t pos = {0, ~0ul};
	reiser4_node_t *parent = NULL;

	aal_assert("umka-1760", tree != NULL, return -1);
	aal_assert("umka-742", key != NULL, return -1);

	if (!coord)
		coord = &fake;
    
	deep = reiser4_tree_height(tree);
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
	
		/* 
		  Looking up for key inside node. Result of lookuping will be
		  stored in &coord->pos.
		*/
		if ((result = reiser4_node_lookup(node, key, &coord->pos)) == -1)
			return -1;

		if (reiser4_node_items(node) == 0)
			return result;

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
			return -1;
		}

		if (!reiser4_item_nodeptr(coord)) {

			if (result == 1)
				reiser4_coord_realize(coord);
			
			return result;
		}
		
		item = &coord->item;
		
		/* Getting the node pointer from internal item */
		plugin_call(return -1, item->plugin->item_ops, fetch, item, 
			    &ptr, coord->pos.unit, 1);
		
		if (ptr.ptr == INVAL_BLK) {
			aal_exception_error("Can't get pointer from nodeptr item %u, "
					    "node %llu.", coord->pos.item,
					    coord->node->blk);
			return -1;
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
			return -1;
		}

		reiser4_node_unlock(parent);
	}
    
	return 0;
    
 error_free_node:
	reiser4_node_close(coord->node);
	return -1;
}

#ifndef ENABLE_COMPACT

/* This function inserts nodeptr item to the tree */
errno_t reiser4_tree_attach(
	reiser4_tree_t *tree,	    /* tree we will attach node to */
	reiser4_node_t *node)       /* child to attached */
{
	int result;

	errno_t res;
	uint8_t stop;
	reiser4_coord_t coord;
	reiser4_ptr_hint_t ptr;
	reiser4_item_hint_t hint;

	aal_assert("umka-913", tree != NULL, return -1);
	aal_assert("umka-916", node != NULL, return -1);
	aal_assert("umka-1703", reiser4_node_items(node) > 0, return -1);
    
	/* Preparing nodeptr item hint */
	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&ptr, 0, sizeof(ptr));

	hint.hint = &ptr;

	ptr.ptr = node->blk;
	ptr.width = 1;

	reiser4_node_lkey(node, &hint.key);

	hint.plugin = libreiser4_factory_ifind(ITEM_PLUGIN_TYPE,
					       tree->profile.nodeptr);
	
	if (!hint.plugin) {
		aal_exception_error("Can't find item plugin by its id 0x%x.",
				    tree->profile.nodeptr);
		return -1;
	}

	stop = reiser4_node_level(node) + 1;

	/*
	  Checking if we have the tree with height smaller than node we are
	  going to attach in it. If so, we should grow the tree by requested
	  level.
	*/
	while (stop > reiser4_tree_height(tree))
		reiser4_tree_grow(tree);
		
	/* Looking up for the insert coord */
	if ((result = reiser4_tree_lookup(tree, &hint.key, stop, &coord))) {

		if (result == FAILED) {
			aal_stream_t stream = EMPTY_STREAM;
			reiser4_key_print(&hint.key, &stream);

			aal_exception_error("Lookup of key %s failed.",
					    stream.data);
			
			aal_stream_fini(&stream);
		}

		return result;
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
	if (reiser4_node_attach(coord.node, node)) {
		aal_exception_error("Can't attach the node %llu to the tree.", 
				    node->blk);
		return -1;
	}

	return 0;
}

errno_t reiser4_tree_detach(reiser4_tree_t *tree,
			    reiser4_node_t *node)
{
	reiser4_coord_t coord;
	reiser4_node_t *parent;
	
	aal_assert("umka-1726", tree != NULL, return -1);
	aal_assert("umka-1727", node != NULL, return -1);

	if (!(parent = node->parent))
		return 0;

	reiser4_coord_init(&coord, parent, &node->pos);
	
	/* Removing item/unit from the parent node */
	if (reiser4_tree_remove(tree, &coord))
		return -1;

	reiser4_node_detach(parent, node);
			
	return 0;
}

/* This function grows and sets up tree after the growing */
errno_t reiser4_tree_grow(
	reiser4_tree_t *tree)	/* tree to be growed up */
{
	uint8_t tree_height;
	reiser4_node_t *old_root;

	aal_assert("umka-1701", tree != NULL, return -1);
	aal_assert("umka-1736", tree->root != NULL, return -1);
	
	if (!(old_root = tree->root))
		return -1;
	
	aal_assert("umka-1702", reiser4_node_items(old_root) > 0,
		   return -1);
	
	tree_height = reiser4_tree_height(tree);
    
	/* Allocating new root node */
	if (!(tree->root = reiser4_tree_allocate(tree, tree_height + 1))) {
		aal_exception_error("Can't allocate new root node.");
		goto error_return_root;
	}

	tree->root->tree = tree;

	/* Updating format-related fields */
    	reiser4_format_set_root(tree->fs->format,
				tree->root->blk);

	reiser4_format_set_height(tree->fs->format,
				  tree_height + 1);
	
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

/* Drys up the try by one level */
errno_t reiser4_tree_dryup(reiser4_tree_t *tree) {
	aal_list_t *children;
	uint32_t tree_height;
	reiser4_node_t *old_root;

	aal_assert("umka-1731", tree != NULL, return -1);
	aal_assert("umka-1737", tree->root != NULL, return -1);

	tree_height = reiser4_tree_height(tree);

	if (tree_height - 1 == LEAF_LEVEL)
		return -1;

	/* Replacing old root ndoe by new root */
	if (!(old_root = tree->root))
		return -1;

	children = old_root->children;
	
	aal_assert("umka-1735", children != NULL, return -1);
	
	if (reiser4_node_items(old_root) > 1)
		return -1;

	tree->root = (reiser4_node_t *)children->data;
	reiser4_node_detach(old_root, tree->root);

	/* Releasing old root */
	reiser4_node_mkclean(old_root);
	reiser4_tree_release(tree, old_root);

	/* Setting up format tree-related fields */
    	reiser4_format_set_root(tree->fs->format,
				tree->root->blk);

	reiser4_format_set_height(tree->fs->format,
				  tree_height - 1);

	return 0;
}

errno_t reiser4_tree_shift(
	reiser4_tree_t *tree,	/* tree we will operate on */
	reiser4_coord_t *coord,	/* insert point coord */
	reiser4_node_t *node,	/* node items will be shifted to */
	uint32_t flags)	        /* some flags (direction, move ip or not, etc) */
{
	shift_hint_t hint;

	aal_assert("umka-1225", tree != NULL, return -1);
	aal_assert("umka-1226", coord != NULL, return -1);
	aal_assert("umka-1227", node != NULL, return -1);
    
	aal_memset(&hint, 0, sizeof(hint));
	
	hint.flags = flags;
	hint.pos = coord->pos;
	
	if (reiser4_node_shift(coord->node, node, &hint) < 0)
		return -1;

	coord->pos = hint.pos;

	if (hint.flags & SF_MOVIP)
		coord->node = node;

	return 0;
}

errno_t reiser4_tree_mkspace(
	reiser4_tree_t *tree,	    /* tree pointer function operates on */
	reiser4_coord_t *coord,	    /* coord of insertion point */
	uint32_t needed)	    /* amount of space that should be freed */
{
	int alloc;
	uint32_t max_space;
	int32_t not_enough;

	reiser4_coord_t old;
	reiser4_node_t *left;
	reiser4_node_t *right;

	aal_assert("umka-766", coord != NULL, return -1);
	aal_assert("umka-929", tree != NULL, return -1);

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
	if ((left = reiser4_node_left(coord->node))) {
	    
		if (reiser4_tree_shift(tree, coord, left, SF_LEFT | SF_UPTIP))
			return -1;
	
		if ((not_enough = needed - reiser4_node_space(coord->node)) <= 0)
			return 0;
	}

	/* Shifting data into right neighbour if it exists */
	if ((right = reiser4_node_right(coord->node))) {
	    
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
	
		level = reiser4_node_level(coord->node);
	
		if (!(node = reiser4_tree_allocate(tree, level)))
			return -1;
		
		flags = SF_RIGHT | SF_UPTIP;

		if (alloc == 0)
			flags |= SF_MOVIP;

		save = *coord;
		
		if (reiser4_tree_shift(tree, coord, node, flags))
			return -1;	

		/*
		  Releasing old node, because it has become empty as result of data
		  shifting.
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

errno_t reiser4_tree_split(reiser4_tree_t *tree, 
			   reiser4_coord_t *coord, 
			   int level) 
{
	int cur_level;
	uint64_t stamp;
	reiser4_node_t *node;
	reiser4_pos_t pos = {0, 0};
	
	aal_assert("vpf-672", tree != NULL, return -1);
	aal_assert("vpf-673", coord != NULL, return -1);
	aal_assert("vpf-674", level > 0, return -1);

	cur_level = reiser4_node_level(coord->node);
			
	aal_assert("vpf-680", cur_level <= level, return -1);
	
	if (reiser4_coord_realize(coord))
		return -1;

	while (cur_level <= level) {
		aal_assert("vpf-676", coord->node->parent != NULL, return -1);
		
		if (coord->pos.item != 0 || coord->pos.unit != 0 || 
		    coord->pos.item != reiser4_node_items(coord->node) || 
		    coord->pos.unit != reiser4_item_units(coord))
		{
			/* We are not on the border, split. */
			if ((node = reiser4_tree_allocate(tree, cur_level)) == NULL) {
				aal_exception_error("Tree failed to allocate "
						    "a new node.");
				return -1;
			}
    
			/* Set flush_id */
			stamp = reiser4_node_get_flush_stamp(coord->node);
			reiser4_node_set_flush_stamp(node, stamp);
    
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
	
	errno_t res;

	aal_assert("umka-779", tree != NULL, return -1);
	aal_assert("umka-779", hint != NULL, return -1);
	
	aal_assert("umka-1644", coord != NULL, return -1);
	aal_assert("umka-1645", hint->plugin != NULL, return -1);

	/* This is the special case. The tree doesn't contain any nodes */
	if (reiser4_node_items(tree->root) == 0) {
		int twig_legal;

		twig_legal = plugin_call(return -1, tree->root->entity->plugin->node_ops,
					 item_legal, tree->root->entity, hint->plugin);

		/*
		  Checking if we are trying to insert an item to illegal level.
		  If soo, we're considering that this is the special case when
		  only empty root exists.
		*/
		if (twig_legal) {
			coord->pos.item = 0;
			coord->pos.unit = ~0ul;
			
			if (!(coord->node = reiser4_tree_allocate(tree, LEAF_LEVEL))) {
				aal_exception_error("Can't allocate new leaf node.");
				return -1;
			}
	
			if (reiser4_node_insert(coord->node, &coord->pos, hint)) {
	    
				aal_exception_error("Can't insert an item into the node %llu.", 
						    coord->node->blk);
	    
				reiser4_tree_release(tree, coord->node);
				return -1;
			}
	
			if (reiser4_tree_attach(tree, coord->node)) {
				aal_exception_error("Can't attach node %llu to the tree.",
						    coord->node->blk);
				reiser4_tree_release(tree, coord->node);
				return -1;
			}
	
			return 0;
		}
	}

	/* Estimating item in order to insert it into found node */
	if (reiser4_item_estimate(coord, hint))
		return -1;
    
	/* Needed space is estimated space plugs item overhead */
	needed = hint->len + (coord->pos.unit == ~0ul ? 
			      reiser4_node_overhead(coord->node) : 0);
	
	old = *coord;
		
	if (tree->traps.preinsert) {
		if ((res = tree->traps.preinsert(coord, hint, tree->traps.data)))
			return res;
	}

	/*
	  Saving mode of insert (insert new item, paste units into an existsent
	  item) before making space for new inset/unit.
	*/
	mode = (coord->pos.unit == ~0ul);
	
	if (reiser4_tree_mkspace(tree, coord, needed)) {
		aal_exception_error("Can't prepare space for insert "
				    "one more item/unit.");
		return -1;
	}

	/*
	  As position after making space is generaly changing, we check is mode
	  of insert was chnaged or not. If so, we should perform estimate one
	  more time. This is because, estimated value depends on insert mode. In
	  the case we are going to insert new item, we should count also
	  internal item overhead.
	*/
	if (mode != (coord->pos.unit == ~0ul)) {
		
		if (reiser4_item_estimate(coord, hint))
			return -1;
	}
	
	if (reiser4_node_insert(coord->node, &coord->pos, hint)) {
		aal_exception_error("Can't insert an %s into the node %llu.", 
				    (coord->pos.unit == ~0ul ? "item" : "unit"),
				    coord->node->blk);
		return -1;
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
    
	if (tree->traps.pstinsert) {
		if ((res = tree->traps.pstinsert(coord, hint, tree->traps.data)))
			return res;
	}
    
	return 0;
}

/* 
    The method should insert/overwrite the specified src_coord to the dst_coord
    from count units started at src_coord->pos.unit.
    
    scr_coord->pos.unit is set to the start of what should be inserted.
    count is amount of units to be inserted.
    dst_coord->pos.unit != ~0ul - item_ops.write does there.
*/
errno_t reiser4_tree_write(
	reiser4_tree_t *tree,	    /* tree insertion is performing into. */
	reiser4_coord_t *dst_coord, /* coord found by lookup */
	reiser4_coord_t *src_coord, /* coord to be inserted. */
	uint32_t count)		    /* number of units to be inserted. */
{
	aal_assert("vpf-683", tree != NULL, return -1);
	aal_assert("vpf-684", dst_coord != NULL, return -1);
	aal_assert("vpf-685", src_coord != NULL, return -1);
	aal_assert("vpf-686", count != 0, return -1);
	
	return 0;
}

/* Removes item by specified key */
errno_t reiser4_tree_remove(
	reiser4_tree_t *tree,	  /* tree item will be removed from */
	reiser4_coord_t *coord)	  /* coord item will be removed at */
{
	errno_t res;

	aal_assert("umka-1018", tree != NULL, return -1);
	aal_assert("umka-1725", coord != NULL, return -1);
	
	if (tree->traps.preremove) {
		if ((res = tree->traps.preremove(coord, tree->traps.data)))
			return res;
	}

	if (reiser4_node_remove(coord->node, &coord->pos))
		return -1;

	if (reiser4_node_items(coord->node) > 0) {
		reiser4_node_t *left, *right;
		
		/*
		  Packing node in order to keep the tree in well packed state
		  anyway. Here we will shift data from the target node to its
		  left neighbour node.
		*/
		if ((left = reiser4_node_left(coord->node))) {
	    
			if (reiser4_tree_shift(tree, coord, left, SF_LEFT)) {
				aal_exception_error("Can't pack node %llu into left.",
						    coord->node->blk);
				return -1;
			}
		}
		
		if (reiser4_node_items(coord->node) > 0) {
			/*
			  Shifting the data from the right neigbour node into
			  the target node.
			*/
			if ((right = reiser4_node_right(coord->node))) {
				
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
		if (coord->node && coord->node == tree->root) {
			
			if (reiser4_node_items(tree->root) == 1)
				reiser4_tree_dryup(tree);
		}
		
	} else {
		if (tree->root != coord->node) {
			/*
			  If node has became empty, then we should release it
			  and release block it is occupying in block allocator.
			*/
			reiser4_node_mkclean(coord->node);
			reiser4_tree_release(tree, coord->node);
		}
	}

	if (tree->traps.pstremove) {
		if ((res = tree->traps.pstremove(coord, tree->traps.data)))
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
	aal_assert("umka-1768", tree != NULL, return -1);

	return reiser4_node_traverse(tree->root, hint, open_func, before_func,
				     setup_func, update_func, after_func);
}

#endif
