/*
  libreiser4.c -- version control functions and library initialization code.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licencing governed by
  reiser4progs/COPYING.
*/

#include <reiser4/reiser4.h>
#include <printf.h>

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
	reiser4_item_hint_t *item,  /* item hint to be inserted into tree */
	uint8_t level,		    /* insert level */
	reiser4_place_t *place)	    /* insertion point will be saved here */
{
	aal_assert("umka-846", tree != NULL, return -1);
	aal_assert("umka-847", item != NULL, return -1);
    
	return reiser4_tree_insert((reiser4_tree_t *)tree, item, 
				   level, (reiser4_coord_t *)place);
}

/* Handler for item removing requests from the all plugins */
static errno_t tree_remove(
	const void *tree,	    /* opaque pointer to the tree */
	reiser4_key_t *key,	    /* key of the item to be removerd */
	uint8_t level)              /* stop level */
{
	aal_assert("umka-848", tree != NULL, return -1);
	aal_assert("umka-849", key != NULL, return -1);
    
	return reiser4_tree_remove((reiser4_tree_t *)tree, key, level);
}

#endif

/* Handler for lookup reqiests from the all plugin can arrive */
static int tree_lookup(
	const void *tree,	    /* opaque pointer to the tree */
	reiser4_key_t *key,	    /* key to be found */
	uint8_t level,		    /* stop level */
	reiser4_place_t *place)	    /* the same as reiser4_coord_t;result will be stored in */
{
	aal_assert("umka-851", key != NULL, return -1);
	aal_assert("umka-850", tree != NULL, return -1);
	aal_assert("umka-852", place != NULL, return -1);
    
	return reiser4_tree_lookup((reiser4_tree_t *)tree, key, level,
				   (reiser4_coord_t *)place);
}

/* Handler for requests for right neighbor */
static errno_t tree_right(
	const void *tree,	    /* opaque pointer to the tree */
	reiser4_place_t *place)	    /* coord of node right neighbor will be obtained for */
{
	reiser4_joint_t *joint;
	reiser4_coord_t *coord;
    
	aal_assert("umka-867", tree != NULL, return -1);
	aal_assert("umka-868", place != NULL, return -1);

	coord = (reiser4_coord_t *)place;
		
	if (coord->context != CT_JOINT) {
		aal_exception_error("Passed place has invalid context.");
		return -1;
	}
    
	joint = (reiser4_joint_t *)place->data; 
    
	/* Rasing from the device tree lies on both neighbors */
	if (reiser4_joint_realize(joint) || !joint->right)
		return -1;

	return reiser4_coord_open((reiser4_coord_t *)place, joint->right, CT_JOINT, &coord->pos);
}

/* Handler for requests for left neighbor */
static errno_t tree_left(
	const void *tree,	    /* opaque pointer to the tree */
	reiser4_place_t *place)	    /* coord of node left neighbor will be obtained for */
{
	reiser4_joint_t *joint;
	reiser4_coord_t *coord;
	
	aal_assert("umka-867", tree != NULL, return -1);
	aal_assert("umka-868", place != NULL, return -1);

	coord = (reiser4_coord_t *)place;
	
	if (coord->context != CT_JOINT) {
		aal_exception_error("Passed place has invalid context.");
		return -1;
	}
	
	joint = (reiser4_joint_t *)place->data;
    
	/* Rasing from the device tree lies on both neighbors */
	if (reiser4_joint_realize(joint) || !joint->left)
		return -1;

	return reiser4_coord_open((reiser4_coord_t *)place, joint->left, CT_JOINT, &coord->pos);
}

static uint32_t tree_blockspace(const void *tree) {
	aal_assert("umka-1220", tree != NULL, return 0);
	return aal_block_size(((reiser4_tree_t *)tree)->root->node->block);
}
	
static uint32_t tree_nodespace(const void *tree) {
	reiser4_node_t *node;
    
	aal_assert("umka-1220", tree != NULL, return 0);

	node = ((reiser4_tree_t *)tree)->root->node;
	return reiser4_node_maxspace(node) - reiser4_node_overhead(node);
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

static int print_key(FILE * stream, const struct printf_info *info, 
		     const void *const *args) 
{
	int len;
	char buffer[100];
	reiser4_key_t *key;

	aal_memset(buffer, 0, sizeof(buffer));
    
	key = *((reiser4_key_t **)(args[0]));
	reiser4_key_print(key, buffer, sizeof(buffer));

	fprintf(stream, "%s", buffer);
    
	return aal_strlen(buffer);
}

#endif

/* 
   Initializes libreiser4 (plugin factory, memory limit, etc). This function
   should be called before any actions performed on libreiser4.
*/
errno_t libreiser4_init(void) {
	if (libreiser4_factory_init()) {
		aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_OK, 
				    "Can't initialize plugin factory.");
		return -1;
	}
    
#ifndef ENABLE_COMPACT
	register_printf_function ('k', print_key, arginfo_k);
#endif
    
	return 0;
}

/* Finalizes libreiser4 */
void libreiser4_done(void) {
	libreiser4_factory_done();
}

