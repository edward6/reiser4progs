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
		aal_exception_error("Can't find node plugin by its id "
				    "0x%x.", pid);
		goto error_free_node;
	}
    
	/* Requesting the plugin for initialization of the entity */
	if (!(node->entity = plugin_call(plugin->node_ops, create, device,
					 blk, level))) 
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
	aal_stream_t *stream)   /* stream for printing in */
{
	aal_assert("umka-1537", node != NULL, return -1);
	aal_assert("umka-1538", stream != NULL, return -1);
	
	return plugin_call(node->entity->plugin->node_ops,
			   print, node->entity, stream, 0);
}

#endif

struct guess_node {
	blk_t blk;
	
	aal_device_t *device;
	object_entity_t *entity;
};

/* Helper callback for comparing plugins durring searching needed one */
static errno_t callback_guess_node(reiser4_plugin_t *plugin, void *data) {
	struct guess_node *guess = (struct guess_node *)data;

	/* We are interested only in node plugins here */
	if (plugin->h.type == NODE_PLUGIN_TYPE) {

		/*
		  Requesting block supposed to be a correct node to be opened
		  and confirmed about its format.
		*/
		if (!(guess->entity = plugin_call(plugin->node_ops, open,
						  guess->device, guess->blk)))
			return -1;

		if (plugin_call(plugin->node_ops, confirm, guess->entity))
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
		    does not disable fatal exceptions - bad environement, failed
		    to allocated a memory, etc.
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

/*
  Closes specified node and ites children. Before the closing, this function
  also detaches nodes from the tree if they were attached.
*/
errno_t reiser4_node_close(reiser4_node_t *node) {
	aal_assert("umka-824", node != NULL, return -1);
	aal_assert("umka-903", node->entity != NULL, return -1);
    
	/* Closing children */
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

	/* Detaching node from the tree */
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

	/*
	  Calling close method from the plugin in odrder to finilize own
	  entity.
	*/
	plugin_call(node->entity->plugin->node_ops, close, node->entity);
	    
	aal_free(node);

	return 0;
}

errno_t reiser4_node_release(reiser4_node_t *node) {
	aal_assert("umka-1761", node != NULL, return -1);
	aal_assert("umka-1762", node->entity != NULL, return -1);

#ifndef ENABLE_COMPACT
	
	/* Closing children */
	if (node->children) {
		aal_list_t *walk;

		for (walk = node->children; walk; ) {
			aal_list_t *temp = aal_list_next(walk);
			reiser4_node_release((reiser4_node_t *)walk->data);
			walk = temp;
		}

		aal_list_free(node->children);
		node->children = NULL;
	}

	if (reiser4_node_isdirty(node)) {
		if (reiser4_node_sync(node)) {
			aal_exception_error("Can't write node %llu.",
					    node->blk);
			return -1;
		}
	}
	
	/* Detaching node from the tree */
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

	/*
	  Calling close method from the plugin in odrder to finilize own
	  entity.
	*/
	plugin_call(node->entity->plugin->node_ops,
		    close, node->entity);
	    
	aal_free(node);
	
	return 0;
#else
	return reiser4_node_close(node);
#endif
}

/* Getting the left delimiting key */
errno_t reiser4_node_lkey(
	reiser4_node_t *node,	/* node the ldkey will be obtained from */
	reiser4_key_t *key)	/* key pointer found key will be stored in */
{
	reiser4_coord_t coord;
	rpos_t pos = {0, ~0ul};

	aal_assert("umka-753", node != NULL, return -1);
	aal_assert("umka-754", key != NULL, return -1);

	if (reiser4_coord_open(&coord, node, &pos))
		return -1;

	if (reiser4_item_get_key(&coord, key))
		return -1;
	
	return 0;
}

/* Returns position of passed node in parent node */
errno_t reiser4_node_pos(
	reiser4_node_t *node,	        /* node position will be obtained for */
	rpos_t *pos)		/* pointer result will be stored in */
{
	reiser4_key_t lkey;
	reiser4_key_t parent_key;
    
	aal_assert("umka-869", node != NULL, return -1);
 
	if (!node->parent)
		return -1;

	reiser4_node_lkey(node, &lkey);
    
	if (reiser4_node_lookup(node->parent, &lkey, &node->pos) != 1)
		return -1;

	if (pos != NULL)
	    *pos = node->pos;
    
	return 0;
}

/*
  Helper callback function for comparing two keys durring registering the new
  child.
*/
static inline int callback_comp_key(
	const void *item,		/* node find will operate on */
	const void *key,		/* key to be find */
	void *data)			/* user-specified data */
{
	reiser4_key_t lkey;
	reiser4_node_t *node;

	node = (reiser4_node_t *)item;

	if (reiser4_node_items(node) == 0)
		return -1;
	
	reiser4_node_lkey(node, &lkey);
	return reiser4_key_compare(&lkey, (reiser4_key_t *)key);
}

/* Finds child by its left delimiting key */
reiser4_node_t *reiser4_node_cbk(
	reiser4_node_t *node,	        /* node to be greped */
	reiser4_key_t *key)		/* left delimiting key */
{
	aal_list_t *list;
	reiser4_node_t *child;
    
	if (!node->children)
		return NULL;
    
	/*
	  Using aal_list_find_custom function with local helper functions for
	  comparing keys.
	*/
	if (!(list = aal_list_find_custom(node->children, (void *)key,
					  callback_comp_key, NULL)))
		return NULL;

	child = (reiser4_node_t *)list->data;

	if (node->tree && aal_lru_touch(node->tree->lru, (void *)child))
		aal_exception_warn("Can't update tree lru.");

	return child;
}

static inline int callback_comp_blk(
	const void *item,		/* node find will operate on */
	const void *blk,		/* key to be find */
	void *data)			/* user-specified data */
{
	reiser4_node_t *node;

	node = (reiser4_node_t *)item;

	if (*(blk_t *)blk < node->blk)
		return -1;

	if (*(blk_t *)blk > node->blk)
		return 1;

	return 0;
}

/* Finds child by block number */
reiser4_node_t *reiser4_node_cbp(
	reiser4_node_t *node,	        /* node to be greped */
	blk_t blk)                      /* left delimiting key */
{
	aal_list_t *list;
	reiser4_node_t *child;
    
	if (!node->children)
		return NULL;
    
	/*
	  Using aal_list_find_custom function with local helper functions for
	  comparing block numbers.
	*/
	if (!(list = aal_list_find_custom(node->children, (void *)&blk,
					  callback_comp_blk, NULL)))
		return NULL;

	child = (reiser4_node_t *)list->data;

	if (node->tree && aal_lru_touch(node->tree->lru, (void *)child))
		aal_exception_warn("Can't update tree lru.");

	return child;
}

/*
  Helper callback function for comparing two nodes durring registering the new
  child.
*/
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

static errno_t reiser4_node_register(reiser4_node_t *node,
				     reiser4_node_t *child)
{
	aal_list_t *current;

	aal_assert("umka-1758", node != NULL, return -1);
	aal_assert("umka-1759", child != NULL, return -1);
	
	current = aal_list_insert_sorted(node->children, child,
					 callback_comp_node, NULL);
	
	child->parent = node;
	child->tree = node->tree;

	/* Checking tree validness and updating node parent pos */
	if (reiser4_node_pos(child, &child->pos)) {
		aal_exception_error("Can't find child %llu in parent "
				    "node %llu.", child->blk, node->blk);
		return -1;
	}

	if (!current->prev)
		node->children = current;

	/* Attaching new node into tree's lru list */
	if (node->tree && aal_lru_attach(node->tree->lru, (void *)child))
		aal_exception_warn("Can't attach node to tree lru.");

	return 0;
}

reiser4_node_t *reiser4_node_neighbour(reiser4_node_t *node,
				       aal_direction_t where)
{
	int found = 0;
	uint32_t orig;
	uint32_t level;

	rpos_t pos;
	reiser4_node_t *child;
	reiser4_coord_t coord;
	reiser4_ptr_hint_t ptr;
	
	level = orig = reiser4_node_level(node);

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

		if (!reiser4_item_nodeptr(&coord))
			return node;
			
		plugin_call(coord.item.plugin->item_ops, fetch,
			    &coord.item, &ptr, 0, 1);

		if (!(child = reiser4_node_cbp(node, ptr.ptr))) {
			aal_device_t *device = node->tree->fs->device;
			
			if (!(child = reiser4_node_open(device, ptr.ptr))) {
				aal_exception_error("Can't read block %llu. %s.",
						    ptr.ptr, device->error);
				return NULL;
			}

			if (reiser4_node_register(node, child))
				return NULL;
		}

		level--;
		node = child;

		pos.item = (where == D_LEFT ?
			    reiser4_node_items(node) - 1 : 0);
	}

	return node;
}

/* Finds specified neighbour node */
static reiser4_node_t *reiser4_node_fnn(reiser4_node_t *node,
					aal_direction_t where)
{	
	uint32_t level;
	reiser4_node_t *old = node;
	
	level = reiser4_node_level(node);
	
	if (!(node = reiser4_node_neighbour(node, where)))
		return NULL;
	
	if (level != reiser4_node_level(node))
		return NULL;
	
	if (where == D_LEFT) {
		old->left = node;
		node->right = old;
	} else {
		old->right = node;
		node->left = old;
	}
	
	return node;
}

static reiser4_node_t *reiser4_node_lnn(reiser4_node_t *node) {
	return reiser4_node_fnn(node, D_LEFT);
}

static reiser4_node_t *reiser4_node_rnn(reiser4_node_t *node) {
	return reiser4_node_fnn(node, D_RIGHT);
}

/* 
   This function raises up to the tree the left neighbour node. This is used by
   mkspace function in tree.c
*/
reiser4_node_t *reiser4_node_left(
	reiser4_node_t *node)	/* node for working with */
{
	aal_assert("umka-776", node != NULL, return NULL);

	/* Parent is not present. The root node. */
	if (!node->parent)
		return NULL;

	if (!node->left) {
		aal_assert("umka-1629", node->tree != NULL, return NULL);
		node->left = reiser4_node_lnn(node);
	}

	return node->left;
}

/* The same as previous function, but for right neighbour. */
reiser4_node_t *reiser4_node_right(reiser4_node_t *node) {
	aal_assert("umka-1510", node != NULL, return NULL);

	if (!node->parent)
		return NULL;
    
	if (!node->right) {
		aal_assert("umka-1630", node->tree != NULL, return NULL);
		node->right = reiser4_node_rnn(node);
	}
    
	return node->right;
}

/*
  Connects child into sorted children list of specified node. Sets up the both
  neighbours and parent pointer.
*/
errno_t reiser4_node_attach(
	reiser4_node_t *node,	       /* node child will be connected to */
	reiser4_node_t *child)	       /* child node to be attached */
{
	aal_assert("umka-561", node != NULL, return -1);
	aal_assert("umka-564", child != NULL, return -1);

	if (reiser4_node_register(node, child))
		return -1;
	
	child->left = reiser4_node_lnn(child);
	child->right = reiser4_node_rnn(child);
	
	return 0;
}

static void reiser4_node_unregister(
	reiser4_node_t *node,	/* node child will be detached from */
	reiser4_node_t *child)	/* pointer to child to be deleted */
{
	aal_list_t *next;
	
	if (!node->children)
		return;
    
	child->parent = NULL;
    
	/* Updating node children list */
	next = aal_list_remove(node->children, child);
	
	if (!next || !next->prev)
		node->children = next;

	if (node->tree && aal_lru_detach(node->tree->lru, (void *)child))
		aal_exception_warn("Can't detach node from tree lru.");
}

/*
  Remove specified childern from the node. Updates all neighbour pointers and
  parent pointer.
*/
void reiser4_node_detach(
	reiser4_node_t *node,	/* node child will be detached from */
	reiser4_node_t *child)	/* pointer to child to be deleted */
{
	aal_list_t *next;
	
	aal_assert("umka-562", node != NULL, return);
	aal_assert("umka-563", child != NULL, return);

	if (child->left) {
		child->left->right = NULL;
		child->left = NULL;
	}
	
	if (child->right) {
		child->right->left = NULL;
		child->right = NULL;
	}

	reiser4_node_unregister(node, child);
}

bool_t reiser4_node_confirm(reiser4_node_t *node) {
	aal_assert("umka-123", node != NULL, return 0);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   confirm, node->entity);
}

/* 
   This function makes search inside specified node for passed key. Position
   will eb stored in passed @pos.
*/
int reiser4_node_lookup(
	reiser4_node_t *node,	/* node to be grepped */
	reiser4_key_t *key,	/* key to be find */
	rpos_t *pos)	/* found pos will be stored here */
{
	int result;

	item_entity_t *item;
	reiser4_key_t maxkey;
	reiser4_coord_t coord;
    
	aal_assert("umka-475", pos != NULL, return -1);
	aal_assert("vpf-048", node != NULL, return -1);
	aal_assert("umka-476", key != NULL, return -1);

	pos->item = 0;
	pos->unit = ~0ul;

	if (reiser4_node_items(node) == 0)
		return 0;
   
	/* Calling node plugin */
	if ((result = plugin_call(node->entity->plugin->node_ops,
				  lookup, node->entity, key, pos)) == -1) 
	{
		aal_exception_error("Lookup in the node %llu failed.",
				    node->blk);
		return -1;
	}

	if (result == 1)
		return 1;

	/* Initializing item coord points to */
	if (reiser4_coord_open(&coord, node, pos)) {
		aal_exception_error("Can't open item by coord. Node "
				    "%llu, item %u.", node->blk, pos->item);
		return -1;
	}

	item = &coord.item;

	/*
	  We are on the position where key is less then wanted. Key could lies
	  within the item or after the item.
	*/
		
	if (reiser4_item_max_poss_key(&coord, &maxkey))
		return -1;

	if (reiser4_key_compare(key, &maxkey) > 0) {
		pos->item++;
		return 0;
	}
	
	/* Calling lookup method of found item (most probably direntry item) */
	if (!item->plugin->item_ops.lookup)
		return 0;

	if ((result = item->plugin->item_ops.lookup(item, key, &pos->unit)) == -1) {
		aal_exception_error("Lookup in the item %d in the node %llu failed.", 
				    pos->item, node->blk);
		return -1;
	}

	return result;
}

/* Returns real item count in specified node */
uint32_t reiser4_node_items(reiser4_node_t *node) {
	aal_assert("umka-453", node != NULL, return 0);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   items, node->entity);
}
/* Returns free space of specified node */
uint16_t reiser4_node_space(reiser4_node_t *node) {
	aal_assert("umka-455", node != NULL, return 0);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   space, node->entity);
}

/* Returns overhead of specified node */
uint16_t reiser4_node_overhead(reiser4_node_t *node) {
	aal_assert("vpf-066", node != NULL, return 0);

	return plugin_call(node->entity->plugin->node_ops, 
			   overhead, node->entity);
}

/* Returns item max size from in specified node */
uint16_t reiser4_node_maxspace(reiser4_node_t *node) {
	aal_assert("umka-125", node != NULL, return 0);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   maxspace, node->entity);
}

/* Checks node for validness */
errno_t reiser4_node_valid(
	reiser4_node_t *node)	/* node to be checked */
{
	aal_assert("umka-123", node != NULL, return -1);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   valid, node->entity);
}

uint8_t reiser4_node_level(
	reiser4_node_t *node)
{
	aal_assert("umka-1642", node != NULL, return -1);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   level, node->entity);
}

#ifndef ENABLE_COMPACT

/*
  Makes shift of some amount of items and units into passed neighbour. Shift
  direction and other flags are passed by @hint. Returns operation error code.
*/
errno_t reiser4_node_shift(
	reiser4_node_t *node,
	reiser4_node_t *neig,
	shift_hint_t *hint)
{
	int retval;
	uint32_t i, items;
	rpos_t ppos;
	reiser4_key_t lkey;
	reiser4_plugin_t *plugin;
    
	aal_assert("umka-1225", node != NULL, return -1);
	aal_assert("umka-1226", neig != NULL, return -1);
	aal_assert("umka-1227", hint != NULL, return -1);
    
	aal_assert("umka-1258", reiser4_node_items(node) > 0, return -1);

	/*
	  Saving node position in parent. It will be used bellow for updating
	  left delemiting key.
	*/
	if (hint->flags & SF_LEFT) {
		if (node->parent) {
			if (reiser4_node_pos(node, &ppos)) {
				aal_exception_error("Can't find node %llu in "
						    "its parent node.", node->blk);
				return -1;
			}
		}
	} else {
		if (neig->parent) {
			if (reiser4_node_pos(neig, &ppos)) {
				aal_exception_error("Can't find node %llu in "
						    "its parent node.", neig->blk);
				return -1;
			}
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
	
	retval = plugin_call(plugin->node_ops, shift,
			     node->entity, neig->entity, hint);

	if (retval < 0)
		return retval;

	/* Updating left delimiting keys in the tree */
	if (hint->flags & SF_LEFT) {
		
		if (reiser4_node_items(node) != 0 &&
		    (hint->items > 0 || hint->units > 0))
		{
			reiser4_node_mkdirty(node);
			
			if (node->parent) {
				if (reiser4_node_lkey(node, &lkey))
					return -1;
				
				if (reiser4_node_ukey(node->parent, &ppos, &lkey))
					return -1;
			}
		}
	} else {
		if (hint->items > 0 || hint->units > 0) {
			reiser4_node_mkdirty(neig);
			
			if (neig->parent) {
				if (reiser4_node_lkey(neig, &lkey))
					return -1;
				
				if (reiser4_node_ukey(neig->parent, &ppos, &lkey))
					return -1;
			}
		}
	}

	/* We do not need update children lists if we are on leaf level */
	if (reiser4_node_level(node) <= LEAF_LEVEL)
		return 0;

	/* Updating children lists in node and its neighbour */
	items = reiser4_node_items(neig);
	
	for (i = 0; i < hint->items; i++) {
		uint32_t units;
		
		reiser4_coord_t coord;
		reiser4_node_t *child;
		reiser4_ptr_hint_t ptr;

		ppos.unit = ~0ul;
		ppos.item = hint->flags & SF_LEFT ? items - i - 1 : i;

		if (reiser4_coord_open(&coord, neig, &ppos))
			return -1;

		units = reiser4_item_units(&coord);
		
		if (!reiser4_item_nodeptr(&coord))
			continue;

		for (ppos.unit = 0; ppos.unit < units; ppos.unit++) {
			
			plugin_call(coord.item.plugin->item_ops,
				    fetch, &coord.item, &ptr, ppos.unit, 1);
			
			if (!(child = reiser4_node_cbp(node, ptr.ptr)))
				continue;

			reiser4_node_unregister(node, child);

			if (reiser4_node_register(neig, child))
				return -1;
		}

	}
	
	return 0;
}

/*
  Synchronizes passed @node by means of using resursive walk though the all
  children. There is possible to pass as parameter of this function the root
  node pointer. In this case the whole tree will be flushed onto device, tree
  lies on.
*/
errno_t reiser4_node_sync(
	reiser4_node_t *node)	/* node to be synchronized */
{
	aal_assert("umka-124", node != NULL, return 0);
	aal_assert("umka-1781", node->tree != NULL, return 0);
    
	/* Synchronizing passed @node */
	if (reiser4_node_isdirty(node)) {

		if (plugin_call(node->entity->plugin->node_ops,
				sync, node->entity))
		{
			aal_device_t *device = node->device;
			aal_exception_error("Can't synchronize node %llu to "
					    "device. %s.", node->blk, device->error);

			return -1;
		}

		reiser4_node_mkclean(node);
	}
    
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

	return 0;
}

/*
  Updates node keys in recursive maner (needed for updating ldkeys on the all
  levels of tre tree).
*/
errno_t reiser4_node_ukey(reiser4_node_t *node,
			  rpos_t *pos,
			  reiser4_key_t *key)
{
	rpos_t ppos;
	reiser4_coord_t coord;
    
	aal_assert("umka-999", node != NULL, return -1);
	aal_assert("umka-1000", pos != NULL, return -1);
	aal_assert("umka-1001", key != NULL, return -1);
    
	aal_assert("umka-1002", reiser4_node_items(node) > 0, return -1);

	if (pos->item == 0 && (pos->unit == ~0ul || pos->unit == 0)) {
		if (node->parent) {
			if (reiser4_node_pos(node, &ppos))
				return -1;
	    
			if (reiser4_node_ukey(node->parent, &ppos, key))
				return -1;
		}
	}
    
	if (reiser4_coord_open(&coord, node, pos))
		return -1;

	if (reiser4_item_set_key(&coord, key))
		return -1;
    
	reiser4_node_mkdirty(node);
	
	return 0;
}

/* 
   Inserts item or unit into node. Keeps track of changes of the left delimiting
   keys.
*/
errno_t reiser4_node_insert(
	reiser4_node_t *node,	            /* node item will be inserted in */
	rpos_t *pos,	    	    /* pos item will be inserted at */
	reiser4_item_hint_t *hint)	    /* item hint to be inserted */
{
	errno_t ret;
	rpos_t ppos;
    
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
	    
			reiser4_coord_init(&coord, node, pos);
			
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
	if ((ret = plugin_call(node->entity->plugin->node_ops,
			       insert, node->entity, pos, hint)))
		return ret;
	
	/* Updating ldkey in parent node */
	if (pos->item == 0 && (pos->unit == 0 || pos->unit == ~0ul)) {
		if (node->parent) {
			if (reiser4_node_ukey(node->parent, &ppos, &hint->key))
				return -1;
		}
	}

	reiser4_node_mkdirty(node);
	
	return 0;
}

/* Inserts/overwrites some amount of items/units */
errno_t reiser4_node_write(
	reiser4_node_t *dst_node,               /* destination node */
	rpos_t *dst_pos,                 /* destination pos */
	reiser4_node_t *src_node,               /* source node */
	rpos_t *src_pos,                 /* source pos */
	uint32_t count)
{
	return -1;
}

/* Removes some amount of item/units */
errno_t reiser4_node_cut(
	reiser4_node_t *node,	            /* node item will be removed from */
	rpos_t *start,		    /* start item will be removed at */
	rpos_t *end)		    /* end item will be removed at */
{
	rpos_t ppos;
	
	aal_assert("umka-1785", node != NULL, return -1);
	aal_assert("umka-1786", start != NULL, return -1);
	aal_assert("umka-1787", end != NULL, return -1);

	if (start->item == 0 && node->parent) {
		if (reiser4_node_pos(node, &ppos))
			return -1;
	}
	
	if (plugin_call(node->entity->plugin->node_ops, cut, node->entity,
			start, end))
	{
		aal_exception_error("Can't cut items/units from the node "
				    "%llu. Start: (%lu, %lu), end: (%lu, %lu).",
				    node->blk, start->item, start->unit,
				    end->item, end->unit);
		return -1;
	}

	reiser4_node_mkdirty(node);
	
	if (start->item == 0 && node->parent) {
		reiser4_key_t lkey;
		
		if (reiser4_node_lkey(node, &lkey))
			return -1;
		
		if (reiser4_node_ukey(node->parent, &ppos, &lkey))
			return -1;
	}

	return 0;
}

/* 
   Deletes item or unit from cached node. Keeps track of changes of the left
   delimiting key.
*/
errno_t reiser4_node_remove(
	reiser4_node_t *node,	            /* node item will be removed from */
	rpos_t *pos)		    /* pos item will be removed at */
{
	int update;
	rpos_t ppos;
	reiser4_coord_t coord;

	aal_assert("umka-993", node != NULL, return -1);
	aal_assert("umka-994", pos != NULL, return -1);

	/*
	  Update parent node will be performed in the case we are going to
	  remove the leftmost item or the leftmost unit of the leftmost item.
	*/
	update = (pos->item == 0 && (pos->unit == 0 ||
				     pos->unit == ~0ul));
	
	if (update && node->parent) {
		if (reiser4_node_pos(node, &ppos))
			return -1;
	}

	/* Initializing the coord of the item/unit we are going to remove */
	if (reiser4_coord_open(&coord, node, pos))
		return -1;

	/* 
	   Updating list of childrens of modified node in the case we modifying
	   an internal node.
	*/
	if (node->children) {
		reiser4_node_t *child;

		if (reiser4_item_get_key(&coord, NULL))
			return -1;

		if ((child = reiser4_node_cbk(node, &coord.item.key)))
			reiser4_node_detach(node, child);
	}

	/*
	  Removing item or unit. We assume that we are going to remove unit if
	  unit component is setted up.
	*/
	if (plugin_call(node->entity->plugin->node_ops, remove, node->entity, pos))
		return -1;

	/* Updating left deleimiting key in all parent nodes */
	if (update && node->parent) {
		if (reiser4_node_items(node) > 0) {
			reiser4_key_t lkey;
			reiser4_node_lkey(node, &lkey);
				
			if (reiser4_node_ukey(node->parent, &ppos, &lkey))
				return -1;
		} else {
			/* 
			   Removing cached node from the tree in the case it has
			   not items anymore.
			*/
			if (reiser4_node_remove(node->parent, &ppos))
				return -1;
		}
	}

	reiser4_node_mkdirty(node);
	
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
	rpos_t *pos = &coord.pos;
	reiser4_node_t *child = NULL;
 
	aal_assert("vpf-418", hint != NULL, return -1);
	aal_assert("vpf-390", node != NULL, return -1);

	reiser4_node_lock(node);

	if ((before_func && (result = before_func(node, hint->data))))
		goto error;

	for (pos->item = 0; pos->item < reiser4_node_items(node); pos->item++) {
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

		if (!reiser4_item_nodeptr(&coord))
			continue;

		/* The loop though the units of the current item */
		for (pos->unit = 0; pos->unit < reiser4_item_units(&coord); pos->unit++) {
			reiser4_ptr_hint_t ptr;

			/* Fetching node ptr */
			plugin_call(coord.item.plugin->item_ops, fetch,
				    &coord.item, &ptr, pos->unit, 1);
		
			if (ptr.ptr != INVAL_BLK && ptr.ptr != 0) {
				child = NULL;
					
				if (setup_func && (result = setup_func(&coord, hint->data)))
					goto error_after_func;

				if (!open_func)
					goto update;

				if (!(child = reiser4_node_cbp(node, ptr.ptr))) {
						
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
				    !reiser4_node_locked(child) && child->data)
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

	reiser4_node_unlock(node);
	return result;

 error_free_child:
	
	if (hint->cleanup && !child->children &&
	    !reiser4_node_locked(child) && child->data)
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
	reiser4_node_unlock(node);
	return result;
}

#endif

uint32_t reiser4_node_get_make_stamp(reiser4_node_t *node) {
	aal_assert("vpf-562", node != NULL, return 0);

	
	if (node->entity->plugin->node_ops.get_make_stamp)
		return node->entity->plugin->node_ops.get_make_stamp(
				node->entity);
	
	return 0;
}

void reiser4_node_set_make_stamp(reiser4_node_t *node, uint32_t stamp) {
	aal_assert("vpf-646", node != NULL, return);

	if (node->entity->plugin->node_ops.set_make_stamp)
		node->entity->plugin->node_ops.set_make_stamp(node->entity, 
							      stamp);
}

uint64_t reiser4_node_get_flush_stamp(reiser4_node_t *node) {
	aal_assert("vpf-647", node != NULL, return 0);

	if (node->entity->plugin->node_ops.get_flush_stamp)
		node->entity->plugin->node_ops.get_flush_stamp(node->entity);

	return 0;
}

void reiser4_node_set_flush_stamp(reiser4_node_t *node, uint64_t stamp) {
	aal_assert("vpf-648", node != NULL, return);

	if (node->entity->plugin->node_ops.get_flush_stamp)
		node->entity->plugin->node_ops.set_flush_stamp(node->entity, 
							       stamp);
}
