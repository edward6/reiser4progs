/*
  libreiser4.c -- version control functions and library initialization code.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef ENABLE_COMPACT
#  include <printf.h>
#endif

#include <aal/aal.h>
#include <reiser4/reiser4.h>

/* 
   Initializing the libreiser4 core instance. It will be passed into all plugins
   in otder to let them ability access libreiser4 methods such as insert or
   remove an item from the tree.
*/

/* Handler for plugin finding requests from all plugins */
static reiser4_plugin_t *factory_ifind(
	rpid_t type,		    /* needed type of plugin*/
	rpid_t id)		    /* needed plugin id */
{
	return libreiser4_factory_ifind(type, id);
}

/* Handler for plugin finding requests from all plugins */
static reiser4_plugin_t *factory_nfind(
	rpid_t type,		    /* needed type of plugin*/
	const char *name)	    /* needed plugin name (label) */
{
	return libreiser4_factory_nfind(type, name);
}

#ifndef ENABLE_COMPACT

/* Handler for item insert requests from the all plugins */
static errno_t tree_insert(
	const void *tree,	    /* opaque pointer to the tree */
	reiser4_place_t *place,	    /* insertion point will be saved here */
	reiser4_item_hint_t *item)  /* item hint to be inserted into tree */
{
	aal_assert("umka-846", tree != NULL, return -1);
	aal_assert("umka-847", item != NULL, return -1);
	aal_assert("umka-1643", place != NULL, return -1);
    
	return reiser4_tree_insert((reiser4_tree_t *)tree,
				   (reiser4_coord_t *)place, item);
}

/* Handler for item removing requests from the all plugins */
static errno_t tree_remove(
	const void *tree,	    /* opaque pointer to the tree */
	reiser4_place_t *place)	    /* coord of the item to be removerd */
{
	aal_assert("umka-848", tree != NULL, return -1);
	aal_assert("umka-849", place != NULL, return -1);
    
	return reiser4_tree_remove((reiser4_tree_t *)tree,
				   (reiser4_coord_t *)place);
}

#endif

/* Handler for lookup reqiests from the all plugin can arrive */
static int tree_lookup(
	const void *tree,	    /* opaque pointer to the tree */
	reiser4_key_t *key,	    /* key to be found */
	reiser4_level_t *level,	    /* stop level */
	reiser4_place_t *place)	    /* result will be stored in */
{
	int lookup;
	reiser4_coord_t *coord;
	
	aal_assert("umka-851", key != NULL, return -1);
	aal_assert("umka-850", tree != NULL, return -1);
	aal_assert("umka-852", place != NULL, return -1);

	if ((lookup = reiser4_tree_lookup((reiser4_tree_t *)tree, key, level,
					  (reiser4_coord_t *)place)) == FAILED)
		return lookup;
	
	coord = (reiser4_coord_t *)place;
	
	if (lookup == PRESENT) {
		item_entity_t *item = &coord->item;
		object_entity_t *entity = coord->node->entity;
		
		if (plugin_call(return -1, entity->plugin->node_ops, 
				get_key, entity, &coord->pos, &item->key))
		{
			aal_exception_error("Can't get item key.");
			return -1;
		}

		if (reiser4_key_guess(&item->key)) {
			aal_exception_error("Can't guess item key plugin.");
			return -1;
		}
	}

	return lookup;
}

/* Handler for requests for right neighbor */
static errno_t tree_right(
	const void *tree,	    /* opaque pointer to the tree */
	reiser4_place_t *place,     /* coord of node */
	reiser4_place_t *right)	    /* right neighbour will be here */
{
	reiser4_pos_t pos;
	reiser4_coord_t *coord;
    
	aal_assert("umka-867", tree != NULL, return -1);
	aal_assert("umka-868", place != NULL, return -1);
	aal_assert("umka-1491", right != NULL, return -1);

	coord = (reiser4_coord_t *)place;
		
	if (!reiser4_node_right(coord->node))
		return -1;
    
	pos.item = 0;
	pos.unit = ~0ul;
	
	if (reiser4_coord_open((reiser4_coord_t *)right,
			       coord->node->right, &pos))
		return -1;

	return reiser4_item_get_key((reiser4_coord_t *)right, NULL);
}

/* Handler for requests for left neighbor */
static errno_t tree_left(
	const void *tree,	    /* opaque pointer to the tree */
	reiser4_place_t *place,	    /* coord of node */
	reiser4_place_t *left)	    /* left neighbour will be here */
{
	reiser4_pos_t pos;
	reiser4_coord_t *coord;
	
	aal_assert("umka-867", tree != NULL, return -1);
	aal_assert("umka-868", place != NULL, return -1);
	aal_assert("umka-1492", left != NULL, return -1);

	coord = (reiser4_coord_t *)place;
	
	if (!reiser4_node_left(coord->node))
		return -1;
	
	pos.unit = ~0ul;
	pos.item = reiser4_node_items(coord->node->left) - 1;
	
	if (reiser4_coord_open((reiser4_coord_t *)left,
			       coord->node->left, &pos))
		return -1;

	return reiser4_item_get_key((reiser4_coord_t *)left, NULL);
}

static errno_t tree_lock(
	const void *tree,         /* tree for working on */
	reiser4_place_t *place)   /* coord to make lock on */
{
	reiser4_coord_t *coord;
	
	aal_assert("umka-1511", tree != NULL, return -1);
	aal_assert("umka-1512", place != NULL, return -1);

	coord = (reiser4_coord_t *)place;
	return reiser4_node_lock(coord->node);
}

static errno_t tree_unlock(
	const void *tree,         /* tree for working on */
	reiser4_place_t *place)   /* coord to make unlock on */
{
	reiser4_coord_t *coord;
	
	aal_assert("umka-1513", tree != NULL, return -1);
	aal_assert("umka-1514", place != NULL, return -1);

	coord = (reiser4_coord_t *)place;
	return reiser4_node_unlock(coord->node);
}

static uint32_t tree_blockspace(const void *tree) {
	aal_assert("umka-1220", tree != NULL, return 0);
	return ((reiser4_tree_t *)tree)->fs->device->blocksize;
}
	
static uint32_t tree_nodespace(const void *tree) {
	reiser4_node_t *root;
    
	aal_assert("umka-1220", tree != NULL, return 0);

	root = ((reiser4_tree_t *)tree)->root;
	return reiser4_node_maxspace(root) - reiser4_node_overhead(root);
}

reiser4_core_t core = {
	.factory_ops = {
		/* Installing callback for making search for a plugin by its
		 * type and id */
		.ifind = factory_ifind,
	
		/* Installing callback for making search for a plugin by its
		 * type and name */
		.nfind = factory_nfind
	},
    
	.tree_ops = {
	
		/* This one for lookuping the tree */
		.lookup	    = tree_lookup,

		/* Returns right neighbour of passed coord */
		.right	    = tree_right,
    
		/* Returns left neighbour of passed coord */
		.left	    = tree_left,

		/* Makes look and unlock of node specified by coord */
		.lock       = tree_lock,
		.unlock     = tree_unlock,

#ifndef ENABLE_COMPACT	
		/* Installing callback function for inserting items into the
		 * tree */
		.insert	    = tree_insert,

		/* Installing callback function for removing items from the
		 * tree */
		.remove	    = tree_remove,
#else
		.insert	    = NULL,
		.remove	    = NULL,
#endif
		.nodespace  = tree_nodespace,
		.blockspace = tree_blockspace
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

#ifndef ENABLE_COMPACT

/* Support for the %k occurences in the formated messages */
#define PA_REISER4_KEY  (PA_LAST)

static int arginfo_k(const struct printf_info *info, size_t n, int *argtypes) {
	if (n > 0)
		argtypes[0] = PA_REISER4_KEY | PA_FLAG_PTR;
    
	return 1;
}

static int print_key(FILE *file, const struct printf_info *info, 
		     const void *const *args) 
{
	int len;
	reiser4_key_t *key;
	aal_stream_t stream;

	aal_stream_init(&stream);
    
	key = *((reiser4_key_t **)(args[0]));
	reiser4_key_print(key, &stream);

	fprintf(file, (char *)stream.data);
    
	len = stream.offset;

	aal_stream_fini(&stream);
	return len;
}

#endif

/* 
   Initializes libreiser4 (plugin factory, etc). This function should be called
   before any actions performed on libreiser4.
*/
errno_t libreiser4_init(void) {
	if (libreiser4_factory_init()) {
		aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_OK, 
				    "Can't initialize plugin factory.");
		return -1;
	}

#ifndef ENABLE_COMPACT
	register_printf_function('k', print_key, arginfo_k);
#endif
    
	return 0;
	
 error_free_factory:
	libreiser4_factory_done();
	return -1;
}

/* Finalizes libreiser4 */
void libreiser4_done(void) {
	libreiser4_factory_done();
}

