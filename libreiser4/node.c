/*
  node.c -- the personalization of the reiser4 on-disk node. The libreiser4
  internal in-memory tree consists of reiser4_node_t structures.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <reiser4/reiser4.h>

#ifndef ENABLE_COMPACT

/* Creates node instance based on passed device and block number */
reiser4_node_t *reiser4_node_create(
	aal_device_t *device,	/* device new node will be created on*/
	blk_t blk,		/* block new node will be created on */
	rpid_t pid,		/* node plugin id to be used */
	uint8_t level)		/* node level */
{
	reiser4_node_t *node;
	reiser4_plugin_t *plugin;

	aal_assert("umka-1268", device != NULL, return NULL);
    
	/* Allocating memory for instance of node */
	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;

	/* Finding the node plugin by its id */
	if (!(plugin = libreiser4_factory_ifind(NODE_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find node plugin by its id 0x%x.", pid);
		goto error_free_node;
	}
    
	/* Requesting the plugin for initialization of the entity */
	if (!(node->entity = plugin_call(goto error_free_node, plugin->node_ops,
					 create, device, blk, level))) 
	{
		aal_exception_error("Can't create node entity.");
		goto error_free_node;
	}

	node->device = device;
	node->blk = blk;
	
	return node;

 error_free_node:    
	aal_free(node);
	return NULL;
}

errno_t reiser4_node_print(
	reiser4_node_t *node,   /* node to be printed */
	aal_stream_t *stream,   /* stream for printing in */
	uint16_t options)       /* some options */
{
	aal_assert("umka-1537", node != NULL, return -1);
	aal_assert("umka-1538", stream != NULL, return -1);
	
	return plugin_call(return -1, node->entity->plugin->node_ops,
			   print, node->entity, stream, options);
}

#endif

struct guess_node {
	blk_t blk;
	
	aal_device_t *device;
	object_entity_t *entity;
};

static errno_t callback_guess_node(reiser4_plugin_t *plugin, void *data) {
	struct guess_node *guess = (struct guess_node *)data;

	if (plugin->h.sign.type == NODE_PLUGIN_TYPE) {

		if (!(guess->entity = plugin_call(return -1, plugin->node_ops,
						  open, guess->device, guess->blk)))
			return -1;
		
		if (plugin_call(return -1, plugin->node_ops, confirm, guess->entity))
			return 1;

	}

	guess->entity = NULL;
	return 0;
}

/* This function is trying to detect node plugin */
static object_entity_t *reiser4_node_guess(
	aal_device_t *device,    /* device node lies on */
	blk_t blk)               /* node block */
{
	struct guess_node guess;
	object_entity_t *entity;
    
	guess.blk = blk;
	guess.device = device;
	guess.entity = NULL;
	
	/* Finding node plugin by its id from node header */
	if (!libreiser4_factory_cfind(callback_guess_node, &guess))
		return NULL;

	return guess.entity;
}

/* Opens node on specified device and block number */
reiser4_node_t *reiser4_node_open(
	aal_device_t *device,	/* device node will be opened on */
	blk_t blk)		/* block number node will be opened on */
{
	reiser4_node_t *node;

	aal_assert("umka-160", device != NULL, return NULL);
   
	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;
   
	if (!(node->entity = reiser4_node_guess(device, blk))) {
		/* 
		    FIXME-VITALY->UMKA: It is needed sometimes, like during 
		    scanning the disk at fsck time, to disable logical 
		    exceptions - e.g. this block does not contain a node - but 
		    does not disable fatal exceptions  - bad environement, 
		    failed to allocated a memory, etc.
		*/
		aal_exception_error("Can't guess node plugin for "
				    "node %llu.", blk);
		goto error_free_node;
	}
    
	node->device = device;
	node->blk = blk;
	
	return node;
    
 error_free_node:
	aal_free(node);
	return NULL;
}

/* Closes specified node */
errno_t reiser4_node_close(reiser4_node_t *node) {
	aal_assert("umka-824", node != NULL, return -1);
	aal_assert("umka-903", node->entity != NULL, return -1);
    
/*	if (node->flags & NF_DIRTY) {
		aal_exception_warn("Destroing dirty node. Block %llu.",
				   node->blk);
	}

	if (node->counter) {
		aal_exception_warn("Destroing locked (%d) node. Block %llu.",
				   node->counter, node->blk);
	}*/
	
	if (node->children) {
		aal_list_t *walk;

		for (walk = node->children; walk; ) {
			aal_list_t *temp = aal_list_next(walk);
			reiser4_node_close((reiser4_node_t *)walk->data);
			walk = temp;
		}

		aal_list_free(node->children);
		node->children = NULL;
	}
 
	if (node->parent) {
		reiser4_node_detach(node->parent, node);
		node->parent = NULL;
	}
	
	/* Uninitializing all fields */
	if (node->left)
		node->left->right = NULL;
    
	if (node->right)
		node->right->left = NULL;
    
	node->left = NULL;
	node->right = NULL;
	
	plugin_call(return -1, node->entity->plugin->node_ops,
		    close, node->entity);
	    
	aal_free(node);

	return 0;
}

errno_t reiser4_node_lock(reiser4_node_t *node) {
	errno_t res = 0;
	
	aal_assert("umka-1515", node != NULL, return -1);

	if (node->parent)
		res = reiser4_node_lock(node->parent);
	
	node->counter++;
	
	return res;
}

errno_t reiser4_node_unlock(reiser4_node_t *node) {
	errno_t res = 0;
	
	aal_assert("umka-1516", node != NULL, return -1);
	aal_assert("umka-1517", node->counter > 0, return -1);

	if (node->parent)
		res = reiser4_node_unlock(node->parent);
	
	node->counter--;
	
	return res;
}

errno_t reiser4_node_lkey(
	reiser4_node_t *node,	/* node the ldkey will be obtained from */
	reiser4_key_t *key)	/* key pointer found key will be stored in */
{
	reiser4_coord_t coord;
	reiser4_pos_t pos = {0, ~0ul};

	aal_assert("umka-753", node != NULL, return -1);
	aal_assert("umka-754", key != NULL, return -1);

	if (reiser4_coord_open(&coord, node, &pos))
		return -1;

	return reiser4_item_key(&coord, key);
}

/* Returns left or right neighbor key for passed node */
errno_t reiser4_node_nkey(
	reiser4_node_t *node,	        /* node for working with */
	direction_t direction,	        /* direction (left or right) */
	reiser4_key_t *key)		/* key pointer result should be stored */
{
	reiser4_pos_t pos;
	reiser4_coord_t coord;
    
	aal_assert("umka-770", node != NULL, return -1);
	aal_assert("umka-771", key != NULL, return -1);
    
	if (reiser4_node_pos(node, &pos))
		return -1;
    
	/* Checking for position */
	if (direction == D_LEFT) {
	    
		if (pos.item == 0) 
			return -1;
	
	} else {
		reiser4_node_t *parent = node->parent;
	
		/* Checking and proceccing the special case called "shaft" */
		if (pos.item == reiser4_node_count(parent) - 1) {

			if (!parent->parent)
				return -1;
		
			return reiser4_node_nkey(parent->parent, direction, key);
		}
	}
    
	pos.item += (direction == D_RIGHT ? 1 : -1);

	if (reiser4_coord_open(&coord, node->parent, &pos))
		return -1;
	
	return reiser4_item_key(&coord, key);
}

/* Returns position of passed node in parent node */
errno_t reiser4_node_pos(
	reiser4_node_t *node,	        /* node position will be obtained for */
	reiser4_pos_t *pos)		/* pointer result will be stored in */
{
	reiser4_key_t lkey;
	reiser4_key_t parent_key;
    
	aal_assert("umka-869", node != NULL, return -1);
	aal_assert("umka-1266", pos != NULL, return -1);
    
	if (!node->parent)
		return -1;

	reiser4_node_lkey(node, &lkey);
    
	if (reiser4_node_lookup(node->parent, &lkey, pos) != 1)
		return -1;

	node->pos = *pos;
    
	return 0;
}

/* 
   This function raises up both neighbours of the passed node. This is used by
   shifting code in tree.c
*/
reiser4_node_t *reiser4_node_left(
	reiser4_node_t *node)	/* node for working with */
{
	reiser4_key_t key;

	/* 
	   Initializing stop level for tree lookup function. Here tree lookup
	   function is used as instrument for reflecting the part of tree into
	   libreiser4 tree cache.  So, connecting to the stop level for lookup
	   we need to map the part of the tree from the root (tree height) to
	   the level of passed node, because we should make sure, that needed
	   neighbour will be mapped into cache and will be accesible by
	   node->left or node->right pointers.
	*/
	reiser4_level_t level = {LEAF_LEVEL, LEAF_LEVEL};
    
	aal_assert("umka-776", node != NULL, return NULL);

	/* Parent is not present. The root node. */
	if (!node->parent)
		return NULL;
    
	/*
	  Attaching left neighbour into the tree. Here we take its key and
	  perform lookup. We use lookup because it attaches all nodes belong to
	  search path and settups them neighbours pointers.
	*/
	if (!node->left) {
		aal_assert("umka-1629", node->tree != NULL, return NULL);
		
		if (reiser4_node_nkey(node, D_LEFT, &key) == 0)
			reiser4_tree_lookup(node->tree, &key, &level, NULL);
	}

	return node->left;
}

reiser4_node_t *reiser4_node_right(reiser4_node_t *node) {
	reiser4_key_t key;

	reiser4_level_t level = {LEAF_LEVEL, LEAF_LEVEL};
    
	aal_assert("umka-1510", node != NULL, return NULL);

	if (!node->parent)
		return NULL;
    
	if (!node->right) {
		aal_assert("umka-1630", node->tree != NULL, return NULL);
		
		if (reiser4_node_nkey(node, D_RIGHT, &key) == 0)
			reiser4_tree_lookup(node->tree, &key, &level, NULL);
	}
    
	return node->right;
}

/* Helper function for registering in node */
static int callback_comp_node(
	const void *item1,		/* the first node instance for comparing */
	const void *item2,		/* the second one */
	void *data)		        /* user-specified data */
{
	reiser4_key_t lkey1, lkey2;

	reiser4_node_t *node1 = (reiser4_node_t *)item1;
	reiser4_node_t *node2 = (reiser4_node_t *)item2;
    
	reiser4_node_lkey(node1, &lkey1);
	reiser4_node_lkey(node2, &lkey2);
    
	return reiser4_key_compare(&lkey1, &lkey2);
}

/* Helper for comparing during finding in the children list */
static inline int callback_comp_key(
	const void *item,		/* node find will operate on */
	const void *key,		/* key to be find */
	void *data)			/* user-specified data */
{
	reiser4_key_t lkey;
	reiser4_node_t *node;

	node = (reiser4_node_t *)item;

	if (reiser4_node_count(node) == 0)
		return -1;
	
	reiser4_node_lkey(node, &lkey);
	return reiser4_key_compare(&lkey, (reiser4_key_t *)key) == 0;
}

/* Finds child by its left delimiting key */
reiser4_node_t *reiser4_node_child(
	reiser4_node_t *node,	        /* node to be greped */
	reiser4_key_t *key)		/* left delimiting key */
{
	aal_list_t *list;
	reiser4_node_t *child;
    
	if (!node->children)
		return NULL;
    
	/* Using aal_list find function */
	if (!(list = aal_list_find_custom(node->children, (void *)key,
					  callback_comp_key, NULL)))
		return NULL;

	child = (reiser4_node_t *)list->data;

	if (node->tree && aal_lru_touch(node->tree->lru, (void *)child))
		aal_exception_warn("Can't update tree lru.");

	return child;
}

/*
  Connects children into sorted children list of specified node. Sets up both
  neighbours and parent pointer.
*/
errno_t reiser4_node_attach(
	reiser4_node_t *node,	       /* node child will be connected to */
	reiser4_node_t *child)	       /* child node to be attached */
{
	reiser4_node_t *left;
	reiser4_node_t *right;
	reiser4_key_t key, lkey;
    
	aal_assert("umka-561", node != NULL, return -1);
	aal_assert("umka-564", child != NULL, return -1);

	node->children = aal_list_insert_sorted(node->children, child,
						callback_comp_node, NULL);
    
	left = node->children->prev ? 
		node->children->prev->data : NULL;
    
	right = node->children->next ? 
		node->children->next->data : NULL;
    
	child->parent = node;
    
	if (reiser4_node_pos(child, &child->pos)) {
		aal_exception_error("Can't find child %llu in parent node %llu.",
				    child->blk, node->blk);
		return -1;
	}
    
	child->tree = node->tree;
    
	/* Setting up neighbours */
	if (left) {
	
		reiser4_node_lkey(left, &lkey);
	    
		/* Getting left neighbour key */
		if (!reiser4_node_nkey(child, D_LEFT, &key))
			child->left = (reiser4_key_compare(&key, &lkey) == 0 ? left : NULL);
    
		if (child->left)
			child->left->right = child;
	}
   
	if (right) {
	
		reiser4_node_lkey(right, &lkey);
	
		/* Getting right neighbour key */
		if (!reiser4_node_nkey(child, D_RIGHT, &key))
			child->right = (reiser4_key_compare(&key, &lkey) == 0 ? right : NULL);

		if (child->right)
			child->right->left = child;
	}

	node->children = aal_list_first(node->children);

	if (node->tree && aal_lru_attach(node->tree->lru, (void *)child))
		aal_exception_warn("Can't attach node to tree lru.");
	
	return 0;
}

/*
  Remove specified childern from the node. Updates all neighbour pointers and
  parent pointer.
*/
void reiser4_node_detach(
	reiser4_node_t *node,	/* node child will be detached from */
	reiser4_node_t *child)	/* pointer to child to be deleted */
{
	aal_assert("umka-562", node != NULL, return);
	aal_assert("umka-563", child != NULL, return);

	if (!node->children)
		return;
    
	if (child->left) {
		child->left->right = NULL;
		child->left = NULL;
	}
	
	if (child->right) {
		child->right->left = NULL;
		child->right = NULL;
	}
	
	child->tree = NULL;
	child->parent = NULL;
    
	/* Updating node children list */
	node->children = aal_list_remove(node->children, child);

	if (node->tree && aal_lru_detach(node->tree->lru, (void *)child))
		aal_exception_warn("Can't detach node from tree lru.");
}

int reiser4_node_confirm(reiser4_node_t *node) {
	aal_assert("umka-123", node != NULL, return 0);
    
	return plugin_call(return 0, node->entity->plugin->node_ops, 
			   confirm, node->entity);
}

/* 
   This function makes lookup inside specified node in order to find item/unit
   stored in it.
*/
int reiser4_node_lookup(
	reiser4_node_t *node,	/* node to be grepped */
	reiser4_key_t *key,	/* key to be find */
	reiser4_pos_t *pos)	/* found pos will be stored here */
{
	int lookup;

	item_entity_t *item;
	reiser4_key_t maxkey;
	reiser4_coord_t coord;
    
	aal_assert("umka-475", pos != NULL, return -1);
	aal_assert("vpf-048", node != NULL, return -1);
	aal_assert("umka-476", key != NULL, return -1);

	pos->item = 0;
	pos->unit = ~0ul;

	if (reiser4_node_count(node) == 0)
		return 0;
   
	/* Calling node plugin */
	if ((lookup = plugin_call(return -1, node->entity->plugin->node_ops,
				  lookup, node->entity, key, pos)) == -1) 
	{
		aal_exception_error("Lookup in the node %llu failed.",
				    node->blk);
		return -1;
	}

	if (lookup == 1)
		return 1;

	if (reiser4_coord_open(&coord, node, pos)) {
		aal_exception_error("Can't open item by coord. Node "
				    "%llu, item %u.", node->blk, pos->item);
		return -1;
	}

	item = &coord.entity;

	/*
	  We are on the position where key is less then wanted. Key could lies
	  within the item or after the item.
	*/
		
	/* FIXME-UMKA: Here should not be hardcoded key40 plugin id */
	maxkey.plugin = libreiser4_factory_ifind(KEY_PLUGIN_TYPE, KEY_REISER40_ID);

	if (reiser4_item_max_poss_key(&coord, &maxkey))
		return -1;

	if (reiser4_key_compare(key, &maxkey) > 0) {
		pos->item++;
		return 0;
	}
	
	/* Calling lookup method of found item (most probably direntry item) */
	if (!item->plugin->item_ops.lookup)
		return 0;
	    
	if ((lookup = item->plugin->item_ops.lookup(item, key, &pos->unit)) == -1) {
		aal_exception_error("Lookup in the item %d in the node %llu failed.", 
				    pos->item, node->blk);
		return -1;
	}

	return lookup;
}

/* Returns real item count in specified node */
uint32_t reiser4_node_count(reiser4_node_t *node) {
	aal_assert("umka-453", node != NULL, return 0);
    
	return plugin_call(return 0, node->entity->plugin->node_ops, 
			   count, node->entity);
}
/* Returns free space of specified node */
uint16_t reiser4_node_space(reiser4_node_t *node) {
	aal_assert("umka-455", node != NULL, return 0);
    
	return plugin_call(return 0, node->entity->plugin->node_ops, 
			   space, node->entity);
}

/* Returns overhead of specified node */
uint16_t reiser4_node_overhead(reiser4_node_t *node) {
	aal_assert("vpf-066", node != NULL, return 0);

	return plugin_call(return 0, node->entity->plugin->node_ops, 
			   overhead, node->entity);
}

/* Returns item max size from in specified node */
uint16_t reiser4_node_maxspace(reiser4_node_t *node) {
	aal_assert("umka-125", node != NULL, return 0);
    
	return plugin_call(return 0, node->entity->plugin->node_ops, 
			   maxspace, node->entity);
}

/* Checks node for validness */
errno_t reiser4_node_valid(
	reiser4_node_t *node)	/* node to be checked */
{
	aal_assert("umka-123", node != NULL, return -1);
    
	return plugin_call(return -1, node->entity->plugin->node_ops, 
			   valid, node->entity);
}

#ifndef ENABLE_COMPACT

errno_t reiser4_node_shift(
	reiser4_node_t *node,
	reiser4_node_t *neig,
	shift_hint_t *hint)
{
	int retval;
	reiser4_pos_t ldpos;
	reiser4_key_t ldkey;
	reiser4_plugin_t *plugin;
    
	aal_assert("umka-1225", node != NULL, return -1);
	aal_assert("umka-1226", neig != NULL, return -1);
	aal_assert("umka-1227", hint != NULL, return -1);
    
	aal_assert("umka-1258", reiser4_node_count(node) > 0, return -1);

	/*
	  Saving node position in parent. It will be used bellow for updating
	  left delemiting key.
	*/
	if (hint->flags & SF_LEFT) {
		if (node->parent) {
			if (reiser4_node_pos(node, &ldpos))
				return -1;
		}
	} else {
		if (neig->parent) {
			if (reiser4_node_pos(neig, &ldpos))
				return -1;
		}
	}

	/*
	  Performing the shifting by calling shift method of node plugin. This
	  method shifts some amount of items and units of last item, based on
	  passed flags. It returns error code and shift hint, which contains
	  usefull information about how many items were shifted, how much bytes
	  were shifted and is insertion point was moved to neigbour node or not.
	*/
	plugin = node->entity->plugin;
	
	retval = plugin_call(return -1, plugin->node_ops, shift,
			     node->entity, neig->entity, hint);

	if (retval < 0)
		return retval;

	/* Updating leaf delimiting keys in the tree */
	if (hint->flags & SF_LEFT) {
		if (reiser4_node_count(node) != 0 &&
		    (hint->items > 0 || hint->units > 0))
		{
			node->flags |= NF_DIRTY;
			
			if (node->parent) {
				if (reiser4_node_lkey(node, &ldkey))
					return -1;
				
				if (reiser4_node_ukey(node->parent, &ldpos, &ldkey))
					return -1;
			}
		}
	} else {
		if (hint->items > 0 || hint->units > 0) {
			neig->flags |= NF_DIRTY;
			
			if (neig->parent) {
				if (reiser4_node_lkey(neig, &ldkey))
					return -1;
				
				if (reiser4_node_ukey(neig->parent, &ldpos, &ldkey))
					return -1;
			}
		}
	}

	return 0;
}

/*
  Synchronizes passed @node by means of using resursive pass though all
  children. There is possible to pass as parameter of this function the root
  node pointer. In this case the whole tree will be flushed onto device, tree
  lies on.
*/
errno_t reiser4_node_sync(
	reiser4_node_t *node)	/* node to be synchronized */
{
	aal_assert("umka-124", node != NULL, return 0);
    
	/*
	  Walking through the list of children and calling reiser4_node_sync
	  function for each element.
	*/
	if (node->children) {
		aal_list_t *walk;
	
		aal_list_foreach_forward(walk, node->children) {
			if (reiser4_node_sync((reiser4_node_t *)walk->data))
				return -1;
		}
	}

	/* Synchronizing passed @node */
	if (node->flags & NF_DIRTY) {
		
		if (plugin_call(return -1, node->entity->plugin->node_ops,
				sync, node->entity))
		{
			aal_device_t *device = node->device;
			aal_exception_error("Can't synchronize node %llu to device. %s.", 
					    node->blk, device->error);

			return -1;
		}

		node->flags &= ~NF_DIRTY;
	}
    
	return 0;
}

/*
  Updates node keys in recursive maner (needed for updating ldkeys on the all
  levels of tre tree).
*/
errno_t reiser4_node_ukey(reiser4_node_t *node,
			  reiser4_pos_t *pos,
			  reiser4_key_t *key)
{
	reiser4_pos_t ppos;
	reiser4_coord_t coord;
    
	aal_assert("umka-999", node != NULL, return -1);
	aal_assert("umka-1000", pos != NULL, return -1);
	aal_assert("umka-1001", key != NULL, return -1);
    
	aal_assert("umka-1002", reiser4_node_count(node) > 0, return -1);

	if (reiser4_coord_open(&coord, node, pos))
		return -1;

	if (reiser4_item_update(&coord, key))
		return -1;
    
	if (pos->item == 0 &&
	    (pos->unit == ~0ul || pos->unit == 0))
	{
	
		if (node->parent) {
			if (reiser4_node_pos(node, &ppos))
				return -1;
	    
			if (reiser4_node_ukey(node->parent, &ppos, key))
				return -1;
		}
	}
    
	node->flags |= NF_DIRTY;
	return 0;
}

/* 
   Inserts item or unit into cached node. Keeps track of changes of the left
   delimiting key.
*/
errno_t reiser4_node_insert(
	reiser4_node_t *node,	            /* node item will be inserted in */
	reiser4_pos_t *pos,	    	    /* pos item will be inserted at */
	reiser4_item_hint_t *hint)	    /* item hint to be inserted */
{
	errno_t ret;
	reiser4_pos_t ppos;
    
	aal_assert("umka-990", node != NULL, return -1);
	aal_assert("umka-991", pos != NULL, return -1);
	aal_assert("umka-992", hint != NULL, return -1);

	/* Saving the node in parent */
	if (pos->item == 0 && (pos->unit == 0 || pos->unit == ~0ul)) {
		if (node->parent) {
			if (reiser4_node_pos(node, &ppos))
				return -1;
		}
	}

	/* Inserting item into the node */
	if (!hint->data) {
		/* 
		   Estimate the size that will be spent for item. This should be
		   done if item->data not installed.
		*/
		if (hint->len == 0) {
			reiser4_coord_t coord;
	    
			if (reiser4_coord_init(&coord, node, pos))
				return -1;
	    
			if (reiser4_item_estimate(&coord, hint)) {
				aal_exception_error("Can't estimate space that "
						    "item being inserted will consume.");
				return -1;
			}
		}
	} else {
		aal_assert("umka-761", hint->len > 0 && 
			   hint->len < reiser4_node_maxspace(node), return -1);
	}
    
	/* Checking if item length is gretter then free space in node */
	if (hint->len + (pos->unit == ~0ul ? reiser4_node_overhead(node) : 0) >
	    reiser4_node_space(node))
	{
		char *target = (pos->unit == ~0ul ? "item" : "unit");
		aal_exception_error("There is no space to insert the %s of (%u) "
				    "size in the node (%llu).", target, 
				    hint->len, node->blk);
		return -1;
	}

	/* 
	   Inserting new item or passting unit into one existent item pointed by
	   pos->item.
	*/
	if (pos->unit == ~0ul) {
		if ((ret = plugin_call(return -1, node->entity->plugin->node_ops, 
				       insert, node->entity, pos, hint)) != 0)
			return ret;
	} else {
		if ((ret = plugin_call(return -1, node->entity->plugin->node_ops, 
				       paste, node->entity, pos, hint)) != 0)
			return ret;
	}
	
	/* Updating ldkey in parent node */
	if (pos->item == 0 && (pos->unit == 0 || pos->unit == ~0ul)) {
		reiser4_node_t *parent = node->parent;
	
		if (parent) {
			if (reiser4_node_ukey(parent, &ppos, &hint->key))
				return -1;
		}
	}

	node->flags |= NF_DIRTY;
	return 0;
}

/* 
   Deletes item or unit from cached node. Keeps track of changes of the left
   delimiting key.
*/
errno_t reiser4_node_remove(
	reiser4_node_t *node,	            /* node item will be inserted in */
	reiser4_pos_t *pos)		    /* pos item will be inserted at */
{
	reiser4_key_t key;
	reiser4_pos_t ppos;

	aal_assert("umka-993", node != NULL, return -1);
	aal_assert("umka-994", pos != NULL, return -1);

	if (pos->item == 0 && (pos->unit == 0 || pos->unit == ~0ul)) {
		if (node->parent) {
			if (reiser4_node_pos(node, &ppos))
				return -1;
		}
	}
    
	/* 
	   Updating list of childrens of modified node in the case we modifying an 
	   internal node.
	*/
	if (node->children) {
		reiser4_coord_t coord;
		reiser4_node_t *child;

		if (reiser4_coord_open(&coord, node, pos))
			return -1;

		if (reiser4_item_key(&coord, &key))
			return -1;
		
		if ((child = reiser4_node_child(node, &key)))
			reiser4_node_detach(node, child);
	}

	/* Removing item or unit */
	if (pos->unit == ~0ul) {
		return plugin_call(return -1, node->entity->plugin->node_ops, 
				   remove, node->entity, pos);
	} else {
		reiser4_coord_t coord;
	
		if (reiser4_coord_open(&coord, node, pos)) {
			aal_exception_error("Can't open item by coord. Node %llu, item %u.",
					    node->blk, pos->item);
			return -1;
		}

		if (reiser4_item_count(&coord) > 1) {
			return plugin_call(return -1, node->entity->plugin->node_ops, 
					   cut, node->entity, pos);
		} else {
			return plugin_call(return -1, node->entity->plugin->node_ops, 
					   remove, node->entity, pos);
		}
	}

	/* Updating left deleimiting key in all parent nodes */
	if (pos->item == 0 && (pos->unit == 0 || pos->unit == ~0ul)) {
		reiser4_node_t *parent = node->parent;
	
		if (parent) {
			if (reiser4_node_count(node) > 0) {
				reiser4_key_t lkey;

				reiser4_node_lkey(node, &lkey);
				
				if (reiser4_node_ukey(parent, &ppos, &lkey))
					return -1;
			} else {
				/* 
				   Removing cached node from the tree in the
				   case it has not items anymore.
				*/
				if (reiser4_node_remove(parent, &ppos))
					return -1;
			}
		}
	}

	node->flags |= NF_DIRTY;
	
	return 0;
}

/* This function traverse passed node. */
errno_t reiser4_node_traverse(
	reiser4_node_t *node,		     /* node which should be traversed */
	traverse_hint_t *hint,		     /* hint for traverse and for callback methods */
	traverse_open_func_t open_func,	     /* callback for node opening */
	traverse_edge_func_t before_func,    /* callback to be called at the beginning */
	traverse_setup_func_t setup_func,    /* callback to be called before a child  */
	traverse_setup_func_t update_func,   /* callback to be called after a child */
	traverse_edge_func_t after_func)     /* callback to be called at the end */
{
	errno_t result = 0;
	reiser4_coord_t coord;
	reiser4_node_t *child = NULL;
	reiser4_pos_t *pos = &coord.pos;
    
	aal_assert("vpf-418", hint != NULL, return -1);
	aal_assert("vpf-390", node != NULL, return -1);

	node->counter++;

	if ((before_func && (result = before_func(node, hint->data))))
		goto error;

	for (pos->item = 0; pos->item < reiser4_node_count(node); pos->item++) {
		pos->unit = ~0ul; 

		/*
		  If there is a suspicion in a corruption, it must be checked in
		  before_func. All items must be opened here.
		*/
		if (reiser4_coord_open(&coord, node, pos)) {
			aal_exception_error("Can't open item by coord. Node %llu, item %u.",
					    node->blk, pos->item);
			goto error_after_func;
		}

		if (!(hint->objects & (1 << reiser4_item_type(&coord))))
			continue;
	    
		for (pos->unit = 0; pos->unit < reiser4_item_count(&coord); pos->unit++) {
			reiser4_ptr_hint_t ptr;
		
			if (plugin_call(continue, coord.entity.plugin->item_ops,
					fetch, &coord.entity, pos->unit, &ptr, 1))
				goto error_after_func;
		
			if (ptr.ptr != FAKE_BLK && ptr.ptr != 0) {
				child = NULL;
					
				if (setup_func && (result = setup_func(&coord, hint->data)))
					goto error_after_func;

				if (!open_func)
					goto update;

				if (!(child = reiser4_node_child(node, &coord.entity.key))) {
						
					if ((result = open_func(&child, ptr.ptr, hint->data)))
						goto error_update_func;

					if (!child)
						goto update;

					child->data = (void *)1;
					
					if (reiser4_node_attach(node, child))
						goto error_free_child;
				}

				if ((result = reiser4_node_traverse(child, hint, 
								    open_func,
								    before_func, 
								    setup_func,
								    update_func,
								    after_func)) < 0)
					goto error_free_child;

				if (hint->cleanup && !child->children &&
				    !child->counter && child->data)
				{
					reiser4_node_close(child);
				}
				
			update:
				if (update_func && (result = update_func(&coord, hint->data)))
					goto error_after_func;
			}
				
			/* We want to get out of the internal loop or the item was removed. */
			if (pos->unit == ~0ul)
				break;
		}
	}
	
	if (after_func && (result = after_func(node, hint->data)))
		goto error;

	node->counter--;
	return result;

 error_free_child:
	
	if (hint->cleanup && !child->children &&
	    !child->counter && child->data)
	{
		reiser4_node_close(child);
	}

 error_update_func:
	
	if (update_func)
		result = update_func(&coord, hint->data);
    
 error_after_func:
	if (after_func)
		result = after_func(node, hint->data);
    
 error:
	node->counter--;
	return result;
}

#endif

uint32_t reiser4_node_stamp(reiser4_node_t *node) {
	aal_assert("vpf-562", node != NULL, return 0);

	return plugin_call(return 0, node->entity->plugin->node_ops, 
			   get_stamp, node->entity);
}
