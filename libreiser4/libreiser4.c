/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   libreiser4.c -- version control functions, library initialization code and
   plugin-accessible library functions implemetation. */

#include <aal/aal.h>
#include <reiser4/reiser4.h>

/* Handler for plugin lookup requests from all plugins */
static reiser4_plug_t *factory_ifind(
	rid_t type,		    /* needed type of plugin*/
	rid_t id)		    /* needed plugin id */
{
	return reiser4_factory_ifind(type, id);
}

#ifndef ENABLE_STAND_ALONE
/* Handler for plugin finding requests from all plugins */
static reiser4_plug_t *factory_nfind(char *name) {
	return reiser4_factory_nfind(name);
}

/* Handler for item insert requests from the all plugins */
static errno_t tree_insert(
	void *tree,	            /* opaque pointer to the tree */
	place_t *place,	            /* insertion point will be saved here */
	insert_hint_t *hint,        /* item hint to be inserted into tree */
	uint8_t level)              /* target level */
{
	aal_assert("umka-846", tree != NULL);
	aal_assert("umka-847", hint != NULL);
	aal_assert("umka-1643", place != NULL);

	return reiser4_tree_insert((reiser4_tree_t *)tree,
				   (reiser4_place_t *)place,
				   hint, level);
}

/* Handler for item removing requests from the all plugins */
static errno_t tree_remove(
	void *tree,	            /* opaque pointer to the tree */
	place_t *place,	            /* place of the item to be removerd */
	remove_hint_t *hint)
{
	aal_assert("umka-848", tree != NULL);
	aal_assert("umka-849", place != NULL);
    
	return reiser4_tree_remove((reiser4_tree_t *)tree,
				   (reiser4_place_t *)place,
				   hint);
}
#endif

/* Handler for lookup reqiests from the all plugin can arrive */
static lookup_res_t tree_lookup(
	void *tree,	            /* opaque pointer to the tree */
	reiser4_key_t *key,	    /* key to be found */
	uint8_t level,	            /* stop level */
	lookup_mod_t mode,          /* position correcting mode */
	place_t *place)             /* result will be stored in */
{
	reiser4_place_t *p;
	
	aal_assert("umka-851", key != NULL);
	aal_assert("umka-850", tree != NULL);
	aal_assert("umka-852", place != NULL);

	p = (reiser4_place_t *)place;
	
	return reiser4_tree_lookup((reiser4_tree_t *)tree,
				   key, level, mode, p);
}

/* Initializes item at passed @place */
static errno_t tree_fetch(void *tree, place_t *place) {
	return reiser4_place_fetch((reiser4_place_t *)place);
}

/* Returns TRUE if passed @place points to some real item in a node */
static int tree_valid(void *tree, place_t *place) {
	return reiser4_place_valid((reiser4_place_t *)place);
}

/* Handler for requests for next item */
static errno_t tree_next(
	void *tree,	            /* opaque pointer to the tree */
	place_t *place,             /* place of node */
	place_t *next)	            /* next item will be stored here */
{
	reiser4_tree_t *t;
	reiser4_place_t *p;
    
	aal_assert("umka-867", tree != NULL);
	aal_assert("umka-868", place != NULL);
	aal_assert("umka-1491", next != NULL);

	t = (reiser4_tree_t *)tree;
	p = (reiser4_place_t *)place;

	if (p->pos.item >= reiser4_node_items(p->node) - 1) {
		reiser4_tree_neigh(t, p->node, D_RIGHT);

		if (!p->node->right) {
			aal_memset(next, 0, sizeof(*next));
			return 0;
		}

		reiser4_place_assign((reiser4_place_t *)next,
				     p->node->right, 0, 0);
	} else {
		reiser4_place_assign((reiser4_place_t *)next,
				     p->node, p->pos.item + 1, 0);
	}

	return reiser4_place_fetch((reiser4_place_t *)next);
}

#ifndef ENABLE_STAND_ALONE
static aal_block_t *tree_get_data(void *tree, key_entity_t *key) {
	reiser4_tree_t *t = (reiser4_tree_t *)tree;
	return aal_hash_table_lookup(t->data, key);
}

static errno_t tree_set_data(void *tree, key_entity_t *key,
			     aal_block_t *block)
{
	reiser4_key_t *k;
	reiser4_tree_t *t;

	k = reiser4_key_clone(key);
	t = (reiser4_tree_t *)tree;
	
	return aal_hash_table_insert(t->data, k, block);
}

static uint32_t tree_blksize(void *tree) {
	reiser4_fs_t *fs;
	
	aal_assert("umka-1220", tree != NULL);
	
	fs = ((reiser4_tree_t *)tree)->fs;
	return reiser4_master_blksize(fs->master);
}

static uint32_t tree_maxspace(void *tree) {
	reiser4_node_t *root;
    
	aal_assert("umka-1220", tree != NULL);
	
	if (!(root = ((reiser4_tree_t *)tree)->root))
		return 0;
	
	return reiser4_node_maxspace(root);
}

static errno_t tree_ukey(void *tree, place_t *place,
			 key_entity_t *key)
{
	aal_assert("vpf-1219", tree != NULL);
	aal_assert("vpf-1206", place != NULL);
	aal_assert("vpf-1207", key != NULL);

	return reiser4_tree_ukey((reiser4_tree_t *)tree,
				 (reiser4_place_t *)place,
				 (reiser4_key_t *)key);
}

static char *key_print(key_entity_t *key, uint16_t options) {
	return reiser4_print_key((reiser4_key_t *)key, options);
}

static errno_t tree_conv(void *tree, place_t *place, reiser4_plug_t *plug) {
	return reiser4_tree_conv(tree, (reiser4_place_t *)place, plug);
}

static uint64_t param_value(char *name) {
	aal_assert("vpf-1202", name != NULL);
	return reiser4_param_value(name);
}
#endif

#ifdef ENABLE_SYMLINKS
static errno_t object_resolve(void *tree, place_t *place, char *filename,
			      key_entity_t *from, key_entity_t *key)
{
	errno_t res;
	reiser4_tree_t *t;
	reiser4_place_t *p;
	reiser4_object_t *o;
	
	t = (reiser4_tree_t *)tree;
	p = (reiser4_place_t *)place;

	if (!(o = reiser4_object_realize(t, NULL, p)))
		return -EINVAL;

	/* Setting up the key resolve will start from */
	reiser4_key_assign(&o->entity->info.object, from);

	/* Resolving symlink */
	if ((res = reiser4_object_resolve(o, filename, TRUE)))
		goto error_free_object;

	/* Assigning found key to passed @key */
	reiser4_key_assign(key, &o->entity->info.object);

 error_free_object:
	reiser4_object_close(o);
	return res;
}
#endif

/* Initializing the libreiser4 core instance. It will be passed into all plugins
   in otder to let them ability access libreiser4 methods such as insert or
   remove an item from the tree. */
reiser4_core_t core = {
	.tree_ops = {
	
		/* This one for lookuping the tree */
		.lookup	    = tree_lookup,

		/* This one for initializing an item at place */
		.fetch      = tree_fetch,

		/* Installing "valid" callback */
		.valid      = tree_valid,

		/* Returns next item from the passed place */
		.next	    = tree_next,

#ifndef ENABLE_STAND_ALONE
		/* Callback function for inserting items into the tree */
		.insert	    = tree_insert,

		/* Callback function for removing items from the tree */
		.remove	    = tree_remove,
		
		.maxspace   = tree_maxspace,
		.blksize    = tree_blksize,

		/* Update the key in the place and the node itsef. */
		.ukey       = tree_ukey,

		/* Data related functions */
		.get_data   = tree_get_data,
		.set_data   = tree_set_data,

		/*Convertion to another item plugin. */
		.conv	    = tree_conv
#endif
	},
#ifndef ENABLE_STAND_ALONE
	.param_ops = {
		.value = param_value
	},
#endif
	.factory_ops = {
		/* Installing callback for making search for a plugin by its
		   type and id. */
		.ifind = factory_ifind,

#ifndef ENABLE_STAND_ALONE
		/* Installing callback for making search for a plugin by its
		   type and name. */
		.nfind = factory_nfind
#endif
	},
#ifdef ENABLE_SYMLINKS
	.object_ops = {
		.resolve = object_resolve
	},
#endif

#ifndef ENABLE_STAND_ALONE
	.key_ops = {
		.print = key_print
	}
#endif
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

/* Initializes libreiser4 (plugin factory, etc). This function should be called
   before any actions performed on libreiser4. */
errno_t libreiser4_init(void) {
#ifndef ENABLE_STAND_ALONE
	if (reiser4_print_init()) {
		aal_exception_error("Can't initialize print factory");
		return -EINVAL;
	}
#endif
    
	if (reiser4_factory_init()) {
		aal_exception_fatal("Can't initialize plugin factory.");
		goto error_free_print;
	}

	return 0;
	
 error_free_print:
#ifndef ENABLE_STAND_ALONE
	reiser4_print_fini();
#endif
	return -EINVAL;
}

/* Finalizes libreiser4 */
void libreiser4_fini(void) {
	reiser4_factory_fini();
	
#ifndef ENABLE_STAND_ALONE
	reiser4_print_init();
#endif
}
