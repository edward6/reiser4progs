/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   obj40.c -- reiser4 file 40 plugins common code. */

#include <sys/stat.h>

#ifndef ENABLE_STAND_ALONE
#  include <time.h>
#  include <unistd.h>
#endif

#include "obj40.h"

/* Returns file's oid */
oid_t obj40_objectid(obj40_t *obj) {
	aal_assert("umka-1899", obj != NULL);

	return plug_call(STAT_KEY(obj)->plug->o.key_ops, 
			 get_objectid, STAT_KEY(obj));
}

/* Returns file's locality  */
oid_t obj40_locality(obj40_t *obj) {
	aal_assert("umka-1900", obj != NULL);
    
	return plug_call(STAT_KEY(obj)->plug->o.key_ops, 
			 get_locality, STAT_KEY(obj));
}

/* Returns file's ordering  */
uint64_t obj40_ordering(obj40_t *obj) {
	aal_assert("umka-2334", obj != NULL);

	return plug_call(STAT_KEY(obj)->plug->o.key_ops, 
			 get_ordering, STAT_KEY(obj));
}

/* Reads stat data extention */
errno_t obj40_read_ext(place_t *place, rid_t id, void *data) {
	insert_hint_t hint;
	statdata_hint_t stat;

	aal_memset(&stat, 0, sizeof(stat));

	/* Preparing hint and mask */
	hint.type_specific = &stat;
	
	if (data)
		stat.ext[id] = data;
	
	/* Calling statdata open method if any */
	if (plug_call(place->plug->o.item_ops, read,
		      place, &hint, 0, 1) != 1)
	{
		return -EINVAL;
	}
	
	return 0;
}

/* Gets size field from the stat data */
uint64_t obj40_get_size(obj40_t *obj) {
	sdext_lw_hint_t lw_hint;

	if (obj40_read_ext(STAT_PLACE(obj), SDEXT_LW_ID, &lw_hint))
		return 0;
	
	return lw_hint.size;
}

#ifndef ENABLE_STAND_ALONE
errno_t obj40_create_stat(obj40_t *obj, rid_t pid, uint64_t mask,
			  uint64_t size, uint64_t bytes, uint32_t nlink,
			  uint16_t mode, char *path)
{
	insert_hint_t hint;
	statdata_hint_t stat;
	sdext_lw_hint_t lw_ext;
	sdext_unix_hint_t unix_ext;
	
	aal_memset(&hint, 0, sizeof(hint));
	
	/* Getting statdata plugin */
	if (!(hint.plug = obj->core->factory_ops.ifind(ITEM_PLUG_TYPE, pid))) {
		aal_exception_error("Can't find stat data item plugin by "
				    "its id 0x%x.", pid);
		return -EINVAL;
	}

	hint.count = 1;
	
	plug_call(obj->info.object.plug->o.key_ops, assign, 
		  &hint.key, &obj->info.object);
    
	/* Initializing stat data item hint. */
	stat.extmask = mask;
    	
	/* Light weight hint initializing. */
	lw_ext.size = size;
	lw_ext.nlink = nlink;
	lw_ext.mode = mode | 0755;
	
	/* Unix extention hint initializing */
	unix_ext.rdev = 0;
	unix_ext.bytes = bytes;
	unix_ext.uid = getuid();
	unix_ext.gid = getgid();
	
	unix_ext.atime = time(NULL);
	unix_ext.mtime = unix_ext.atime;
	unix_ext.ctime = unix_ext.atime;

	aal_memset(&stat.ext, 0, sizeof(stat.ext));

	/* Initializing extentions array */
	stat.ext[SDEXT_LW_ID] = &lw_ext;
	stat.ext[SDEXT_UNIX_ID] = &unix_ext;

	if ((1 << SDEXT_SYMLINK_ID) & mask)
		stat.ext[SDEXT_SYMLINK_ID] = path;
	
	hint.type_specific = &stat;

	/* Lookup place new item to be insert at and insert it to tree */
	if (obj40_lookup(obj, &hint.key, LEAF_LEVEL, 
			 STAT_PLACE(obj)) != ABSENT)
	{
		return -EINVAL;
	}
	
	/* Insert stat data to tree */
	return obj40_insert(obj, STAT_PLACE(obj), &hint, LEAF_LEVEL);
}

/* Updates size, bytes and atime fielsds */
errno_t obj40_touch(obj40_t *obj, uint64_t size,
		    uint64_t bytes, uint32_t atime)
{
	errno_t res;
	sdext_unix_hint_t unix_hint;

	/* Updating stat data place */
	if ((res = obj40_update(obj)))
		return res;
	
	/* Updating size if new file offset is further than size. This means,
	   that file realy got some data additionaly, not only got rewtitten
	   something. */
	if (size != obj40_get_size(obj)) {
		if ((res = obj40_set_size(obj, size)))
			return res;
	}

	/* Updating atime and mtime */
	if ((res = obj40_read_ext(STAT_PLACE(obj),
				  SDEXT_UNIX_ID,
				  &unix_hint)))
	{
		return res;
	}
	
	unix_hint.atime = atime;
	unix_hint.mtime = atime;

	if (bytes != unix_hint.bytes)
		unix_hint.bytes = bytes;

	return obj40_write_ext(STAT_PLACE(obj),
			       SDEXT_UNIX_ID,
			       &unix_hint);
}

/* Writes stat data extention. */
errno_t obj40_write_ext(place_t *place, rid_t id,
			void *data)
{
	insert_hint_t hint;
	statdata_hint_t stat;

	aal_memset(&stat, 0, sizeof(stat));

	hint.type_specific = &stat;

	if (plug_call(place->plug->o.item_ops, read,
		      place, &hint, 0, 1) != 1)
	{
		return -EINVAL;
	}

	stat.ext[id] = data;

	return plug_call(place->plug->o.item_ops,
			 insert, place, 0, &hint);
}

uint64_t obj40_extmask(place_t *place) {
	insert_hint_t hint;
	statdata_hint_t stat;

	aal_memset(&stat, 0, sizeof(stat));

	/* Preparing hint and mask */
	hint.type_specific = &stat;
	
	/* Calling statdata open method if any */
	if (plug_call(place->plug->o.item_ops, read,
		      place, &hint, 0, 1) != 1)
	{
		return MAX_UINT64;
	}
	
	return stat.extmask;
}

/* Gets mode field from the stat data */
uint16_t obj40_get_mode(obj40_t *obj) {
	sdext_lw_hint_t lw_hint;

	if (obj40_read_ext(STAT_PLACE(obj),
			   SDEXT_LW_ID, &lw_hint))
	{
		return 0;
	}
	
	return lw_hint.mode;
}

/* Updates mode field in statdata */
errno_t obj40_set_mode(obj40_t *obj, uint16_t mode) {
	errno_t res;
	sdext_lw_hint_t lw_hint;

	if ((res = obj40_read_ext(STAT_PLACE(obj),
				  SDEXT_LW_ID, &lw_hint)))
	{
		return res;
	}

	lw_hint.mode = mode;
	
	return obj40_write_ext(STAT_PLACE(obj),
			       SDEXT_LW_ID, &lw_hint);
}

/* Updates size field in the stat data */
errno_t obj40_set_size(obj40_t *obj, uint64_t size) {
	errno_t res;
	sdext_lw_hint_t lw_hint;

	if ((res = obj40_read_ext(STAT_PLACE(obj),
				  SDEXT_LW_ID, &lw_hint)))
	{
		return res;
	}

	lw_hint.size = size;
	
	return obj40_write_ext(STAT_PLACE(obj),
			       SDEXT_LW_ID, &lw_hint);
}

/* Gets nlink field from the stat data */
uint32_t obj40_get_nlink(obj40_t *obj) {
	sdext_lw_hint_t lw_hint;

	if (obj40_read_ext(STAT_PLACE(obj),
			   SDEXT_LW_ID, &lw_hint))
	{
		return 0;
	}
	
	return lw_hint.nlink;
}

/* Updates nlink field in the stat data */
errno_t obj40_set_nlink(obj40_t *obj, uint32_t nlink) {
	errno_t res;
	sdext_lw_hint_t lw_hint;

	if ((res = obj40_read_ext(STAT_PLACE(obj),
				  SDEXT_LW_ID, &lw_hint)))
	{
		return res;
	}

	lw_hint.nlink = nlink;
	
	return obj40_write_ext(STAT_PLACE(obj),
			       SDEXT_LW_ID, &lw_hint);
}

/* Gets atime field from the stat data */
uint32_t obj40_get_atime(obj40_t *obj) {
	sdext_unix_hint_t unix_hint;

	if (obj40_read_ext(STAT_PLACE(obj),
			   SDEXT_UNIX_ID, &unix_hint))
	{
		return 0;
	}
	
	return unix_hint.atime;
}

/* Updates atime field in the stat data */
errno_t obj40_set_atime(obj40_t *obj, uint32_t atime) {
	errno_t res;
	sdext_unix_hint_t unix_hint;

	if ((res = obj40_read_ext(STAT_PLACE(obj),
				  SDEXT_UNIX_ID, &unix_hint)))
	{
		return res;
	}

	unix_hint.atime = atime;
	
	return obj40_write_ext(STAT_PLACE(obj),
			       SDEXT_UNIX_ID, &unix_hint);
}

/* Gets mtime field from the stat data */
uint32_t obj40_get_mtime(obj40_t *obj) {
	sdext_unix_hint_t unix_hint;

	if (obj40_read_ext(STAT_PLACE(obj),
			   SDEXT_UNIX_ID, &unix_hint))
	{
		return 0;
	}
	
	return unix_hint.mtime;
}

/* Updates mtime field in the stat data */
errno_t obj40_set_mtime(obj40_t *obj, uint32_t mtime) {
	errno_t res;
	sdext_unix_hint_t unix_hint;

	if ((res = obj40_read_ext(STAT_PLACE(obj),
				  SDEXT_UNIX_ID, &unix_hint)))
	{
		return res;
	}

	unix_hint.mtime = mtime;
	
	return obj40_write_ext(STAT_PLACE(obj),
			       SDEXT_UNIX_ID, &unix_hint);
}

/* Gets bytes field from the stat data */
uint64_t obj40_get_bytes(obj40_t *obj) {
	sdext_unix_hint_t unix_hint;

	if (obj40_read_ext(STAT_PLACE(obj),
			   SDEXT_UNIX_ID, &unix_hint))
	{
		return 0;
	}
	
	return unix_hint.bytes;
}

/* Updates mtime field in the stat data */
errno_t obj40_set_bytes(obj40_t *obj, uint64_t bytes) {
	errno_t res;
	sdext_unix_hint_t unix_hint;

	if ((res = obj40_read_ext(STAT_PLACE(obj),
				  SDEXT_UNIX_ID, &unix_hint)))
	{
		return res;
	}

	unix_hint.bytes = bytes;
	
	return obj40_write_ext(STAT_PLACE(obj),
			       SDEXT_UNIX_ID, &unix_hint);
}

/* Changes nlink field in statdata by passed @value */
errno_t obj40_link(obj40_t *obj, uint32_t value) {
	uint32_t nlink = obj40_get_nlink(obj);
	return obj40_set_nlink(obj, nlink + value);
}
#endif

/* Fetches item at passed @place */
errno_t obj40_fetch(obj40_t *obj, place_t *place) {
	return obj->core->tree_ops.fetch(obj->info.tree, place);
}

/* Obtains the plugin of the plugid returned by obj40_pid(). */
reiser4_plug_t *obj40_plug(obj40_t *obj, rid_t type, char *name) {
	rid_t pid = obj40_pid(obj, type, name);

	/* Obtain the plugin by id. */
	if (pid == INVAL_PID)
		return NULL;
	
	return obj->core->factory_ops.ifind(type, pid);
}

/* Obtains plugid of the type @type from the SD if it is kept 
   there, othewise obtains the default one from the profile. */
rid_t obj40_pid(obj40_t *obj, rid_t type, char *name) {
	rid_t pid;
	
	aal_assert("vpf-1235", obj != NULL);
	aal_assert("vpf-1236", STAT_PLACE(obj)->plug != NULL);
	
	pid = plug_call(STAT_PLACE(obj)->plug->o.item_ops,
			plugid, STAT_PLACE(obj), type);
	
	/* If nothing found in SD, obtain the default one. */
	if (pid == INVAL_PID)
		pid = obj->core->profile_ops.value(name);
	
	return pid;
}

/*
  Initializes object handle by plugin, key, core operations and 
  opaque pointer to tree file is going to be opened/created in. */
errno_t obj40_init(obj40_t *obj, reiser4_plug_t *plug,
		   reiser4_core_t *core, object_info_t *info)
{
	aal_assert("umka-1574", obj != NULL);
	aal_assert("umka-1756", plug != NULL);
	aal_assert("umka-1757", info != NULL);

	obj->core = core;
	obj->plug = plug;
	obj->info = *info;

	if (info->object.plug) {
		plug_call(info->object.plug->o.key_ops,
			  assign, STAT_KEY(obj), &info->object);
	}
	
	return 0;
}

/* Makes sure, that passed place points to right location in tree by means of
   calling tree_lookup() for its key. This is needed, because items may move to
   somewhere after each balancing. */
errno_t obj40_update(obj40_t *obj) {
	aal_assert("umka-1905", obj != NULL);
		
	/* Looking for stat data place by */
	switch (obj40_lookup(obj, &STAT_PLACE(obj)->key,
			     LEAF_LEVEL, STAT_PLACE(obj)))
	{
	case PRESENT:
		return 0;
	default:
		return -EINVAL;
	}
}

/* Performs lookup and returns result to caller */
lookup_t obj40_lookup(obj40_t *obj, key_entity_t *key,
		      uint8_t level, place_t *place)
{
	aal_assert("umka-1966", obj != NULL);
	
	return obj->core->tree_ops.lookup(obj->info.tree, key,
					  level, place);
}

#ifndef ENABLE_STAND_ALONE
/* Inserts passed item hint into the tree. After function is finished, place
   contains the place of the inserted item. */
errno_t obj40_insert(obj40_t *obj, place_t *place,
		     insert_hint_t *hint, uint8_t level)
{
	return obj->core->tree_ops.insert(obj->info.tree,
					  place, hint, level);
}

/* Removes item/unit by @key */
errno_t obj40_remove(obj40_t *obj, place_t *place, remove_hint_t *hint) {
	return obj->core->tree_ops.remove(obj->info.tree, place, hint);
}
#endif
