/*
  obj40.c -- reiser4 file plugins common code.

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

#include "obj40.h"

/* Returns file's oid */
oid_t obj40_objectid(obj40_t *obj) {
	aal_assert("umka-1899", obj != NULL);
    
	return plugin_call(obj->key.plugin->key_ops, 
			   get_objectid, &obj->key);
}

/* Returns file's locality  */
oid_t obj40_locality(obj40_t *obj) {
	aal_assert("umka-1900", obj != NULL);
    
	return plugin_call(obj->key.plugin->key_ops, 
			   get_locality, &obj->key);
}

/* Locks the node place points to */
errno_t obj40_lock(obj40_t *obj, place_t *place) {
	aal_assert("umka-1901", obj != NULL);
	aal_assert("umka-1902", place != NULL);
	aal_assert("umka-1964", place->node != NULL);
	
	return obj->core->tree_ops.lock(obj->tree, place);
}

/* Unlocks the node place points to */
errno_t obj40_unlock(obj40_t *obj, place_t *place) {
	aal_assert("umka-1903", obj != NULL);
	aal_assert("umka-1904", place != NULL);
	aal_assert("umka-1965", place->node != NULL);
	
	return obj->core->tree_ops.unlock(obj->tree, place);
}

/* Reads light weight stat data extention into passed @lw_hint */
static errno_t obj40_read_lw(obj40_t *obj, reiser4_sdext_lw_hint_t *lw_hint) {
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));

	/* Preparing hint and mask */
	hint.type_specific = &stat;
	stat.ext[SDEXT_LW_ID] = lw_hint;

	item = &obj->statdata.item;

	/* Calling statdata open method if it exists */
	if (!item->plugin->item_ops.read)
		return -1;

	return -(item->plugin->item_ops.read(item, &hint, 0, 1) != 1);
}

#ifndef ENABLE_ALONE

/*
  Writes light weight stat data extention from passed @lw_hint into @obj stat
  data item.
*/
static errno_t obj40_write_lw(obj40_t *obj, reiser4_sdext_lw_hint_t *lw_hint) {
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.type_specific = &stat;

	item = &obj->statdata.item;
	item->plugin->item_ops.read(item, &hint, 0, 1);

	stat.ext[SDEXT_LW_ID] = lw_hint;

	if (!item->plugin->item_ops.write)
		return -1;

	return -(item->plugin->item_ops.write(item, &hint, 0, 1) != 1);
}

#endif

/* Reads unix stat data extention into passed @unix_hint */
static errno_t obj40_read_unix(obj40_t *obj, reiser4_sdext_unix_hint_t *unix_hint) {
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));

	/* Preparing hint and mask */
	hint.type_specific = &stat;
	stat.ext[SDEXT_UNIX_ID] = unix_hint;

	item = &obj->statdata.item;

	/* Calling statdata open method if it exists */
	if (!item->plugin->item_ops.read)
		return -1;

	return -(item->plugin->item_ops.read(item, &hint, 0, 1) != 1);
}

#ifndef ENABLE_ALONE

/* Writes unix stat data extention into @obj stat data item */
static errno_t obj40_write_unix(obj40_t *obj, reiser4_sdext_unix_hint_t *unix_hint) {
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.type_specific = &stat;

	item = &obj->statdata.item;
	item->plugin->item_ops.read(item, &hint, 0, 1);

	stat.ext[SDEXT_UNIX_ID] = unix_hint;

	if (!item->plugin->item_ops.write)
		return -1;

	return -(item->plugin->item_ops.write(item, &hint, 0, 1) != 1);
}

#endif

/* Gets mode field from the stat data */
uint16_t obj40_get_mode(obj40_t *obj) {
	reiser4_sdext_lw_hint_t lw_hint;

	if (obj40_read_lw(obj, &lw_hint))
		return 0;
	
	return lw_hint.mode;
}

#ifndef ENABLE_ALONE

/* Updates mode field in statdata */
errno_t obj40_set_mode(obj40_t *obj, uint16_t mode) {
	reiser4_sdext_lw_hint_t lw_hint;

	if (obj40_read_lw(obj, &lw_hint))
		return -1;

	lw_hint.mode = mode;

	return obj40_write_lw(obj, &lw_hint);
}

#endif

/* Gets size field from the stat data */
uint64_t obj40_get_size(obj40_t *obj) {
	reiser4_sdext_lw_hint_t lw_hint;

	if (obj40_read_lw(obj, &lw_hint))
		return 0;
	
	return lw_hint.size;
}

#ifndef ENABLE_ALONE

/* Updates size field in the stat data */
errno_t obj40_set_size(obj40_t *obj, uint64_t size) {
	reiser4_sdext_lw_hint_t lw_hint;

	if (obj40_read_lw(obj, &lw_hint))
		return -1;

	lw_hint.size = size;

	return obj40_write_lw(obj, &lw_hint);
}

#endif

/* Gets nlink field from the stat data */
uint32_t obj40_get_nlink(obj40_t *obj) {
	reiser4_sdext_lw_hint_t lw_hint;

	if (obj40_read_lw(obj, &lw_hint))
		return 0;
	
	return lw_hint.nlink;
}

#ifndef ENABLE_ALONE

/* Updates nlink field in the stat data */
errno_t obj40_set_nlink(obj40_t *obj, uint32_t nlink) {
	reiser4_sdext_lw_hint_t lw_hint;

	if (obj40_read_lw(obj, &lw_hint))
		return -1;

	lw_hint.nlink = nlink;

	return obj40_write_lw(obj, &lw_hint);
}

#endif

/* Gets atime field from the stat data */
uint32_t obj40_get_atime(obj40_t *obj) {
	reiser4_sdext_unix_hint_t unix_hint;

	if (obj40_read_unix(obj, &unix_hint))
		return 0;
	
	return unix_hint.atime;
}

#ifndef ENABLE_ALONE

/* Updates atime field in the stat data */
errno_t obj40_set_atime(obj40_t *obj, uint32_t atime) {
	reiser4_sdext_unix_hint_t unix_hint;

	if (obj40_read_unix(obj, &unix_hint))
		return -1;

	unix_hint.atime = atime;

	return obj40_write_unix(obj, &unix_hint);
}

#endif

/* Gets mtime field from the stat data */
uint32_t obj40_get_mtime(obj40_t *obj) {
	reiser4_sdext_unix_hint_t unix_hint;

	if (obj40_read_unix(obj, &unix_hint))
		return 0;
	
	return unix_hint.mtime;
}

#ifndef ENABLE_ALONE

/* Updates mtime field in the stat data */
errno_t obj40_set_mtime(obj40_t *obj, uint32_t mtime) {
	reiser4_sdext_unix_hint_t unix_hint;

	if (obj40_read_unix(obj, &unix_hint))
		return -1;

	unix_hint.mtime = mtime;

	return obj40_write_unix(obj, &unix_hint);
}

#endif

/* Gets symlink from the stat data */
errno_t obj40_get_sym(obj40_t *obj, char *data) {
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.type_specific = &stat;
	stat.ext[SDEXT_SYMLINK_ID] = data;

	item = &obj->statdata.item;

	if (!item->plugin->item_ops.read)
		return -1;

	if (item->plugin->item_ops.read(item, &hint, 0, 1) != 1) {
		aal_exception_error("Can't open statdata item.");
		return -1;
	}

	return 0;
}

#ifndef ENABLE_ALONE

/* Updates symlink data */
errno_t obj40_set_sym(obj40_t *obj, char *data) {
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.type_specific = &stat;
	stat.ext[SDEXT_SYMLINK_ID] = data;

	item = &obj->statdata.item;

	if (!item->plugin->item_ops.write)
		return -1;

	if (item->plugin->item_ops.write(item, &hint, 0, 1) != 1) {
		aal_exception_error("Can't update symlink.");
		return -1;
	}

	return 0;
}

#endif

/*
  Initializes object handle by plugin, key, core operations and opaque pointer
  to tree file is going to be opened/created in.
*/
errno_t obj40_init(obj40_t *obj, reiser4_plugin_t *plugin,
		   key_entity_t *key, reiser4_core_t *core,
		   void *tree)
{
	aal_assert("umka-1574", obj != NULL);
	aal_assert("umka-1756", plugin != NULL);
	aal_assert("umka-1757", tree != NULL);

	obj->tree = tree;
	obj->core = core;
	obj->plugin = plugin;

	obj->key.plugin = key->plugin;
	
	return plugin_call(key->plugin->key_ops, assign,
			   &obj->key, key);
}

/* Performs lookup for the object's stat data */
errno_t obj40_stat(obj40_t *obj) {
	lookup_t res;
	uint64_t objectid, locality;
	
	aal_assert("umka-1905", obj != NULL);

	objectid = obj40_objectid(obj);
	locality = obj40_locality(obj);
	
	plugin_call(obj->key.plugin->key_ops, build_generic,
		    &obj->key, KEY_STATDATA_TYPE, locality,
		    objectid, 0);

	/* Unlocking old node if it exists */
	if (obj->statdata.node != NULL)
		obj40_unlock(obj, &obj->statdata);
	
	/*
	  Requesting libreiser4 lookup in order to find stat data place in the
	  tree.
	*/
	res = obj->core->tree_ops.lookup(obj->tree, &obj->key,
					 LEAF_LEVEL, &obj->statdata);

	if (res != LP_PRESENT) {
		aal_exception_error("Can't find stat data of object "
				    "0x%llx.", objectid);
		
		obj->statdata.node = NULL;
		return -1;
	}

	aal_memcpy(&obj->key, &obj->statdata.item.key,
		   sizeof(obj->key));
	
	/* Locking new node */
	obj40_lock(obj, &obj->statdata);
	
	return 0;
}

/* Performs lookup and returns result to caller */
lookup_t obj40_lookup(obj40_t *obj, key_entity_t *key,
		      uint8_t level, place_t *place)
{
	aal_assert("umka-1966", obj != NULL);
	
	return obj->core->tree_ops.lookup(obj->tree, key,
					  level, place);
}

#ifndef ENABLE_ALONE

/* Changes nlink field in statdata by passed @value */
errno_t obj40_link(obj40_t *obj, uint32_t value) {
	uint32_t nlink;
	
	if (obj40_stat(obj))
		return -1;
	
	nlink = obj40_get_nlink(obj);
	return obj40_set_nlink(obj, nlink + value);
}

/*
  Inserts passed item hint into the tree. After function is finished, place
  contains the place of the inserted item.
*/
errno_t obj40_insert(obj40_t *obj, reiser4_item_hint_t *hint,
		     uint8_t level, place_t *place)
{
	oid_t objectid = obj40_objectid(obj);

	/*
	  Making lookup in order to find place new item/unit will be inserted
	  at. If item/unit already exists, or lookup failed, we throw an
	  exception and return the error code.
	*/
	switch (obj40_lookup(obj, &hint->key, level, place)) {
	case LP_ABSENT:
		if (obj->core->tree_ops.insert(obj->tree, place, hint)) {
			aal_exception_error("Can't insert new item/unit of object "
					    "0x%llx into the tree.", objectid);
			return -1;
		}
		break;
	case LP_PRESENT:
		aal_exception_error("Key already exists in the tree.");
		return -1;
	case LP_FAILED:
		return -1;
	}

	return 0;
}

errno_t obj40_remove(obj40_t *obj, key_entity_t *key,
		     uint64_t count)
{
	place_t place;
	oid_t objectid = obj40_objectid(obj);
	
	/*
	  Making lookup in order to find the place the item/unit will be removed
	  at.
	*/
	switch (obj40_lookup(obj, key, LEAF_LEVEL, &place)) {
	case LP_ABSENT:
		aal_exception_error("Can't find item/unit durring remove.");
		return -1;
	case LP_PRESENT:
		if (obj->core->tree_ops.remove(obj->tree, &place,
					       (uint32_t)count))
		{
			aal_exception_error("Can't remove item/unit from "
					    "object 0x%llx.", objectid);
			return -1;
		}
		
		break;
	case LP_FAILED:
		return -1;
	}

	return 0;
}

#endif
