/*
  node.c -- the personalization of the reiser4 on-disk node. The libreiser4
  internal in-memory tree consists of reiser4_node_t instances.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <reiser4/reiser4.h>

#ifndef ENABLE_ALONE

/* Creates node instance based on passed device and block number */
reiser4_node_t *reiser4_node_create(
	aal_device_t *device,	/* device new node will be created on*/
	blk_t blk,		/* block new node will be created on */
	rpid_t pid,		/* node plugin id to be used */
	uint8_t level)		/* node level */
{
	reiser4_node_t *node;
	reiser4_plugin_t *plugin;

	aal_assert("umka-1268", device != NULL);
    
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
	aal_assert("umka-1537", node != NULL);
	aal_assert("umka-1538", stream != NULL);
	
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

	aal_assert("umka-160", device != NULL);
   
	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;
   
	if (!(node->entity = reiser4_node_guess(device, blk)))
		goto error_free_node;
    
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
	aal_assert("umka-824", node != NULL);
	aal_assert("umka-903", node->entity != NULL);
    
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
		reiser4_node_disconnect(node->parent, node);
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
	aal_assert("umka-1761", node != NULL);
	aal_assert("umka-1762", node->entity != NULL);

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

#ifndef ENABLE_ALONE
	if (reiser4_node_isdirty(node)) {
		if (reiser4_node_sync(node)) {
			aal_exception_error("Can't write node %llu.",
					    node->blk);
			return -1;
		}
	}
#endif
	
	/* Detaching node from the tree */
	if (node->parent) {
		reiser4_node_disconnect(node->parent, node);
		node->parent = NULL;
	}
	
	/* Uninitializing all fields */
	if (node->left)
		node->left->right = NULL;
    
	if (node->right)
		node->right->left = NULL;
    
	node->left = NULL;
	node->right = NULL;

	/* Calling node pluign close method to finilize node entity */
	plugin_call(node->entity->plugin->node_ops, close, node->entity);
   
	aal_free(node);
	return 0;
}

/* Getting the left delimiting key */
errno_t reiser4_node_lkey(
	reiser4_node_t *node,	         /* node for working with */
	reiser4_key_t *key)	         /* key will be stored here */
{
	reiser4_place_t place;
	rpos_t pos = {0, ~0ul};

	aal_assert("umka-753", node != NULL);
	aal_assert("umka-754", key != NULL);

	if (reiser4_place_open(&place, node, &pos))
		return -1;

	if (reiser4_item_get_key(&place, key))
		return -1;
	
	return 0;
}

/* Returns position of passed node in parent node */
errno_t reiser4_node_pos(
	reiser4_node_t *node,	        /* node position will be obtained for */
	rpos_t *pos)		        /* pointer result will be stored in */
{
	reiser4_key_t lkey;
	reiser4_key_t parent_key;
    
	aal_assert("umka-869", node != NULL);
 
	if (!node->parent)
		return -1;

	reiser4_node_lkey(node, &lkey);
    
	if (reiser4_node_lookup(node->parent, &lkey, &node->pos) != 1)
		return -1;

	if (pos != NULL)
	    *pos = node->pos;
    
	return 0;
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
	reiser4_node_t *node1;
	reiser4_node_t *node2;
	
	uint32_t items1, items2;
	reiser4_key_t lkey1, lkey2;

	node1 = (reiser4_node_t *)item1;
	node2 = (reiser4_node_t *)item2;

	/*
	  FIXME-UMKA: Is it correct? Should we consider that empty nodes may
	  exist in children list.
	*/
	items1 = reiser4_node_items(node1);
	items2 = reiser4_node_items(node2);

	if (items1 == 0 || items2 == 0)
		return -1;
    
	reiser4_node_lkey(node1, &lkey1);
	reiser4_node_lkey(node2, &lkey2);
    
	return reiser4_key_compare(&lkey1, &lkey2);
}

errno_t reiser4_node_connect(reiser4_node_t *node,
			     reiser4_node_t *child)
{
	aal_list_t *current;

	aal_assert("umka-1758", node != NULL);
	aal_assert("umka-1759", child != NULL);
	aal_assert("umka-1884", reiser4_node_items(child) > 0);
	
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

/* Removes passed @child from children list of @node */
errno_t reiser4_node_disconnect(
	reiser4_node_t *node,	/* node child will be detached from */
	reiser4_node_t *child)	/* pointer to child to be deleted */
{
	aal_list_t *next;
	
	if (!node->children)
		return -1;
    
	child->parent = NULL;
    
	/* Updating node children list */
	next = aal_list_remove(node->children, child);
	
	if (!next || !next->prev)
		node->children = next;

	if (node->tree && aal_lru_detach(node->tree->lru, (void *)child))
		return -1;

	return 0;
}

bool_t reiser4_node_confirm(reiser4_node_t *node) {
	aal_assert("umka-123", node != NULL);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   confirm, node->entity);
}

/* 
  This function makes search inside specified node for passed key. Position will
  eb stored in passed @pos.
*/
int reiser4_node_lookup(
	reiser4_node_t *node,	/* node to be grepped */
	reiser4_key_t *key,	/* key to be find */
	rpos_t *pos)	        /* found pos will be stored here */
{
	int result;

	item_entity_t *item;
	reiser4_key_t maxkey;
	reiser4_place_t place;
    
	aal_assert("umka-475", pos != NULL);
	aal_assert("vpf-048", node != NULL);
	aal_assert("umka-476", key != NULL);

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

	/* Initializing item place points to */
	if (reiser4_place_open(&place, node, pos)) {
		aal_exception_error("Can't open item by place. Node "
				    "%llu, item %u.", node->blk, pos->item);
		return -1;
	}

	item = &place.item;

	/*
	  We are on the position where key is less then wanted. Key could lies
	  within the item or after the item.
	*/
		
	if (reiser4_item_maxposs_key(&place, &maxkey))
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
	aal_assert("umka-453", node != NULL);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   items, node->entity);
}
/* Returns free space of specified node */
uint16_t reiser4_node_space(reiser4_node_t *node) {
	aal_assert("umka-455", node != NULL);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   space, node->entity);
}

/* Returns overhead of specified node */
uint16_t reiser4_node_overhead(reiser4_node_t *node) {
	aal_assert("vpf-066", node != NULL);

	return plugin_call(node->entity->plugin->node_ops, 
			   overhead, node->entity);
}

/* Returns item max size from in specified node */
uint16_t reiser4_node_maxspace(reiser4_node_t *node) {
	aal_assert("umka-125", node != NULL);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   maxspace, node->entity);
}

/* Checks node for validness */
errno_t reiser4_node_valid(
	reiser4_node_t *node)	/* node to be checked */
{
	aal_assert("umka-123", node != NULL);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   valid, node->entity);
}

#ifndef ENABLE_ALONE

/* Makes copy @count items from @src_node into @dst_node */
errno_t reiser4_node_copy(reiser4_node_t *dst_node, rpos_t *dst_pos,
			  reiser4_node_t *src_node, rpos_t *src_pos,
			  uint32_t count)
{
	aal_assert("umka-1819", src_node != NULL);
	aal_assert("umka-1821", dst_node != NULL);
	aal_assert("umka-1820", src_pos != NULL);
	aal_assert("umka-1822", dst_pos != NULL);

	return plugin_call(src_node->entity->plugin->node_ops, copy,
			   src_node->entity, src_pos, dst_node->entity,
			   dst_pos, count);
}

errno_t reiser4_node_expand(reiser4_node_t *node, rpos_t *pos,
			    uint32_t len, uint32_t count)
{
	aal_assert("umka-1815", node != NULL);
	aal_assert("umka-1816", pos != NULL);

	return plugin_call(node->entity->plugin->node_ops, expand,
			   node->entity, pos, len, count);
}

errno_t reiser4_node_shrink(reiser4_node_t *node, rpos_t *pos,
			    uint32_t len, uint32_t count)
{
	aal_assert("umka-1817", node != NULL);
	aal_assert("umka-1818", pos != NULL);

	return plugin_call(node->entity->plugin->node_ops, shrink,
			   node->entity, pos, len, count);
}

/*
  Makes shift of some amount of items and units into passed neighbour. Shift
  direction and other flags are passed by @hint. Returns operation error code.
*/
errno_t reiser4_node_shift(
	reiser4_node_t *node,
	reiser4_node_t *neig,
	shift_hint_t *hint)
{
	reiser4_plugin_t *plugin;
    
	aal_assert("umka-1225", node != NULL);
	aal_assert("umka-1226", neig != NULL);
	aal_assert("umka-1227", hint != NULL);

	/*
	  Trying shift something from @node into @neig. As result insert point
	  may be shifted to.
	*/
	plugin = node->entity->plugin;
	
	return plugin_call(plugin->node_ops, shift, node->entity,
			   neig->entity, hint);
}

/*
  Saves passed @node by means of using resursive walk though the all children.
  There is possible to pass as parameter of this function the root node
  pointer. In this case the whole tree will be flushed onto device, tree lies
  on.
*/
errno_t reiser4_node_sync(
	reiser4_node_t *node)	/* node to be synchronized */
{
	aal_assert("umka-124", node != NULL);
    
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
	
		aal_list_foreach_forward(node->children, walk) {
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
	reiser4_place_t place;
    
	aal_assert("umka-999", node != NULL);
	aal_assert("umka-1000", pos != NULL);
	aal_assert("umka-1001", key != NULL);

	if (reiser4_place_open(&place, node, pos))
		return -1;

	if (reiser4_item_set_key(&place, key))
		return -1;
    
	reiser4_node_mkdirty(node);
	return 0;
}

/*
  Updates children in-parent position. It is used durring internal nodes
  modifying.
*/
static errno_t reiser4_node_uchildren(reiser4_node_t *node,
				      rpos_t *start)
{
	rpos_t pos;
	reiser4_place_t item;

	reiser4_ptr_hint_t ptr;
	aal_list_t *walk = NULL;
	aal_list_t *list = NULL;

	aal_assert("umka-1887", node != NULL);
	aal_assert("umka-1888", start != NULL);
	
	if (node->children == NULL)
		return 0;

	POS_INIT(&pos, start->item, 0);

	/* Getting nodeptr item */
	for (; pos.item < reiser4_node_items(node); pos.item++) {
		if (reiser4_place_open(&item, node, start))
			return -1;

		if (reiser4_item_branch(&item))
			break;
	}
	
	if (!reiser4_item_branch(&item))
		return 0;

	for (; pos.item < reiser4_node_items(node); pos.item++) {
		plugin_call(item.item.plugin->item_ops, read,
			    &item.item, &ptr, pos.unit, 1);
	
		if ((list = aal_list_find_custom(node->children, (void *)&ptr.ptr,
						 callback_comp_blk, NULL)))
			break;
	}

	if (!list)
		return 0;
	
	aal_list_foreach_forward(list, walk) {
		reiser4_node_t *child = (reiser4_node_t *)walk->data;

		aal_assert("umka-1886", child->parent == node);

		if (reiser4_node_pos(child, &child->pos))
			return -1;
	}
	
	return 0;
}

/* 
  Inserts item or unit into node. Keeps track of changes of the left delimiting
  keys in all parent nodes.
*/
errno_t reiser4_node_insert(
	reiser4_node_t *node,	         /* node item will be inserted in */
	rpos_t *pos,                     /* pos item will be inserted at */
	reiser4_item_hint_t *hint)	 /* item hint to be inserted */
{
	errno_t res;
	uint32_t needed;
	uint32_t maxspace;
    
	aal_assert("umka-990", node != NULL);
	aal_assert("umka-991", pos != NULL);
	aal_assert("umka-992", hint != NULL);

#ifdef ENABLE_DEBUG
	maxspace = reiser4_node_maxspace(node);

	aal_assert("umka-761", hint->len > 0 &&
		   hint->len < maxspace);
#endif

	needed = hint->len + (pos->unit == ~0ul ?
			      reiser4_node_overhead(node) : 0);
	
	/* Checking if item length is gretter then free space in node */
	if (needed > reiser4_node_space(node)) {
		aal_exception_error("There is no space to insert new "
				    "item/unit of (%u) size in the node "
				    "(%llu).", hint->len, node->blk);
		return -1;
	}

	/* 
	  Inserting new item or pasting unit into one existent item pointed by
	  pos->item.
	*/
	if ((res = plugin_call(node->entity->plugin->node_ops,
			       insert, node->entity, pos, hint)))
		return res;

	if (reiser4_node_uchildren(node, pos))
		return -1;
	
	reiser4_node_mkdirty(node);
	
	return 0;
}

/* Inserts/overwrites some amount of items/units */
errno_t reiser4_node_write(
	reiser4_node_t *dst_node,        /* destination node */
	rpos_t *dst_pos,                 /* destination pos */
	reiser4_node_t *src_node,        /* source node */
	rpos_t *src_pos,                 /* source pos */
	uint32_t count)
{
	aal_exception_error("Sorry, not implemented yet!");
	return -1;
}

/* Removes some amount of item/units */
errno_t reiser4_node_cut(
	reiser4_node_t *node,	         /* node item will be removed from */
	rpos_t *start,		         /* start item will be removed at */
	rpos_t *end)		         /* end item will be removed at */
{
	aal_assert("umka-1785", node != NULL);
	aal_assert("umka-1786", start != NULL);
	aal_assert("umka-1787", end != NULL);

	if (plugin_call(node->entity->plugin->node_ops,
			cut, node->entity, start, end))
	{
		aal_exception_error("Can't cut items/units from the node "
				    "%llu. Start: (%lu, %lu), end: (%lu, %lu).",
				    node->blk, start->item, start->unit,
				    end->item, end->unit);
		return -1;
	}

	/* Updating children */
	if (reiser4_node_uchildren(node, end))
		return -1;
	
	reiser4_node_mkdirty(node);
	
	return 0;
}

/* 
  Deletes item or unit from cached node. Keeps track of changes of the left
  delimiting key.
*/
errno_t reiser4_node_remove(
	reiser4_node_t *node,	            /* node item will be removed from */
	rpos_t *pos,                        /* pos item will be removed at */
	uint32_t count)                     /* the number of item/units */
{
	aal_assert("umka-993", node != NULL);
	aal_assert("umka-994", pos != NULL);

	/*
	  Removing item or unit. We assume that we are going to remove unit if
	  unit component is set up.
	*/
	if (plugin_call(node->entity->plugin->node_ops,
			remove, node->entity, pos, count))
	{
		aal_exception_error("Can't remove %lu items/units from "
				    "node %llu.", count, node->blk);
		return -1;
	}

	/* Updating children */
	if (node->children) {
		if (reiser4_node_uchildren(node, pos))
			return -1;
	}
	
	reiser4_node_mkdirty(node);
	
	return 0;
}

/* This function traverse passed node */
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
	reiser4_place_t place;
	rpos_t *pos = &place.pos;
	reiser4_node_t *child = NULL;
 
	aal_assert("vpf-418", hint != NULL);
	aal_assert("vpf-390", node != NULL);

	reiser4_node_lock(node);

	if ((before_func && (result = before_func(node, hint->data))))
		goto error;

	for (pos->item = 0; pos->item < reiser4_node_items(node); pos->item++) {
		pos->unit = ~0ul; 

		/*
		  If there is a suspicion in a corruption, it must be checked in
		  before_func. All items must be opened here.
		*/
		if (reiser4_place_open(&place, node, pos)) {
			aal_exception_error("Can't open item by place. Node "
					    "%llu, item %u.", node->blk, pos->item);
			goto error_after_func;
		}

		if (!reiser4_item_branch(&place))
			continue;

		/* The loop though the units of the current item */
		for (pos->unit = 0; pos->unit < reiser4_item_units(&place); pos->unit++) {
			reiser4_ptr_hint_t ptr;

			/* Fetching node ptr */
			plugin_call(place.item.plugin->item_ops, read,
				    &place.item, &ptr, pos->unit, 1);
		
			if (ptr.ptr != INVAL_BLK && ptr.ptr != 0) {
				child = NULL;
					
				if (setup_func && (result = setup_func(&place, hint->data)))
					goto error_after_func;

				if (!open_func)
					goto update;

				if (!(child = reiser4_node_cbp(node, ptr.ptr))) {
						
					if ((result = open_func(&child, ptr.ptr, hint->data)))
						goto error_update_func;

					if (!child)
						goto update;

					child->data = (void *)1;
					
					if (reiser4_node_connect(node, child))
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
				if (update_func && (result = update_func(&place, hint->data)))
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
		result = update_func(&place, hint->data);
    
 error_after_func:
	if (after_func)
		result = after_func(node, hint->data);
    
 error:
	reiser4_node_unlock(node);
	return result;
}

void reiser4_node_set_mstamp(reiser4_node_t *node, uint32_t stamp) {
	aal_assert("vpf-646", node != NULL);

	if (node->entity->plugin->node_ops.set_mstamp)
		node->entity->plugin->node_ops.set_mstamp(node->entity, stamp);
}

void reiser4_node_set_fstamp(reiser4_node_t *node, uint64_t stamp) {
	aal_assert("vpf-648", node != NULL);

	if (node->entity->plugin->node_ops.get_fstamp) {
		node->entity->plugin->node_ops.set_fstamp(node->entity,
							  stamp);
	}
}

void reiser4_node_set_level(reiser4_node_t *node,
			    uint8_t level)
{
	aal_assert("umka-1863", node != NULL);
    
	plugin_call(node->entity->plugin->node_ops, 
		    set_level, node->entity, level);
}

#endif

uint32_t reiser4_node_get_mstamp(reiser4_node_t *node) {
	aal_assert("vpf-562", node != NULL);
	
	if (node->entity->plugin->node_ops.get_mstamp)
		return node->entity->plugin->node_ops.get_mstamp(
			node->entity);
	
	return 0;
}

uint64_t reiser4_node_get_fstamp(reiser4_node_t *node) {
	aal_assert("vpf-647", node != NULL);

	if (node->entity->plugin->node_ops.get_fstamp)
		node->entity->plugin->node_ops.get_fstamp(node->entity);

	return 0;
}

/* Returns node level */
uint8_t reiser4_node_get_level(reiser4_node_t *node) {
	aal_assert("umka-1642", node != NULL);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   get_level, node->entity);
}

