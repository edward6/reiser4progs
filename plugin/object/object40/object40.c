/*
  object40.c -- reiser4 file plugins common code.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/stat.h>

#ifndef ENABLE_ALONE
#  include <time.h>
#  include <unistd.h>
#endif

#include "object40.h"

/* Returns file's oid */
roid_t object40_objectid(object40_t *object) {
	aal_assert("umka-1899", object != NULL);
    
	return plugin_call(object->key.plugin->key_ops, 
			   get_objectid, &object->key);
}

/* Returns file's locality  */
roid_t object40_locality(object40_t *object) {
	aal_assert("umka-1900", object != NULL);
    
	return plugin_call(object->key.plugin->key_ops, 
			   get_locality, &object->key);
}

/* Locks the node an item belong to file lies in */
errno_t object40_lock(object40_t *object,
		      place_t *place)
{
	aal_assert("umka-1901", object != NULL);
	aal_assert("umka-1902", place != NULL);
	
	if (place->node) {
		return object->core->tree_ops.lock(object->tree,
						   place);
	}

	return 0;
}

/* Unlocks the node an item belong to file lies in */
errno_t object40_unlock(object40_t *object,
			place_t *place)
{
	aal_assert("umka-1903", object != NULL);
	aal_assert("umka-1904", place != NULL);
	
	if (place->node) {
		return object->core->tree_ops.unlock(object->tree,
						     place);
	}

	return 0;
}

static errno_t object40_read_lw(object40_t *object,
				reiser4_sdext_lw_hint_t *lw_hint)
{
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));

	/* Preparing hint and mask */
	hint.type_specific = &stat;
	stat.ext[SDEXT_LW_ID] = lw_hint;

	item = &object->statdata.item;

	/* Calling statdata open method if it exists */
	if (!item->plugin->item_ops.read)
		return -1;

	return -(item->plugin->item_ops.read(item, &hint, 0, 1) != 1);
}

static errno_t object40_write_lw(object40_t *object,
				 reiser4_sdext_lw_hint_t *lw_hint)
{
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.type_specific = &stat;

	item = &object->statdata.item;
	item->plugin->item_ops.read(item, &hint, 0, 1);

	stat.ext[SDEXT_LW_ID] = lw_hint;

	if (!item->plugin->item_ops.write)
		return -1;

	return -(item->plugin->item_ops.write(item, &hint, 0, 1) != 1);
}

static errno_t object40_read_unix(object40_t *object,
				  reiser4_sdext_unix_hint_t *unix_hint)
{
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));

	/* Preparing hint and mask */
	hint.type_specific = &stat;
	stat.ext[SDEXT_UNIX_ID] = unix_hint;

	item = &object->statdata.item;

	/* Calling statdata open method if it exists */
	if (!item->plugin->item_ops.read)
		return -1;

	return -(item->plugin->item_ops.read(item, &hint, 0, 1) != 1);
}

static errno_t object40_write_unix(object40_t *object,
				   reiser4_sdext_unix_hint_t *unix_hint)
{
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.type_specific = &stat;

	item = &object->statdata.item;
	item->plugin->item_ops.read(item, &hint, 0, 1);

	stat.ext[SDEXT_UNIX_ID] = unix_hint;

	if (!item->plugin->item_ops.write)
		return -1;

	return -(item->plugin->item_ops.write(item, &hint, 0, 1) != 1);
}

/* Gets mode field from the stat data */
uint16_t object40_get_mode(object40_t *object) {
	reiser4_sdext_lw_hint_t lw_hint;

	if (object40_read_lw(object, &lw_hint))
		return 0;
	
	return lw_hint.mode;
}

/* Updates mode field in statdata */
errno_t object40_set_mode(object40_t *object,
			  uint16_t mode)
{
	reiser4_sdext_lw_hint_t lw_hint;

	if (object40_read_lw(object, &lw_hint))
		return -1;

	lw_hint.mode = mode;

	return object40_write_lw(object, &lw_hint);
}

/* Gets size field from the stat data */
uint64_t object40_get_size(object40_t *object) {
	reiser4_sdext_lw_hint_t lw_hint;

	if (object40_read_lw(object, &lw_hint))
		return 0;
	
	return lw_hint.size;
}

/* Updates size field in the stat data */
errno_t object40_set_size(object40_t *object,
			  uint64_t size)
{
	reiser4_sdext_lw_hint_t lw_hint;

	if (object40_read_lw(object, &lw_hint))
		return -1;

	lw_hint.size = size;

	return object40_write_lw(object, &lw_hint);
}

/* Gets nlink field from the stat data */
uint32_t object40_get_nlink(object40_t *object) {
	reiser4_sdext_lw_hint_t lw_hint;

	if (object40_read_lw(object, &lw_hint))
		return 0;
	
	return lw_hint.nlink;
}

/* Updates nlink field in the stat data */
errno_t object40_set_nlink(object40_t *object,
			   uint32_t nlink)
{
	reiser4_sdext_lw_hint_t lw_hint;

	if (object40_read_lw(object, &lw_hint))
		return -1;

	lw_hint.nlink = nlink;

	return object40_write_lw(object, &lw_hint);
}

/* Gets atime field from the stat data */
uint32_t object40_get_atime(object40_t *object) {
	reiser4_sdext_unix_hint_t unix_hint;

	if (object40_read_unix(object, &unix_hint))
		return 0;
	
	return unix_hint.atime;
}

/* Updates atime field in the stat data */
errno_t object40_set_atime(object40_t *object,
			   uint32_t atime)
{
	reiser4_sdext_unix_hint_t unix_hint;

	if (object40_read_unix(object, &unix_hint))
		return -1;

	unix_hint.atime = atime;

	return object40_write_unix(object, &unix_hint);
}

/* Gets mtime field from the stat data */
uint32_t object40_get_mtime(object40_t *object) {
	reiser4_sdext_unix_hint_t unix_hint;

	if (object40_read_unix(object, &unix_hint))
		return 0;
	
	return unix_hint.mtime;
}

/* Updates mtime field in the stat data */
errno_t object40_set_mtime(object40_t *object,
			   uint32_t mtime)
{
	reiser4_sdext_unix_hint_t unix_hint;

	if (object40_read_unix(object, &unix_hint))
		return -1;

	unix_hint.mtime = mtime;

	return object40_write_unix(object, &unix_hint);
}

/* Gets symlink from the stat data */
errno_t object40_get_sym(object40_t *object,
			 char *data)
{
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.type_specific = &stat;
	stat.ext[SDEXT_SYMLINK_ID] = data;

	item = &object->statdata.item;

	if (!item->plugin->item_ops.read)
		return -1;

	if (item->plugin->item_ops.read(item, &hint, 0, 1) != 1) {
		aal_exception_error("Can't open statdata item.");
		return -1;
	}

	return 0;
}

/* Updates symlink data */
errno_t object40_set_sym(object40_t *object,
			 char *data)
{
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.type_specific = &stat;
	stat.ext[SDEXT_SYMLINK_ID] = data;

	item = &object->statdata.item;

	if (!item->plugin->item_ops.write)
		return -1;

	if (item->plugin->item_ops.write(item, &hint, 0, 1) != 1) {
		aal_exception_error("Can't update symlink.");
		return -1;
	}

	return 0;
}

/*
  Initializes object handle by plugin, key, core operations and opaque pointer
  to tree file is going to be opened/created in.
*/
errno_t object40_init(object40_t *object, reiser4_plugin_t *plugin,
		      key_entity_t *key, reiser4_core_t *core,
		      void *tree)
{
	aal_assert("umka-1574", object != NULL);
	aal_assert("umka-1756", plugin != NULL);
	aal_assert("umka-1757", tree != NULL);

	object->tree = tree;
	object->core = core;
	object->plugin = plugin;

	object->key.plugin = key->plugin;
	
	return plugin_call(key->plugin->key_ops, assign,
			   &object->key, key);
}

/* Performs lookup for the object's stat data */
errno_t object40_stat(object40_t *object) {
	aal_assert("umka-1905", object != NULL);

	plugin_call(object->key.plugin->key_ops, build_generic,
		    &object->key, KEY_STATDATA_TYPE, object40_locality(object),
		    object40_objectid(object), 0);

	/* Unlocking old node */
	if (object->statdata.node)
		object40_unlock(object, &object->statdata);

	/* Requesting libreiser4 lookup in order to find stat data position */
	if (object->core->tree_ops.lookup(object->tree, &object->key, LEAF_LEVEL,
					  &object->statdata) != LP_PRESENT) 
	{
		aal_exception_error("Can't find stat data of object 0x%llx.", 
				    object40_objectid(object));

		if (object->statdata.node)
			object40_lock(object, &object->statdata);
		
		return -1;
	}
	
	/* Locking new node */
	object40_lock(object, &object->statdata);
	
	return 0;
}

/* Changes nlink field in statdata by passed @value */
errno_t object40_link(object40_t *object,
		      uint32_t value)
{
	uint32_t nlink;
	
	if (object40_stat(object))
		return -1;
	
	nlink = object40_get_nlink(object);

	return object40_set_nlink(object, nlink + value);
}

/* Performs lookup and returns result to caller */
lookup_t object40_lookup(object40_t *object, key_entity_t *key,
			 uint8_t stop, place_t *place)
{
	return object->core->tree_ops.lookup(object->tree, key,
					     stop, place);
}

/*
  Inserts passed item hint into the tree. After function is finished, place
  contains the coord of the inserted item.
*/
errno_t object40_insert(object40_t *object, reiser4_item_hint_t *hint,
			uint8_t stop, place_t *place)
{
	roid_t objectid = object40_objectid(object);

	/*
	  Making lookup in order to find place new item/unit will be inserted
	  at. If item/unit already exists, or lookup failed, we throw an
	  exception and return the error code.
	*/
	switch (object40_lookup(object, &hint->key, stop, place)) {
	case LP_ABSENT:
		if (object->core->tree_ops.insert(object->tree, place, hint)) {
			aal_exception_error("Can't insert new item of object "
					    "0x%llx into the tree.", objectid);
			return -1;
		}
		break;
	case LP_PRESENT:
		aal_exception_error("Key already exists in the tree.");
		return -1;
	case LP_FAILED:
		aal_exception_error("Lookup is failed while trying to insert "
				    "new item into object 0x%llx.", objectid);
		return -1;
	}

	return 0;
}
