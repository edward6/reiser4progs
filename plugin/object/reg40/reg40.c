/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40.c -- reiser4 default regular file plugin. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE
#  include <time.h>
#  include <unistd.h>
#endif

#include "reg40.h"

extern reiser4_plug_t reg40_plug;

static errno_t reg40_truncate(object_entity_t *entity,
			      uint64_t n);


static uint64_t reg40_size(object_entity_t *entity) {
	reg40_t *reg;

	aal_assert("umka-2278", entity != NULL);
	
	reg = (reg40_t *)entity;
	
#ifndef ENABLE_STAND_ALONE
	/* Updating stat data place */
	if (obj40_update(&reg->obj, STAT_PLACE(&reg->obj)))
		return -EINVAL;
#endif

	return obj40_get_size(&reg->obj);
}

#ifndef ENABLE_STAND_ALONE
/* Returns plugin (tail or extent) for next write operation basing on passed
   @size -- new file size. This function will use tail policy plugin for find
   out what next item should be writen. */
static reiser4_plug_t *reg40_bplug(object_entity_t *entity, uint32_t size) {
	/* FIXME-UMKA: Here will be tail policy plugin calling in odrer to
	   determine what body plugin should be used when file size has reached
	   passed @size. */
	return core->factory_ops.ifind(ITEM_PLUG_TYPE,
				       ITEM_EXTENT40_ID);
}
#endif

/* Updates body place in correspond to file offset */
static lookup_t reg40_next(reg40_t *reg) {
	lookup_t res;
	place_t place;
	key_entity_t key;

	aal_assert("umka-1161", reg != NULL);
	
	/* Building key to be searched by current offset */
	plug_call(STAT_KEY(&reg->obj)->plug->o.key_ops, build_gener,
		  &key, KEY_FILEBODY_TYPE, obj40_locality(&reg->obj), 
		  obj40_ordering(&reg->obj), obj40_objectid(&reg->obj),
		  reg->offset);

	/* Getting the next body item from the tree */
	if ((res = obj40_lookup(&reg->obj, &key, LEAF_LEVEL,
				&reg->body)) == PRESENT)
	{
		if (reg->body.pos.unit == MAX_UINT32)
			reg->body.pos.unit = 0;
	}

	return res;
}

/* Resets file position. That is it searches first body item and sets file's
   offset to zero. */
errno_t reg40_reset(object_entity_t *entity) {
	aal_assert("umka-1963", entity != NULL);

	((reg40_t *)entity)->offset = 0;
	return 0;
}

/* Reads @n bytes to passed buffer @buff */
static int32_t reg40_read(object_entity_t *entity, 
			  void *buff, uint32_t n)
{
	errno_t res;
	reg40_t *reg;
	
#ifndef ENABLE_STAND_ALONE
	uint64_t size;
	uint64_t offset;
#else
	uint32_t size;
	uint32_t offset;
#endif
	
	place_t *place;
	uint32_t read, chunk;

	aal_assert("umka-1183", buff != NULL);
	aal_assert("umka-1182", entity != NULL);

	reg = (reg40_t *)entity;
	size = reg40_size(entity);

	if (reg->offset > size)
		return -EINVAL;
	
	/* The file has not data or nothing can be read */
	if (reg->offset == size)
		return 0;

	if (n > size - reg->offset)
		n = size - reg->offset;

	for (read = 0; read < n; ) {

		if (reg40_next(reg) != PRESENT)
			return read;

		place = &reg->body;
		chunk = n - read;

		/* Calculating in-item offset */
		offset = reg->offset - plug_call(place->key.plug->o.key_ops,
						 get_offset, &place->key);

		/* Calling body item's read() method */
		chunk = plug_call(place->plug->o.item_ops, read,
				  place, buff, offset, chunk);

		aal_assert("umka-2216", chunk > 0);
		
		reg->offset += chunk;
		buff += chunk; read += chunk;
	}

	return read;
}

/* Opening reg40 by statdata place passed in @place */
static object_entity_t *reg40_open(object_info_t *info) {
	reg40_t *reg;

	aal_assert("umka-1163", info != NULL);
	aal_assert("umka-1164", info->tree != NULL);
    	
	if (info->start.plug->id.group != STATDATA_ITEM)
		return NULL;

	if (obj40_pid(&info->start) != reg40_plug.id.id)
		return NULL;

	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;

	/* Initializing file handle */
	obj40_init(&reg->obj, &reg40_plug, core, info);

	/* Reseting file (setting offset to 0) */
	reg40_reset((object_entity_t *)reg);

#ifndef ENABLE_STAND_ALONE
	reg->bplug = reg40_bplug((object_entity_t *)reg, 0);
#endif

	return (object_entity_t *)reg;
}

#ifndef ENABLE_STAND_ALONE
errno_t reg40_create_stat(obj40_t *obj, rid_t pid) {
	statdata_hint_t stat;
	sdext_lw_hint_t lw_ext;
	create_hint_t stat_hint;
	reiser4_plug_t *stat_plug;
	sdext_unix_hint_t unix_ext;
	
	/* Getting statdata plugin */
	if (!(stat_plug = core->factory_ops.ifind(ITEM_PLUG_TYPE, pid))) {
		aal_exception_error("Can't find stat data item plugin "
				    "by its id 0x%x.", pid);
		return -EINVAL;
	}

	/* Initializing the stat data hint */
	aal_memset(&stat_hint, 0, sizeof(stat_hint));

	stat_hint.count = 1;
	stat_hint.plug = stat_plug;
	
	plug_call(obj->info.object.plug->o.key_ops, assign, 
		  &stat_hint.key, &obj->info.object);
    
	/* Initializing stat data item hint. */
	stat.extmask = 1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID;
    
	lw_ext.nlink = 0;
	lw_ext.mode = S_IFREG | 0755;

	/* This should be modified by write function later */
	lw_ext.size = 0;
    
	unix_ext.rdev = 0;
	unix_ext.bytes = 0;

	unix_ext.uid = getuid();
	unix_ext.gid = getgid();
	
	unix_ext.atime = time(NULL);
	unix_ext.mtime = unix_ext.atime;
	unix_ext.ctime = unix_ext.atime;

	aal_memset(&stat.ext, 0, sizeof(stat.ext));
    
	stat.ext[SDEXT_LW_ID] = &lw_ext;
	stat.ext[SDEXT_UNIX_ID] = &unix_ext;

	stat_hint.type_specific = &stat;

	if (obj40_lookup(obj, &stat_hint.key, LEAF_LEVEL, 
			 STAT_PLACE(obj)) != ABSENT)
		return -EINVAL;
	
	/* Insert statdata item into the tree */
	return obj40_insert(obj, &stat_hint, LEAF_LEVEL, STAT_PLACE(obj));
}

/* Creating the file described by pased @hint */
static object_entity_t *reg40_create(object_info_t *info,
				     object_hint_t *hint)
{
	reg40_t *reg;
	uint64_t ordering;
	oid_t objectid, locality;
	
	aal_assert("umka-1169", info != NULL);
	aal_assert("vpf-1093",  info->tree != NULL);
	aal_assert("umka-1738", hint != NULL);

	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;
	
	reg->offset = 0;
	
	/* Preparing dir oid and locality */
	locality = plug_call(info->object.plug->o.key_ops,
			     get_locality, &info->object);
	
	objectid = plug_call(info->object.plug->o.key_ops,
			     get_objectid, &info->object);

	ordering = plug_call(info->object.plug->o.key_ops,
			     get_ordering, &info->object);
	
	/* Key contains valid locality and objectid only, build start key */
	plug_call(info->object.plug->o.key_ops, build_gener,
		  &info->object, KEY_STATDATA_TYPE, locality,
		  ordering, objectid, 0);
	
	/* Initializing file handle */
	obj40_init(&reg->obj, &reg40_plug, core, info);
	
	if (reg40_create_stat(&reg->obj, hint->statdata))
	    goto error_free_reg;

	aal_memcpy(&info->start, STAT_PLACE(&reg->obj),
		   sizeof(info->start));
	
	reg->bplug = reg40_bplug((object_entity_t *)reg, 0);
	return (object_entity_t *)reg;

 error_free_reg:
	aal_free(reg);
	return NULL;
}

static key_entity_t *reg40_origin(object_entity_t *entity) {
	return STAT_KEY(&((reg40_t *)entity)->obj);
}

static uint32_t reg40_links(object_entity_t *entity) {
	reg40_t *reg;
	
	aal_assert("umka-2296", entity != NULL);

	reg = (reg40_t *)entity;
	
	if (obj40_update(&reg->obj, STAT_PLACE(&reg->obj)))
		return -EINVAL;
	
	return obj40_get_nlink(&reg->obj);
}

static errno_t reg40_link(object_entity_t *entity) {
	reg40_t *reg;
	
	aal_assert("umka-1912", entity != NULL);

	reg = (reg40_t *)entity;
	
	/* Updating stat data place */
	if (obj40_update(&reg->obj, STAT_PLACE(&reg->obj)))
		return -EINVAL;
	
	return obj40_link(&reg->obj, 1);
}

static errno_t reg40_unlink(object_entity_t *entity) {
	reg40_t *reg;
	
	aal_assert("umka-1911", entity != NULL);

	reg = (reg40_t *)entity;
	
	if (obj40_update(&reg->obj, STAT_PLACE(&reg->obj)))
		return -EINVAL;
	
	return obj40_link(&reg->obj, -1);
}

static uint32_t reg40_chunk(reg40_t *reg) {
	return core->tree_ops.maxspace(reg->obj.info.tree);
}

/* Writes passed data to the file */
static int32_t reg40_put(object_entity_t *entity, 
			 void *buff, uint32_t n)
{
	errno_t res;
	reg40_t *reg;
	place_t *place;
	
	uint64_t size;
	uint32_t atime;

	uint32_t written;
	uint32_t maxspace;

	sdext_unix_hint_t unix_hint;

	reg = (reg40_t *)entity;
	maxspace = reg40_chunk(reg);
	
	for (written = 0; written < n; ) {
		place_t place;
		uint32_t level;
		uint64_t offset;

		create_hint_t hint;
		reiser4_plug_t *plug;
		key_entity_t maxreal_key;

		/* Preparing @hint->key */
		plug = STAT_KEY(&reg->obj)->plug;
		
		plug_call(plug->o.key_ops, build_gener, &hint.key,
			  KEY_FILEBODY_TYPE,obj40_locality(&reg->obj),
			  obj40_ordering(&reg->obj), obj40_objectid(&reg->obj),
			  reg->offset);

		/* Setting up @hint */
		hint.count = n - written;
		
		if (hint.count > maxspace)
			hint.count = maxspace;
		
		hint.type_specific = buff;
		hint.plug = reg40_bplug(entity, reg->offset + n);

		/* Extent related fields */
		hint.offset = 0;
		hint.tree = reg->obj.info.tree;

		/* Lookup place data will be inserted at */
		switch (obj40_lookup(&reg->obj, &hint.key,
				     LEAF_LEVEL, &place))
		{
		case FAILED:
			return -EIO;
		case PRESENT:
			if (hint.plug->id.group == TAIL_ITEM) {
				/* Checking if we need write chunk by chunk in
				   odrer to rewrite tail correctly in the case
				   we write the tail, that overlaps two
				   neighbour tails by key. */
				plug_call(place.plug->o.item_ops, maxreal_key,
					  &place, &maxreal_key);

				offset = plug_call(hint.key.plug->o.key_ops,
						   get_offset, &maxreal_key);

				/* Rewritting only tails' last part */
				if (reg->offset + hint.count > offset + 1)
					hint.count = (offset + 1) - reg->offset;
			} else {
				hint.offset = plug_call(hint.key.plug->o.key_ops,
							get_offset, &hint.key);
			
				hint.offset -= plug_call(place.key.plug->o.key_ops,
							 get_offset, &place.key);
			}
		default:
			break;
		}

		level = hint.plug->id.group == EXTENT_ITEM ?
			LEAF_LEVEL + 1 : LEAF_LEVEL;

		/* Inserting data to the tree */
		if ((res = obj40_insert(&reg->obj, &hint, level, &place)))
			return res;

		aal_memcpy(&reg->body, &place, sizeof(reg->body));

		buff += hint.count;
		written += hint.count;
		reg->offset += hint.count;
	}
	
	/* Updating stat data place */
	if (obj40_update(&reg->obj, STAT_PLACE(&reg->obj)))
		return -EINVAL;
	
	/* Updating stat data fields */
	size = obj40_get_size(&reg->obj);

	/* Updating size if new file offset is further than size. This means,
	   that file realy got some data additionaly, not only got rewtitten
	   something. */
	if (reg->offset > size) {
		if ((res = obj40_set_size(&reg->obj, reg->offset)))
			return res;
	}
	
	place = STAT_PLACE(&reg->obj);
	
	if ((res = obj40_read_ext(place, SDEXT_UNIX_ID, &unix_hint)))
		return res;
	
	atime = time(NULL);
	
	unix_hint.atime = atime;
	unix_hint.mtime = atime;

	if (reg->offset > unix_hint.bytes)
		unix_hint.bytes = reg->offset;

	if ((res = obj40_write_ext(place, SDEXT_UNIX_ID, &unix_hint)))
		return res;
	
	return written;
}

/* Takes care about holes */
errno_t reg40_holes(object_entity_t *entity) {
	errno_t res;
	reg40_t *reg;
	uint64_t size;

	reg = (reg40_t *)entity;

	/* Updating stat data place */
	if (obj40_update(&reg->obj, STAT_PLACE(&reg->obj)))
		return -EINVAL;
	
	size = obj40_get_size(&reg->obj);

	/* Writing holes */
	if (reg->offset > size) {
		void *holes;
		int32_t hole;
		int32_t written;

		hole = reg->offset - size;
		reg->offset -= hole;

		for (written = 0; written < hole; ) {
			uint32_t chunk;

			chunk = hole - written;

			if (chunk > reg40_chunk(reg))
				chunk = reg40_chunk(reg);
			
			if (!(holes = aal_calloc(chunk, 0)))
				return -ENOMEM;

			if (reg40_put(entity, holes, chunk) < 0) {
				aal_free(holes);
				return -EIO;
			}

			aal_free(holes);
			written += chunk;
		}
	}

	return 0;
}

static errno_t reg40_cut(object_entity_t *entity) {
	errno_t res;
	reg40_t *reg;
	uint32_t cut;
	uint64_t size;
	
	reg = (reg40_t *)entity;
	
	/* Updating stat data place */
	if (obj40_update(&reg->obj, STAT_PLACE(&reg->obj)))
		return -EINVAL;
	
	/* Getting file size */
	size = obj40_get_size(&reg->obj);

	if (reg->offset >= size)
		return 0;

	cut = size - reg->offset;
	
	while (cut) {
		place_t place;
		key_entity_t key;

		key.plug = STAT_KEY(&reg->obj)->plug;
		
		plug_call(key.plug->o.key_ops, build_gener, &key,
			  KEY_FILEBODY_TYPE, obj40_locality(&reg->obj),
			  obj40_ordering(&reg->obj), obj40_objectid(&reg->obj),
			  size - 1);
		
		if (obj40_lookup(&reg->obj, &key, LEAF_LEVEL,
				 &place) != PRESENT)
		{
			return -EINVAL;
		}

		if (core->tree_ops.fetch(reg->obj.info.tree, &place))
			return -EINVAL;

		/* Check if we can remove whole item at @place */
		if (place.len <= cut) {
			if (core->tree_ops.remove(reg->obj.info.tree,
						  &place, 1))
			{
				aal_exception_error("Can't remove item "
						    "from object 0x%llx.",
						    obj40_objectid(&reg->obj));
				return -EINVAL;
			}

			cut -= place.len;
			size -= place.len;
		} else {
			place.pos.unit = place.len - cut;

			if (core->tree_ops.remove(reg->obj.info.tree,
						  &place, cut))
			{
				aal_exception_error("Can't remove units "
						    "from object 0x%llx.",
						    obj40_objectid(&reg->obj));
				return -EINVAL;
			}
			
			cut = 0;
			size -= cut;
		}
	}

	return 0;
}

/* Writes "n" bytes from "buff" to passed file */
static int32_t reg40_write(object_entity_t *entity, 
			   void *buff, uint32_t n) 
{
	int32_t res;
	
	aal_assert("umka-2280", buff != NULL);
	aal_assert("umka-2281", entity != NULL);

	if ((res = reg40_holes(entity)))
		return res;
	
	if ((res = reg40_put(entity, buff, n)) < 0)
		return res;
	
	return res;
}

static errno_t reg40_truncate(object_entity_t *entity,
			      uint64_t n)
{
	errno_t res;
	reg40_t *reg;
	uint64_t size;
	uint64_t offset;

	reg = (reg40_t *)entity;
	
	/* Updating stat data place */
	if (obj40_update(&reg->obj, STAT_PLACE(&reg->obj)))
		return -EINVAL;
	
	/* Getting file size */
	size = obj40_get_size(&reg->obj);

	if (n == size)
		return 0;

	/* Saving current file offset */
	offset = reg->offset;
		
	/* Checking if truncate will increase file size */
	reg->offset = n;
	
	if (n > size) {
		if ((res = reg40_holes(entity)))
			goto error_restore_offset;
	} else {
		if ((res = reg40_cut(entity)))
			goto error_restore_offset;
	}

	if (obj40_update(&reg->obj, STAT_PLACE(&reg->obj)))
		return -EINVAL;

	obj40_set_size(&reg->obj, n);

	/* In the sace of tails bytes should be the same as size field in stat
	   data. */
	return obj40_set_bytes(&reg->obj, n);

 error_restore_offset:
	reg->offset = offset;
	return res;
}

static errno_t reg40_clobber(object_entity_t *entity) {
	errno_t res;
	reg40_t *reg;
	
	aal_assert("umka-2299", entity != NULL);

	if ((res = reg40_truncate(entity, 0)))
		return res;

	reg = (reg40_t *)entity;
	
	if (obj40_update(&reg->obj, STAT_PLACE(&reg->obj)))
		return -EINVAL;

	return obj40_remove(&reg->obj, STAT_PLACE(&reg->obj), 1);
}

struct layout_hint {
	object_entity_t *entity;
	block_func_t block_func;
	void *data;
};

typedef struct layout_hint layout_hint_t;

static errno_t callback_item_data(void *object, uint64_t start,
				  uint64_t count, void *data)
{
	blk_t blk;
	errno_t res;
	
	place_t *place = (place_t *)object;
	layout_hint_t *hint = (layout_hint_t *)data;

	for (blk = start; blk < start + count; blk++) {
		if ((res = hint->block_func(hint->entity, blk, hint->data)))
			return res;
	}

	return 0;
}

/* Implements reg40 layout function. It traverses all blocks belong to the file
   and needed for calculating fragmentation, printing, etc. */
static errno_t reg40_layout(object_entity_t *entity,
			    block_func_t block_func,
			    void *data)
{
	errno_t res;
	reg40_t *reg;
	uint64_t size;
	key_entity_t key;
	layout_hint_t hint;
	
	aal_assert("umka-1471", entity != NULL);
	aal_assert("umka-1472", block_func != NULL);

	reg = (reg40_t *)entity;
	
	if ((size = reg40_size(entity)) == 0)
		return 0;

	if (obj40_update(&reg->obj, &reg->body))
		return -EINVAL;

	hint.data = data;
	hint.entity = entity;
	hint.block_func = block_func;
		
	while (reg->offset < size) {
		place_t *place;
		
		if (reg40_next(reg) != PRESENT)
			break;
		
		place = &reg->body;
		
		if (place->plug->o.item_ops->layout) {
			if ((res = plug_call(place->plug->o.item_ops, layout,
					     place, callback_item_data, &hint)))
			{
				return res;
			}
		} else {
			if ((res = callback_item_data(place, place->con.blk,
						      1, &hint)))
			{
				return res;
			}
		}
		
		plug_call(place->plug->o.item_ops, maxreal_key,
			  place, &key);
		
		reg->offset = plug_call(key.plug->o.key_ops,
					get_offset, &key) + 1;
	}
	
	return 0;
}

/* Implements reg40 metadata function. It traverses items belong to the file and
   needed for calculating fragmentation, printing, etc. */
static errno_t reg40_metadata(object_entity_t *entity,
			      place_func_t place_func,
			      void *data)
{
	errno_t res;
	key_entity_t key;
	uint64_t size, offset;
	
	reg40_t *reg = (reg40_t *)entity;
	
	aal_assert("umka-1716", entity != NULL);
	aal_assert("umka-1717", place_func != NULL);

	if ((res = place_func(entity, STAT_PLACE(&reg->obj), data)))
		return res;

	if ((size = reg40_size(entity)) == 0)
		return 0;
	
	if (obj40_update(&reg->obj, &reg->body))
		return -EINVAL;

	while (reg->offset < size) {
		place_t *place;
		
		if (reg40_next(reg) != PRESENT)
			break;
		
		place = &reg->body;
			
		if ((res = place_func(entity, &reg->body, data)))
			return res;

		plug_call(place->plug->o.item_ops, maxreal_key,
			  place, &key);

		reg->offset = plug_call(key.plug->o.key_ops,
					get_offset, &key) + 1;
	}
	
	return 0;
}

extern object_entity_t *reg40_realize(object_info_t *info);

extern errno_t reg40_check_struct(object_entity_t *object,
				  place_func_t place_func,
				  region_func_t region_func,
				  void *data, uint8_t mode);

#endif

static errno_t reg40_seek(object_entity_t *entity, 
			  uint64_t offset) 
{
	aal_assert("umka-1968", entity != NULL);

	((reg40_t *)entity)->offset = offset;
	return 0;
}

static void reg40_close(object_entity_t *entity) {
	aal_assert("umka-1170", entity != NULL);

	aal_free(entity);
}

static uint64_t reg40_offset(object_entity_t *entity) {
	aal_assert("umka-1159", entity != NULL);
	return ((reg40_t *)entity)->offset;
}

static reiser4_object_ops_t reg40_ops = {
#ifndef ENABLE_STAND_ALONE
	.create	        = reg40_create,
	.write	        = reg40_write,
	.truncate       = reg40_truncate,
	.layout         = reg40_layout,
	.metadata       = reg40_metadata,
	.link           = reg40_link,
	.unlink         = reg40_unlink,
	.links          = reg40_links,
	.clobber        = reg40_clobber,
	.origin         = reg40_origin,
	.realize        = reg40_realize,
	.check_struct   = reg40_check_struct,

	.check_attach 	= NULL,
	.add_entry      = NULL,
	.rem_entry      = NULL,
	.attach         = NULL,
	.detach         = NULL,
#endif
	.lookup	        = NULL,
	.follow         = NULL,
	.readdir        = NULL,
	.telldir        = NULL,
	.seekdir        = NULL,
		
	.open	        = reg40_open,
	.close	        = reg40_close,
	.reset	        = reg40_reset,
	.seek	        = reg40_seek,
	.offset	        = reg40_offset,
	.size           = reg40_size,
	.read	        = reg40_read,
};

reiser4_plug_t reg40_plug = {
	.cl    = CLASS_INIT,
	.id    = {OBJECT_FILE40_ID, FILE_OBJECT, OBJECT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "reg40",
	.desc  = "Regular file for reiser4, ver. " VERSION,
#endif
	.o = {
		.object_ops = &reg40_ops
	}
};

static reiser4_plug_t *reg40_start(reiser4_core_t *c) {
	core = c;
	return &reg40_plug;
}

plug_register(reg40, reg40_start, NULL);
