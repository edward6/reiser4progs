/*
  obj40.c -- reiser4 file plugins common code.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/stat.h>

#ifndef ENABLE_STAND_ALONE
#  include <time.h>
#  include <unistd.h>
#endif

#include "obj40.h"

/* Returns file's oid */
oid_t obj40_objectid(obj40_t *obj) {
	aal_assert("umka-1899", obj != NULL);

	return plugin_call(STAT_KEY(obj)->plugin->o.key_ops, 
			   get_objectid, STAT_KEY(obj));
}

/* Returns file's locality  */
oid_t obj40_locality(obj40_t *obj) {
	aal_assert("umka-1900", obj != NULL);
    
	return plugin_call(STAT_KEY(obj)->plugin->o.key_ops, 
			   get_locality, STAT_KEY(obj));
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

/* Relocks pased places */
void obj40_relock(obj40_t *obj, place_t *curr,
		  place_t *next)
{
	aal_assert("umka-2060", obj != NULL);
	aal_assert("umka-2061", curr != NULL);
	
	if (curr->node != NULL)
		obj40_unlock(obj, curr);

	if (next)
		obj40_lock(obj, next);
}

/* Reads light weight stat data extention into passed @lw_hint */
errno_t obj40_read_lw(item_entity_t *item,
		      sdext_lw_hint_t *lw_hint)
{
	create_hint_t hint;
	statdata_hint_t stat;

	aal_memset(&stat, 0, sizeof(stat));

	/* Preparing hint and mask */
	hint.type_specific = &stat;
	stat.ext[SDEXT_LW_ID] = lw_hint;

	/* Calling statdata open method if any */
	if (plugin_call(item->plugin->o.item_ops, read,
			item, &hint, 0, 1) != 1)
	{
		return -EINVAL;
	}
	
	return 0;
}

/* Gets size field from the stat data */
uint64_t obj40_get_size(obj40_t *obj) {
	sdext_lw_hint_t lw_hint;

	if (obj40_read_lw(&obj->statdata.item, &lw_hint))
		return 0;
	
	return lw_hint.size;
}

#ifndef ENABLE_STAND_ALONE
/*
  Writes light weight stat data extention from passed @lw_hint into @obj stat
  data item.
*/
errno_t obj40_write_lw(item_entity_t *item,
		       sdext_lw_hint_t *lw_hint)
{
	create_hint_t hint;
	statdata_hint_t stat;

	aal_memset(&stat, 0, sizeof(stat));
	
	hint.type_specific = &stat;

	if (plugin_call(item->plugin->o.item_ops, read,
			item, &hint, 0, 1) != 1)
	{
		return -EINVAL;
	}

	stat.ext[SDEXT_LW_ID] = lw_hint;

	return plugin_call(item->plugin->o.item_ops,
			   insert, item, &hint, 0);
}

/* Reads unix stat data extention into passed @unix_hint */
errno_t obj40_read_unix(item_entity_t *item,
			sdext_unix_hint_t *unix_hint)
{
	create_hint_t hint;
	statdata_hint_t stat;

	aal_memset(&stat, 0, sizeof(stat));

	/* Preparing hint and mask */
	hint.type_specific = &stat;
	stat.ext[SDEXT_UNIX_ID] = unix_hint;

	/* Calling statdata open method if it exists */
	if (plugin_call(item->plugin->o.item_ops, read,
			item, &hint, 0, 1) != 1)
	{
		return -EINVAL;
	}

	return 0;
}

/* Writes unix stat data extention into @obj stat data item */
errno_t obj40_write_unix(item_entity_t *item,
			 sdext_unix_hint_t *unix_hint)
{
	create_hint_t hint;
	statdata_hint_t stat;

	aal_memset(&stat, 0, sizeof(stat));
	
	hint.type_specific = &stat;

	if (plugin_call(item->plugin->o.item_ops, read,
			item, &hint, 0, 1) != 1)
	{
		return -EINVAL;
	}

	stat.ext[SDEXT_UNIX_ID] = unix_hint;

	return plugin_call(item->plugin->o.item_ops,
			   insert, item, &hint, 0);
}

/* Gets mode field from the stat data */
uint16_t obj40_get_mode(obj40_t *obj) {
	sdext_lw_hint_t lw_hint;

	if (obj40_read_lw(&obj->statdata.item, &lw_hint))
		return 0;
	
	return lw_hint.mode;
}

/* Updates mode field in statdata */
errno_t obj40_set_mode(obj40_t *obj, uint16_t mode) {
	errno_t res;
	sdext_lw_hint_t lw_hint;

	if ((res = obj40_read_lw(&obj->statdata.item, &lw_hint)))
		return res;

	lw_hint.mode = mode;
	
	return obj40_write_lw(&obj->statdata.item, &lw_hint);
}

/* Updates size field in the stat data */
errno_t obj40_set_size(obj40_t *obj, uint64_t size) {
	errno_t res;
	sdext_lw_hint_t lw_hint;

	if ((res = obj40_read_lw(&obj->statdata.item, &lw_hint)))
		return res;

	lw_hint.size = size;
	
	return obj40_write_lw(&obj->statdata.item, &lw_hint);
}

/* Gets nlink field from the stat data */
uint32_t obj40_get_nlink(obj40_t *obj) {
	sdext_lw_hint_t lw_hint;

	if (obj40_read_lw(&obj->statdata.item, &lw_hint))
		return 0;
	
	return lw_hint.nlink;
}

/* Updates nlink field in the stat data */
errno_t obj40_set_nlink(obj40_t *obj, uint32_t nlink) {
	errno_t res;
	sdext_lw_hint_t lw_hint;

	if ((res = obj40_read_lw(&obj->statdata.item, &lw_hint)))
		return res;

	lw_hint.nlink = nlink;
	
	return obj40_write_lw(&obj->statdata.item, &lw_hint);
}

/* Gets atime field from the stat data */
uint32_t obj40_get_atime(obj40_t *obj) {
	sdext_unix_hint_t unix_hint;

	if (obj40_read_unix(&obj->statdata.item, &unix_hint))
		return 0;
	
	return unix_hint.atime;
}

/* Updates atime field in the stat data */
errno_t obj40_set_atime(obj40_t *obj, uint32_t atime) {
	errno_t res;
	sdext_unix_hint_t unix_hint;

	if ((res = obj40_read_unix(&obj->statdata.item, &unix_hint)))
		return res;

	unix_hint.atime = atime;
	
	return obj40_write_unix(&obj->statdata.item, &unix_hint);
}

/* Gets mtime field from the stat data */
uint32_t obj40_get_mtime(obj40_t *obj) {
	sdext_unix_hint_t unix_hint;

	if (obj40_read_unix(&obj->statdata.item, &unix_hint))
		return 0;
	
	return unix_hint.mtime;
}

/* Updates mtime field in the stat data */
errno_t obj40_set_mtime(obj40_t *obj, uint32_t mtime) {
	errno_t res;
	sdext_unix_hint_t unix_hint;

	if ((res = obj40_read_unix(&obj->statdata.item, &unix_hint)))
		return res;

	unix_hint.mtime = mtime;
	
	return obj40_write_unix(&obj->statdata.item, &unix_hint);
}

/* Gets bytes field from the stat data */
uint64_t obj40_get_bytes(obj40_t *obj) {
	sdext_unix_hint_t unix_hint;

	if (obj40_read_unix(&obj->statdata.item, &unix_hint))
		return 0;
	
	return unix_hint.bytes;
}

/* Updates mtime field in the stat data */
errno_t obj40_set_bytes(obj40_t *obj, uint64_t bytes) {
	errno_t res;
	sdext_unix_hint_t unix_hint;

	if ((res = obj40_read_unix(&obj->statdata.item, &unix_hint)))
		return res;

	unix_hint.bytes = bytes;
	
	return obj40_write_unix(&obj->statdata.item, &unix_hint);
}
#endif

#ifdef ENABLE_SYMLINKS_SUPPORT
/* Gets symlink from the stat data */
errno_t obj40_get_sym(obj40_t *obj, char *data) {
	create_hint_t hint;
	item_entity_t *item;
	statdata_hint_t stat;

	aal_memset(&stat, 0, sizeof(stat));
	
	hint.type_specific = &stat;
	stat.ext[SDEXT_SYMLINK_ID] = data;

	item = &obj->statdata.item;

	if (!item->plugin->o.item_ops->read)
		return -EINVAL;

	if (item->plugin->o.item_ops->read(item, &hint, 0, 1) != 1)
		return -EINVAL;

	return 0;
}
#endif

rid_t obj40_pid(item_entity_t *item) {
	sdext_lw_hint_t lw_hint;

	if (obj40_read_lw(item, &lw_hint))
		return INVAL_PID;

	/*
	  FIXME-UMKA: Here also should be discovering the stat data extentions
	  on order to find out not standard file plugin in it.
	*/
	
	if (S_ISLNK(lw_hint.mode))
		return OBJECT_SYMLINK40_ID;
	else if (S_ISREG(lw_hint.mode))
		return OBJECT_FILE40_ID;
	else if (S_ISDIR(lw_hint.mode))
		return OBJECT_DIRTORY40_ID;
#ifndef ENABLE_STAND_ALONE	
	else if (S_ISCHR(lw_hint.mode))
		return OBJECT_SPECIAL40_ID;
	else if (S_ISBLK(lw_hint.mode))
		return OBJECT_SPECIAL40_ID;
	else if (S_ISFIFO(lw_hint.mode))
		return OBJECT_SPECIAL40_ID;
	else if (S_ISSOCK(lw_hint.mode))
		return OBJECT_SPECIAL40_ID;
#endif

	return INVAL_PID;
}

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

	/* Initializing stat data key */
	plugin_call(key->plugin->o.key_ops, assign,
		    STAT_KEY(obj), key);

	return 0;
}

/* Performs lookup for the object's stat data */
errno_t obj40_stat(obj40_t *obj) {
	lookup_t res;
	
	aal_assert("umka-1905", obj != NULL);

	/* Unlocking old node if it exists */
	if (obj->statdata.node != NULL)
		obj40_unlock(obj, &obj->statdata);
	
	/* Lookuing for stat data place by */
	switch (obj->core->tree_ops.lookup(obj->tree, STAT_KEY(obj),
					   LEAF_LEVEL, &obj->statdata))
	{
	case PRESENT:
		obj40_lock(obj, &obj->statdata);
		return 0;
	default:
		aal_exception_error("Can't find stat data of object "
				    "0x%llx.", obj40_objectid(obj));
		return -EINVAL;
	}
}

/* Performs lookup and returns result to caller */
lookup_t obj40_lookup(obj40_t *obj, key_entity_t *key,
		      uint8_t level, place_t *place)
{
	aal_assert("umka-1966", obj != NULL);
	
	return obj->core->tree_ops.lookup(obj->tree, key,
					  level, place);
}

#ifndef ENABLE_STAND_ALONE
/* Changes nlink field in statdata by passed @value */
errno_t obj40_link(obj40_t *obj, uint32_t value) {
	uint32_t nlink;
	
	if (obj40_stat(obj))
		return -EINVAL;
	
	nlink = obj40_get_nlink(obj);
	
	return obj40_set_nlink(obj, nlink + value);
}

/*
  Inserts passed item hint into the tree. After function is finished, place
  contains the place of the inserted item.
*/
errno_t obj40_insert(obj40_t *obj, create_hint_t *hint,
		     uint8_t level, place_t *place)
{
	oid_t objectid = obj40_objectid(obj);

	/*
	  Making lookup in order to find place new item/unit will be inserted
	  at. If item/unit already exists, or lookup failed, we throw an
	  exception and return the error code.
	*/
	switch (obj40_lookup(obj, &hint->key, level, place)) {
	case ABSENT:
		if (obj->core->tree_ops.insert(obj->tree, place,
					       level, hint))
		{
			aal_exception_error("Can't insert new "
					    "item/unit of object "
					    "0x%llx into the tree.",
					    objectid);
			return -EINVAL;
		}
		break;
	case PRESENT:
		aal_exception_error("Key already exists in the tree.");
		return -EINVAL;
	case FAILED:
		return -EINVAL;
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
	case ABSENT:
		aal_exception_error("Can't find item/unit durring remove.");
		return -EINVAL;
	case PRESENT:
		if (obj->core->tree_ops.remove(obj->tree, &place,
					       (uint32_t)count))
		{
			aal_exception_error("Can't remove item/unit from "
					    "object 0x%llx.", objectid);
			return -EINVAL;
		}
		
		break;
	case FAILED:
		return -EINVAL;
	}

	return 0;
}
#endif
