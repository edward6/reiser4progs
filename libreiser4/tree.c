/*
    tree.c -- reiser4 tree cache code.
    Copyright (C) 1996-2002 Hans Reiser.
    Author Yury Umanets.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_COMPACT
#  include <sys/stat.h>
#endif

#include <reiser4/reiser4.h>

#ifndef ENABLE_COMPACT

/* Requests block allocator for new block and creates empty node in it */
static reiser4_cache_t *reiser4_tree_allocate(
    reiser4_tree_t *tree,	    /* tree for operating on */
    uint8_t level		    /* level of new node */
) {
    blk_t blk;
    aal_block_t *block;
    
    rpid_t pid;
    reiser4_node_t *node;
    reiser4_cache_t *cache;
    
    aal_assert("umka-756", tree != NULL, return NULL);
    
    /* Allocating the block */
    if (!(blk = reiser4_alloc_allocate(tree->fs->alloc))) {
        aal_exception_error("Can't allocate block for a node.");
	return NULL;
    }

    if (!(block = aal_block_create(tree->fs->format->device, blk, 0))) {
	aal_exception_error("Can't allocate block %llu in memory.", blk);
	return NULL;
    }
    
    if ((pid = reiser4_node_pid(tree->cache->node)) == 
	INVALID_PLUGIN_ID) 
    {
	aal_exception_error("Invalid node plugin has been detected.");
	goto error_free_block;
    }
    
    /* Creating new node */
    if (!(node = reiser4_node_create(block, pid, level)))
	goto error_free_block;

    plugin_call(goto error_free_block, node->entity->plugin->node_ops,
	set_stamp, node->entity, reiser4_format_get_stamp(tree->fs->format));
    
    if (!(cache = reiser4_cache_create(node))) {
	reiser4_node_close(node);
	return NULL;
    }
    
    cache->level = level;
    
    return cache;
    
error_free_block:
    aal_block_free(block);
    return NULL;
}

static void reiser4_tree_release(reiser4_tree_t *tree, 
    reiser4_cache_t *cache) 
{
    aal_assert("umka-917", cache != NULL, return);
    aal_assert("umka-918", cache->node != NULL, return);

    reiser4_alloc_release(tree->fs->alloc, 
	aal_block_number(cache->node->block));
    
    reiser4_cache_close(cache);
}

#endif

static reiser4_cache_t *reiser4_tree_load(reiser4_tree_t *tree, 
    blk_t blk, uint8_t level) 
{
    aal_block_t *block;
    reiser4_node_t *node;
    reiser4_cache_t *cache;

    if (!(block = aal_block_open(tree->fs->format->device, blk))) {
	aal_exception_error("Can't allocate block %llu in memory.", blk);
	return NULL;
    }
    
    if (!(node = reiser4_node_open(block))) 
	goto error_free_block;
	    
    if (!(cache = reiser4_cache_create(node))) {
	reiser4_node_close(node);
	return NULL;
    }

    cache->level = level;
    
    return cache;
    
error_free_block:
    aal_block_free(block);
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
    reiser4_tree_t *tree	/* tree to be used */
) {
    rpid_t pid;
    reiser4_oid_t *oid;
    roid_t objectid, locality;
    reiser4_plugin_t *plugin;
    
    aal_assert("umka-1090", tree != NULL, return -1);
    aal_assert("umka-1091", tree->fs != NULL, return -1);
    aal_assert("umka-1092", tree->fs->oid != NULL, return -1);
    
    oid = tree->fs->oid;
    
    /* FIXME-UMKA: hardcoded key plugin id */
    pid = KEY_REISER40_ID;
        
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

/* Opens the tree (that is, the tree cache) on specified filesystem */
reiser4_tree_t *reiser4_tree_open(reiser4_fs_t *fs) {
    blk_t tree_root;
    blk_t tree_height;
    
    reiser4_tree_t *tree;

    aal_assert("umka-737", fs != NULL, return NULL);

    /* Allocating memory for the tree instance */
    if (!(tree = aal_calloc(sizeof(*tree), 0)))
	return NULL;
    
    tree->fs = fs;

    /* Building the tree root key */
    if (reiser4_tree_build_key(tree)) {
	aal_exception_error("Can't build the tree root key.");
	goto error_free_tree;
    }
    
    /* Opening root node */
    tree_root = reiser4_tree_root(tree);
    tree_height = reiser4_tree_height(tree);
    
    if (!(tree->cache = reiser4_tree_load(tree, tree_root, tree_height)))
	goto error_free_tree;
    
    tree->cache->tree = tree;
    
    return tree;

error_free_tree:
    aal_free(tree);
    return NULL;
}

/* Returns tree root cache */
blk_t reiser4_tree_root(reiser4_tree_t *tree) {
    aal_assert("umka-738", tree != NULL, return 0);
    return reiser4_format_get_root(tree->fs->format);
}

#ifndef ENABLE_COMPACT

/* Creates new balanced tree on specified filesystem */
reiser4_tree_t *reiser4_tree_create(
    reiser4_fs_t *fs,		    /* filesystem new tree will be created on */
    reiser4_profile_t *profile	    /* profile to be used */
) {
    blk_t blk;
    aal_block_t *block;
    reiser4_node_t *node;
    reiser4_tree_t *tree;

    aal_assert("umka-741", fs != NULL, return NULL);
    aal_assert("umka-749", profile != NULL, return NULL);

    /* Allocating memory needed for tree instance */
    if (!(tree = aal_calloc(sizeof(*tree), 0)))
	return NULL;

    tree->fs = fs;
    
    /* Building the tree root key */
    if (reiser4_tree_build_key(tree)) {
	aal_exception_error("Can't build the tree root key.");
	goto error_free_tree;
    }
    
    /* Getting free block from block allocator for place root block in it */
    if (!(blk = reiser4_alloc_allocate(fs->alloc))) {
        aal_exception_error("Can't allocate block for the root node.");
	goto error_free_tree;
    }

    if (!(block = aal_block_create(fs->format->device, blk, 0))) {
        aal_exception_error("Can't allocate in memory root block.");
	goto error_free_tree;
    }
    
    /* Creating root node */
    if (!(node = reiser4_node_create(block, profile->node, 
	reiser4_tree_height(tree)))) 
    {
	aal_exception_error("Can't create root node.");
	goto error_free_block;
    }

    /* Creating cache for root node */
    if (!(tree->cache = reiser4_cache_create(node)))
	goto error_free_node;
    
    tree->cache->level = reiser4_tree_height(tree);
    tree->cache->tree = tree;
    
    return tree;

error_free_node:
    reiser4_node_close(node);
error_free_block:
    aal_block_free(block);
error_free_tree:
    aal_free(tree);
    return NULL;
}

/* 
    Syncs whole tree cache and removes all nodes except root node from the 
    cache.
*/
errno_t reiser4_tree_flush(reiser4_tree_t *tree) {
    aal_list_t *list;
    
    aal_assert("umka-573", tree != NULL, return -1);

    list = tree->cache->list ? aal_list_first(tree->cache->list) : NULL;
    
    if (list) {
	aal_list_t *walk;
	
	aal_list_foreach_forward(walk, list)
	    reiser4_cache_close((reiser4_cache_t *)walk->item);
	
	tree->cache->list = NULL;
    }

    return 0;
}

/* Syncs whole tree cache */
errno_t reiser4_tree_sync(reiser4_tree_t *tree) {
    aal_assert("umka-560", tree != NULL, return -1);
    
    return reiser4_cache_sync(tree->cache);
}

#endif

/* Closes specified tree (frees all assosiated memory) */
void reiser4_tree_close(reiser4_tree_t *tree) {
    aal_assert("umka-134", tree != NULL, return);
    
    reiser4_cache_close(tree->cache);
    aal_free(tree);
}

uint8_t reiser4_tree_height(reiser4_tree_t *tree) {
    aal_assert("umka-1065", tree != NULL, return 0);
    return reiser4_format_get_height(tree->fs->format);
}

/* 
    Makes search in the tree by specified key. Fills passed coord by coords of 
    found item. 
*/
int reiser4_tree_lookup(
    reiser4_tree_t *tree,	/* tree to be grepped */
    reiser4_key_t *key,		/* key to be find */
    uint8_t level,		/* stop level for search */
    reiser4_coord_t *coord	/* coord of found item */
) {
    blk_t target;
    int lookup, deep;
    
    reiser4_key_t ldkey;
    reiser4_item_t item;
    reiser4_cache_t *parent;

    aal_assert("umka-742", key != NULL, return -1);
    aal_assert("umka-742", coord != NULL, return -1);
  
    deep = reiser4_tree_height(tree);
    reiser4_coord_init(coord, tree->cache, 0, ~0ul);
    
    /* 
        Check for the case when looked key smaller than root key. This is 
        the case, when somebody is trying to go up of the root by ".." entry
        of root directory.
    */
    if (reiser4_key_compare(key, &tree->key) < 0)
        *key = tree->key;
		    
    while (1) {
	reiser4_node_t *node;

	node = coord->cache->node;
	
	/* 
	    Looking up for key inside node. Result of lookuping will be stored
	    in &coord->pos.
	*/
	if ((lookup = reiser4_node_lookup(node, key, &coord->pos)) == -1)
	    return -1;

	if (deep <= level || reiser4_node_count(coord->cache->node) == 0)
	    return lookup;
       	
	if (lookup == 0 && coord->pos.item > 0)
	    coord->pos.item--;

	if (reiser4_item_open(&item, coord->cache->node, &coord->pos)) {
	    aal_exception_error("Can't open item by its coord. Node %llu, item %u.",
	        aal_block_number(coord->cache->node->block), coord->pos.item);
	    return -1;
	}
	    
	if (!reiser4_item_internal(&item)) {
	    aal_exception_error("Not internal item was found on the twig level. "
	        "Sorry, drilling is not supported yet!");
	    return -1;
	}
	    
	/* Getting the node pointer from internal item */
	if (!(target = reiser4_item_get_iptr(&item))) {
	    reiser4_node_t *node = coord->cache->node;

	    aal_exception_error("Can't get pointer from internal item %u, "
		"node %llu.", item.pos->item, aal_block_number(node->block));
	    return -1;
	}
	
	deep--; 
	parent = coord->cache;
	
	/* 
	    Check whether specified node already in cache. If so, we use node
	    from the cache.
	*/
	reiser4_node_get_key(coord->cache->node, &coord->pos, &ldkey);

	if (!(coord->cache = reiser4_cache_find(parent, &ldkey))) {
	    /* 
		Node was not found in the cache, we open it and register in the 
		cache.
	    */
	    if (!(coord->cache = reiser4_tree_load(tree, target, deep))) {
		aal_exception_error("Can't load node %llu durring lookup.", target);
		return -1;
	    }

	    /* Registering node in tree cache */
	    if (reiser4_cache_register(parent, coord->cache)) {
		aal_exception_error("Can't register node %llu in the tree.", target);
		
		reiser4_cache_close(coord->cache);
		return -1;
	    }
	}
    }
    
    return 0;
}

#ifndef ENABLE_COMPACT

/* This function inserts internal item to the tree */
static errno_t reiser4_tree_attach(
    reiser4_tree_t *tree,	    /* tree we will attach node to */
    reiser4_cache_t *cache	    /* child to attached */
) {
    int lookup, level;
    reiser4_key_t ldkey;
    reiser4_coord_t coord;
    reiser4_item_hint_t hint;
    reiser4_internal_hint_t internal_hint;

    aal_assert("umka-913", tree != NULL, return -1);
    aal_assert("umka-916", cache != NULL, return -1);
    aal_assert("umka-919", reiser4_node_count(cache->node) > 0, return -1);
    
    /* Preparing internal item hint */
    aal_memset(&hint, 0, sizeof(hint));
	
    /* 
	FIXME-UMKA: Hardcoded internal item id. Here should be getting internal
	item plugin id from parent. In the case parent doesn't exist, it should
	be got from filesystem default profile.
    */
    if (!(hint.plugin = 
	libreiser4_factory_ifind(ITEM_PLUGIN_TYPE, ITEM_INTERNAL40_ID)))
    {
	aal_exception_error("Can't find internal item plugin by its id 0x%x.", 
	    ITEM_INTERNAL40_ID);
	return -1;
    }

    aal_memset(&internal_hint, 0, sizeof(internal_hint));
    internal_hint.ptr = aal_block_number(cache->node->block);

    reiser4_node_lkey(cache->node, &ldkey);
    reiser4_key_init(&hint.key, ldkey.plugin, ldkey.body);
    hint.hint = &internal_hint;

    if (reiser4_tree_insert(tree, &hint, LEAF_LEVEL + 1, &coord)) {
        aal_exception_error("Can't insert internal item to the tree.");
	return -1;
    }
    
    if (reiser4_cache_register(coord.cache, cache)) {
        aal_exception_error("Can't register node %llu in tree cache.", 
	    aal_block_number(cache->node->block));
	return -1;
    }

    return 0;
}

/* This function grows and sets up tree after the growing */
static errno_t reiser4_tree_grow(
    reiser4_tree_t *tree,	/* tree to be growed */
    reiser4_cache_t *cache	/* old root cached node */
) {
    /* Allocating new root node */
    if (!(tree->cache = reiser4_tree_allocate(tree,
	reiser4_tree_height(tree) + 1))) 
    {
	aal_exception_error("Can't allocate root node.");
	return -1;
    }

    if (reiser4_tree_attach(tree, cache)) {
	aal_exception_error("Can't attach node to the tree.");
	goto error_free_cache;
    }
	    
    /* Updating the tree height and tree root values in disk format */
    reiser4_format_set_height(tree->fs->format, 
	reiser4_tree_height(tree) + 1);
    
    reiser4_format_set_root(tree->fs->format, 
	aal_block_number(tree->cache->node->block));

    return 0;

error_free_cache:
    reiser4_tree_release(tree, tree->cache);
    tree->cache = cache;
    return -1;
}

#define LEFT	(0)
#define RIGHT	(1)

static errno_t reiser4_tree_shift(reiser4_tree_t *tree, int direction, 
    reiser4_coord_t *coord, reiser4_cache_t *cache, int mip) 
{
    uint32_t overhead;
    reiser4_item_t item;
    
    reiser4_coord_t old;
    reiser4_coord_t src, dst;
    
    aal_assert("umka-1225", tree != NULL, return -1);
    aal_assert("umka-1226", coord != NULL, return -1);
    aal_assert("umka-1227", cache != NULL, return -1);

    old = *coord;
    
    while (1) {
	
	if (direction == LEFT) {
	    reiser4_coord_init(&src, old.cache, 0, ~0ul);
	    reiser4_coord_init(&dst, cache, reiser4_node_count(cache->node), ~0ul);
	} else {
	    reiser4_coord_init(&src, old.cache, 
		reiser4_node_count(old.cache->node) - 1, ~0ul);
	    
	    reiser4_coord_init(&dst, cache, 0, ~0ul);
	}
	
	if (reiser4_item_open(&item, src.cache->node, &src.pos)) {
	    aal_exception_error("Can't open item by its coord. Node %llu, item %u.",
		aal_block_number(src.cache->node->block), src.pos.item);
	    return -1;
	}
	
	overhead = reiser4_node_overhead(cache->node);
	
	if (reiser4_node_space(cache->node) < reiser4_item_len(&item) + overhead)
	    return 0;
	
	if (reiser4_node_count(src.cache->node) == 1)
	    return 0;

	if (!mip) {
	    if (direction == LEFT) {
		if (coord->cache == old.cache && coord->pos.item == 0)
		    return 0;
	    } else {
		if (coord->cache == old.cache && 
			coord->pos.item == reiser4_node_count(coord->cache->node) - 1)
		    return 0;
	    }
	}
	
	if (direction == LEFT) {
	    if (coord->cache == old.cache) {
		if (coord->pos.item == 0) {
		    coord->cache = dst.cache;
		    coord->pos.item = reiser4_node_count(dst.cache->node);
		} else
		    coord->pos.item--;
	    }
	} else {
	    if (coord->cache == old.cache) {
		
		uint32_t count = reiser4_node_count(old.cache->node);
		
		if (coord->pos.item >= count - 1) {
		    coord->pos.item = 0;
		    coord->cache = dst.cache;
		}
	    } else
		coord->pos.item++;
	}

        if (reiser4_tree_move(tree, &dst, &src)) {
            aal_exception_error("Can't move item %u into %s neighbour.",
		src.pos.item, (direction == LEFT ? "left" : "right"));
	    return -1;
	}
    }

    return 0;
}

errno_t reiser4_tree_mkspace(
    reiser4_tree_t *tree,	    /* tree pointer function operates on */
    reiser4_coord_t *old,	    /* old coord of insertion point */
    reiser4_coord_t *new,	    /* new coord will be stored here */
    uint32_t needed		    /* amount of space that should be freed */
) {
    int alloc;
    uint32_t max_space;
    int32_t not_enough;
    reiser4_cache_t *parent;

    aal_assert("umka-759", old != NULL, return -1);
    aal_assert("umka-766", new != NULL, return -1);
    aal_assert("umka-929", tree != NULL, return -1);

    *new = *old;

    if (!old->cache->parent || needed == 0)
	return 0;
    
    max_space = reiser4_node_maxspace(old->cache->node);
	
    /* 
        Checking if item hint to be inserted to tree has length more than 
        max possible space in a node.
    */
    if (needed > max_space) {
        aal_exception_error("Item size is too big. Maximal possible "
	   "item size can be %u bytes long.", max_space);
	return -1;
    }
    
    if ((not_enough = needed  - reiser4_node_space(old->cache->node)) <= 0)
	return 0;
    
    if (reiser4_cache_raise(old->cache)) {
	aal_exception_error("Can't raise up neighbours of node %llu.", 
	    aal_block_number(old->cache->node->block));
	return -1;
    }
    
    if (new->cache->left) {
	    
	if (reiser4_tree_shift(tree, LEFT, new, new->cache->left, 1))
	    return -1;
	
	if ((not_enough = needed - reiser4_node_space(new->cache->node)) <= 0)
	    return 0;
    }

    if (new->cache->right) {
	    
	if (reiser4_tree_shift(tree, RIGHT, new, new->cache->right, 0))
	    return -1;
	
	if ((not_enough = needed - reiser4_node_space(new->cache->node)) <= 0)
	    return 0;
    }
    
    for (alloc = 0; (not_enough > 0) && (alloc < 2); alloc++) {
        reiser4_cache_t *cache;
        reiser4_coord_t coord;
	
        if (!(cache = reiser4_tree_allocate(tree, new->cache->level)))
	   return -1;
	
        coord = *new;
	    
        if (reiser4_tree_shift(tree, RIGHT, new, cache, 1))
	   return -1;
	
        if ((not_enough = needed - reiser4_node_space(new->cache->node)) <= 0)
	   return 0;

	if ((not_enough = needed - reiser4_node_space(coord.cache->node)) <= 0)
	    *new = coord;
    }

    return -(not_enough > 0);
}

/* Inserts new item described by item hint into the tree */
errno_t reiser4_tree_insert(
    reiser4_tree_t *tree,	    /* tree new item will be inserted in */
    reiser4_item_hint_t *hint,	    /* item hint to be inserted */
    uint8_t level,		    /* target level insertion will be performed on */
    reiser4_coord_t *coord	    /* coord item or unit inserted at */
) {
    int lookup;
    uint32_t needed;
    
    reiser4_key_t *key;
    reiser4_item_t item;
    
    reiser4_coord_t fake;
    reiser4_coord_t insert;

    aal_assert("umka-779", tree != NULL, return -1);
    aal_assert("umka-779", hint != NULL, return -1);
  
    if (!coord) coord = &fake;
    
    key = (reiser4_key_t *)&hint->key;

    /* Looking up for target node */
    if ((lookup = reiser4_tree_lookup(tree, key, level, coord)) == -1)
	return -1;

    if (lookup == 1) {
	aal_exception_error(
	    "Key (0x%llx 0x%x 0x%llx 0x%llx) already exists in tree.", 
	    reiser4_key_get_locality(key), reiser4_key_get_type(key), 
	    reiser4_key_get_objectid(key), reiser4_key_get_offset(key));
	return -1;
    }

    /* Estimating item in order to insert it into found node */
    reiser4_item_init(&item, coord->cache->node, &coord->pos);
	
    if (reiser4_item_estimate(&item, hint))
        return -1;
    
    /* Needed space is estimated space plugs item overhead */
    needed = hint->len + (coord->pos.unit == ~0ul ? 
	reiser4_node_overhead(coord->cache->node) : 0);
   
    /* This is the special case. The tree doesn't contain any nodes */
    if (level == LEAF_LEVEL && !tree->cache->list) {
	reiser4_cache_t *cache;
	
	if (!(cache = reiser4_tree_allocate(tree, LEAF_LEVEL))) {
	    aal_exception_error("Can't allocate new leaf node.");
	    return -1;
	}

	/* Updating coord by just allocated leaf */
	reiser4_coord_init(coord, cache, 0, ~0ul);
	
        if (reiser4_cache_insert(coord->cache, &coord->pos, hint)) {
	    aal_exception_error("Can't insert an item into the node %llu.", 
		aal_block_number(coord->cache->node->block));
	    reiser4_tree_release(tree, cache);
	    return -1;
	}
	
	if (reiser4_tree_attach(tree, cache)) {
	    aal_exception_error("Can't attach node to the tree.");
	    reiser4_tree_release(tree, cache);
	    return -1;
	}

	return 0;
    }
    
    if (reiser4_tree_mkspace(tree, coord, &insert, needed))
        return -1;

    *coord = insert;
    
    if (reiser4_cache_insert(coord->cache, &coord->pos, hint)) {
        aal_exception_error("Can't insert an %s into the node %llu.", 
	    (coord->pos.unit == ~0ul ? "item" : "unit"),
	    aal_block_number(coord->cache->node->block));
	return -1;
    }

    if (coord->cache != tree->cache && !coord->cache->parent) {
	if (reiser4_tree_attach(tree, coord->cache)) {
	    aal_exception_error("Can't attach node to the tree.");
	    reiser4_tree_release(tree, coord->cache);
	    return -1;
	}
    }
    
    return 0;
}

/* Removes item by specified key */
errno_t reiser4_tree_remove(
    reiser4_tree_t *tree,	/* tree item will be removed from */
    reiser4_key_t *key,		/* key item will be found by */
    uint8_t level		/* the level removing will be performed on */
) {
    int lookup;
    reiser4_coord_t coord;
    
    aal_assert("umka-1018", tree != NULL, return -1);
    aal_assert("umka-1019", key != NULL, return -1);
    
    /* Looking up for target */
    if ((lookup = reiser4_tree_lookup(tree, key, level, &coord)) == -1)
	return -1;

    if (lookup == 0) {
	aal_exception_error(
	    "Key (0x%llx:0x%x:0x%llx:0x%llx) doesn't found in tree.", 
	    reiser4_key_get_locality(key), reiser4_key_get_type(key),
	    reiser4_key_get_objectid(key), reiser4_key_get_offset(key));
	return -1;
    }
    
    if (reiser4_cache_remove(coord.cache, &coord.pos))
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
    reiser4_coord_t *src	    /* src coord */
) {
    uint32_t count;
    
    aal_assert("umka-1020", tree != NULL, return -1);
    aal_assert("umka-1020", dst != NULL, return -1);
    aal_assert("umka-1020", src != NULL, return -1);
    
    count = reiser4_node_count(src->cache->node);
    
    if (reiser4_cache_move(dst->cache, &dst->pos, src->cache, &src->pos))
	return -1;
    
    if (count == 1)
	src->cache = NULL;
    
    return 0;
}

#endif

/* Returns tree limit for number of blocks in cache */
count_t reiser4_tree_limit(reiser4_tree_t *tree) {
    aal_assert("umka-1087", tree != NULL, return 0);

    return tree->limit.max;
}

/* Sets up limit block number for the tree */
void reiser4_tree_setup(reiser4_tree_t *tree, count_t limit) {
    aal_assert("umka-1088", tree != NULL, return);
    tree->limit.max = limit;
}

