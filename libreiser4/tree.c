/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tree.c -- reiser4 tree related code. */

#include <reiser4/reiser4.h>

#ifndef ENABLE_STAND_ALONE
extern errno_t callback_node_insert(reiser4_node_t *node, 
				    pos_t *pos, trans_hint_t *hint);

extern errno_t callback_node_write(reiser4_node_t *node, 
				   pos_t *pos, trans_hint_t *hint);

/* Tree packing callback. Used for packing the tree, after delete items. This is
   needed for keeping tree in well packed state. */
static errno_t callback_tree_pack(reiser4_tree_t *tree,
				  reiser4_place_t *place,
				  void *data)
{
	aal_assert("umka-1897", tree != NULL);
	aal_assert("umka-1898", place != NULL);

	return reiser4_tree_shrink(tree, place);
}

/* Updates root block number in format by passed @blk. Takes care about correct
   block number in loaded root node if any. */
void reiser4_tree_set_root(reiser4_tree_t *tree,
			   blk_t blk)
{
	aal_assert("umka-2409", tree != NULL);
	aal_assert("umka-2412", tree->fs != NULL);
	aal_assert("umka-2413", tree->fs->format != NULL);
	
	if (tree->root)
		reiser4_node_move(tree->root, blk);

	reiser4_format_set_root(tree->fs->format, blk);
}

/* Updates height in format by passed @height. */
void reiser4_tree_set_height(reiser4_tree_t *tree,
			     uint8_t height)
{
	aal_assert("umka-2410", tree != NULL);
	aal_assert("umka-2416", tree->fs != NULL);
	aal_assert("umka-2417", tree->fs->format != NULL);
	
	reiser4_format_set_height(tree->fs->format,
				  height);
}
#endif

/* Fetches data from the @tree to passed @hint */
int64_t reiser4_tree_fetch(reiser4_tree_t *tree,
			   reiser4_place_t *place,
			   trans_hint_t *hint)
{
	return plug_call(place->plug->o.item_ops->object,
			 fetch_units, (place_t *)place, hint);
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
#ifndef ENABLE_STAND_ALONE
	if (reiser4_tree_fresh(tree))
		return -EINVAL;
#endif

	/* Getting root node and loading it. */
	root_blk = reiser4_tree_get_root(tree);
	
	if (!(tree->root = reiser4_tree_load_node(tree, NULL,
						  root_blk)))
	{
		aal_exception_error("Can't load root node %llu.",
				    root_blk);
		return -EIO;
	}

	tree->root->tree = tree;
	
	return 0;
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
/* Assignes passed @node to root. Takes care about root block number and tree
   height in format. */
static errno_t reiser4_tree_assign_root(reiser4_tree_t *tree,
					reiser4_node_t *node)
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

	return 0;
}

/* Dealing with allocating root node if it is not allocated yet */
static errno_t reiser4_tree_alloc_root(reiser4_tree_t *tree) {
	uint32_t height;
	reiser4_node_t *root;
	
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
   nodes. This function does not do any modifications. */
errno_t reiser4_tree_connect_node(
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
					    "tree.", node_blocknr(node));
			return res;
		}
	}

	return res;
}

/* Remove specified child from the node children list. Updates all neighbour
   pointers and parent pointer.*/
errno_t reiser4_tree_disconnect_node(
	reiser4_tree_t *tree,    /* tree instance */
	reiser4_node_t *parent,	 /* node child will be detached from */
	reiser4_node_t *node)	 /* pointer to child to be deleted */
{
	aal_assert("umka-1858", tree != NULL);
	aal_assert("umka-563", node != NULL);
	
	/* Disconnecting left and right neighbours */
	if (node->left) {
		node->left->right = NULL;
		node->left = NULL;
	}
	
	if (node->right) {
		node->right->left = NULL;
		node->right = NULL;
	}
	
	/* Disconnecting node from the tree */
	node->tree = NULL;
	
	if (reiser4_tree_root_node(tree, node)) {
		/* The case when we're disconnecting root node for some
		   reasons. And we will let to do so? Yes, why not? */
		tree->root = NULL;
	} else {
		/* Disconnecting generic node */
		reiser4_node_disconnect(parent, node);
	}

	return 0;
}

/* Loads node from @blk and connects it to @parent. */
reiser4_node_t *reiser4_tree_load_node(reiser4_tree_t *tree,
				       reiser4_node_t *parent,
				       blk_t blk)
{
	reiser4_node_t *node = NULL;

	aal_assert("umka-1289", tree != NULL);

	/* Checking if node in the local cache of @parent */
	if (!parent || !(node = reiser4_node_child(parent, blk))) {
		/* Check for memory pressure event. If memory pressure is uppon
		   us, we call memory cleaning function. For now we call
		   tree_adjust() in order to release not locked nodes. */
		if (tree->mpc_func && tree->mpc_func()) {

			/* Locking parent. This guaranties, that it will not be
			   released until we unlock it. And it may be released
			   in tree_adjust(), as it is allocating new nodes
			   durring balancing. */
			if (parent) {
				reiser4_node_lock(parent);
			}

			/* Adjusting the tree. It will be finished as soon as
			   memory pressure condition will gone. */
			if (tree->root) {
				reiser4_tree_adjust_node(tree, tree->root, 1);
			}

			/* Unlock parent node. */
			if (parent) {
				reiser4_node_unlock(parent);
			}
		}
		
		/* Node is not loaded yet. Loading it and connecting to @parent
		   node cache. */
		if (!(node = reiser4_node_open(tree, blk)))	{
			aal_exception_error("Can't open node %llu.", blk);
			return NULL;
		}

		/* Connect loaded node to cache. */
		if (reiser4_tree_connect_node(tree, parent, node))
			goto error_free_node;
	}

	return node;

 error_free_node:
	reiser4_node_close(node);
	return NULL;
}

/* Unloading node from tree cache. */
errno_t reiser4_tree_unload_node(reiser4_tree_t *tree,
				 reiser4_node_t *node)
{
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
		reiser4_node_t *parent = node->p.node;
		reiser4_tree_disconnect_node(tree, parent, node);
	}

	/* Releasing node */
	return reiser4_node_close(node);
}

/* Loads denoted by passed nodeptr @place child node */
reiser4_node_t *reiser4_tree_child_node(reiser4_tree_t *tree,
					reiser4_place_t *place)
{
	ptr_hint_t ptr;
	trans_hint_t hint;
	
	aal_assert("umka-1889", tree != NULL);
	aal_assert("umka-1890", place != NULL);
	aal_assert("umka-1891", place->node != NULL);

	/* Initializing @place. */
	if (reiser4_place_fetch(place))
		return NULL;

	/* Checking if item is a branch of tree */
	if (!reiser4_item_branch(place->plug))
		return NULL;

	/* Reading node pointer. */
	hint.count = 1;
	hint.specific = &ptr;

	if (reiser4_tree_fetch(tree, place, &hint) < 0)
		return NULL;

	/* Loading node @ptr point to. */
	return reiser4_tree_load_node(tree, place->node,
				      ptr.start);
}

/* Finds both left and right neighbours and connects them into the tree. */
static reiser4_node_t *reiser4_tree_ltrt_node(reiser4_tree_t *tree,
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
		aal_memcpy(&place, &place.node->p, sizeof(place));

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
                if (!(place.node = reiser4_tree_child_node(tree, &place)))
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

/* Moves @place by one item to right. If node is over, returns node next to
   passed @place. Needed for moving though the tree node by node, for instance
   in directory read code. */
errno_t reiser4_tree_next_node(reiser4_tree_t *tree, 
			       reiser4_place_t *place, 
			       reiser4_place_t *next)
{
	aal_assert("umka-867", tree != NULL);
	aal_assert("umka-868", place != NULL);
	aal_assert("umka-1491", next != NULL);

	/* Check if we have to get right neoghbour node. */
	if (place->pos.item >= reiser4_node_items(place->node) - 1) {
		reiser4_tree_neigh_node(tree, place->node, D_RIGHT);

		/* There is no right neighbour. */
		if (!place->node->right) {
			aal_memset(next, 0, sizeof(*next));
			return 0;
		}

		/* Assigning new coord to @place. */
		reiser4_place_assign((reiser4_place_t *)next,
				     place->node->right, 0, 0);
	} else {
		/* Assigning new coord to @place. */
		reiser4_place_assign((reiser4_place_t *)next,
				     place->node, place->pos.item + 1, 0);
	}

	/* Initializing @place. */
	return reiser4_place_fetch((reiser4_place_t *)next);
}

/* Gets left or right neighbour nodes */
reiser4_node_t *reiser4_tree_neigh_node(reiser4_tree_t *tree,
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
		reiser4_tree_ltrt_node(tree, node, where);
		reiser4_node_unlock(node);
	}
	
	return (where == D_LEFT) ? node->left :
		node->right;
}

#ifndef ENABLE_STAND_ALONE
/* Requests block allocator for new block and creates empty node in it. */
reiser4_node_t *reiser4_tree_alloc_node(
	reiser4_tree_t *tree,	    /* tree for operating on */
	uint8_t level)	 	    /* level of new node */
{
	rid_t pid;
	blk_t fake_blk;
	uint32_t stamp;
	uint64_t free_blocks;
	reiser4_node_t *node;
    
	aal_assert("umka-756", tree != NULL);
    
	/* Setting up of the free blocks in format */
	if (!(free_blocks = reiser4_format_get_free(tree->fs->format)))
		return NULL;

	/* Check for memory pressure event. */
	if (tree->mpc_func && tree->mpc_func() && tree->root) {
		/* Memory pressure is here, trying to release some nodes. */
		if (reiser4_tree_adjust_node(tree, tree->root, 1)) {
			aal_exception_warn("Error when adjusting the "
					   "tree durring allocating "
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
				  reiser4_node_t *node)
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
#endif

/* Builds root key and stores it in passed @tree instance */
static errno_t reiser4_tree_key(reiser4_tree_t *tree) {
	rid_t pid;
    
	aal_assert("umka-1090", tree != NULL);
	aal_assert("umka-1091", tree->fs != NULL);
	aal_assert("umka-1092", tree->fs->oid != NULL);

	pid = reiser4_format_key_pid(tree->fs->format);

	/* Finding needed key plugin by its identifier */
	if (!(tree->key.plug = reiser4_factory_ifind(KEY_PLUG_TYPE,
						     pid)))
	{
		aal_exception_error("Can't find key plugin by its "
				    "id 0x%x.", pid);
		return -EINVAL;
	}

	return reiser4_fs_root_key(tree->fs, &tree->key);
}

#ifndef ENABLE_STAND_ALONE
static void callback_keyrem_func(const void *key) {
	reiser4_key_free((reiser4_key_t *)key);
}

static void callback_valrem_func(const void *val) {
	aal_block_free((aal_block_t *)val);
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
	return reiser4_key_compfull((reiser4_key_t *)k1,
				    (reiser4_key_t *)k2);
}
#endif

/* Opens the tree (that is, the tree cache) on specified filesystem */
reiser4_tree_t *reiser4_tree_init(reiser4_fs_t *fs) {
	reiser4_tree_t *tree;

	aal_assert("umka-737", fs != NULL);

	/* Allocating memory for the tree instance */
	if (!(tree = aal_calloc(sizeof(*tree), 0)))
		return NULL;

	tree->fs = fs;
	tree->fs->tree = tree;

#ifndef ENABLE_STAND_ALONE
	if (!(tree->data = aal_hash_table_alloc(callback_hash_func,
						callback_comp_func,
						callback_keyrem_func,
						callback_valrem_func)))
	{
		goto error_free_tree;
	}
#endif

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
#ifndef ENABLE_STAND_ALONE
	aal_hash_table_free(tree->data);
 error_free_tree:
	aal_free(tree);
#endif
	return NULL;
}

/* Closes specified tree (frees all assosiated memory) */
void reiser4_tree_fini(reiser4_tree_t *tree) {
	aal_assert("umka-134", tree != NULL);

#ifndef ENABLE_STAND_ALONE
	/* Flushing tree cache */
	reiser4_tree_sync(tree);
#endif

	/* Releasing all loaded formetted nodes and tree itself */
	reiser4_tree_close(tree);
}

/* Closes specified tree (frees all assosiated memory) */
void reiser4_tree_close(reiser4_tree_t *tree) {
	aal_assert("vpf-1316", tree != NULL);

	/* Releasing loaded formatted nodes */
	reiser4_tree_collapse(tree);
	tree->fs->tree = NULL;

	/* Releasing data cache */
#ifndef ENABLE_STAND_ALONE
	aal_hash_table_free(tree->data);
#endif
	
	/* Freeing the tree */
	aal_free(tree);
}

#ifndef ENABLE_STAND_ALONE
/* Allocates nodeptr item at passed @place */
static errno_t reiser4_tree_alloc_nodeptr(reiser4_tree_t *tree,
					  reiser4_place_t *place)
{
	uint32_t units;
	reiser4_node_t *child;
	
	units = plug_call(place->plug->o.item_ops->balance,
			  number_units, (place_t *)place);

	for (place->pos.unit = 0; place->pos.unit < units;
	     place->pos.unit++)
	{
		blk_t blk = reiser4_item_down_link(place);

		if (!reiser4_fake_ack(blk))
			continue;

		if (!(child = reiser4_node_child(place->node, blk))) {
			aal_exception_error("Can't find child node "
					    "by pointer %llu.", blk);
			return -EINVAL;
		}
					
		/* If @child is fake one it needs to be allocated here and its
		   nodeptr should be updated. */
		if (!reiser4_alloc_allocate(tree->fs->alloc, &blk, 1))
			return -ENOSPC;

		if (reiser4_item_update_link(place, blk))
			return -EIO;
					
		/* Assigning node to new node blk */
		reiser4_node_move(child, blk);
	}

	return 0;
}

/* Allocates extent item at passed @place */
static errno_t reiser4_tree_alloc_extent(reiser4_tree_t *tree,
					 reiser4_place_t *place)
{
	errno_t res;
	ptr_hint_t ptr;
	uint32_t units;
	uint32_t blksize;
	trans_hint_t hint;

	units = plug_call(place->plug->o.item_ops->balance,
			  number_units, (place_t *)place);

	blksize = reiser4_tree_get_blksize(tree);
	
	for (place->pos.unit = 0; place->pos.unit < units;
	     place->pos.unit++)
	{
		uint64_t width;
		uint64_t blocks;
		uint64_t offset;
		key_entity_t key;

		hint.count = 1;
		hint.specific = &ptr;
		
		if (plug_call(place->plug->o.item_ops->object,
			      fetch_units, (place_t *)place, &hint) != 1)
		{
			return -EIO;
		}

		/* Check if we have accessed unallocated extent */
		if (ptr.start != EXTENT_UNALLOC_UNIT)
			continue;

		/* Getting unit key */
		plug_call(place->plug->o.item_ops->balance, fetch_key,
			  (place_t *)place, &key);

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
					      update_units, (place_t *)place,
					      &hint) != 1)
				{
					return -EIO;
				}
			} else {
				errno_t res;
				uint32_t level;
				reiser4_place_t iplace;

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

/* Flushes some part of tree cache to device starting from passed @node */
errno_t reiser4_tree_adjust_node(reiser4_tree_t *tree,
				 reiser4_node_t *node,
				 bool_t mpcheck)
{
	errno_t res;
	aal_list_t *walk;
	reiser4_node_t *child;
	
	aal_assert("umka-2302", tree != NULL);
	aal_assert("umka-2303", node != NULL);

#ifndef ENABLE_STAND_ALONE
	/* We are dealing only with dirty nodes */
	if (reiser4_node_isdirty(node)) {
		blk_t allocnr;

		/* Requesting block allocator to allocate the real block number
		   for fake allocated node. */
		if (reiser4_fake_ack(node_blocknr(node))) {
			if (!reiser4_alloc_allocate(tree->fs->alloc,
						    &allocnr, 1))
			{
				return -ENOSPC;
			}

			if (reiser4_tree_root_node(tree, node)) {
				/* Setting up root node */
				reiser4_tree_set_root(tree, allocnr);
			} else {
				/* Assigning new block number to @node */
				reiser4_node_move(node, allocnr);
				
				if ((res = reiser4_node_uptr(node)))
					return res;
			}
		}
	}
#endif
	
	/* Walking through the list of children and calling tree_adjust()
	   function for next level nodes if memory pressure event is still
	   actual. */
	if (node->children) {
		aal_list_t *next;

		/* Here should be construction like this (with @next used),
		   because current item may be removed by tree_adjust()
		   function. */
		for (walk = node->children; walk; walk = next) {
			next = walk->next;
			child = (reiser4_node_t *)walk->data;
			
			if ((res = reiser4_tree_adjust_node(tree, child,
							    mpcheck)))
			{
				return res;
			}
		}
	}

#ifndef ENABLE_STAND_ALONE
	/* We are dealing only with dirty nodes */
	if (reiser4_node_isdirty(node)) {
		blk_t allocnr;

		/* Requesting block allocator to allocate the real block number
		   for fake allocated node. */
		if (reiser4_fake_ack(node_blocknr(node))) {
			if (!reiser4_alloc_allocate(tree->fs->alloc,
						    &allocnr, 1))
			{
				return -ENOSPC;
			}

			if (reiser4_tree_root_node(tree, node)) {
				reiser4_tree_set_root(tree, allocnr);
			} else {
				/* Assigning new block number to @node */
				reiser4_node_move(node, allocnr);
				
				if ((res = reiser4_node_uptr(node)))
					return res;
			}
		}

		/* Allocating all children nodes if we are on internal level */
		if (reiser4_node_get_level(node) > LEAF_LEVEL) {
			uint32_t i;
			
			/* Going though the all items in node */
			for (i = 0; i < reiser4_node_items(node); i++) {
				reiser4_place_t place;

				reiser4_place_assign(&place, node,
						     i, MAX_UINT32);

				if ((res = reiser4_place_fetch(&place)))
					return res;

				if (place.plug->id.group == NODEPTR_ITEM) {
					if ((res = reiser4_tree_alloc_nodeptr(tree, &place)))
						return res;
				} else if (place.plug->id.group == EXTENT_ITEM) {
					if ((res = reiser4_tree_alloc_extent(tree, &place)))
						return res;
				}
			}
		}
	}

        /* Updating free space in super block */
	{
		count_t free;
			
		free = reiser4_alloc_free(tree->fs->alloc);
		reiser4_format_set_free(tree->fs->format, free);
	}
#endif
		
	/* Checking if we should try to release some nodes.*/
	if (mpcheck) {
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
			aal_exception_error("Can't write node %llu."
					    " %s.", node_blocknr(node),
					    tree->fs->device->error);
			return -EIO;
		}

#endif
		reiser4_tree_unload_node(tree, node);
	}

	return 0;
}

/* Walking though the tree cache and closing all nodes */
errno_t reiser4_tree_walk_node(reiser4_tree_t *tree,
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
			if ((res = reiser4_tree_walk_node(tree, child,
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
static errno_t callback_save_node(const void *entry,
				  void *data)
{
	aal_hash_node_t *node = (aal_hash_node_t *)entry;
	aal_block_t *block = (aal_block_t *)node->value;

	if (block->dirty) {
		errno_t res;
		
		if ((res = aal_block_write(block)))
			return res;
	}

	return 0;
}

/* Syncs whole tree cache */
errno_t reiser4_tree_sync(reiser4_tree_t *tree) {
	errno_t res;
	
	aal_assert("umka-2259", tree != NULL);

	if (!tree->root)
		return 0;

        /* Flushing whole tree. */
	if ((res = reiser4_tree_adjust_node(tree, tree->root, 0))) {
		aal_exception_error("Can't synchronize metadata.");
		return res;
	}

	/* Saving data blocks */
	if ((res = aal_hash_table_foreach(tree->data,
					  callback_save_node, tree)))
	{
		aal_exception_error("Can't synchronize tree data.");
		return res;
	}
	
	return res;
}

/* Returns TRUE if tree has not root node allocated */
bool_t reiser4_tree_fresh(reiser4_tree_t *tree) {
	aal_assert("umka-1930", tree != NULL);
	return (reiser4_tree_get_root(tree) == INVAL_BLK);
}
#endif

/* Unloads all tree nodes */
errno_t reiser4_tree_collapse(reiser4_tree_t *tree) {
	aal_assert("umka-2265", tree != NULL);

	if (!tree->root)
		return 0;

	return reiser4_tree_walk_node(tree, tree->root,
				      reiser4_tree_unload_node);
}

#ifndef ENABLE_STAND_ALONE
/* Makes search of the leftmost item/unit with the same key as passed @place
   has. This is needed to work with key collitions. */
static errno_t reiser4_tree_leftmost(reiser4_tree_t *tree,
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
				reiser4_place_t *p = &walk;
				
				switch (plug_call(walk.plug->o.item_ops->balance,
						  lookup, (place_t *)p, key,
						  FIND_EXACT))
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
		reiser4_tree_neigh_node(tree, walk.node, D_LEFT);

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

/* Makes search in the tree by specified key. Fills passed place by places of
   found item. */
lookup_t reiser4_tree_lookup(
	reiser4_tree_t *tree,	  /* tree to be grepped */
	reiser4_key_t *key,	  /* key to be find */
	uint8_t level,	          /* stop level for search */
	bias_t bias,              /* position correcting mode (insert or read) */
	reiser4_place_t *place)	  /* place the found item to be stored */
{
	lookup_t res;
	reiser4_key_t wan;

	aal_assert("umka-742", key != NULL);
	aal_assert("umka-1760", tree != NULL);
	aal_assert("umka-2057", place != NULL);

	/* We store @key in @wan. All consequence code will use @wan. This is
	   needed, because @key might point to @place->item.key in @place and
	   will be corrupted durring lookup. */
	reiser4_key_assign(&wan, key);

	/* Making sure that root exists. If not, getting out with @place
	   initialized by NULL root. */
	if (reiser4_tree_load_root(tree)) {
		reiser4_place_assign(place, NULL,
				     0, MAX_UINT32);
		return ABSENT;
	} else {
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
		   or some error occured durring last node lookup. */
		if (curr_level <= level || res < 0) {
			if (res == PRESENT) {
#ifndef ENABLE_STAND_ALONE
				/* If collision handling is allwoed, we will
				   find leftmost coord with the same key. This
				   is needed for correct key collitions
				   handling. */
				if (reiser4_tree_leftmost(tree, place, &wan)) {
					aal_exception_error("Can't find leftmost "
							    "position durring lookup.");
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

/* Reads data from the @tree to passed @hint */
int64_t reiser4_tree_read(reiser4_tree_t *tree,
			  reiser4_place_t *place,
			  trans_hint_t *hint)
{
	return plug_call(place->plug->o.item_ops->object,
			 read_units, (place_t *)place, hint);
}

/* Reads one convert chunk from src item */
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
		reiser4_place_t place;

		/* Looking for the place to read */
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
/* Returns TRUE if passed @tree has minimal possible height and thus cannot be
   dried out. */
static bool_t reiser4_tree_minimal(reiser4_tree_t *tree) {
	return (reiser4_tree_get_height(tree) <= 2);
}

/* Returns TRUE if root node contain one item, that is, tree is singular and
   should be dried out. */
static bool_t reiser4_tree_singular(reiser4_tree_t *tree) {
	return (reiser4_node_items(tree->root) == 1);
}

/* Updates key at passed @place by passed @key by means of using
   reiser4_node_ukey functions in recursive maner. */
errno_t reiser4_tree_update_key(reiser4_tree_t *tree,
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
			
			if ((res = reiser4_tree_update_key(tree, p, key)))
				return res;
		}
	}

	return reiser4_node_update_key(place->node, &place->pos, key);
}

/* This function inserts new nodeptr item to the tree and in such way it
   attaches passed @node to it. It also connects passed @node into tree
   cache. */
errno_t reiser4_tree_attach_node(
	reiser4_tree_t *tree,	    /* tree we will attach node to */
	reiser4_node_t *node)       /* child to attached */
{
	rid_t pid;
	errno_t res;
	uint8_t level;
	
	trans_hint_t hint;
	reiser4_place_t place;
	ptr_hint_t nodeptr_hint;

	aal_assert("umka-913", tree != NULL);
	aal_assert("umka-916", node != NULL);
    
	/* Preparing nodeptr item hint */
	aal_memset(&hint, 0, sizeof(hint));

	hint.count = 1;
	hint.specific = &nodeptr_hint;

	/* Prepare nodeptr hint from passed @node */
	nodeptr_hint.width = 1;
	nodeptr_hint.start = node_blocknr(node);

	reiser4_node_lkey(node, &hint.offset);
	pid = reiser4_param_value("nodeptr");

	if (!(hint.plug = reiser4_factory_ifind(ITEM_PLUG_TYPE,
						pid)))
	{
		aal_exception_error("Can't find item plugin by "
				    "its id 0x%x.", pid);
		return -EINVAL;
	}

	level = reiser4_node_get_level(node) + 1;

	/* Looking up for the insert point place */
	if ((res = reiser4_tree_lookup(tree, &hint.offset,
				       level, FIND_CONV,
				       &place)) < 0)
	{
		return res;
	}

	/* Inserting node pointer into tree */
	if ((res = reiser4_tree_insert(tree, &place, &hint, level)) < 0) {
		aal_exception_error("Can't insert nodeptr item "
				    "to the tree.");
		return res;
	}

	/* Attaching node to insert point node. */
	if ((res = reiser4_tree_connect_node(tree, place.node, node))) {
		aal_exception_error("Can't attach the node %llu to "
				    "the tree.", node_blocknr(node));
		return res;
	}

	reiser4_tree_neigh_node(tree, node, D_LEFT);
	reiser4_tree_neigh_node(tree, node, D_RIGHT);
	
	return 0;
}

/* Removes passed @node from the on-disk tree and cache structures */
errno_t reiser4_tree_detach_node(reiser4_tree_t *tree,
				 reiser4_node_t *node)
{
	reiser4_place_t p;
	trans_hint_t hint;
	
	aal_assert("umka-1726", tree != NULL);
	aal_assert("umka-1727", node != NULL);

        /* Disconnecting node from tree cache */
	p = node->p;
	
	if (node->tree) {
		reiser4_node_t *parent = p.node;
		reiser4_tree_disconnect_node(tree, parent, node);
	}
	
	/* Removing item/unit from the parent node */
	hint.count = 1;
	return reiser4_tree_remove(tree, &p, &hint);
}

/* This function forces tree to grow by one level and sets it up after the
   growing. */
errno_t reiser4_tree_growup(
	reiser4_tree_t *tree)	/* tree to be growed up */
{
	errno_t res;
	uint32_t new_height;
	reiser4_node_t *new_root;
	reiser4_node_t *old_root;

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

	/* Attaching old root node to tree. */
	if ((res = reiser4_tree_attach_node(tree, old_root)))
		goto error_return_root;

	return 0;

 error_return_root:
	reiser4_tree_release_node(tree, new_root);
	reiser4_tree_assign_root(tree, old_root);
	
	return res;
}

/* Decreases tree height by one level */
errno_t reiser4_tree_dryout(reiser4_tree_t *tree) {
	errno_t res;
	reiser4_place_t place;
	reiser4_node_t *new_root;
	reiser4_node_t *old_root;

	aal_assert("umka-1731", tree != NULL);
	aal_assert("umka-1737", tree->root != NULL);

	if (reiser4_tree_minimal(tree))
		return -EINVAL;

	/* Rasing up the root node if it exists */
	if ((res = reiser4_tree_load_root(tree)))
		return res;

	old_root = tree->root;
	
	/* Check if we can dry tree out safely */
	if (reiser4_node_items(old_root) > 1)
		return -EINVAL;

	/* Getting new root as the first child of the old root node */
	reiser4_place_assign(&place, old_root, 0, 0);

	if (!(new_root = reiser4_tree_child_node(tree, &place))) {
		aal_exception_error("Can't load new root durring "
				    "drying tree out.");
		return -EINVAL;
	}

	/* Disconnect old root and its child from the tree */
	reiser4_tree_disconnect_node(tree, NULL, old_root);
	reiser4_tree_disconnect_node(tree, old_root, new_root);

	/* Assign new root. Setting tree height to new root level and root block
	   number to new root block number. */
	reiser4_tree_assign_root(tree, new_root);
	
        /* Releasing old root node */
	reiser4_node_mkclean(old_root);
	reiser4_tree_release_node(tree, old_root);

	return 0;
}

/* Tries to shift items and units from @place to passed @neig node. After that
   it's finished, place will contain new insert point. */
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

	/* Preapre shift hint. Initializing shift flags (shift direction, is it
	   allowed to create new nodes, etc) and insert point. */
	node = place->node;
	hint.control = flags;
	hint.pos = place->pos;

	/* Perform node shift from @node to @neig. */
	if ((res = reiser4_node_shift(node, neig, &hint)))
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
			   and updating left delimiting keys makes sence at
			   all. */
			if (node->p.node) {
				reiser4_place_t p;

				/* Getting leftmost key from @node.  */
				if ((res = reiser4_node_lkey(node, &lkey)))
					return res;

				/* Recursive updating of all internal keys that
				   supposed to be updated. */
				reiser4_place_init(&p, node->p.node,
						   &node->p.pos);
				
				reiser4_key_assign(&node->p.key, &lkey);
				
				if ((res = reiser4_tree_update_key(tree,
								   &p, &lkey)))
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
				reiser4_place_t p;
				
				if ((res = reiser4_node_lkey(neig, &lkey)))
					return res;
				
				reiser4_place_init(&p, neig->p.node,
						   &neig->p.pos);
				
				reiser4_key_assign(&neig->p.key, &lkey);
				
				if ((res = reiser4_tree_update_key(tree,
								   &p, &lkey)))
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
				 reiser4_node_t *left,
				 reiser4_node_t *right)
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
		if (reiser4_node_items(left) == 0) {
			if ((res = reiser4_tree_detach_node(tree, left)))
				return res;
			
			reiser4_node_mkclean(left);
			reiser4_tree_release_node(tree, left);
		}
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
   point, or negative values for errors. */
int32_t reiser4_tree_expand(
	reiser4_tree_t *tree,	    /* tree pointer function operates on */
	reiser4_place_t *place,	    /* place of insertion point */
	uint32_t needed,	    /* amount of space that should be freed */
	uint32_t flags)
{
	int alloc;
	errno_t res;
	int32_t enough;
	uint32_t overhead;

	reiser4_node_t *left;
	reiser4_node_t *right;

	aal_assert("umka-766", place != NULL);
	aal_assert("umka-929", tree != NULL);

	/* Check if tree is fresh. If so, allocating new node with level of tree
	   height and assigning it to passed @place. */
	if (reiser4_tree_fresh(tree)) {
		uint8_t level = reiser4_tree_get_height(tree);
		
		if (!(place->node = reiser4_tree_alloc_node(tree, level)))
			return -ENOSPC;

		POS_INIT(&place->pos, 0, MAX_UINT32);
	}

	overhead = reiser4_node_overhead(place->node);
	
	/* Adding node overhead to @needed */
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

	/* Shifting data into left neighbour if it exists */
	if ((SF_LEFT_SHIFT & flags) &&
	    (left = reiser4_tree_neigh_node(tree, place->node, D_LEFT)))
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

	/* Shifting data into right neighbour if it exists */
	if ((SF_RIGHT_SHIFT & flags) &&
	    (right = reiser4_tree_neigh_node(tree, place->node, D_RIGHT)))
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
	   the tree. Allocating new node and trying to shift data into it. */
	for (alloc = 0; enough < 0 && alloc < 2; alloc++) {
		uint8_t level;
		uint32_t alloc_flags;
		reiser4_place_t save;
		reiser4_node_t *node;

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
			reiser4_node_mkclean(node);
			reiser4_tree_release_node(tree, node);
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
errno_t reiser4_tree_shrink(reiser4_tree_t *tree,
			    reiser4_place_t *place)
{
	errno_t res;
	uint32_t flags;
	reiser4_node_t *left, *right;

	aal_assert("umka-1784", tree != NULL);
	aal_assert("umka-1783", place != NULL);

	/* Shift flags to be used in packing. */
	flags = (SF_LEFT_SHIFT | SF_ALLOW_MERGE);
	
	/* Packing node in order to keep the tree in well packed state
	   anyway. Here we will shift data from the target node to its left
	   neighbour node. */
	if ((left = reiser4_tree_neigh_node(tree, place->node, D_LEFT))) {
		if ((res = reiser4_tree_shift(tree, place, left, flags))) {
			aal_exception_error("Can't pack node %llu into left.",
					    node_blocknr(place->node));
			return res;
		}
	}
		
	if (reiser4_node_items(place->node) > 0) {
		/* Shifting the data from the right neigbour node into the
		   target node. */
		if ((right = reiser4_tree_neigh_node(tree, place->node, D_RIGHT))) {
			reiser4_place_t bogus;

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
			if (reiser4_node_items(right) == 0) {
				reiser4_node_mkclean(right);
				reiser4_tree_detach_node(tree, right);
				reiser4_tree_release_node(tree, right);
			}
		}
	} else {
		/* Release node, because it got empty. */
		reiser4_node_mkclean(place->node);
		reiser4_tree_detach_node(tree, place->node);
		reiser4_tree_release_node(tree, place->node);
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
				  reiser4_place_t *place, 
				  uint8_t level)
{
	errno_t res;
	uint32_t flags;
	uint32_t curr_level;
	reiser4_node_t *node;
	
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

			/* Check if we should growup the tree */
			if (reiser4_tree_root_node(tree, place->node)) {
				if ((res = reiser4_tree_growup(tree)))
					return res;
			}

			/* Attach new node to tree. */
			if ((res = reiser4_tree_attach_node(tree, node))) {
				reiser4_tree_release_node(tree, node);
				aal_exception_error("Tree is failed to attach "
						    "node durring split opeartion.");
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

/* Releases passed region in block allocator. This is used in tail durring tree
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
		reiser4_place_t place;

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

/* Truncates item @hint->offset point to by value stored in @hint->count. This
   is used durring tail conversion. */
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
		reiser4_place_t place;
		
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
				reiser4_place_t p;
				reiser4_key_t lkey;

				/* Updating parent keys */
				reiser4_node_lkey(place.node, &lkey);
				
				reiser4_place_init(&p, place.node->p.node,
						   &place.node->p.pos);

				reiser4_key_assign(&place.node->p.key, &lkey);

				if ((res = reiser4_tree_update_key(tree,
								   &p, &lkey)))
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
			/* Detaching node from the tree, because it became
			   empty. */
			reiser4_node_mkclean(place.node);
			reiser4_tree_detach_node(tree, place.node);

			/* Freeing node and updating place node component in
			   order to let user know that node do not exist any
			   longer. */
			reiser4_tree_release_node(tree, place.node);
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

/* Converts item at passed @place from tail to extent and back from extent to
   tail. */
errno_t reiser4_tree_conv_flow(reiser4_tree_t *tree,
			       conv_hint_t *hint)
{
	char *buff;
	errno_t res;
	int64_t conv;
	uint64_t size;
	uint32_t blksize;
	trans_hint_t trans;
	
	aal_assert("umka-2406", tree != NULL);
	aal_assert("umka-2407", hint != NULL);
	aal_assert("umka-2481", hint->plug != NULL);

	blksize = reiser4_tree_get_blksize(tree);
	reiser4_key_assign(&trans.offset, &hint->offset);

	for (size = hint->count, hint->bytes = 0;
	     size > 0; size -= conv)
	{
		/* Preparing buffer to read to it and size to read. */
		trans.count = blksize;

		if (trans.count > size)
			trans.count = size;

		if (!(buff = aal_calloc(trans.count, 0)))
			return -ENOMEM;

		trans.specific = buff;

		/* Reading data from tree */
		if ((conv = reiser4_tree_read_flow(tree, &trans)) < 0) {
			res = conv;
			goto error_free_buff;
		}
		
		/* Removing read data from the tree. */
		trans.data = tree;
		trans.count = conv;
		trans.plug = hint->plug;

		if ((conv = reiser4_tree_trunc_flow(tree, &trans)) < 0) {
			res = conv;
			goto error_free_buff;
		}

		trans.count = conv;
		
		/* Writing data to the tree */
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
static errno_t callback_prep_insert(reiser4_place_t *place, 
				    trans_hint_t *hint) 
{
	aal_assert("umka-2440", hint != NULL);
	aal_assert("umka-2439", place != NULL);

	hint->ohd = 0;
	hint->len = 0;

	return plug_call(hint->plug->o.item_ops->object,
			 prep_insert, (place_t *)place, hint);
}

/* Estimates how many bytes is needed to write data described by @hint. */
static errno_t callback_prep_write(reiser4_place_t *place, 
				   trans_hint_t *hint) 
{
	aal_assert("umka-2440", hint != NULL);
	aal_assert("umka-2439", place != NULL);

	hint->ohd = 0;
	hint->len = 0;

	return plug_call(hint->plug->o.item_ops->object,
			 prep_write, (place_t *)place, hint);
}

/* Function for tree modifications. It is used for inserting data to tree (stat
   data items, directries) or writting (tails, extents). */
int64_t reiser4_tree_modify(
	reiser4_tree_t *tree,	    /* tree new item will be inserted in */
	reiser4_place_t *place,	    /* place item or unit inserted at */
	trans_hint_t *hint,         /* item hint to be inserted */
	uint8_t level,              /* level item/unit will be inserted on */
	estimate_func_t estimate,   /* estimate the space for the modification */
	modify_func_t modify)	    /* modification callback */
{
	bool_t mode;
	errno_t res;
	int32_t space;
	int32_t write;
	uint32_t needed;
	reiser4_place_t old;

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

		/* Getting new place item/unit will be inserted at after tree 
		   is growed up. It is needed because we want to insert item 
		   into the node of the given @level. */
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

	/* Handling the case when tree is empty (just after mkfs). */
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
	if ((res = estimate(place, hint)))
		return res;
	
	/* Needed space to be prepared in tree */
	needed = hint->len + hint->ohd;
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
		if (!(hint->len = hint->count = space)) {
			return -ENOSPC;
		}
	}

	/* As insert point is changing durring make space, we check if insertq
	   mode was changed too. If so, we should perform estimate one more time
	   in order to get right space for @hint. That is because, estimated
	   value depends on insert point. */
	if (mode != (place->pos.unit == MAX_UINT32)) {
		if ((res = estimate(place, hint)))
			return res;
	}

	/* Inserting/writing data to node. */
	if ((write = reiser4_node_modify(place->node, &place->pos,
					 hint, modify)) < 0)
	{
		aal_exception_error("Can't insert data to node %llu.",
				    node_blocknr(place->node));
		return write;
	}

	/* Parent keys will be updated if we inserted item/unit into leftmost
	   pos and if target node has the parent. */
	if (reiser4_place_leftmost(place) && place->node != tree->root) {
		reiser4_place_t *parent = &place->node->p;
		
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
	
	/* Initializing insert point place */
	if ((res = reiser4_place_fetch(place)))
		return res;

	return write;
}

/* Inserts data to the tree */
int64_t reiser4_tree_insert(reiser4_tree_t *tree,
			    reiser4_place_t *place,
			    trans_hint_t *hint,
			    uint8_t level)
{
	aal_assert("umka-779", tree != NULL);
	aal_assert("umka-779", hint != NULL);
	
	aal_assert("umka-1644", place != NULL);
	aal_assert("umka-1645", hint->plug != NULL);

	return reiser4_tree_modify(tree, place, hint, level, 
				   callback_prep_insert,
				   callback_node_insert);
}

/* Writes data to the tree */
int64_t reiser4_tree_write(reiser4_tree_t *tree,
			   reiser4_place_t *place,
			   trans_hint_t *hint,
			   uint8_t level)
{
	aal_assert("umka-2441", tree != NULL);
	aal_assert("umka-2442", hint != NULL);
	
	aal_assert("umka-2443", place != NULL);
	aal_assert("umka-2444", hint->plug != NULL);

	return reiser4_tree_modify(tree, place, hint, level,
				   callback_prep_write,
				   callback_node_write);
}

/* Removes item/unit at passed @place. This functions also perform so called
   "local packing". */
errno_t reiser4_tree_remove(
	reiser4_tree_t *tree,	  /* tree item will be removed from */
	reiser4_place_t *place,   /* place the item will be removed at */
	trans_hint_t *hint)
{
	errno_t res;

	aal_assert("umka-2055", tree != NULL);
	aal_assert("umka-2056", place != NULL);
	aal_assert("umka-2392", hint != NULL);

	if (hint->count == 0)
		return -EINVAL;
	
	/* Removing iten/unit from the node */
	if ((res = reiser4_node_remove(place->node,
				       &place->pos, hint)))
	{
		return res;
	}

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
		/* Detaching node from the tree, because it became empty */
		reiser4_node_mkclean(place->node);
		reiser4_tree_detach_node(tree, place->node);

		/* Freeing node and updating place node component in order to
		   let user know that node do not exist any longer. */
		reiser4_tree_release_node(tree, place->node);
		place->node = NULL;
	}

	/* Drying tree up in the case root node has only one item */
	if (reiser4_tree_singular(tree) && !reiser4_tree_minimal(tree)) {
		if ((res = reiser4_tree_dryout(tree)))
			return res;
	}
	
	return 0;
}

/* Traverses @node with passed functions as actions. */
errno_t reiser4_tree_trav_node(
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
errno_t reiser4_tree_trav(
	reiser4_tree_t *tree,		/* node to be traversed */
	tree_open_func_t open_func,	/* callback for node opening */
	tree_edge_func_t before_func,	/* start callback */
	tree_update_func_t update_func,	/* after child callback */
	tree_edge_func_t after_func,	/* end callback */
	void *data)			/* caller specific data */
{
	errno_t res;
	
	aal_assert("umka-1768", tree != NULL);

	if ((res = reiser4_tree_load_root(tree)))
		return res;
	
	return reiser4_tree_trav_node(tree, tree->root, open_func,
				      before_func, update_func,
				      after_func, data);
}

/* Makes copy of @node from @src_tree to @dst_tree */
static reiser4_node_t *reiser4_tree_clone_node(reiser4_tree_t *src_tree,
					       reiser4_node_t *src_node,
					       reiser4_tree_t *dst_tree)
{
	rid_t pid;
	uint32_t level;
	blk_t fake_blk;
	reiser4_node_t *dst_node;

	fake_blk = reiser4_fake_get();
	pid = src_node->entity->plug->id.id;
	level = reiser4_node_get_level(src_node);
	
	if (!(dst_node = reiser4_node_create(dst_tree, fake_blk,
					     pid, level)))
	{
		aal_exception_error("Can't initialize destination "
				    "node durring tree copy.");
		return NULL;
	}

	if (reiser4_node_clone(src_node, dst_node)) {
		reiser4_node_close(dst_node);
		return NULL;
	}

	return dst_node;
}

struct clone_hint {
	aal_list_t *path;
	reiser4_tree_t *tree;
};

typedef struct clone_hint clone_hint_t;

static errno_t clone_down(reiser4_tree_t *src_tree,
			  reiser4_node_t *src_node,
			  void *data)
{
	errno_t res = 0;
	clone_hint_t *hint;

	aal_list_t *last;
	aal_list_t *current;
	reiser4_node_t *parent;
	reiser4_node_t *dst_node;
	reiser4_tree_t *dst_tree;

	hint = (clone_hint_t *)data;

	if (!(dst_tree = hint->tree))
		return -EINVAL;

	if (!(dst_node = reiser4_tree_clone_node(src_tree, src_node,
						 dst_tree)))
	{
		aal_exception_error("Can't clone node %llu.",
				    node_blocknr(src_node));
		return -EINVAL;
	}

	last = aal_list_last(hint->path);
	parent = last ?	(reiser4_node_t *)last->data : NULL;
	
	dst_node->flags |= NF_FOREIGN;
	
	if ((res = reiser4_tree_connect_node(dst_tree, parent, dst_node))) {
		aal_exception_error("Can't connect node %llu to "
				    "target tree.", node_blocknr(src_node));
		goto error_free_dst_node;
	}

	if ((res = reiser4_node_uptr(dst_node)))
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
static errno_t clone_up(reiser4_tree_t *src_tree,
			reiser4_node_t *src_node,
			void *data)
{
	aal_list_t *next;
	aal_list_t *last;
	clone_hint_t *hint;

	if (reiser4_node_get_level(src_node) > LEAF_LEVEL) {
		hint = (clone_hint_t *)data;
		last = aal_list_last(hint->path);
		next = aal_list_remove(last, last->data);

		if (!next || !next->prev)
			hint->path = next;
	}
	
	return 0;
}

/* Makes copy of @src_tree to @dst_tree */
errno_t reiser4_tree_clone(reiser4_tree_t *src_tree,
			   reiser4_tree_t *dst_tree)
{
	clone_hint_t hint;
	
	aal_assert("umka-2304", src_tree != NULL);
	aal_assert("umka-2305", dst_tree != NULL);

	hint.path = NULL;
	hint.tree = dst_tree;

	return reiser4_tree_trav(src_tree, NULL, clone_down,
				 NULL, clone_up, &hint);
}

/* Resizes @tree by @blocks */
errno_t reiser4_tree_resize(reiser4_tree_t *tree,
			    count_t blocks)
{
	aal_assert("umka-2323", tree != NULL);
	return -EINVAL;
}
#endif
