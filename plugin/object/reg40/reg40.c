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

uint64_t reg40_offset(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;
	
	aal_assert("umka-1159", entity != NULL);

	return plug_call(reg->offset.plug->o.key_ops,
			 get_offset, &reg->offset);
}

static uint64_t reg40_size(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;

	aal_assert("umka-2278", entity != NULL);
	
	if (obj40_update(&reg->obj))
		return -EINVAL;

	return obj40_get_size(&reg->obj);
}

errno_t reg40_seek(object_entity_t *entity, 
		   uint64_t offset) 
{
	reg40_t *reg = (reg40_t *)entity;
	
	aal_assert("umka-1968", entity != NULL);

	plug_call(reg->offset.plug->o.key_ops,
		  set_offset, &reg->offset, offset);

	return 0;
}

static void reg40_close(object_entity_t *entity) {
	aal_assert("umka-1170", entity != NULL);
	aal_free(entity);
}

#ifndef ENABLE_STAND_ALONE
/* Returns plugin (tail or extent) for next write operation basing on passed
   @size -- new file size. This function will use tail policy plugin for find
   out what next item should be writen. */
static reiser4_plug_t *reg40_bplug(object_entity_t *entity, uint64_t size) {
	/* FIXME-UMKA: Here will be tail policy plugin calling in odrer to
	   determine what body plugin should be used when file size has reached
	   passed @size. */
	return core->factory_ops.ifind(ITEM_PLUG_TYPE,
				       ITEM_EXTENT40_ID);
}
#endif

/* Updates body place in correspond to file offset */
static errno_t reg40_update(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;

	aal_assert("umka-1161", entity != NULL);
	
	/* Getting the next body item from the tree */
	switch (obj40_lookup(&reg->obj, &reg->offset,
			     LEAF_LEVEL, &reg->body))
	{
	case PRESENT:
		return 0;
	default:
		return -EINVAL;
	}
}

/* Resets file position. That is it searches first body item and sets file's
   offset to zero. */
errno_t reg40_reset(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;
	
	aal_assert("umka-1963", entity != NULL);
	
	plug_call(STAT_KEY(&reg->obj)->plug->o.key_ops, build_gener,
		  &reg->offset, KEY_FILEBODY_TYPE, obj40_locality(&reg->obj),
		  obj40_ordering(&reg->obj), obj40_objectid(&reg->obj), 0);

	return 0;
}

/* Reads @n bytes to passed buffer @buff */
static int32_t reg40_read(object_entity_t *entity, 
			  void *buff, uint32_t n)
{
	errno_t res;
	reg40_t *reg;
	uint64_t size;
	uint32_t read;
	uint32_t chunk;
	uint64_t offset;

	aal_assert("umka-1183", buff != NULL);
	aal_assert("umka-1182", entity != NULL);

	reg = (reg40_t *)entity;
	size = reg40_size(entity);
	offset = reg40_offset(entity);

	if (offset > size)
		return -EINVAL;
	
	/* The file has not data or nothing can be read */
	if (offset == size)
		return 0;

	if (n > size - offset)
		n = size - offset;

	for (read = 0; read < n; ) {

		if ((res = reg40_update(entity)))
			return res;

		chunk = n - read;

		/* Calculating in-item offset */
		offset = reg40_offset(entity);;

		offset -= plug_call(reg->body.key.plug->o.key_ops,
				    get_offset, &reg->body.key);

		/* Calling body item's read() method */
		chunk = plug_call(reg->body.plug->o.item_ops, read,
				  &reg->body, buff, offset, chunk);

		/* Updating offset */
		reg40_seek(entity, reg40_offset(entity) + chunk);
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

	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;

	/* Initializing file handle */
	obj40_init(&reg->obj, &reg40_plug, core, info);
	
	if (obj40_pid(&reg->obj, OBJECT_PLUG_TYPE,
		      "regular") !=  reg40_plug.id.id)
	{
		goto error_free_reg;
	}
	
	/* Reseting file (setting offset to 0) */
	reg40_reset((object_entity_t *)reg);
	return (object_entity_t *)reg;
	
 error_free_reg:
	aal_free(reg);
	return NULL;
}

#ifndef ENABLE_STAND_ALONE
/* Creating the file described by pased @hint */
static object_entity_t *reg40_create(object_info_t *info,
				     object_hint_t *hint)
{
	reg40_t *reg;
	uint64_t ordering;
	oid_t objectid, locality;
	
	aal_assert("umka-1169", info != NULL);
	aal_assert("umka-1738", hint != NULL);
	aal_assert("vpf-1093",  info->tree != NULL);

	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;
	
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
	
	if (obj40_create_stat(&reg->obj, hint->statdata, 0, 0, 0, S_IFREG))
		goto error_free_reg;

	aal_memcpy(&info->start, STAT_PLACE(&reg->obj),
		   sizeof(info->start));
	
	reg40_reset((object_entity_t *)reg);
	return (object_entity_t *)reg;

 error_free_reg:
	aal_free(reg);
	return NULL;
}

static uint32_t reg40_links(object_entity_t *entity) {
	reg40_t *reg;
	
	aal_assert("umka-2296", entity != NULL);

	reg = (reg40_t *)entity;
	
	if (obj40_update(&reg->obj))
		return -EINVAL;
	
	return obj40_get_nlink(&reg->obj);
}

static errno_t reg40_link(object_entity_t *entity) {
	reg40_t *reg;
	
	aal_assert("umka-1912", entity != NULL);

	reg = (reg40_t *)entity;
	
	if (obj40_update(&reg->obj))
		return -EINVAL;
	
	return obj40_link(&reg->obj, 1);
}

static errno_t reg40_unlink(object_entity_t *entity) {
	reg40_t *reg;
	
	aal_assert("umka-1911", entity != NULL);

	reg = (reg40_t *)entity;
	
	if (obj40_update(&reg->obj))
		return -EINVAL;
	
	return obj40_link(&reg->obj, -1);
}

static uint32_t reg40_chunk(reg40_t *reg) {
	return core->tree_ops.maxspace(reg->obj.info.tree);
}

/* Writes passed data to the file */
int32_t reg40_put(object_entity_t *entity, void *buff, uint32_t n) {
	errno_t res;
	reg40_t *reg;
	
	uint64_t size;
	uint32_t written;
	uint32_t maxspace;

	reg = (reg40_t *)entity;
	maxspace = reg40_chunk(reg);
	
	for (written = 0; written < n; ) {
		uint32_t level;
		uint64_t offset;
		create_hint_t hint;
		key_entity_t maxkey;

		/* Preparing hint key */
		plug_call(reg->offset.plug->o.key_ops,
			  assign, &hint.key, &reg->offset);

		/* Setting up @hint */
		if ((hint.count = n - written) > maxspace)
			hint.count = maxspace;

		/* Preparing insert hint */
		hint.offset = 0;
		hint.type_specific = buff;
		hint.tree = reg->obj.info.tree;
		hint.plug = reg40_bplug(entity, reg40_offset(entity) + n);

		/* Lookup place data will be inserted at */
		switch (obj40_lookup(&reg->obj, &hint.key,
				     LEAF_LEVEL, &reg->body))
		{
		case PRESENT:
			if (reg->body.plug->id.group == TAIL_ITEM) {
				/* Checking if we need write chunk by chunk in
				   odrer to rewrite tail correctly in the case
				   we write the tail, that overlaps two
				   neighbour tails by key. */
				plug_call(reg->body.plug->o.item_ops, maxreal_key,
					  &reg->body, &maxkey);

				offset = plug_call(hint.key.plug->o.key_ops,
						   get_offset, &maxkey);

				/* Rewritting only tails' last part */
				if (reg40_offset(entity) +
				    hint.count > offset + 1)
				{
					hint.count = (offset + 1) -
						reg40_offset(entity);
				}
			} else {
				/* Initializing offset from unit start */
				hint.offset = reg40_offset(entity);

				hint.offset -= plug_call(hint.key.plug->o.key_ops,
							 get_offset, &reg->body.key);
			}
			
			break;
		case FAILED:
			return -EIO;
		default:
			break;
		}

		/* Setting up target level */
		level = hint.plug->id.group == EXTENT_ITEM ?
			LEAF_LEVEL + 1 : LEAF_LEVEL;

		/* Inserting data to the tree */
		if ((res = obj40_insert(&reg->obj, &hint, level,
					&reg->body)))
		{
			return res;
		}

		reg40_seek(entity, reg40_offset(entity) +
			   hint.count);
		
		/* @buff may be NULL for inserting holes */
		if (buff)
			buff += hint.count;

		/* Updating counters and offset */
		written += hint.count;
	}
	
	return written;
}

static errno_t reg40_cut(object_entity_t *entity,
			 uint32_t n)
{
	errno_t res;
	reg40_t *reg;
	uint64_t size;
	
	reg = (reg40_t *)entity;
	size = reg40_size(entity);

	while (n) {
		place_t place;
		key_entity_t key;

		/* Preparing key of the last unit of last item. It is needed to
		   find last item and remove it (or some its part) from the the
		   tree. */
		plug_call(STAT_KEY(&reg->obj)->plug->o.key_ops,
			  assign, &key, &reg->offset);
		
		plug_call(STAT_KEY(&reg->obj)->plug->o.key_ops,
			  set_offset, &key, size - 1);

		/* Making lookup for last item. */
		switch (obj40_lookup(&reg->obj, &key,
				     LEAF_LEVEL, &place))
		{
		case PRESENT:
			break;
		default:
			return -EINVAL;
		}

		/* Check if we should remove whole item at @place or just some
		   units from it. */
		if (place.len <= n) {
			place.pos.unit = MAX_UINT32;
			
			if ((res = obj40_remove(&reg->obj, &place, 1)))
				return res;

			n -= place.len;
			size -= place.len;
		} else {
			place.pos.unit = place.len - n;
			return obj40_remove(&reg->obj, &place, n);
		}
	}

	return 0;
}

/* Writes "n" bytes from "buff" to passed file */
static int32_t reg40_write(object_entity_t *entity, 
			   void *buff, uint32_t n) 
{
	int32_t res;
	reg40_t *reg;
	
	uint64_t size;
	uint64_t offset;
	
	aal_assert("umka-2280", buff != NULL);
	aal_assert("umka-2281", entity != NULL);

	reg = (reg40_t *)entity;
	size = reg40_size(entity);
	offset = reg40_offset(entity);

	/* Inserting holes if it is needed */
	if (offset > size) {
		if ((res = reg40_put(entity, NULL,
				     offset - size)) < 0)
		{
			return res;
		}
	}

	/* Putting data to tree */
	if ((res = reg40_put(entity, buff, n)) < 0)
		return res;

	/* Updating stat data fields */
	return obj40_touch(&reg->obj, size + n,
			   size + n, time(NULL));
}

/* Truncates file to passed size */
static errno_t reg40_truncate(object_entity_t *entity,
			      uint64_t n)
{
	errno_t res;
	reg40_t *reg;
	uint64_t size;
	uint64_t offset;

	reg = (reg40_t *)entity;
	
	size = reg40_size(entity);
	offset = reg40_offset(entity);

	if (size == n)
		return 0;

	if (n > size) {
		/* Inserting holes */
		if ((res = reg40_put(entity, NULL,
				     n - size)))
		{
			return res;
		}
	} else {
		/* Cutting items/units */
		if ((res = reg40_cut(entity,
				     size - n)))
		{
			return res;
		}
	}

	/* Updating offset */
	reg40_seek(entity, n);

	/* Updating stat dat fields */
	return obj40_touch(&reg->obj, n, n, time(NULL));
}

/* Removes all file items. */
static errno_t reg40_clobber(object_entity_t *entity) {
	errno_t res;
	reg40_t *reg;
	
	aal_assert("umka-2299", entity != NULL);

	reg = (reg40_t *)entity;
	
	if ((res = reg40_truncate(entity, 0)))
		return res;

	if ((res = obj40_update(&reg->obj)))
		return res;

	return obj40_remove(&reg->obj, STAT_PLACE(&reg->obj), 1);
}

/* Layout related stuff */
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

/* Traverses all blocks belong to the passed file and calls passed @block_func
   for each of them. It is needed for calculating fragmentation, printing,
   etc. */
static errno_t reg40_layout(object_entity_t *entity,
			    block_func_t block_func,
			    void *data)
{
	errno_t res;
	reg40_t *reg;
	uint64_t size;
	layout_hint_t hint;
	
	aal_assert("umka-1471", entity != NULL);
	aal_assert("umka-1472", block_func != NULL);

	reg = (reg40_t *)entity;
	
	if (!(size = reg40_size(entity)))
		return 0;

	/* Preparing layout hint */
	hint.data = data;
	hint.entity = entity;
	hint.block_func = block_func;
		
	while (reg40_offset(entity) < size) {
		uint64_t offset;
		key_entity_t maxkey;
		
		/* Updating place fo current body item */
		if ((res = reg40_update(entity)))
			return res;

		/* Calling enumerator funtions */
		if (reg->body.plug->o.item_ops->layout) {
			if ((res = plug_call(reg->body.plug->o.item_ops,
					     layout, &reg->body,
					     callback_item_data, &hint)))
			{
				return res;
			}
		} else {
			blk_t blk = reg->body.block->nr;
			
			if ((res = callback_item_data(&reg->body, blk,
						      1, &hint)))
			{
				return res;
			}
		}

		/* Getting current item max real key inside, in order to know
		   how much increase file offset. */
		plug_call(reg->body.plug->o.item_ops, maxreal_key,
			  &reg->body, &maxkey);

		/* Updating file offset */
		offset = plug_call(maxkey.plug->o.key_ops,
				   get_offset, &maxkey);

		reg40_seek(entity, reg40_offset(entity) +
			   offset + 1);
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
	reg40_t *reg;
	uint64_t size;
	
	aal_assert("umka-2386", entity != NULL);
	aal_assert("umka-2387", place_func != NULL);

	reg = (reg40_t *)entity;
	
	if (!(size = reg40_size(entity)))
		return 0;

	while (reg40_offset(entity) < size) {
		uint64_t offset;
		key_entity_t maxkey;
		
		/* Updating place fo current body item */
		if ((res = reg40_update(entity)))
			return res;

		/* Callng per-place callck function */
		if ((res = place_func(entity, &reg->body, data)))
			return res;

		/* Getting current item max real key inside, in order to know
		   how much increase file offset. */
		plug_call(reg->body.plug->o.item_ops, maxreal_key,
			  &reg->body, &maxkey);

		/* Updating file offset */
		offset = plug_call(maxkey.plug->o.key_ops,
				   get_offset, &maxkey);

		reg40_seek(entity, reg40_offset(entity) +
			   offset + 1);
	}
	
	return 0;
}

extern object_entity_t *reg40_realize(object_info_t *info);

extern errno_t reg40_check_struct(object_entity_t *object,
				  place_func_t place_func,
				  region_func_t region_func,
				  void *data, uint8_t mode);

#endif

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
