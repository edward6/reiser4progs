/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   libreiser4.c -- version control functions, library initialization code and
   plugin-accessible library functions. */

#include <reiser4/libreiser4.h>

#ifndef ENABLE_STAND_ALONE
const char *reiser4_igname[] = {
	"SD",
	"NPTR",
	"DENTRY",
	"TAIL",
	"EXTENT",
	"PERM"
};
#endif

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
static int64_t tree_insert(void *tree, reiser4_place_t *place,
			   trans_hint_t *hint, uint8_t level)
{
	return reiser4_tree_insert((reiser4_tree_t *)tree,
				   place, hint, level);
}

/* Handler for write operation from @core. */
static int64_t tree_write(void *tree, trans_hint_t *hint) {
	reiser4_tree_t *t = (reiser4_tree_t *)tree;
	return reiser4_flow_write(t, hint);
}

/* Handler for truncate operation from @core. */
static int64_t tree_truncate(void *tree, trans_hint_t *hint) {
	reiser4_tree_t *t = (reiser4_tree_t *)tree;
	return reiser4_flow_truncate(t, hint);
}

/* Handler for item removing requests from all plugins. */
static errno_t tree_remove(void *tree, reiser4_place_t *place,
			   trans_hint_t *hint)
{
	return reiser4_tree_remove((reiser4_tree_t *)tree,
				   place, hint);
}
#endif

/* Handler for lookup reqiests from the all plugin can arrive. */
static lookup_t tree_lookup(void *tree, lookup_hint_t *hint,
			    lookup_bias_t bias, reiser4_place_t *place)
{
	return reiser4_tree_lookup((reiser4_tree_t *)tree,
				   hint, bias, place);
}

static lookup_t tree_collision(reiser4_place_t *place, lookup_hint_t *hint,
			       lookup_bias_t bias, lookup_t lookup)
{
	return reiser4_collision_handler(place, hint, bias, lookup);
}

static int64_t tree_read(void *tree, trans_hint_t *hint) {
	reiser4_tree_t *t = (reiser4_tree_t *)tree;
	return reiser4_flow_read(t, hint);
}

/* Handler for requests for next item. */
static errno_t tree_next(void *tree, reiser4_place_t *place,
			 reiser4_place_t *next)
{
	return reiser4_tree_next_node((reiser4_tree_t *)tree, 
				      place, next);
}

#ifndef ENABLE_STAND_ALONE
static errno_t tree_put_data(void *tree, reiser4_key_t *key,
			     aal_block_t *block)
{
	reiser4_key_t *k = reiser4_key_clone(key);
	reiser4_tree_t *t = (reiser4_tree_t *)tree;
	return aal_hash_table_insert(t->data, k, block);
}

static errno_t tree_rem_data(void *tree, reiser4_key_t *key) {
	reiser4_tree_t *t = (reiser4_tree_t *)tree;
	return aal_hash_table_remove(t->data, key);
}

static aal_block_t *tree_get_data(void *tree, reiser4_key_t *key) {
	reiser4_tree_t *t = (reiser4_tree_t *)tree;
	return aal_hash_table_lookup(t->data, key);
}

static errno_t tree_update_key(void *tree, reiser4_place_t *place,
			       reiser4_key_t *key)
{
	return reiser4_tree_update_key((reiser4_tree_t *)tree,
				       place, (reiser4_key_t *)key);
}

static char *key_print(reiser4_key_t *key, uint16_t options) {
	return reiser4_print_key((reiser4_key_t *)key, options);
}

static errno_t tree_convert(void *tree, conv_hint_t *hint) {
	reiser4_tree_t *t = (reiser4_tree_t *)tree;
	return reiser4_flow_convert(t, hint);
}

static uint64_t param_value(char *name) {
	return reiser4_param_value(name);
}

static int item_mergeable(reiser4_place_t *place1,
			  reiser4_place_t *place2)
{
	return reiser4_item_mergeable(place1, place2);
}
#endif

#ifdef ENABLE_SYMLINKS
static errno_t object_resolve(void *tree, char *path,
			      reiser4_key_t *from,
			      reiser4_key_t *key)
{
	reiser4_tree_t *t;
	object_entity_t *o;
	
	t = (reiser4_tree_t *)tree;

	/* Resolving symlink path. */
	if (!(o = reiser4_semantic_resolve(t, path, from, 1)))
		return -EINVAL;

	/* Save object stat data key to passed @key. */
	reiser4_key_assign(key, &o->info.object);

	/* Close found object. */
	plug_call(o->plug->o.object_ops, close, o);

	return 0;
}
#endif

/* Initializing the libreiser4 core instance. It will be passed into all plugins
   in otder to let them ability access libreiser4 methods such as insert or
   remove an item from the tree. */
reiser4_core_t core = {
	.flow_ops = {
		/* Reads data from the tree. */
		.read	    = tree_read,
		
#ifndef ENABLE_STAND_ALONE
		/* Callback for truncating data in tree. */
		.truncate   = tree_truncate,

		/*Convertion to another item plugin. */
		.convert    = tree_convert,
		
		/* Callback for writting data to tree. */
		.write	    = tree_write
#endif
	},
	.tree_ops = {
		/* This one for lookuping the tree */
		.lookup	    = tree_lookup,

		/* Correct the position if collision exists. */
		.collision  = tree_collision,
#ifndef ENABLE_STAND_ALONE
		/* Callback function for inserting items into the tree */
		.insert	    = tree_insert,

		/* Callback function for removing items from the tree */
		.remove	    = tree_remove,

		/* Update the key in the place and the node itself. */
		.update_key = tree_update_key,

		/* Data related functions */
		.put_data   = tree_put_data,
		.rem_data   = tree_rem_data,
		.get_data   = tree_get_data,
#endif
		/* Returns next item from the passed place */
		.next	    = tree_next
	},
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
#ifndef ENABLE_STAND_ALONE
	.param_ops = {
		.value = param_value
	},
#endif
#ifdef ENABLE_SYMLINKS
	.object_ops = {
		.resolve = object_resolve
	},
#endif
#ifndef ENABLE_STAND_ALONE
	.key_ops = {
		.print = key_print
	},
	.item_ops = {
		.mergeable = item_mergeable
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
	reiser4_print_init(20);
#endif
    
	if (reiser4_factory_init()) {
		aal_fatal("Can't initialize plugin factory.");
		goto error_fini_print;
	}

	return 0;
	
 error_fini_print:
#ifndef ENABLE_STAND_ALONE
	reiser4_print_fini();
#endif
	return -EINVAL;
}

/* Finalizes libreiser4 */
void libreiser4_fini(void) {
	reiser4_factory_fini();
	
#ifndef ENABLE_STAND_ALONE
	reiser4_print_fini();
#endif
}
