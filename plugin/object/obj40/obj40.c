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

/* Reads one stat data extension to @data. */
errno_t obj40_read_ext(place_t *place, rid_t id, void *data) {
	trans_hint_t trans;
	statdata_hint_t stat;

	aal_memset(&stat, 0, sizeof(stat));

	/* Preparing hint and mask */
	trans.specific = &stat;
	trans.place_func = NULL;
	trans.region_func = NULL;
	
	if (data) {
		stat.ext[id] = data;
	}
	
	/* Calling statdata open method. */
	if (plug_call(place->plug->o.item_ops->object,
		      fetch_units, place, &trans) != 1)
	{
		return -EIO;
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

/* Loads stat data to passed @hint. */
errno_t obj40_load_stat(obj40_t *obj, statdata_hint_t *hint) {
	trans_hint_t trans;
	
	aal_assert("umka-2553", obj != NULL);

	/* Preparing hint and mask. */
	trans.specific = hint;
	trans.place_func = NULL;
	trans.region_func = NULL;
	
	/* Calling statdata fetch method. */
	if (plug_call(STAT_PLACE(obj)->plug->o.item_ops->object,
		      fetch_units, STAT_PLACE(obj), &trans) != 1)
	{
		return -EIO;
	}
	
	return 0;
}

#ifndef ENABLE_STAND_ALONE
/* Saves stat data to passed @hint. */
errno_t obj40_save_stat(obj40_t *obj, statdata_hint_t *hint) {
	trans_hint_t trans;
	
	aal_assert("umka-2554", obj != NULL);

	/* Preparing hint and mask */
	trans.specific = hint;
	trans.place_func = NULL;
	trans.region_func = NULL;

	/* Updating stat data. */
	if (plug_call(STAT_PLACE(obj)->plug->o.item_ops->object,
		      update_units, STAT_PLACE(obj), &trans) <= 0)
	{
		return -EIO;
	}

	return 0;
}

/* Create stat data item basing on passed extensions @mask, @size, @bytes,
   @nlinks, @mode and @path for symlinks. Returns error or zero for success. */
errno_t obj40_create_stat(obj40_t *obj, rid_t pid, uint64_t mask,
			  uint64_t size, uint64_t bytes, uint64_t rdev,
			  uint32_t nlink, uint16_t mode, char *path)
{
	int64_t res;
	lookup_t lookup;
	trans_hint_t hint;
	statdata_hint_t stat;
	sdext_lw_hint_t lw_ext;
	sdext_unix_hint_t unix_ext;
	
	aal_memset(&hint, 0, sizeof(hint));
	
	/* Getting statdata plugin */
	if (!(hint.plug = obj->core->factory_ops.ifind(ITEM_PLUG_TYPE, pid))) {
		aal_error("Can't find stat data item plugin by "
			  "its id 0x%x.", pid);
		return -EIO;
	}

	hint.count = 1;
	hint.place_func = NULL;
	hint.region_func = NULL;
	
	plug_call(obj->info.object.plug->o.key_ops, assign, 
		  &hint.offset, &obj->info.object);
    
	/* Initializing stat data item hint. */
	stat.extmask = mask;
    	
	/* Light weight hint initializing. */
	lw_ext.size = size;
	lw_ext.nlink = nlink;
	lw_ext.mode = mode | 0755;
	
	/* Unix extension hint initializing */
	if (rdev && bytes) {
		aal_error("Invalid stat data params (rdev or bytes).");
		return -EINVAL;
	} else {
		unix_ext.rdev = rdev;
		unix_ext.bytes = bytes;
	}
	
	unix_ext.uid = getuid();
	unix_ext.gid = getgid();
	
	unix_ext.atime = time(NULL);
	unix_ext.mtime = unix_ext.atime;
	unix_ext.ctime = unix_ext.atime;

	aal_memset(&stat.ext, 0, sizeof(stat.ext));

	/* Initializing extensions array */
	stat.ext[SDEXT_LW_ID] = &lw_ext;
	stat.ext[SDEXT_UNIX_ID] = &unix_ext;

	if ((1 << SDEXT_SYMLINK_ID) & mask)
		stat.ext[SDEXT_SYMLINK_ID] = path;
	
	hint.specific = &stat;

	/* Lookup place new item to be insert at and insert it to tree */
	switch ((lookup = obj40_lookup(obj, &hint.offset, LEAF_LEVEL,
				       FIND_CONV, STAT_PLACE(obj))))
	{
	case ABSENT:
		break;
	default:
		return lookup;
	}
	
	/* Insert stat data to tree */
	res = obj40_insert(obj, STAT_PLACE(obj), &hint, LEAF_LEVEL);

	return res < 0 ? res : 0;
}

/* Updates size and bytes fielsds */
errno_t obj40_touch(obj40_t *obj, uint64_t size, uint64_t bytes) {
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

	/* Updating bytes */
	if ((res = obj40_read_ext(STAT_PLACE(obj), SDEXT_UNIX_ID, &unix_hint)))
		return res;

	/* Updating values and write unix extension back. */
	unix_hint.rdev = 0;
	unix_hint.bytes = bytes;

	return obj40_write_ext(STAT_PLACE(obj), SDEXT_UNIX_ID, &unix_hint);
}

/* Writes one stat data extension. */
errno_t obj40_write_ext(place_t *place, rid_t id,
			void *data)
{
	trans_hint_t hint;
	statdata_hint_t stat;

	aal_memset(&stat, 0, sizeof(stat));

	hint.specific = &stat;
	hint.place_func = NULL;
	hint.region_func = NULL;

	if (plug_call(place->plug->o.item_ops->object,
		      fetch_units, place, &hint) != 1)
	{
		return -EIO;
	}

	stat.ext[id] = data;

	if (plug_call(place->plug->o.item_ops->object,
		      update_units, place, &hint) <= 0)
	{
		return -EIO;
	}

	return 0;
}

/* Returns extensions mask from stat data item at @place. */
uint64_t obj40_extmask(place_t *place) {
	trans_hint_t hint;
	statdata_hint_t stat;

	aal_memset(&stat, 0, sizeof(stat));

	/* Preparing hint and mask */
	hint.specific = &stat;
	hint.place_func = NULL;
	hint.region_func = NULL;
	
	/* Calling statdata open method if any */
	if (plug_call(place->plug->o.item_ops->object,
		      fetch_units, place, &hint) != 1)
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

	if (obj40_read_ext(STAT_PLACE(obj), SDEXT_UNIX_ID, &unix_hint))
		return 0;
	
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

	if (obj40_read_ext(STAT_PLACE(obj), SDEXT_UNIX_ID, &unix_hint))
		return 0;
	
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

	if (obj40_read_ext(STAT_PLACE(obj), SDEXT_UNIX_ID, &unix_hint))
		return 0;
	
	return unix_hint.bytes;
}

/* Updates bytes field in the stat data */
errno_t obj40_set_bytes(obj40_t *obj, uint64_t bytes) {
	errno_t res;
	sdext_unix_hint_t unix_hint;

	if ((res = obj40_read_ext(STAT_PLACE(obj),
				  SDEXT_UNIX_ID, &unix_hint)))
	{
		return res;
	}

	unix_hint.rdev = 0;
	unix_hint.bytes = bytes;
	
	return obj40_write_ext(STAT_PLACE(obj),
			       SDEXT_UNIX_ID, &unix_hint);
}

/* Changes nlink field in statdata by passed @value */
errno_t obj40_inc_link(obj40_t *obj, uint32_t value) {
	uint32_t nlink = obj40_get_nlink(obj);
	return obj40_set_nlink(obj, nlink + value);
}

/* Removes object stat data. */
errno_t obj40_clobber(obj40_t *obj) {
	errno_t res;
	trans_hint_t hint;
	
	aal_assert("umka-2546", obj != NULL);

	if ((res = obj40_update(obj)))
		return res;

	hint.count = 1;
	hint.place_func = NULL;
	hint.region_func = NULL;
	
	return obj40_remove(obj, STAT_PLACE(obj), &hint);
}

/* Enumerates object data (stat data only for special files and symlinks). */
errno_t obj40_layout(obj40_t *obj, region_func_t region_func,
		     void *data)
{
	blk_t blk;
	errno_t res;

	aal_assert("umka-2547", obj != NULL);
	aal_assert("umka-2548", region_func != NULL);

	if ((res = obj40_update(obj)))
		return res;
	
	blk = STAT_PLACE(obj)->block->nr;
	return region_func(obj, blk, 1, data);
}

/* Enumerates object metadata. */
errno_t obj40_metadata(obj40_t *obj, place_func_t place_func,
		       void *data)
{
	errno_t res;
	
	aal_assert("umka-2549", obj != NULL);
	aal_assert("umka-2550", place_func != NULL);

	if ((res = obj40_update(obj)))
		return res;

	return place_func(STAT_PLACE(obj), data);
}

uint32_t obj40_links(obj40_t *obj) {
	errno_t res;

	aal_assert("umka-2567", obj != NULL);
	
	if ((res = obj40_update(obj)))
		return res;
	
	return obj40_get_nlink(obj);
}

errno_t obj40_link(obj40_t *obj) {
	errno_t res;
	
	aal_assert("umka-2568", obj != NULL);

	if ((res = obj40_update(obj)))
		return res;
	
	return obj40_inc_link(obj, 1);
}

errno_t obj40_unlink(obj40_t *obj) {
	errno_t res;
	
	aal_assert("umka-2569", obj != NULL);
	
	if ((res = obj40_update(obj)))
		return res;
	
	return obj40_inc_link(obj, -1);
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

/* Obtains plugid of the type @type from the SD if it is kept there, othewise
   obtains the default one from the params. */
rid_t obj40_pid(obj40_t *obj, rid_t type, char *name) {
	rid_t pid;
	
	aal_assert("vpf-1235", obj != NULL);
	aal_assert("vpf-1236", STAT_PLACE(obj)->plug != NULL);
	
	pid = plug_call(STAT_PLACE(obj)->plug->o.item_ops->object,
			object_plug, STAT_PLACE(obj), type);

	/* If nothing found in SD, obtain the default one. */
	if (pid == INVAL_PID) {
#ifndef ENABLE_STAND_ALONE
		pid = obj->core->param_ops.value(name);
#else
		if (type == HASH_PLUG_TYPE)
			pid = HASH_R5_ID;
#endif
	}
	
	return pid;
}

/*
  Initializes object handle by plugin, key, core operations and opaque pointer
  to tree file is going to be opened/created in. */
errno_t obj40_init(obj40_t *obj, reiser4_plug_t *plug,
		   reiser4_core_t *core, object_info_t *info)
{
	aal_assert("umka-1574", obj != NULL);
	aal_assert("umka-1756", plug != NULL);
	aal_assert("umka-1757", info != NULL);

	obj->core = core;
	obj->plug = plug;
	
	aal_memcpy(&obj->info, info, sizeof(*info));

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
	lookup_t res;
	
	aal_assert("umka-1905", obj != NULL);
		
	/* Looking for stat data place by */
	switch ((res = obj40_lookup(obj, &STAT_PLACE(obj)->key,
				    LEAF_LEVEL, FIND_EXACT,
				    STAT_PLACE(obj))))
	{
	case PRESENT:
		return 0;
	default:
		return res;
	}
}

/* Performs lookup and returns result to caller */
lookup_t obj40_lookup(obj40_t *obj, key_entity_t *key,
		      uint8_t level, bias_t bias,
		      place_t *place)
{
	aal_assert("umka-1966", obj != NULL);
	
	return obj->core->tree_ops.lookup(obj->info.tree, key,
					  level, bias, place);
}

/* Reads data from the tree to passed @hint. */
int64_t obj40_read(obj40_t *obj, trans_hint_t *hint) {
	return obj->core->tree_ops.read(obj->info.tree, hint);
}

#ifndef ENABLE_STAND_ALONE
int64_t obj40_convert(obj40_t *obj, conv_hint_t *hint) {
	return obj->core->tree_ops.convert(obj->info.tree, hint);
}

/* Writes data to tree */
int64_t obj40_write(obj40_t *obj, trans_hint_t *hint) {
	return obj->core->tree_ops.write(obj->info.tree, hint);
}

/* Truncates data in tree */
int64_t obj40_truncate(obj40_t *obj, trans_hint_t *hint) {
	return obj->core->tree_ops.truncate(obj->info.tree, hint);
}

/* Inserts passed item hint into the tree. After function is finished, place
   contains the place of the inserted item. */
int64_t obj40_insert(obj40_t *obj, place_t *place,
		     trans_hint_t *hint, uint8_t level)
{
	return obj->core->tree_ops.insert(obj->info.tree,
					  place, hint, level);
}

/* Removes item/unit by @key */
errno_t obj40_remove(obj40_t *obj, place_t *place, trans_hint_t *hint) {
	return obj->core->tree_ops.remove(obj->info.tree, place, hint);
}
#endif
