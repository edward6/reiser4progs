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
	
	if (node->counter || node->children || !node->parent)
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

	uint32_t free;
	reiser4_node_t *node;
	aal_device_t *device;
    
	aal_assert("umka-756", tree != NULL, return NULL);
    
	/* Allocating the block */
	if ((blk = reiser4_alloc_allocate(tree->fs->alloc)) == INVAL_BLK) {
		aal_exception_error("Can't allocate block for new node. "
				    "No space left?");
		return NULL;
	}

	device = tree->fs->format->device;
	pid = tree->root->entity->plugin->h.sign.id;
    
	/* Creating new node */
	if (!(node = reiser4_node_create(device, blk, pid, level)))
		return NULL;

	plugin_call(goto error_free_node, node->entity->plugin->node_ops,
		    set_stamp, node->entity, reiser4_format_get_stamp(tree->fs->format));
    
	/* Setting up of the free blocks in format */
	free = reiser4_alloc_free(tree->fs->alloc);
	reiser4_format_set_free(tree->fs->format, free);
    
	node->flags |= NF_DIRTY;
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
	reiser4_alloc_release(tree->fs->alloc, node->blk);
	reiser4_format_set_free(tree->fs->format, free);
    
	reiser4_node_close(node);
}

#endif

reiser4_node_t *reiser4_tree_load(reiser4_tree_t *tree, blk_t blk) {
	aal_device_t *device;
	reiser4_node_t *node;

	aal_assert("umka-1289", tree != NULL, return NULL);
    
	device = tree->fs->format->device;
    
	if (!(node = reiser4_node_open(device, blk))) 
		return NULL;
	    
	node->tree = tree;
	node->flags &= ~NF_DIRTY;
	
	return node;
}

/* Returns tree root key */
reiser4_key_t *reiser4_tree_key(reiser4_tree_t *tree) {
	aal_assert("umka-1089", tree != NULL, return NULL);
	return &tree->key;
}

/*
  Builds the tree root key. It is used for lookups and other as init key. This
  method id needed because of root key in reiser3 and reiser4 has a diffrent
  locality and object id values.
*/
static errno_t reiser4_tree_build_key(
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
	if (reiser4_tree_build_key(tree, KEY_REISER40_ID)) {
		aal_exception_error("Can't build the tree root key.");
		goto error_free_tree;
	}
    
	/* Opening root node */
	if ((tree_root = reiser4_format_get_root(fs->format)) == INVAL_BLK)
		goto error_free_tree;
	
	if (!(tree->root = reiser4_tree_load(tree, tree_root)))
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
	aal_device_t *device;
	reiser4_tree_t *tree;

	aal_assert("umka-741", fs != NULL, return NULL);
	aal_assert("umka-749", profile != NULL, return NULL);

	/* Allocating memory needed for tree instance */
	if (!(tree = aal_calloc(sizeof(*tree), 0)))
		return NULL;

	tree->fs = fs;
    
	/* Building the tree root key */
	if (reiser4_tree_build_key(tree, profile->key)) {
		aal_exception_error("Can't build the tree root key.");
		goto error_free_tree;
	}
    
	/* Getting free block from block allocator for place root block in it */
	if ((blk = reiser4_alloc_allocate(fs->alloc)) == INVAL_BLK) {
		aal_exception_error("Can't allocate block for the root node.");
		goto error_free_tree;
	}

	level = reiser4_format_get_height(fs->format);
	device = fs->format->device;
    
	/* Creating root node */
	if (!(tree->root = reiser4_node_create(device, blk,
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
	reiser4_level_t *level,	/* stop level for search */
	reiser4_coord_t *coord)	/* coord of found item */
{
	int lookup, deep;

	reiser4_coord_t fake;
	reiser4_pos_t pos = {0, ~0ul};
	reiser4_node_t *parent = NULL;

	reiser4_ptr_hint_t ptr;

	aal_assert("umka-742", key != NULL, return -1);
	aal_assert("umka-1498", level != NULL, return -1);

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
	
		aal_assert("umka-1632", deep >= level->top, return -1);
		
		/* 
		  Looking up for key inside node. Result of lookuping will be
		  stored in &coord->pos.
		*/
		if ((lookup = reiser4_node_lookup(node, key, &coord->pos)) == -1)
			return -1;

		if (reiser4_node_count(node) == 0)
			return lookup;

		/* Check if we should finish lookup because we reach stop level */
		if (deep <= level->top) {
			
			if (lookup == 1)
				reiser4_coord_realize(coord);
			
			return lookup;
		}
		
		if (lookup == 0 && coord->pos.item > 0)
			coord->pos.item--;
				
		if (reiser4_coord_realize(coord)) {
			blk_t blk = reiser4_coord_blk(coord);
			aal_exception_error("Can't open item by its coord. Node "
					    "%llu, item %u.", blk, coord->pos.item);
			return -1;
		}

		if (!reiser4_item_nodeptr(coord))
			return deep <= level->bottom ? lookup : 0;
		
		item = &coord->entity;
		
		/* Getting the node pointer from internal item */
		if (plugin_call(return -1, item->plugin->item_ops, fetch, item, 
				pos.unit, &ptr, 1))
			return -1;
		
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
		parent->counter++;
		
		if (!(coord->node = reiser4_node_cbp(parent, ptr.ptr))) {
			/* 
			   Node was not found in the cache, we open it and
			   attach to the cache.
			*/
			if (!(coord->node = reiser4_tree_load(tree, ptr.ptr))) {
				aal_exception_error("Can't load node %llu durring "
						    "lookup.", ptr.ptr);
				parent->counter--;
				return -1;
			}

			/* Registering node in tree cache */
			if (reiser4_node_attach(parent, coord->node)) {
				aal_exception_error("Can't attach the node %llu "
						    "in the tree.", ptr.ptr);
				parent->counter--;
				goto error_free_node;
			}
		}

		parent->counter--;
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
	reiser4_node_t *node,       /* child to attached */
	uint8_t level)
{
	reiser4_coord_t coord;
	reiser4_ptr_hint_t ptr;
	reiser4_item_hint_t hint;

	aal_assert("umka-913", tree != NULL, return -1);
	aal_assert("umka-916", node != NULL, return -1);
    
	/* Preparing nodeptr item hint */
	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&ptr, 0, sizeof(ptr));

	hint.hint = &ptr;
	ptr.ptr = node->blk;

	reiser4_node_lkey(node, &hint.key);

	hint.plugin = libreiser4_factory_ifind(ITEM_PLUGIN_TYPE,
					       tree->profile.nodeptr);
	
	if (!hint.plugin) {
		aal_exception_error("Can't find item plugin by its id 0x%x.",
				    tree->profile.nodeptr);
		return -1;
	}

	if (reiser4_tree_insert(tree, &hint, level, &coord)) {
		aal_exception_error("Can't insert nodeptr item to the tree.");
		return -1;
	}

	/*
	  Attaching node to insert point node. We should attach formatted nodes
	  only.
	*/
	if (reiser4_node_attach(coord.node, node)) {
		aal_exception_error("Can't attach the node %llu in tree cache.", 
				    node->blk);
		return -1;
	}

	return 0;
}

/* This function grows and sets up tree after the growing */
static errno_t reiser4_tree_grow(
	reiser4_tree_t *tree)	/* tree to be growed up */
{
	blk_t blk;
	uint8_t tree_height;
	reiser4_node_t *old_root = tree->root;
    
	tree_height = reiser4_tree_height(tree);
    
	/* Allocating new root node */
	if (!(tree->root = reiser4_tree_allocate(tree, tree_height + 1))) {
		aal_exception_error("Can't allocate new root node.");
		return -1;
	}

	tree->root->tree = tree;
	
	if (reiser4_tree_attach(tree, old_root, tree_height)) {
		aal_exception_error("Can't attach old root to the tree.");
		goto error_free_root;
	}

	blk = tree->root->blk;
	
	reiser4_format_set_height(tree->fs->format, tree_height + 1);
    	reiser4_format_set_root(tree->fs->format, blk);
	
	return 0;

 error_free_root:
	reiser4_tree_release(tree, tree->root);
	tree->root = old_root;
	return -1;
}

errno_t reiser4_tree_shift(
	reiser4_tree_t *tree,	/* tree we will operate on */
	reiser4_coord_t *coord,	/* insert point coord */
	reiser4_node_t *neig,	/* node items will be shifted to */
	shift_flags_t flags)	/* some flags (direction, move ip or not, etc) */
{
	shift_hint_t hint;

	aal_assert("umka-1225", tree != NULL, return -1);
	aal_assert("umka-1226", coord != NULL, return -1);
	aal_assert("umka-1227", neig != NULL, return -1);
    
	aal_memset(&hint, 0, sizeof(hint));
	
	hint.flags = flags;
	hint.pos = coord->pos;
	
	if (reiser4_node_shift(coord->node, neig, &hint) < 0)
		return -1;

	coord->pos = hint.pos;

	if (hint.flags & SF_MOVIP)
		coord->node = neig;

	return 0;
}

errno_t reiser4_tree_mkspace(
	reiser4_tree_t *tree,	    /* tree pointer function operates on */
	reiser4_coord_t *old,	    /* old coord of insertion point */
	reiser4_coord_t *new,	    /* new coord will be stored here */
	uint32_t needed)	    /* amount of space that should be freed */
{
	int alloc;
	uint32_t max_space;
	int32_t not_enough;

	reiser4_node_t *left;
	reiser4_node_t *right;
	reiser4_node_t *parent;

	aal_assert("umka-759", old != NULL, return -1);
	aal_assert("umka-766", new != NULL, return -1);
	aal_assert("umka-929", tree != NULL, return -1);

	*new = *old;

	if (needed == 0)
		return 0;
    
	max_space = reiser4_node_maxspace(old->node);
	
	/* 
	   Checking if item hint to be inserted to tree has length more than 
	   max possible space in a node.
	*/
	if (needed > max_space) {
		aal_exception_error("Item size is too big. Maximal possible "
				    "item can be %u bytes long.", max_space);
		return -1;
	}
    
	if ((not_enough = needed  - reiser4_node_space(old->node)) <= 0)
		return 0;

	/* Shifting data into left neighbour if it exists */
	if ((left = reiser4_node_left(new->node))) {
	    
		if (reiser4_tree_shift(tree, new, left, SF_LEFT))
			return -1;
	
		if ((not_enough = needed - reiser4_node_space(new->node)) <= 0)
			return 0;
	}

	/* Shifting data into right neighbour if it exists */
	if ((right = reiser4_node_right(new->node))) {
	    
		if (reiser4_tree_shift(tree, new, right, SF_RIGHT))
			return -1;
	
		if ((not_enough = needed - reiser4_node_space(new->node)) <= 0)
			return 0;
	}
    
	/*
	  Here we still have not enough free space for inserting item/unit into
	  the tree. Allocating new noe and trying to shift data into it.
	*/
	for (alloc = 0; (not_enough > 0) && (alloc < 2); alloc++) {
		uint8_t level;
		reiser4_coord_t save;
		reiser4_node_t *node;
	
		level = plugin_call(return -1, new->node->entity->plugin->node_ops,
				    get_level, new->node->entity);
	
		if (!(node = reiser4_tree_allocate(tree, level)))
			return -1;

		save = *new;

		if (reiser4_tree_shift(tree, new, node, SF_RIGHT | SF_MOVIP))
			return -1;
	
		/* Attaching new allocated node into the tree, if it is not empty */
		if (reiser4_node_count(node)) {

			/* Growing the tree */
			if (!old->node->parent)
				reiser4_tree_grow(tree);
			
			/* Attaching new node to the tree */
			if (reiser4_tree_attach(tree, node, level + 1)) {
				aal_exception_error("Can't attach node to the tree.");
				reiser4_tree_release(tree, node);
				return -1;
			}
		}
	
		not_enough = needed - reiser4_node_space(new->node);
	
		/* Checking if the old have enough free space after shifting */
		if (not_enough > 0 && save.node != new->node &&
		    new->pos.unit == ~0ul)
		{
			*new = save;
			not_enough = needed - reiser4_node_space(new->node);
		}
	}

	/*
	  Releasing old node, because it becames empty as result of data
	  shifting.
	*/
	if (new->node != old->node && reiser4_node_count(old->node) == 0) {
		old->node->flags &= ~NF_DIRTY;

		if (old->node->parent) {
			if (reiser4_node_remove(old->node->parent, &old->node->pos))
				return -1;
		}

		reiser4_tree_release(tree, old->node);
		old->node = NULL;
	}
	
	return -(not_enough > 0);
}

/* Inserts new item described by item hint into the tree */
errno_t reiser4_tree_insert(
	reiser4_tree_t *tree,	    /* tree new item will be inserted in */
	reiser4_item_hint_t *hint,  /* item hint to be inserted */
	uint8_t level,		    /* level insertion will be performed on */
	reiser4_coord_t *coord)	    /* coord item or unit inserted at */
{
	int lookup;
	uint32_t needed;

	reiser4_key_t *key;
	reiser4_coord_t fake;
	reiser4_coord_t insert;
	
	reiser4_level_t stop = {level, level};

	aal_assert("umka-779", tree != NULL, return -1);
	aal_assert("umka-779", hint != NULL, return -1);
  
	if (!coord) coord = &fake;
    
	key = (reiser4_key_t *)&hint->key;

	/* Looking up for target node */
	if ((lookup = reiser4_tree_lookup(tree, key, &stop, coord)) == -1)
		return -1;

	/* Passed key already exists in the tree */
	if (lookup == 1) {
		aal_stream_t stream;

		aal_stream_init(&stream);
		reiser4_key_print(key, &stream);
			
		aal_exception_error("Key %s already exists in tree.",
				    stream.data);
		
		aal_stream_fini(&stream);
		return -1;
	}

	/* Estimating item in order to insert it into found node */
	if (reiser4_item_estimate(coord, hint))
		return -1;
    
	/* Needed space is estimated space plugs item overhead */
	needed = hint->len + (coord->pos.unit == ~0ul ? 
			      reiser4_node_overhead(coord->node) : 0);
   
	/* This is the special case. The tree doesn't contain any nodes */
	if (level == LEAF_LEVEL && !tree->root->children) {
		reiser4_node_t *node;
		reiser4_pos_t pos = {0, ~0ul};
	
		if (!(node = reiser4_tree_allocate(tree, LEAF_LEVEL))) {
			aal_exception_error("Can't allocate new leaf node.");
			return -1;
		}

		/* Updating coord by just allocated leaf */
		reiser4_coord_init(coord, node, &pos);
	
		if (reiser4_node_insert(coord->node, &coord->pos, hint)) {
	    
			aal_exception_error("Can't insert an item into the node %llu.", 
					    coord->node->blk);
	    
			reiser4_tree_release(tree, node);
			return -1;
		}
	
		if (reiser4_tree_attach(tree, node, level + 1)) {
			aal_exception_error("Can't attach node to the tree.");
			
			reiser4_tree_release(tree, node);
			return -1;
		}
	
		return 0;
	}

	if (reiser4_tree_mkspace(tree, coord, &insert, needed)) {
		aal_exception_error("Can't prepare space for insert "
				    "one more item.");
		return -1;
	}
    
	if (tree->preinsert) {
		if (!tree->preinsert(&insert, hint))
			return -1;
	}

	if (reiser4_node_insert(insert.node, &insert.pos, hint)) {
		aal_exception_error("Can't insert an %s into the node %llu.", 
				    (insert.pos.unit == ~0ul ? "item" : "unit"),
				    insert.node->blk);
		return -1;
	}

	/* 
	   If make space function allocate new node, we should attach it to the
	   tree. Also, here we should handle the spacial case, when tree root
	   should be changed.
	*/
	if (insert.node != tree->root && !insert.node->parent) {

		/* Growing the tree */
		if (!coord->node->parent)
			reiser4_tree_grow(tree);
	
		/* Attaching new node to the tree */
		if (reiser4_tree_attach(tree, insert.node, level + 1)) {
			aal_exception_error("Can't attach node to the tree.");
			reiser4_tree_release(tree, insert.node);
			return -1;
		}
	}
    
	*coord = insert;

	if (tree->pstinsert) {
		if (!tree->pstinsert(&insert, hint))
			return -1;
	}
    
	return 0;
}

/* Removes item by specified key */
errno_t reiser4_tree_remove(
	reiser4_tree_t *tree,	/* tree item will be removed from */
	reiser4_key_t *key,	/* key item will be found by */
	uint8_t level)		/* the level removing will be performed on */
{
	int lookup;
	reiser4_coord_t coord;
	reiser4_level_t stop = {level, level};
    
	aal_assert("umka-1018", tree != NULL, return -1);
	aal_assert("umka-1019", key != NULL, return -1);
    
	/* Looking up for target */
	if ((lookup = reiser4_tree_lookup(tree, key, &stop, &coord)) == -1)
		return -1;

	if (lookup == 0) {
		aal_stream_t stream;

		aal_stream_init(&stream);
		reiser4_key_print(key, &stream);
			
		aal_exception_error("Key %s is not found in tree.",
				    stream.data);
		
		aal_stream_fini(&stream);
		return -1;
	}
    
	if (tree->preremove) {
		if (!tree->preremove(&coord, key))
			return -1;
	}
	
	if (reiser4_node_remove(coord.node, &coord.pos))
		return -1;

	if (tree->pstremove) {
		if (!tree->pstremove(&coord, key))
			return -1;
	}
	
	/*
	  FIXME-UMKA: Here should be also checking if we need descrease tree
	  height.
	*/
    
	return 0;
}

#endif
