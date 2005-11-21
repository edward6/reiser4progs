/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   libreiser4.c -- version control functions, library initialization code and
   plugin-accessible library functions. */

#include <reiser4/libreiser4.h>

#ifndef ENABLE_MINIMAL
const char *reiser4_igname[] = {
	"SD",
	"NPTR",
	"DIRITEM",
	"TAIL",
	"EXTENT",
	"SLINK",
	"CTAIL"
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

#ifndef ENABLE_MINIMAL
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

/* Handler for cut operation from @core. */
static int64_t tree_cut(tree_entity_t *tree, trans_hint_t *hint) {
	return reiser4_flow_cut((reiser4_tree_t *)tree, hint);
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

static errno_t tree_mpressure(tree_entity_t *tree) {
	return reiser4_tree_mpressure((reiser4_tree_t *)tree);
}

static reiser4_plug_t *pset_find(rid_t member, rid_t id) {
	return reiser4_opset_plug(member, id);
}

#ifndef ENABLE_MINIMAL
static errno_t tree_update_key(tree_entity_t *tree, 
			       reiser4_place_t *place,
			       reiser4_key_t *key)
{
	return reiser4_tree_update_keys((reiser4_tree_t *)tree, place, key);
}

static char *key_print(reiser4_key_t *key, uint16_t options) {
	if (options == PO_INODE)
		return reiser4_print_inode(key);
	else if (options == 0)
		return reiser4_print_key(key);
	return NULL;
		
}

static errno_t tree_convert(tree_entity_t *tree, conv_hint_t *hint) {
	return reiser4_flow_convert((reiser4_tree_t *)tree, hint);
}

static uint64_t pset_build_mask(tree_entity_t *tree, reiser4_opset_t *opset) {
	return reiser4_opset_build_mask((reiser4_tree_t *)tree, opset);
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
	
	return plugcall(t->fs->oid->ent->plug, slink_locality);
}

static errno_t tree_inc_free(tree_entity_t *tree, count_t count) {
	reiser4_format_t *format;

	aal_assert("vpf-1723", tree != NULL);
	
	format = ((reiser4_tree_t *)tree)->fs->format;
	return reiser4_format_inc_free(format, count);
}

static errno_t tree_dec_free(tree_entity_t *tree, count_t count) {
	reiser4_format_t *format;

	aal_assert("vpf-1723", tree != NULL);
	
	format = ((reiser4_tree_t *)tree)->fs->format;
	return reiser4_format_dec_free(format, count);
}
#endif

#ifdef ENABLE_SYMLINKS
static errno_t object_resolve(tree_entity_t *tree, char *path,
			      reiser4_key_t *from, reiser4_key_t *key)
{
	reiser4_object_t *object;
	reiser4_tree_t *t;
	
	t = (reiser4_tree_t *)tree;

	/* Resolving symlink path. */
	if (!(object = reiser4_semantic_open(t, path, from, 1)))
		return -EINVAL;

	/* Save object stat data key to passed @key. */
	aal_memcpy(key, &object->info.object, sizeof(*key));

	/* Close found object. */
	reiser4_object_close(object);

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
		
#ifndef ENABLE_MINIMAL
		/* Callback for truncating data in tree. */
		.cut		= tree_cut,

		/*Convertion to another item plugin. */
		.convert	= tree_convert,
		
		/* Callback for writting data to tree. */
		.write		= tree_write
#endif
	},
	.tree_ops = {
		/* This one for lookuping the tree */
		.lookup		= tree_lookup,

#ifndef ENABLE_MINIMAL
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
		
		/* increment/decriment the free block count in the format. */
		.inc_free	= tree_inc_free,
		.dec_free	= tree_dec_free,
#endif
		/* Returns next item from the passed place. */
		.next_item	= tree_next_item,

		.mpressure	= tree_mpressure,
	},
	.factory_ops = {
		/* search a plugin by its type and id. */
		.ifind		= factory_ifind
	},
	.pset_ops = {
		.find		= pset_find,
#ifndef ENABLE_MINIMAL
		.build_mask	= pset_build_mask,
#endif
	},
#ifdef ENABLE_SYMLINKS
	.object_ops = {
		.resolve	= object_resolve
	},
#endif
#ifndef ENABLE_MINIMAL
	.key_ops = {
		.print		= key_print
	},
	.item_ops = {
		.mergeable	= item_mergeable
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
#ifndef ENABLE_MINIMAL
	reiser4_print_init(20);
#endif
    
	if (reiser4_factory_init()) {
		aal_fatal("Can't initialize plugin factory.");
		goto error_fini_print;
	}

	return 0;
	
 error_fini_print:
#ifndef ENABLE_MINIMAL
	reiser4_print_fini();
#endif
	return -EINVAL;
}

/* Finalizes libreiser4 */
void libreiser4_fini(void) {
	reiser4_factory_fini();
	
#ifndef ENABLE_MINIMAL
	reiser4_print_fini();
#endif
}
