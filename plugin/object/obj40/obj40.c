/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   obj40.c -- reiser4 file 40 plugins common code. */

#include "obj40.h"

reiser4_core_t *obj40_core = NULL;

/* Initializing instance in the common way, reseting directory. */
errno_t obj40_open(reiser4_object_t *obj) {
	aal_assert("vpf-1827", obj != NULL);
	aal_assert("vpf-1826", obj->info.tree != NULL);
	aal_assert("vpf-1828", reiser4_oplug(obj)->id.type == OBJECT_PLUG_TYPE);
	
	if (obj->info.start.plug->id.group != STAT_ITEM)
		return -EIO;
	
	/* Positioning to the first directory unit. */
	if (reiser4_oplug(obj)->pl.object->reset)
		reiser4_oplug(obj)->pl.object->reset(obj);
	
	return 0;
}

/* Set the file position offset to the given @offset. */
errno_t obj40_seek(reiser4_object_t *obj, uint64_t offset) {
	aal_assert("umka-1968", obj != NULL);

	plug_call(obj->position.plug->pl.key,
		  set_offset, &obj->position, offset);

	return 0;
}

/* Resets file position. The file position is stored inside @obj->position,
   so it just builds new zero offset key.*/
errno_t obj40_reset(reiser4_object_t *obj) {
	aal_assert("umka-1963", obj != NULL);
	
	plug_call(obj->info.object.plug->pl.key, build_generic,
		  &obj->position, KEY_FILEBODY_TYPE, obj40_locality(obj),
		  obj40_ordering(obj), obj40_objectid(obj), 0);

	return 0;
}

/* Returns the file position offset. */
uint64_t obj40_offset(reiser4_object_t *obj) {
	aal_assert("umka-1159", obj != NULL);

	return plug_call(obj->position.plug->pl.key,
			 get_offset, &obj->position);
}


/* Returns file's oid */
oid_t obj40_objectid(reiser4_object_t *obj) {
	aal_assert("umka-1899", obj != NULL);

	return plug_call(obj->info.object.plug->pl.key, 
			 get_objectid, &obj->info.object);
}

/* Returns file's locality  */
oid_t obj40_locality(reiser4_object_t *obj) {
	aal_assert("umka-1900", obj != NULL);
    
	return plug_call(obj->info.object.plug->pl.key, 
			 get_locality, &obj->info.object);
}

/* Returns file's ordering  */
uint64_t obj40_ordering(reiser4_object_t *obj) {
	aal_assert("umka-2334", obj != NULL);

	return plug_call(obj->info.object.plug->pl.key, 
			 get_ordering, &obj->info.object);
}

/* Fetches item info at @place. */
errno_t obj40_fetch_item(reiser4_place_t *place) {
	return plug_call(place->node->plug->pl.node, fetch,
			 place->node, &place->pos, place);
}

/* Checks if @place has valid position. */
bool_t obj40_valid_item(reiser4_place_t *place) {
	uint32_t items = plug_call(place->node->plug->pl.node,
				   items, place->node);
	
	return (place->pos.item < items);
}

/* Checks if passed @place belongs to some object by the short @key. */
lookup_t obj40_belong(reiser4_place_t *place, reiser4_key_t *key) {
	errno_t res;
	
	/* If there is no valid node, it does defenetely not belong to 
	   the object. */
	if (!place->node)
		return ABSENT;
	
	/* Checking if item component in @place->pos is valid one. This is
	   needed because tree_lookup() does not fetch item data at place if it
	   was not found. So, it may point to unexistent item and we should
	   check this here. */
	if (!obj40_valid_item(place))
		return ABSENT;

	/* Fetching item info at @place. This is needed to make sue, that 
	   all @place fields are initialized rigth. Normally it is done by
	   tree_lookup(), if it is sure, that place points to valid postion 
	   in node. This happen if lookup found a key. Otherwise it leaves 
	   place not initialized and caller has to care about it. */
	if ((res = obj40_fetch_item(place)))
		return res;
	
	/* Is the place of the same object? */
	return plug_call(key->plug->pl.key, compshort, 
			 key, &place->key) ? ABSENT : PRESENT;
}

/* Performs lookup and returns result to caller */
lookup_t obj40_find_item(reiser4_object_t *obj, reiser4_key_t *key, 
			 lookup_bias_t bias, coll_func_t func, 
			 coll_hint_t *chint, reiser4_place_t *place)
{
	lookup_hint_t hint;
	
	aal_assert("umka-1966", obj != NULL);
	aal_assert("umka-3111", key != NULL);
	aal_assert("umka-3112", place != NULL);

	hint.key = key;
	hint.level = LEAF_LEVEL;
	
#ifndef ENABLE_MINIMAL
	hint.hint = chint;
	hint.collision = func;
#endif
	
	return obj40_core->tree_ops.lookup(obj->info.tree,
					   &hint, bias, place);
}

/* Takes the next item, perform some checks if it is of the same object. */
lookup_t obj40_next_item(reiser4_object_t *obj) {
	reiser4_place_t place;
	lookup_t res;

	aal_assert("vpf-1833", obj != NULL);

	/* Getting next item into the @place. */
	if ((res = obj40_core->tree_ops.next_item(obj->info.tree,
						  &obj->body, &place)))
	{
		return res;
	}

	/* Check if this item owned by this object. */
	if (obj40_belong(&place, &obj->position) == ABSENT)
		return ABSENT;
	
	/* If @place belongs to the object, copy it to the object body. */
	aal_memcpy(&obj->body, &place, sizeof(place));
	
	/* Correcting unit pos for next body item. */
	if (obj->body.pos.unit == MAX_UINT32)
		obj->body.pos.unit = 0;

	return PRESENT;
}

lookup_t obj40_update_body(reiser4_object_t *obj, obj_func_t adjust_func) {
	uint32_t units;
	errno_t res;

#ifndef ENABLE_MINIMAL
	uint32_t adjust = obj->position.adjust;
#endif
	
	aal_assert("vpf-1344", obj != NULL);
	
	/* Lookup the current object position in the tree. */
	if ((res = obj40_find_item(obj, &obj->position, FIND_EXACT, 
				   NULL, NULL, &obj->body)) < 0)
	{
		return res;
	}
	
	if (res == ABSENT) {
		/* Getting next object item if not valid place & just 
		   check if the current is ours if a valid one. */
		if (!obj40_valid_item(&obj->body)) {
			res = obj40_next_item(obj);
		} else {
			res = obj40_belong(&obj->body, &obj->position);
		}
		
		if (res != PRESENT)
			return res;

#ifndef ENABLE_MINIMAL
		/* No adjusting for the ABSENT result. */
		adjust = 0;
#endif
	}
	
	units = plug_call(obj->body.plug->pl.item->balance,
			  units, &obj->body);
	
	/* Correcting unit pos for next body item. */
	if (obj->body.pos.unit == MAX_UINT32)
		obj->body.pos.unit = 0;

	/* Adjusting current position by key's adjust. This is needed
	   for working fine when a key collision takes place. */
	while (
#ifndef ENABLE_MINIMAL
	       adjust || 
#endif
	       obj->body.pos.unit >= units) 
	{

		if (obj->body.pos.unit >= units) {
			if ((res = obj40_next_item(obj)) != PRESENT)
				return res;
			
			units = plug_call(obj->body.plug->pl.item->balance,
					  units, &obj->body);
			
			continue;
		}
		
#ifndef ENABLE_MINIMAL
		if (adjust) {
			if ((res = adjust_func(obj, NULL)) < 0)
				return res;
			
			if (res) return PRESENT;
			
			adjust--;
		}
#endif		
		obj->body.pos.unit++;
	}
	
	return PRESENT;
}

/* Reads one stat data extension to @data. */
errno_t obj40_read_ext(reiser4_object_t *obj, rid_t id, void *data) {
	stat_hint_t stat;

	aal_memset(&stat, 0, sizeof(stat));

	stat.extmask |= (1 << id);
	if (data) stat.ext[id] = data;
	
	return obj40_load_stat(obj, &stat);
}

/* Gets size field from the stat data */
uint64_t obj40_get_size(reiser4_object_t *obj) {
	sdhint_lw_t lwh;

	if (obj40_read_ext(obj, SDEXT_LW_ID, &lwh))
		return MAX_UINT64;
	
	return lwh.size;
}

/* Loads stat data to passed @hint. */
errno_t obj40_load_stat(reiser4_object_t *obj, stat_hint_t *hint) {
	trans_hint_t trans;

	aal_assert("umka-2553", obj != NULL);

	/* Preparing hint and mask. */
	trans.specific = hint;
	trans.place_func = NULL;
	trans.region_func = NULL;
	trans.shift_flags = SF_DEFAULT;
	
	/* Calling statdata fetch method. */
	if (plug_call(STAT_PLACE(obj)->plug->pl.item->object,
		      fetch_units, STAT_PLACE(obj), &trans) != 1)
	{
		return -EIO;
	}
	
	return 0;
}

#ifndef ENABLE_MINIMAL
/* Saves stat data to passed @hint. */
errno_t obj40_save_stat(reiser4_object_t *obj, stat_hint_t *hint) {
	trans_hint_t trans;
	
	aal_assert("umka-2554", obj != NULL);

	/* Preparing hint and mask */
	trans.specific = hint;
	trans.place_func = NULL;
	trans.region_func = NULL;
	trans.shift_flags = SF_DEFAULT;

	/* Updating stat data. */
	if (plug_call(STAT_PLACE(obj)->plug->pl.item->object,
		      update_units, STAT_PLACE(obj), &trans) <= 0)
	{
		return -EIO;
	}

	return 0;
}

errno_t obj40_inherit(object_info_t *info, object_info_t *parent) {
	int i;
	
	aal_assert("vpf-1855", info != NULL);
	aal_assert("vpf-1855", info->opset.plug[OPSET_OBJ] != NULL);
	aal_assert("vpf-1855", parent != NULL);
	
	info->opset.plug_mask |= (1 << OPSET_OBJ);
	
	/* Get not specified plugins from the parent. Do not set flags into 
	   plug_mask as these plugins are inherited but not specified 
	   explicitly. */
	for (i = 0; i < OPSET_LAST; i++) {
		if (info->opset.plug_mask & (1 << i))
			continue;

		info->opset.plug[i] = parent->opset.plug[i];
	}

	return 0;
}

/* Prepapre unix hint in StatData */
errno_t obj40_stat_unix_init(stat_hint_t *stat, sdhint_unix_t *unixh, 
			     uint64_t bytes, uint64_t rdev) 
{
	aal_assert("vpf-1772", stat != NULL);
	aal_assert("vpf-1773", unixh != NULL);

	/* Unix extension hint initializing */
	if (rdev && bytes) {
		aal_error("Invalid stat data params (rdev or bytes).");
		return -EINVAL;
	} else {
		unixh->rdev = rdev;
		unixh->bytes = bytes;
	}

	unixh->uid = getuid();
	unixh->gid = getgid();
	
	unixh->atime = time(NULL);
	unixh->mtime = unixh->atime;
	unixh->ctime = unixh->atime;

	stat->extmask |= (1 << SDEXT_UNIX_ID);
	stat->ext[SDEXT_UNIX_ID] = unixh;
	
	return 0;
}

errno_t obj40_stat_lw_init(reiser4_object_t *obj, 
			   stat_hint_t *stat, 
			   sdhint_lw_t *lwh, 
			   uint64_t size,  
			   uint32_t nlink, 
			   uint16_t mode)
{
	aal_assert("vpf-1848", obj != NULL);
	aal_assert("vpf-1774", stat != NULL);
	aal_assert("vpf-1775", lwh != NULL);

	/* Light weight hint initializing. */
	lwh->size = size;
	lwh->nlink = nlink;

	/* mode is the bitwise OR between the given mode, the file type mode 
	   and the defaul rwx permissions. The later is 0755 for directories 
	   and 0644 for others. */
	mode |= reiser4_oplug(obj)->id.group == REG_OBJECT ? S_IFREG : 
		reiser4_oplug(obj)->id.group == DIR_OBJECT ? S_IFDIR : 
		reiser4_oplug(obj)->id.group == SYM_OBJECT ? S_IFLNK :
		0;
	
	if (reiser4_oplug(obj)->id.group == DIR_OBJECT)
		mode |= 0755;
	else
		mode |= 0644;

	
	lwh->mode = mode;
	
	stat->extmask |= (1 << SDEXT_LW_ID);
	stat->ext[SDEXT_LW_ID] = lwh;

	return 0;
}

static errno_t obj40_stat_plug_init(reiser4_object_t *obj, 
				    stat_hint_t *stat, 
				    sdhint_plug_t *plugh) 
{
	aal_assert("vpf-1777", stat != NULL);
	aal_assert("vpf-1776", plugh != NULL);
	aal_assert("vpf-1778", obj != NULL);

	/* Get plugins that must exists in the PLUGID extention. */
	obj->info.opset.plug_mask = 
		obj40_core->pset_ops.build_mask(obj->info.tree,
						&obj->info.opset);
	
	if (obj->info.opset.plug_mask) {
		aal_memcpy(plugh, &obj->info.opset, sizeof(*plugh));
		stat->extmask |= (1 << SDEXT_PLUG_ID);
		stat->ext[SDEXT_PLUG_ID] = plugh;
	}

	return 0;
}

static errno_t obj40_stat_sym_init(reiser4_object_t *obj, 
				   stat_hint_t *stat, 
				   char *path)
{
	aal_assert("vpf-1851", obj != NULL);
	aal_assert("vpf-1779", stat != NULL);
	
	if (obj->info.opset.plug[OPSET_OBJ]->id.group != SYM_OBJECT)
		return 0;
	
	if (!path || !aal_strlen(path)) {
		aal_error("No SymLink target point is given.");
		return -EINVAL;
	}
	
	stat->extmask |= (1 << SDEXT_SYMLINK_ID);
	stat->ext[SDEXT_SYMLINK_ID] = path;
	
	return 0;
}

static errno_t obj40_stat_crc_init(reiser4_object_t *obj, 
				   stat_hint_t *stat,
				   sdhint_crypto_t *crch,
				   char *key) 
{
	reiser4_plug_t *plug;
	
	aal_assert("vpf-1847", obj  != NULL);
	aal_assert("vpf-1780", stat != NULL);

	plug = obj->info.opset.plug[OPSET_OBJ];

	/* Plugin must be of the regular file group. */
	if (plug->id.group != REG_OBJECT)
		return 0;
	
	/* Check if cryto is specified. */
	if (obj->info.opset.plug[OPSET_CRYPTO] != (void *)CRYPTO_NONE_ID) {
		if (!key || !aal_strlen(key)) {
			aal_error("No proper key is given: %s.", key);
			return -EINVAL;
		}
		
		stat->extmask |= (1 << SDEXT_CRYPTO_ID);
		stat->ext[SDEXT_CRYPTO_ID] = crch;

		crch->keylen = aal_strlen(key);
		/* To get the fingerprint digest plugin is needed. */
		aal_error("Crypto files cannot be created yet.");
		return -EINVAL;
	}
	
	return 0;
}

/* Create stat data item basing on passed extensions @mask, @size, @bytes,
   @nlinks, @mode and @path for symlinks. Returns error or zero for success. */
errno_t obj40_create_stat(reiser4_object_t *obj, 
			  uint64_t size, uint64_t bytes, 
			  uint64_t rdev, uint32_t nlink, 
			  uint16_t mode, char *str)
{
	sdhint_unix_t unixh;
	sdhint_plug_t plugh;
	sdhint_crypto_t crch;
	sdhint_lw_t lwh;
	stat_hint_t stat;
	trans_hint_t hint;
	lookup_t lookup;
	int64_t res;
	
	aal_assert("vpf-1592", obj != NULL);
	aal_assert("vpf-1593", obj->info.opset.plug[OPSET_STAT] != NULL);
	
	aal_memset(&hint, 0, sizeof(hint));
	
	/* Getting statdata plugin */
	hint.plug = obj->info.opset.plug[OPSET_STAT];

	hint.count = 1;
	hint.shift_flags = SF_DEFAULT;
	
	aal_memcpy(&hint.offset, &obj->info.object, sizeof(hint.offset));
   
	/* Prepapre the StatData hint. */
	aal_memset(&stat, 0, sizeof(stat));
	
	if ((res = obj40_stat_unix_init(&stat, &unixh, bytes, rdev)))
		return res;
	
	if ((res = obj40_stat_lw_init(obj, &stat, &lwh, size, nlink, mode)))
		return res;

	if ((res = obj40_stat_plug_init(obj, &stat, &plugh)))
		return res;
	
	if ((res = obj40_stat_sym_init(obj, &stat, str)))
		return res;

	if ((res = obj40_stat_crc_init(obj, &stat, &crch, str)))
		return res;
	
	hint.specific = &stat;

	/* Lookup place new item to be insert at and insert it to tree */
	switch ((lookup = obj40_find_item(obj, &hint.offset, FIND_CONV, 
					  NULL, NULL, STAT_PLACE(obj))))
	{
	case ABSENT:
		break;
	default:
		return lookup;
	}
	
	/* Insert stat data to tree */
	res = obj40_insert(obj, STAT_PLACE(obj), &hint, LEAF_LEVEL);
	
	/* Reset file. */
	if (reiser4_oplug(obj)->pl.object->reset)
		reiser4_oplug(obj)->pl.object->reset(obj);

	return res < 0 ? res : 0;
}

errno_t obj40_create(reiser4_object_t *obj, object_hint_t *hint) {
	if (hint == NULL)
		return obj40_create_stat(obj, 0, 0, 0, 0, 0, NULL);
	
	return obj40_create_stat(obj, 0, 0, hint->rdev, 0, 
				 hint->mode, hint->str);
}

/* Writes one stat data extension. */
errno_t obj40_write_ext(reiser4_object_t *obj, rid_t id, void *data) {
	stat_hint_t stat;

	aal_memset(&stat, 0, sizeof(stat));

	stat.extmask |= (1 << id);
	if (data) stat.ext[id] = data;

	return obj40_save_stat(obj, &stat);
}

/* Returns extensions mask from stat data item at @place. */
uint64_t obj40_extmask(reiser4_place_t *place) {
	trans_hint_t hint;
	stat_hint_t stat;

	aal_memset(&stat, 0, sizeof(stat));

	/* Preparing hint and mask */
	hint.specific = &stat;
	hint.place_func = NULL;
	hint.region_func = NULL;
	hint.shift_flags = SF_DEFAULT;
	
	/* Calling statdata open method if any */
	if (plug_call(place->plug->pl.item->object,
		      fetch_units, place, &hint) != 1)
	{
		return MAX_UINT64;
	}
	
	return stat.extmask;
}

/* Updates size field in the stat data */
errno_t obj40_set_size(reiser4_object_t *obj, uint64_t size) {
	sdhint_lw_t lwh;
	errno_t res;

	if ((res = obj40_read_ext(obj, SDEXT_LW_ID, &lwh)))
		return res;

	lwh.size = size;
	
	return obj40_write_ext(obj, SDEXT_LW_ID, &lwh);
}

/* Gets nlink field from the stat data */
int64_t obj40_get_nlink(reiser4_object_t *obj, int update) {
	sdhint_lw_t lwh;
	errno_t res;

	if (update) {
		if ((res = obj40_update(obj)))
			return res;
	}
	
	if ((res = obj40_read_ext(obj, SDEXT_LW_ID, &lwh)))
		return res;
	
	return lwh.nlink;
}

/* Updates nlink field in the stat data */
errno_t obj40_set_nlink(reiser4_object_t *obj, uint32_t nlink) {
	sdhint_lw_t lwh;
	errno_t res;

	if ((res = obj40_read_ext(obj, SDEXT_LW_ID, &lwh)))
		return res;

	lwh.nlink = nlink;
	
	return obj40_write_ext(obj, SDEXT_LW_ID, &lwh);
}

/* Gets bytes field from the stat data */
uint64_t obj40_get_bytes(reiser4_object_t *obj) {
	sdhint_unix_t unixh;

	if (obj40_read_ext(obj, SDEXT_UNIX_ID, &unixh))
		return 0;
	
	return unixh.bytes;
}

/* Updates bytes field in the stat data */
errno_t obj40_set_bytes(reiser4_object_t *obj, uint64_t bytes) {
	sdhint_unix_t unixh;
	errno_t res;

	if ((res = obj40_read_ext(obj, SDEXT_UNIX_ID, &unixh)))
		return res;

	unixh.rdev = 0;
	unixh.bytes = bytes;
	
	return obj40_write_ext(obj, SDEXT_UNIX_ID, &unixh);
}

/* Changes nlink field in statdata by passed @value */
static errno_t obj40_inc_link(reiser4_object_t *obj, 
			      int32_t value, int update)
{
	uint32_t nlink = obj40_get_nlink(obj, update);
	return obj40_set_nlink(obj, nlink + value);
}

/* Removes object stat data. */
errno_t obj40_clobber(reiser4_object_t *obj) {
	errno_t res;
	trans_hint_t hint;
	
	aal_assert("umka-2546", obj != NULL);

	if ((res = obj40_update(obj)))
		return res;

	aal_memset(&hint, 0, sizeof(hint));
	hint.count = 1;
	hint.shift_flags = SF_DEFAULT;
	STAT_PLACE(obj)->pos.unit = MAX_UINT32;
	
	return obj40_remove(obj, STAT_PLACE(obj), &hint);
}

/* Enumerates object metadata. */
errno_t obj40_metadata(reiser4_object_t *obj, 
		       place_func_t place_func,
		       void *data)
{
	errno_t res;
	
	aal_assert("umka-2549", obj != NULL);
	aal_assert("umka-2550", place_func != NULL);

	if ((res = obj40_update(obj)))
		return res;

	return place_func(STAT_PLACE(obj), data);
}

errno_t obj40_link(reiser4_object_t *obj) {
	return obj40_inc_link(obj, 1, 1);
}

errno_t obj40_unlink(reiser4_object_t *obj) {
	return obj40_inc_link(obj, -1, 1);
}

/* Check if linked. Needed to let higher API levels know, that file has
   zero links and may be clobbered. */
bool_t obj40_linked(reiser4_object_t *entity) {
	return obj40_get_nlink(entity, 1) != 0;
}
#endif

/* Makes sure, that passed place points to right location in tree by means of
   calling tree_lookup() for its key. This is needed, because items may move to
   somewhere after each balancing. */
errno_t obj40_update(reiser4_object_t *obj) {
	lookup_t res;
	
	aal_assert("umka-1905", obj != NULL);
		
	/* Looking for stat data place by */
	switch ((res = obj40_find_item(obj, &obj->info.object,
				       FIND_EXACT, NULL, NULL,
				       STAT_PLACE(obj))))
	{
	case PRESENT:
		return 0;
	case ABSENT:
		return -EIO;
	default:
		return res;
	}
}

/* Reads data from the tree to passed @hint. */
int64_t obj40_read(reiser4_object_t *obj, trans_hint_t *hint, 
		   void *buff, uint64_t off, uint64_t count)
{	
	/* Preparing hint to be used for calling read with it. Here we
	   initialize @count -- number of bytes to read, @specific -- pointer to
	   buffer data will be read into, and pointer to tree instance, file is
	   opened on. */ 
	aal_memset(hint, 0, sizeof(*hint));
	
	/* Initializing offset data must be read from. This is current file
	   offset, so we use @reg->position. */
	aal_memcpy(&hint->offset, &obj->position, sizeof(hint->offset));
	plug_call(hint->offset.plug->pl.key, set_offset, &hint->offset, off);
	
	hint->count = count;
	hint->specific = buff;
	
	return obj40_core->flow_ops.read(obj->info.tree, hint);
}

#ifndef ENABLE_MINIMAL
int64_t obj40_convert(reiser4_object_t *obj, conv_hint_t *hint) {
	return obj40_core->flow_ops.convert(obj->info.tree, hint);
}

/* Truncates data in tree */
int64_t obj40_truncate(reiser4_object_t *obj, uint64_t n,
		       reiser4_plug_t *item_plug)
{
	trans_hint_t hint;
	uint64_t size;
	uint64_t bytes;
	errno_t res;
	
	aal_assert("vpf-1882", obj != NULL);
	aal_assert("vpf-1884", item_plug != NULL);
	
	if ((res = obj40_update(obj)))
		return res;
	
	size = obj40_get_size(obj);
	
	if (size == n)
		return 0;
	
	if (n > size) {
		if ((res = obj40_write(obj, &hint, NULL, size, n - size, 
				       item_plug, NULL, NULL)) < 0)
		{
			return res;
		}
		
		bytes = hint.bytes;
	} else {
		aal_memset(&hint, 0, sizeof(hint));
		
		/* Preparing key of the data to be truncated. */
		aal_memcpy(&hint.offset, &obj->position, sizeof(hint.offset));
		
		plug_call(obj->info.object.plug->pl.key,
			  set_offset, &hint.offset, n);
		
		/* Removing data from the tree. */
		hint.count = MAX_UINT64;
		hint.shift_flags = SF_DEFAULT;
		hint.data = obj->info.tree;
		hint.plug = item_plug;
		
		res = obj40_core->flow_ops.truncate(obj->info.tree, &hint);
		if (res < 0) return res;

		bytes = -hint.bytes;
	}
	
	/* Updating stat data fields. */
	return obj40_touch(obj, n - size, bytes);
}

/* Inserts passed item hint into the tree. After function is finished, place
   contains the place of the inserted item. */
int64_t obj40_insert(reiser4_object_t *obj, 
		     reiser4_place_t *place,
		     trans_hint_t *hint, 
		     uint8_t level)
{
	return obj40_core->tree_ops.insert(obj->info.tree,
					   place, hint, level);
}

/* Removes item/unit by @key */
errno_t obj40_remove(reiser4_object_t *obj, 
		     reiser4_place_t *place,
		     trans_hint_t *hint)
{
	return obj40_core->tree_ops.remove(obj->info.tree,
					   place, hint);
}

/* This fucntion implements object item enumerator function. 
   Used for getting directory metadata on packing, etc. */
errno_t obj40_traverse(reiser4_object_t *obj, 
		       place_func_t place_func, 
		       obj_func_t obj_func,
		       void *data)
{
	errno_t res;
	
	aal_assert("umka-1712", obj != NULL);
	aal_assert("umka-1713", place_func != NULL);
	
	/* Calculating stat data item. */
	if ((res = obj40_metadata(obj, place_func, data)))
		return res;

	/* Update current body item coord. */
	if ((res = obj40_update_body(obj, obj_func)) != PRESENT)
		return res == ABSENT ? 0 : res;

	/* Loop until all items are enumerated. */
	while (1) {
		/* Calling callback function. */
		if ((res = place_func(&obj->body, data)))
			return res;

		/* Getting next item. */
		if ((res = obj40_next_item(obj)) < 0)
			return res;
		
		if (res == ABSENT)
			return 0;
	}

	return 0;
}

/* File data enumeration related stuff. */
typedef struct layout_hint {
	void *data;
	region_func_t region_func;
} layout_hint_t;

static errno_t cb_item_layout(blk_t start, count_t width, void *data) {
	layout_hint_t *hint = (layout_hint_t *)data;
	return hint->region_func(start, width, hint->data);
}

/* This fucntion implements hashed directory enumerator function.
   It is used when calculating fargmentation, prining. */
errno_t obj40_layout(reiser4_object_t *obj,
		     region_func_t region_func,
		     obj_func_t obj_func,
		     void *data)
{
	layout_hint_t hint;
	errno_t res;

	aal_assert("umka-1473", obj != NULL);
	aal_assert("umka-1474", region_func != NULL);
	
	/* Update current body item coord. */
	if ((res = obj40_update_body(obj, obj_func)) != PRESENT)
		return res == ABSENT ? 0 : res;

	/* Prepare layout hint. */
	hint.data = data;
	hint.region_func = region_func;

	/* Loop until all items are enumerated. */
	while (1) {
		reiser4_place_t *place = &obj->body;
		
		if (obj->body.plug->pl.item->object->layout) {
			/* Calling item's layout method */
			if ((res = plug_call(place->plug->pl.item->object,
					     layout, place, cb_item_layout,
					     &hint)))
			{
				return res;
			}
		} else {
			/* Layout method is not implemented. Counting item
			   itself. */
			blk_t blk = place_blknr(place);
			
			if ((res = cb_item_layout(blk, 1, &hint)))
				return res;
		}

		/* Getting next item. */
		if ((res = obj40_next_item(obj)) < 0)
			return res;

		/* Object is over? */
		if (res == ABSENT)
			return 0;
	}
	
	return 0;
}

/* Writes passed data to the file. Returns amount of written bytes. */
int64_t obj40_write(reiser4_object_t *obj, trans_hint_t *hint, void *buff, 
		    uint64_t off, uint64_t count, reiser4_plug_t *item_plug, 
		    place_func_t func, void *data)
{
	/* Preparing hint to be used for calling write method. This is
	   initializing @count - number of bytes to write, @specific - buffer to
	   write into and @offset -- file offset data must be written at. */
	aal_memset(hint, 0, sizeof(*hint));
	hint->count = count;
	
	hint->specific = buff;
	hint->shift_flags = SF_DEFAULT;
	hint->place_func = func;
	hint->plug = item_plug;
	hint->data = data;
	
	aal_memcpy(&hint->offset, &obj->position, sizeof(hint->offset));
	plug_call(hint->offset.plug->pl.key, set_offset, &hint->offset, off);
	
	/* Write data to tree. */
	return obj40_core->flow_ops.write(obj->info.tree, hint);
}

errno_t obj40_touch(reiser4_object_t *obj, int64_t size, int64_t bytes) {
	uint64_t fbytes;
	uint64_t fsize;
	errno_t res;
	
	/* Updating the SD place and update size, bytes there. */
	if ((res = obj40_update(obj)))
		return res;
	
	fsize = obj40_get_size(obj);
	fbytes = obj40_get_bytes(obj);

	/* Update size & bytes unless they do not change. */
	if (size && (res = obj40_set_size(obj, fsize + size)))
		return res;

	if (bytes && (res = obj40_set_bytes(obj, fbytes + bytes)))
		return res;
	
	return 0;
}
#endif
