/*
  libreiser4.c -- version control functions, library initialization code and
  plugin-accessible library functions implemetation.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE
#  include <stdlib.h>
#endif

#include <aal/aal.h>
#include <reiser4/reiser4.h>

#ifndef ENABLE_STAND_ALONE

static void libreiser4_abort(char *message);
reiser4_abort_t abort_func = libreiser4_abort;

/*
  Default shutdown handler. It will be called when libreiser4 is determined that
  some function in a plugin is not implemented.
*/
static void libreiser4_abort(char *message) {
	aal_exception_fatal(message);
	
#ifndef ENABLE_STAND_ALONE
	exit(-1);
#endif
}

/* Sets/gets shutdown handler function */
reiser4_abort_t libreiser4_get_abort(void) {
	return abort_func;
}

void libreiser4_set_abort(reiser4_abort_t func) {
	if (!func)
		abort_func = libreiser4_abort;
	else
		abort_func = func;
}

#endif

/* 
  Initializing the libreiser4 core instance. It will be passed into all plugins
  in otder to let them ability access libreiser4 methods such as insert or
  remove an item from the tree.
*/

/* Handler for plugin finding requests from all plugins */
static reiser4_plugin_t *factory_ifind(
	rid_t type,		    /* needed type of plugin*/
	rid_t id)		    /* needed plugin id */
{
	return libreiser4_factory_ifind(type, id);
}

/* Handler for plugin finding requests from all plugins */
static reiser4_plugin_t *factory_nfind(
	rid_t type,		    /* needed type of plugin*/
	const char *name)	    /* needed plugin name (label) */
{
	return libreiser4_factory_nfind(name);
}

#ifndef ENABLE_STAND_ALONE

/* Handler for item insert requests from the all plugins */
static errno_t tree_insert(
	void *tree,	            /* opaque pointer to the tree */
	place_t *place,	            /* insertion point will be saved here */
	uint8_t level,              /* target level */
	reiser4_item_hint_t *item)  /* item hint to be inserted into tree */
{
	aal_assert("umka-846", tree != NULL);
	aal_assert("umka-847", item != NULL);
	aal_assert("umka-1643", place != NULL);

	return reiser4_tree_insert((reiser4_tree_t *)tree,
				   (reiser4_place_t *)place,
				   level, item);
}

/* Handler for item removing requests from the all plugins */
static errno_t tree_remove(
	void *tree,	            /* opaque pointer to the tree */
	place_t *place,	            /* place of the item to be removerd */
	uint32_t count)
{
	aal_assert("umka-848", tree != NULL);
	aal_assert("umka-849", place != NULL);
    
	return reiser4_tree_remove((reiser4_tree_t *)tree,
				   (reiser4_place_t *)place, count);
}

#endif

/* Handler for lookup reqiests from the all plugin can arrive */
static lookup_t tree_lookup(
	void *tree,	            /* opaque pointer to the tree */
	reiser4_key_t *key,	    /* key to be found */
	uint8_t stop,	            /* stop level */
	place_t *place)             /* result will be stored in */
{
	lookup_t res;
	reiser4_place_t *p;
	
	aal_assert("umka-851", key != NULL);
	aal_assert("umka-850", tree != NULL);
	aal_assert("umka-852", place != NULL);

	p = (reiser4_place_t *)place;
	
	if ((res = reiser4_tree_lookup((reiser4_tree_t *)tree,
				       key, stop, p)) == LP_FAILED)
		return res;
	
	if (res == LP_PRESENT) {
		
		item_entity_t *item = &p->item;
		object_entity_t *entity = p->node->entity;
		
		if (plugin_call(entity->plugin->node_ops, get_key,
				entity, &p->pos, &item->key))
		{
			aal_exception_error("Can't get item key.");
			return LP_FAILED;
		}

		if (reiser4_key_guess(&item->key)) {
			aal_exception_error("Can't guess item key plugin.");
			return LP_FAILED;
		}
	}

	return res;
}

/* Initializes item at passed @place */
static errno_t tree_realize(void *tree, place_t *place) {
	reiser4_place_t *p = (reiser4_place_t *)place;
	
	if (reiser4_place_realize(p))
		return -EINVAL;

	return reiser4_item_get_key(p, NULL);
}

/* Handler for requests for next item */
static errno_t tree_next(
	void *tree,	            /* opaque pointer to the tree */
	place_t *place,             /* place of node */
	place_t *next)	            /* next item will be stored here */
{
	reiser4_place_t *curr;
    
	aal_assert("umka-867", tree != NULL);
	aal_assert("umka-868", place != NULL);
	aal_assert("umka-1491", next != NULL);

	curr = (reiser4_place_t *)place;

	if (curr->pos.item < reiser4_node_items(curr->node) - 1) {

		reiser4_place_assign((reiser4_place_t *)next,
				     curr->node, curr->pos.item + 1, ~0ul);

		if (reiser4_place_realize((reiser4_place_t *)next))
			return -EINVAL;
	} else {
		reiser4_tree_right((reiser4_tree_t *)tree, curr->node);

		if (!curr->node->right)
			return -EINVAL;
		
		reiser4_place_assign((reiser4_place_t *)next,
				     curr->node->right, 0, ~0ul);

		if (reiser4_place_realize((reiser4_place_t *)next))
			return -EINVAL;
	}

	return reiser4_item_realize((reiser4_place_t *)next);
}

/* Handler for requests for left neighbor */
static errno_t tree_prev(
	void *tree,	            /* opaque pointer to the tree */
	place_t *place,             /* place of node */
	place_t *prev)              /* left neighbour will be here */
{
	reiser4_place_t *curr;
	
	aal_assert("umka-867", tree != NULL);
	aal_assert("umka-868", place != NULL);
	aal_assert("umka-1492", prev != NULL);

	curr = (reiser4_place_t *)place;

	if (curr->pos.item > 0) {

		reiser4_place_assign((reiser4_place_t *)prev,
				     curr->node, curr->pos.item - 1, ~0ul);

		if (reiser4_place_realize((reiser4_place_t *)prev))
			return -EINVAL;
	} else {
		uint32_t items;
		
		reiser4_tree_left((reiser4_tree_t *)tree, curr->node);

		if (!curr->node->left)
			return -EINVAL;
		
		items = reiser4_node_items(curr->node->left);
			
		reiser4_place_assign((reiser4_place_t *)prev,
				     curr->node->left, items - 1, ~0ul);

		if (reiser4_place_realize((reiser4_place_t *)prev))
			return -EINVAL;
	}

	return reiser4_item_realize((reiser4_place_t *)prev);
}

static errno_t tree_lock(
	void *tree,               /* tree for working on */
	place_t *place)           /* place to make lock on */
{
	reiser4_place_t *p;
	
	aal_assert("umka-1511", tree != NULL);
	aal_assert("umka-1512", place != NULL);

	p = (reiser4_place_t *)place;
	reiser4_node_lock(p->node);
	
	return 0;
}

static errno_t tree_unlock(
	void *tree,               /* tree for working on */
	place_t *place)           /* place to make unlock on */
{
	reiser4_place_t *p;
	
	aal_assert("umka-1513", tree != NULL);
	aal_assert("umka-1514", place != NULL);

	p = (reiser4_place_t *)place;	
	reiser4_node_unlock(p->node);

	return 0;
}

#ifndef ENABLE_STAND_ALONE

static uint32_t tree_blocksize(void *tree) {
	aal_assert("umka-1220", tree != NULL);
	return ((reiser4_tree_t *)tree)->fs->device->blocksize;
}

static uint32_t tree_maxspace(void *tree) {
	reiser4_node_t *root;
    
	aal_assert("umka-1220", tree != NULL);
	
	if (!(root = ((reiser4_tree_t *)tree)->root))
		return 0;
	
	return reiser4_node_maxspace(root);
}

#endif

static errno_t tree_rootkey(void *tree, key_entity_t *key) {
	key_entity_t *rootkey = &((reiser4_tree_t *)tree)->key;
	return reiser4_key_assign(key, rootkey);
}

reiser4_core_t core = {
	.tree_ops = {
	
		/* This one for lookuping the tree */
		.lookup	    = tree_lookup,

		/* This one for initializing an item at place */
		.realize    = tree_realize,

		/* Returns next item form the passed place */
		.next	    = tree_next,
    
		/* Returns prev item from the passed place */
		.prev	    = tree_prev,

#ifndef ENABLE_STAND_ALONE
		/* Callback function for inserting items into the tree */
		.insert	    = tree_insert,

		/* Callback function for removing items from the tree */
		.remove	    = tree_remove,
		
		.maxspace  = tree_maxspace,
		.blocksize = tree_blocksize,
#endif
		/* Makes look and unlock of node specified by place */
		.lock       = tree_lock,
		.unlock     = tree_unlock,
		
		.rootkey    = tree_rootkey
	},
	.factory_ops = {
		/*
		  Installing callback for making search for a plugin by its type
		  and id.
		*/
		.ifind = factory_ifind,
	
		/*
		  Installing callback for making search for a plugin by its type
		  and name.
		*/
		.nfind = factory_nfind
	}
};

/* Returns libreiser4 max supported interface version */
int libreiser4_max_interface_version(void) {
	return LIBREISER4_MAX_INTERFACE_VERSION;
}

/* Returns libreiser4 min supported interface version */
int libreiser4_min_interface_version(void) {
	return LIBREISER4_MIN_INTERFACE_VERSION;
}

/* Returns libreiser4 version */
const char *libreiser4_version(void) {
	return VERSION;
}

/* 
  Initializes libreiser4 (plugin factory, etc). This function should be called
  before any actions performed on libreiser4.
*/
errno_t libreiser4_init(void) {
	extern aal_list_t *plugins;
	
	plugins = NULL;
	
	if (libreiser4_factory_init()) {
		aal_exception_fatal("Can't initialize plugin factory.");
		return -EINVAL;
	}

#ifndef ENABLE_STAND_ALONE
	if (reiser4_print_init()) {
		aal_exception_error("Can't initialize print factory");
		goto error_free_factory;
	}
#endif
    
	return 0;
	
 error_free_factory:
	libreiser4_factory_fini();
	return -EINVAL;
}

/* Finalizes libreiser4 */
void libreiser4_fini(void) {
	libreiser4_factory_fini();
}
