/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   libreiser4.c -- version control functions, library initialization code and
   plugin-accessible library functions. */

#include <reiser4/libreiser4.h>

#ifndef ENABLE_STAND_ALONE
const char *reiser4_igname[] = {
	"SD",
	"NPTR",
	"DIRITEM",
	"TAIL",
	"EXTENT",
	"PERM",
	"SLINK"
};

const char *reiser4_slink_name[] = {
	"UNLINK",
	"TRUNCATE",
	"Extent2Tail Convertion",
	"Tail2Extent Convertion"
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
/* Handler for item insert requests from the all plugins */
static int64_t tree_insert(tree_entity_t *tree, reiser4_place_t *place,
			   trans_hint_t *hint, uint8_t level)
{
	return reiser4_tree_insert((reiser4_tree_t *)tree,
				   place, hint, level);
}

/* Handler for write operation from @core. */
static int64_t tree_write(tree_entity_t *tree, trans_hint_t *hint) {
	return reiser4_flow_write((reiser4_tree_t *)tree, hint);
}

/* Handler for truncate operation from @core. */
static int64_t tree_truncate(tree_entity_t *tree, trans_hint_t *hint) {
	return reiser4_flow_truncate((reiser4_tree_t *)tree, hint);
}

/* Handler for item removing requests from all plugins. */
static errno_t tree_remove(tree_entity_t *tree, 
			   reiser4_place_t *place,
			   trans_hint_t *hint)
{
	return reiser4_tree_remove((reiser4_tree_t *)tree, place, hint);
}

static lookup_t tree_collision(tree_entity_t *tree, 
			       reiser4_place_t *place, 
			       coll_hint_t *hint)
{
	return reiser4_tree_collision((reiser4_tree_t *)tree, place, hint);
}
#endif

/* Handler for lookup reqiests from the all plugin can arrive. */
static lookup_t tree_lookup(tree_entity_t *tree, lookup_hint_t *hint,
			    lookup_bias_t bias, reiser4_place_t *place)
{
	return reiser4_tree_lookup((reiser4_tree_t *)tree,
				   hint, bias, place);
}

static int64_t tree_read(tree_entity_t *tree, trans_hint_t *hint) {
	return reiser4_flow_read((reiser4_tree_t *)tree, hint);
}

/* Handler for requests for next item. */
static errno_t tree_next_item(tree_entity_t *tree, reiser4_place_t *place,
			      reiser4_place_t *next)
{
	return reiser4_tree_next_place((reiser4_tree_t *)tree, place, next);
}

#ifndef ENABLE_STAND_ALONE
static errno_t tree_update_key(tree_entity_t *tree, 
			       reiser4_place_t *place,
			       reiser4_key_t *key)
{
	return reiser4_tree_update_keys((reiser4_tree_t *)tree, place, key);
}

static char *key_print(reiser4_key_t *key, uint16_t options) {
	return reiser4_print_key(key, options);
}

static errno_t tree_convert(tree_entity_t *tree, conv_hint_t *hint) {
	return reiser4_flow_convert((reiser4_tree_t *)tree, hint);
}

static reiser4_plug_t *pset_find(rid_t member, rid_t id) {
	return reiser4_opset_plug(member, id);
}

static void pset_diff(tree_entity_t *tree, reiser4_opset_t *opset) {
	reiser4_opset_diff((reiser4_tree_t *)tree, opset);
}

static int item_mergeable(reiser4_place_t *place1,
			  reiser4_place_t *place2)
{
	return reiser4_item_mergeable(place1, place2);
}

static uint64_t tree_slink_locality(tree_entity_t *tree) {
	reiser4_tree_t *t = (reiser4_tree_t *)tree;
	
	aal_assert("vpf-1579", t != NULL);
	aal_assert("vpf-1580", t->fs != NULL);
	aal_assert("vpf-1581", t->fs->oid != NULL);
	
	return plug_call(t->fs->oid->ent->plug->o.oid_ops, 
			 slink_locality);
}
#endif

#ifdef ENABLE_SYMLINKS
static errno_t object_resolve(tree_entity_t *tree, char *path,
			      reiser4_key_t *from, reiser4_key_t *key)
{
	reiser4_tree_t *t;
	object_entity_t *o;
	
	t = (reiser4_tree_t *)tree;

	/* Resolving symlink path. */
	if (!(o = reiser4_semantic_resolve(t, path, from, 1)))
		return -EINVAL;

	/* Save object stat data key to passed @key. */
	reiser4_key_assign(key, &o->object);

	/* Close found object. */
	plug_call(o->opset.plug[OPSET_OBJ]->o.object_ops, close, o);

	return 0;
}
#endif

/* Initializing the libreiser4 core instance. It will be passed into all plugins
   in otder to let them ability access libreiser4 methods such as insert or
   remove an item from the tree. */
reiser4_core_t core = {
	.flow_ops = {
		/* Reads data from the tree. */
		.read		= tree_read,
		
#ifndef ENABLE_STAND_ALONE
		/* Callback for truncating data in tree. */
		.truncate	= tree_truncate,

		/*Convertion to another item plugin. */
		.convert	= tree_convert,
		
		/* Callback for writting data to tree. */
		.write		= tree_write
#endif
	},
	.tree_ops = {
		/* This one for lookuping the tree */
		.lookup		= tree_lookup,

#ifndef ENABLE_STAND_ALONE
		/* Correct the position if collision exists. */
		.collision	= tree_collision,
		
		/* Callback function for inserting items into the tree. */
		.insert		= tree_insert,

		/* Callback function for removing items from the tree. */
		.remove		= tree_remove,

		/* Update the key in the place and the node itself. */
		.update_key	= tree_update_key,

		/* Get the safe link locality. */
		.slink_locality	= tree_slink_locality,
#endif
		/* Returns next item from the passed place. */
		.next_item	= tree_next_item
	},
	.factory_ops = {
		/* search a plugin by its type and id. */
		.ifind		= factory_ifind
	},
#ifndef ENABLE_STAND_ALONE
	.pset_ops = {
		.find		= pset_find,
		.diff		= pset_diff,
	},
#endif
#ifdef ENABLE_SYMLINKS
	.object_ops = {
		.resolve	= object_resolve
	},
#endif
#ifndef ENABLE_STAND_ALONE
	.key_ops = {
		.print		= key_print
	},
	.item_ops = {
		.mergeable	= item_mergeable
	},
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
