/*
  tree.c -- reiser4 tree cache code.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/reiser4.h>

#ifndef ENABLE_COMPACT

/* Requests block allocator for new block and creates empty node in it */
reiser4_joint_t *reiser4_tree_allocate(
	reiser4_tree_t *tree,	    /* tree for operating on */
	uint8_t level)	 	    /* level of new node */
{
	blk_t blk;
	rpid_t pid;
    
	reiser4_node_t *node;
	reiser4_joint_t *joint;
    
	aal_assert("umka-756", tree != NULL, return NULL);
    
	/* Allocating the block */
	if ((blk = reiser4_alloc_allocate(tree->fs->alloc)) == FAKE_BLK) {
		aal_exception_error("Can't allocate block for a node.");
		return NULL;
	}

	if ((pid = reiser4_node_pid(tree->root->node)) == FAKE_PLUGIN) {
		aal_exception_error("Invalid node plugin has been detected.");
		return NULL;
	}
    
	/* Creating new node */
	if (!(node = reiser4_node_create(tree->fs->format->device, blk, pid, level)))
		return NULL;

	plugin_call(goto error_free_node, node->entity->plugin->node_ops,
		    set_stamp, node->entity, reiser4_format_get_stamp(tree->fs->format));
    
	if (!(joint = reiser4_joint_create(node)))
		goto error_free_node;
    
	/* Setting up of the free blocks in format */
	reiser4_format_set_free(tree->fs->format, 
				reiser4_alloc_free(tree->fs->alloc));
    
	joint->flags |= JF_DIRTY;
	
	return joint;
    
 error_free_node:
	reiser4_node_close(node);
	return NULL;
}

void reiser4_tree_release(reiser4_tree_t *tree, reiser4_joint_t *joint) {
	blk_t blk, free;
	
	aal_assert("umka-917", joint != NULL, return);
	aal_assert("umka-918", joint->node != NULL, return);

	blk = joint->node->blk;
	free = reiser4_alloc_free(tree->fs->alloc);
	
    	/* Sets up the free blocks in block allocator */
	reiser4_alloc_release(tree->fs->alloc, blk);
	reiser4_format_set_free(tree->fs->format, free);
    
	reiser4_joint_close(joint);
}

#endif

reiser4_joint_t *reiser4_tree_load(reiser4_tree_t *tree, 
				   blk_t blk) 
{
	aal_device_t *device;
	reiser4_node_t *node;
	reiser4_joint_t *joint;

	aal_assert("umka-1289", tree != NULL, return NULL);
    
	device = tree->fs->format->device;
    
	if (!(node = reiser4_node_open(device, blk))) 
		return NULL;
	    
	if (!(joint = reiser4_joint_create(node)))
		goto error_free_node;

	joint->flags &= ~JF_DIRTY;
	
	return joint;
    
 error_free_node:
	reiser4_node_close(node);
	return NULL;
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
	aal_assert("umka-738", tree != NULL, return FAKE_BLK);
	return tree->root->node->blk;
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
	if ((tree_root = reiser4_format_get_root(fs->format)) == FAKE_BLK)
		goto error_free_tree;
	
	if (!(tree->root = reiser4_tree_load(tree, tree_root)))
		goto error_free_tree;
    
	tree->root->tree = tree;
	reiser4_lru_init(&tree->lru);
    
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
	reiser4_node_t *node;
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
	if ((blk = reiser4_alloc_allocate(fs->alloc)) == FAKE_BLK) {
		aal_exception_error("Can't allocate block for the root node.");
		goto error_free_tree;
	}

	level = reiser4_format_get_height(fs->format);
	device = fs->format->device;
    
	/* Creating root node */
	if (!(node = reiser4_node_create(device, blk, profile->node, level))) {
		aal_exception_error("Can't create root node.");
		goto error_free_tree;
	}

	/* Creating joint for the root node */
	if (!(tree->root = reiser4_joint_create(node)))
		goto error_free_node;
    
	/* Setting up of the root block */
	reiser4_format_set_root(fs->format, node->blk);
    
	/* Setting up of the free blocks */
	reiser4_format_set_free(fs->format, reiser4_alloc_free(fs->alloc));

	tree->root->tree = tree;
	reiser4_lru_init(&tree->lru);

	return tree;

 error_free_node:
	reiser4_node_close(node);
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
			reiser4_joint_close((reiser4_joint_t *)walk->data);
	
		tree->root->children = NULL;
	}

	return 0;
}

/* Syncs whole tree cache */
errno_t reiser4_tree_sync(reiser4_tree_t *tree) {
	aal_assert("umka-560", tree != NULL, return -1);
	return reiser4_joint_sync(tree->root);
}

#endif

/* Closes specified tree (frees all assosiated memory) */
void reiser4_tree_close(reiser4_tree_t *tree) {
	aal_assert("umka-134", tree != NULL, return);

	/* Freeing tree cashe and tree itself*/
	reiser4_joint_close(tree->root);

	/* Freeing tree lru */
	reiser4_lru_fini(&tree->lru);

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
	reiser4_joint_t *parent = NULL;

	reiser4_ptr_hint_t ptr;

	aal_assert("umka-742", key != NULL, return -1);
	aal_assert("umka-1498", level != NULL, return -1);

	if (!coord)
		coord = &fake;
    
	deep = reiser4_tree_height(tree);
	reiser4_coord_init(coord, tree->root, CT_JOINT, &pos);
    
	/* 
	  Check for the case when wanted key smaller than root key. This is the
	  case, when somebody is trying to go up of the root by ".." entry of
	  root directory.
	*/
	if (reiser4_key_compare(key, &tree->key) < 0)
		*key = tree->key;
		    
	while (1) {
		item_entity_t *item;
		reiser4_node_t *node = coord->u.joint->node;
	
		/* 
		  Looking up for key inside node. Result of lookuping will be
		  stored in &coord->pos.
		*/
		if ((lookup = reiser4_node_lookup(node, key, &coord->pos)) == -1)
			return -1;

		if (reiser4_node_count(coord->u.joint->node) == 0)
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
		
		if (ptr.ptr == FAKE_BLK) {
			blk_t blk = reiser4_coord_blk(coord);
			aal_exception_error("Can't get pointer from nodeptr item %u, "
					    "node %llu.", coord->pos.item, blk);
			return -1;
		}
	
		deep--;
		
		parent = coord->u.joint;
	
		/* 
		   Check whether specified node already in cache. If so, we use
		   node from the cache.
		*/
		parent->counter++;
		
		if (!(coord->u.joint = reiser4_joint_find(parent, &item->key))) {
			/* 
			   Node was not found in the cache, we open it and
			   attach to the cache.
			*/
			if (!(coord->u.joint = reiser4_tree_load(tree, ptr.ptr))) {
				aal_exception_error("Can't load node %llu durring "
						    "lookup.", ptr.ptr);
				parent->counter--;
				return -1;
			}

			/* Registering node in tree cache */
			if (reiser4_joint_attach(parent, coord->u.joint)) {
				aal_exception_error("Can't attach the node %llu "
						    "in the tree.", ptr.ptr);
				parent->counter--;
				goto error_free_joint;
			}
		}

		parent->counter--;
	}
    
	return 0;
    
 error_free_joint:
	reiser4_joint_close(coord->u.joint);
	return -1;
}

#ifndef ENABLE_COMPACT

/* This function inserts nodeptr item to the tree */
static errno_t reiser4_tree_attach(
	reiser4_tree_t *tree,	    /* tree we will attach node to */
	reiser4_joint_t *joint)	    /* child to attached */
{
	rpid_t id;
	int lookup;
	reiser4_coord_t coord;
	reiser4_ptr_hint_t ptr;
	reiser4_item_hint_t hint;

	aal_assert("umka-913", tree != NULL, return -1);
	aal_assert("umka-916", joint != NULL, return -1);
    
	/* Preparing nodeptr item hint */
	aal_memset(&hint, 0, sizeof(hint));
	
	/* 
	   FIXME-UMKA: Hardcoded nodeptr item id. Here should be getting nodeptr
	   item plugin id from parent. In the case parent doesn't exist, it should
	   be got from filesystem default profile.
	*/
	id = ITEM_NODEPTR40_ID;

	if (!(hint.plugin = libreiser4_factory_ifind(ITEM_PLUGIN_TYPE, id))) {
		aal_exception_error("Can't find nodeptr item plugin "
				    "by its id 0x%x.", id);
		return -1;
	}

	aal_memset(&ptr, 0, sizeof(ptr));
	ptr.ptr = joint->node->blk;

	reiser4_node_lkey(joint->node, &hint.key);
	hint.hint = &ptr;

	if (reiser4_tree_insert(tree, &hint, LEAF_LEVEL + 1, &coord)) {
		aal_exception_error("Can't insert nodeptr item to the tree.");
		return -1;
	}
    
	if (reiser4_joint_attach(coord.u.joint, joint)) {
		aal_exception_error("Can't attach the node %llu in tree cache.", 
				    joint->node->blk);
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
	reiser4_joint_t *old_root = tree->root;
    
	tree_height = reiser4_tree_height(tree);
    
	/* Allocating new root node */
	if (!(tree->root = reiser4_tree_allocate(tree, tree_height + 1))) {
		aal_exception_error("Can't allocate new root node.");
		return -1;
	}

	if (reiser4_tree_attach(tree, old_root)) {
		aal_exception_error("Can't attach old root to the tree.");
		goto error_free_root;
	}

	blk = tree->root->node->blk;
	
	reiser4_format_set_height(tree->fs->format, tree_height + 1);
    	reiser4_format_set_root(tree->fs->format, blk);

	old_root->flags |= JF_DIRTY;
	
	return 0;

 error_free_root:
	reiser4_tree_release(tree, tree->root);
	tree->root = old_root;
	return -1;
}

static errno_t reiser4_tree_shift(
	reiser4_tree_t *tree,	/* tree we will operate on */
	direction_t direction,	/* direction of the shifting */
	reiser4_coord_t *coord,	/* insert point coord */
	reiser4_joint_t *joint,	/* destination node */
	uint32_t needed,        /* space need to be freed */
	int move_ip)		/* should we move insert point too */
{
	uint32_t overhead;
	reiser4_coord_t old;
	reiser4_coord_t src, dst;

	int retval;
	reiser4_key_t ldkey;
	shift_flags_t flags = 0;
	reiser4_coord_t ldcoord;
	reiser4_pos_t ldpos = {0, ~0ul};
    
	aal_assert("umka-1225", tree != NULL, return -1);
	aal_assert("umka-1226", coord != NULL, return -1);
	aal_assert("umka-1227", joint != NULL, return -1);
    
	aal_assert("umka-1258", 
		   reiser4_node_count(coord->u.joint->node) > 0, return -1);

	old = *coord;

/*	flags |= (direction == D_LEFT ? SF_LEFT : SF_RIGHT);
		
	if (move_ip)
		flags |= SF_MOVIP;

	retval = plugin_call(return -1, joint->node->entity->plugin->node_ops,
			      shift, old.u.joint->node->entity, joint->node->entity,
			      &coord->pos, flags);

	if (retval < 0)
		return retval;

	if (retval)
		coord->u.joint = joint;

	if (reiser4_node_count(old.u.joint->node) == 0) {
		old.u.joint->flags &= ~JF_DIRTY;
		reiser4_tree_release(tree, old.u.joint);
		old.u.joint = NULL;
	}

	if (direction == D_LEFT) {
		if (old.u.joint && old.u.joint->parent) {
			if (reiser4_coord_open(&ldcoord, old.u.joint, CT_JOINT, &ldpos))
				return -1;

			if (reiser4_item_key(&ldcoord, &ldkey))
				return -1;
				
			if (reiser4_joint_update(old.u.joint->parent, &ldpos, &ldkey))
				return -1;
		}
	} else {
		if (joint->parent) {
			if (reiser4_coord_open(&ldcoord, joint, CT_JOINT, &ldpos))
				return -1;

			if (reiser4_item_key(&ldcoord, &ldkey))
				return -1;
				
			if (reiser4_joint_update(joint, &ldpos, &ldkey))
				return -1;
		}
	}
		
	return 0;*/

	while (1) {
    
		/* Prepare the src and dst coords for moving */
		if (direction == D_LEFT) {
			reiser4_pos_t src_pos = {0, ~0ul};
			reiser4_pos_t dst_pos = {reiser4_node_count(joint->node), ~0ul};

			if (reiser4_coord_open(&src, old.u.joint, CT_JOINT, &src_pos))
				return -1;
	    
			reiser4_coord_init(&dst, joint, CT_JOINT, &dst_pos);
		} else {
			reiser4_pos_t dst_pos = {0, ~0ul};
			reiser4_pos_t src_pos = {reiser4_node_count(old.u.joint->node) - 1, ~0ul};
			
			if (reiser4_coord_open(&src, old.u.joint, CT_JOINT, &src_pos))
				return -1;
	    
			reiser4_coord_init(&dst, joint, CT_JOINT, &dst_pos);
		}
	
		overhead = reiser4_node_overhead(joint->node);
	
		if (reiser4_node_space(joint->node) < reiser4_item_len(&src) + overhead)
			return 0;
	
		if (reiser4_node_space(joint->node) - (reiser4_item_len(&src) + overhead) < needed)
			return 0;
	
		/* 
		   Check if we should finish shifting due to insert point
		   reached and @move_ip flag is not turned on.
		*/
		if (!move_ip && coord->u.joint == old.u.joint) {
			if (direction == D_LEFT) {
				if (coord->pos.item == 0)
					return 0;
			} else {
				uint32_t count = reiser4_node_count(coord->u.joint->node);
				if (coord->pos.item == count - 1)
					return 0;
			}
		}

		/* Updating the insertion point coord */
		if (direction == D_LEFT) {
			if (coord->u.joint == old.u.joint) {
				if (coord->pos.item == 0) {
					coord->u.joint = dst.u.joint;
					coord->pos.item = reiser4_node_count(dst.u.joint->node);
				} else
					coord->pos.item--;
			}
		} else {
			if (coord->u.joint == old.u.joint) {
				uint32_t count = reiser4_node_count(old.u.joint->node);
				if (coord->pos.item >= count - 1) {
					if (coord->pos.item > count - 1) {
						coord->pos.item = 0;
						coord->u.joint = dst.u.joint;
						return 0;
					}
					coord->pos.item = 0;
					coord->u.joint = dst.u.joint;
				}
			} else
				coord->pos.item++;
		}
	
		/* Moving the item denoted by @src coord to @dst one */
		if (reiser4_tree_move(tree, &dst, &src)) {
			char *side = (direction == D_LEFT ? "left" : "right");
			aal_exception_error("Can't move item %u into %s neighbour.",
					    src.pos.item, side);
			return -1;
		}
	
		if (coord->u.joint != src.u.joint)
			return 0;

		aal_assert("umka-1260", 
			   reiser4_node_count(src.u.joint->node) > 0, return -1);
	}

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

	reiser4_joint_t *left;
	reiser4_joint_t *right;
	reiser4_joint_t *parent;

	aal_assert("umka-759", old != NULL, return -1);
	aal_assert("umka-766", new != NULL, return -1);
	aal_assert("umka-929", tree != NULL, return -1);

	*new = *old;

	if (needed == 0)
		return 0;
    
	max_space = reiser4_node_maxspace(old->u.joint->node);
	
	/* 
	   Checking if item hint to be inserted to tree has length more than 
	   max possible space in a node.
	*/
	if (needed > max_space) {
		aal_exception_error("Item size is too big. Maximal possible "
				    "item size can be %u bytes long.", max_space);
		return -1;
	}
    
	if ((not_enough = needed  - reiser4_node_space(old->u.joint->node)) <= 0)
		return 0;
    
	if ((left = reiser4_joint_left(new->u.joint))) {
	    
		if (reiser4_tree_shift(tree, D_LEFT, new, left, needed, 0))
			return -1;
	
		if ((not_enough = needed - reiser4_node_space(new->u.joint->node)) <= 0)
			return 0;
	}

	if ((right = reiser4_joint_right(new->u.joint))) {
	    
		if (reiser4_tree_shift(tree, D_RIGHT, new, right, needed, 0))
			return -1;
	
		if ((not_enough = needed - reiser4_node_space(new->u.joint->node)) <= 0)
			return 0;
	}
    
	for (alloc = 0; (not_enough > 0) && (alloc < 2); alloc++) {
		uint8_t level;
		reiser4_coord_t save;
		reiser4_joint_t *joint;
	
		level = plugin_call(return -1, new->u.joint->node->entity->plugin->node_ops,
				    get_level, new->u.joint->node->entity);
	
		if (!(joint = reiser4_tree_allocate(tree, level)))
			return -1;

		save = *new;
		
		if (reiser4_tree_shift(tree, D_RIGHT, new, joint, needed, 1))
			return -1;
	
		/* Attaching new allocated node into the tree */
		if (reiser4_node_count(joint->node)) {
		
			if (reiser4_tree_attach(tree, joint)) {
				aal_exception_error("Can't attach node to the tree.");
				reiser4_tree_release(tree, joint);
				return -1;
			}
		}
	
		not_enough = needed - reiser4_node_space(new->u.joint->node);
	
		/* Checking if the old have enough free space after shifting */
		if (not_enough > 0 && save.u.joint != new->u.joint && 
		    new->pos.unit == ~0ul) 
		{
			*new = save;
			not_enough = needed - reiser4_node_space(new->u.joint->node);
		}
	}

	if (new->u.joint != old->u.joint && 
	    reiser4_node_count(old->u.joint->node) == 0)
	{
		reiser4_tree_release(tree, old->u.joint);
		old->u.joint = NULL;
	}
	
	return -(not_enough > 0);
}

/* Inserts new item described by item hint into the tree */
errno_t reiser4_tree_insert(
	reiser4_tree_t *tree,	    /* tree new item will be inserted in */
	reiser4_item_hint_t *hint,  /* item hint to be inserted */
	uint8_t level,		    /* target level insertion will be performed on */
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

	if (lookup == 1) {
		aal_exception_error(
			"Key (0x%llx:0x%x:0x%llx:0x%llx) already exists in tree.", 
			reiser4_key_get_locality(key), reiser4_key_get_type(key), 
			reiser4_key_get_objectid(key), reiser4_key_get_offset(key));
		return -1;
	}

	/* Estimating item in order to insert it into found node */
	if (reiser4_item_estimate(coord, hint))
		return -1;
    
	/* Needed space is estimated space plugs item overhead */
	needed = hint->len + (coord->pos.unit == ~0ul ? 
			      reiser4_node_overhead(coord->u.joint->node) : 0);
   
	/* This is the special case. The tree doesn't contain any nodes */
	if (level == LEAF_LEVEL && !tree->root->children) {
		reiser4_joint_t *joint;
		reiser4_pos_t pos = {0, ~0ul};
	
		if (!(joint = reiser4_tree_allocate(tree, LEAF_LEVEL))) {
			aal_exception_error("Can't allocate new leaf node.");
			return -1;
		}

		/* Updating coord by just allocated leaf */
		reiser4_coord_init(coord, joint, CT_JOINT, &pos);
	
		if (reiser4_joint_insert(coord->u.joint, &coord->pos, hint)) {
	    
			aal_exception_error("Can't insert an item into the node %llu.", 
					    coord->u.joint->node->blk);
	    
			reiser4_tree_release(tree, joint);
			return -1;
		}
	
		if (reiser4_tree_attach(tree, joint)) {
			aal_exception_error("Can't attach node to the tree.");
			reiser4_tree_release(tree, joint);
			return -1;
		}
	
		return 0;
	}
    
	if (reiser4_tree_mkspace(tree, coord, &insert, needed)) {
		aal_exception_error("Can't prepare space for isnert one more item. "
				    "No space left on device?");
		return -1;
	}
    
	if (reiser4_joint_insert(insert.u.joint, &insert.pos, hint)) {
		aal_exception_error("Can't insert an %s into the node %llu.", 
				    (insert.pos.unit == ~0ul ? "item" : "unit"),
				    insert.u.joint->node->blk);
		return -1;
	}

	/* 
	   If make space function allocate new node, we should attach it to the tree. Also,
	   here we should handle the spacial case, when tree root should be changed.
	*/
	if (insert.u.joint != tree->root && !insert.u.joint->parent) {
	
		if (!coord->u.joint->parent)
			reiser4_tree_grow(tree);
	
		if (reiser4_tree_attach(tree, insert.u.joint)) {
			aal_exception_error("Can't attach node to the tree.");
			reiser4_tree_release(tree, insert.u.joint);
			return -1;
		}
	}
    
	*coord = insert;
    
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
		aal_exception_error(
			"Key (0x%llx:0x%x:0x%llx:0x%llx) doesn't found in tree.", 
			reiser4_key_get_locality(key), reiser4_key_get_type(key),
			reiser4_key_get_objectid(key), reiser4_key_get_offset(key));
		return -1;
	}
    
	if (reiser4_joint_remove(coord.u.joint, &coord.pos))
		return -1;

	/*
	  FIXME-UMKA: Here should be also checking if we need descrease tree
	  height.
	*/
    
	return 0;
}

/* 
   Moves item or unit from src coord to dst. Also it keeps track of change
   of root node in the tree.
*/
errno_t reiser4_tree_move(
	reiser4_tree_t *tree,	    /* tree we are operating on */
	reiser4_coord_t *dst,	    /* dst coord */
	reiser4_coord_t *src)	    /* src coord */
{
	aal_assert("umka-1020", tree != NULL, return -1);
	aal_assert("umka-1020", dst != NULL, return -1);
	aal_assert("umka-1020", src != NULL, return -1);

	if (reiser4_joint_move(dst->u.joint, &dst->pos, src->u.joint, &src->pos))
		return -1;
    
	return 0;
}

#endif
