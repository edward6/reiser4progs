/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40.c -- reiser4 default regular file plugin. */

#ifndef ENABLE_STAND_ALONE
#  include <time.h>
#  include <unistd.h>
#endif

#include "reg40.h"

reiser4_core_t *rcore = NULL;
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

/* Updates body place in correspond to file offset */
errno_t reg40_update(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;

	aal_assert("umka-1161", entity != NULL);
	
	/* Getting the next body item from the tree */
	switch (obj40_lookup(&reg->obj, &reg->offset,
			     LEAF_LEVEL, READ, &reg->body))
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

	/* The file has not data or there is nothing to read more */
	if (offset >= size)
		return 0;

	if (n > size - offset)
		n = size - offset;

	for (read = 0; read < n; ) {
		trans_hint_t hint;

		if ((res = reg40_update(entity)))
			return res;

		chunk = n - read;

		/* Calculating in-item offset */
		offset = reg40_offset(entity);

		offset -= plug_call(reg->body.key.plug->o.key_ops,
				    get_offset, &reg->body.key);

		hint.count = chunk;
		hint.offset = offset;
		hint.specific = buff;

		/* Calling body item's read() method */
		if (!(chunk = plug_call(reg->body.plug->o.item_ops,
					read, &reg->body, &hint)))
		{
			return chunk;
		}

		/* Updating offset */
		reg40_seek(entity, reg40_offset(entity) + chunk);

		/* Updating local counter and @buff */
		buff += chunk;
		read += chunk;
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
	obj40_init(&reg->obj, &reg40_plug, rcore, info);
	
	if (obj40_pid(&reg->obj, OBJECT_PLUG_TYPE,
		      "regular") !=  reg40_plug.id.id)
	{
		goto error_free_reg;
	}

#ifndef ENABLE_STAND_ALONE
	if (!(reg->policy = obj40_plug(&reg->obj, POLICY_PLUG_TYPE,
				       "policy")))
	{
		aal_exception_error("Can't get file policy plugin.");
		goto error_free_reg;
	}
#endif
	
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
	uint64_t mask;
	
	aal_assert("umka-1169", info != NULL);
	aal_assert("umka-1738", hint != NULL);
	aal_assert("vpf-1093",  info->tree != NULL);

	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;
	
	/* Initializing file handle */
	obj40_init(&reg->obj, &reg40_plug, rcore, info);

	/* Initializing tail policy plugin */
	if (hint->body.reg.policy == INVAL_PID) {
		/* Getting default tail policy from param if passed hint
		   contains no valid tail policy plugin id. */
		hint->body.reg.policy = rcore->param_ops.value("policy");

		if (hint->body.reg.policy == INVAL_PID) {
			aal_exception_error("Invalid default tail policy "
					    "plugin id has been detected.");
			goto error_free_reg;
		}
	}
	
	if (!(reg->policy = rcore->factory_ops.ifind(POLICY_PLUG_TYPE,
						     hint->body.reg.policy)))
	{
		aal_exception_error("Can't find tail policy plugin by "
				    "its id 0x%x.", hint->body.reg.policy);
		goto error_free_reg;
	}
	
	mask = (1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID);
	
	if (obj40_create_stat(&reg->obj, hint->statdata,
			      mask, 0, 0, 0, S_IFREG, NULL))
	{
		goto error_free_reg;
	}

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
	return rcore->tree_ops.maxspace(reg->obj.info.tree);
}

/* Returns plugin (tail or extent) for next write operation basing on passed
   @size -- new file size. This function will use tail policy plugin for find
   out what next item should be writen. */
reiser4_plug_t *reg40_bplug(object_entity_t *entity, uint64_t new_size) {
	reg40_t *reg = (reg40_t *)entity;
	
	aal_assert("umka-2394", entity != NULL);
	aal_assert("umka-2393", reg->policy != NULL);

	/* FIXME-UMKA: Here is not enough to have only plugin type to get it
	   from stat data as for tails and extents plugin type is the same and
	   namely ITEM_PLUG_TYPE. */
	
	/* Calling tail policy plugin to detect body plugin */
	if (plug_call(reg->policy->o.policy_ops, tails, new_size)) {
		/* Trying to get non-standard tail plugin from stat data. And if
		   it is not found, default one from params will be taken. */
		return obj40_plug(&reg->obj, ITEM_PLUG_TYPE, "tail");
	} else {
		/* The same for extent plugin */
		return obj40_plug(&reg->obj, ITEM_PLUG_TYPE, "extent");
	}
}

/* Makes extent2tail conversion */
static errno_t reg40_extent2tail(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;

	if (reg->body.plug->id.group == TAIL_ITEM)
		return -EINVAL;
	
	return 0;
}

/* Makes tail2extent conversion */
static errno_t reg40_tail2extent(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;
	
	if (reg->body.plug->id.group == EXTENT_ITEM)
		return -EINVAL;
	
	return 0;
}

/* Makes tail2extent and extent2tail conversion */
errno_t reg40_convert(object_entity_t *entity, uint64_t new_size) {
	reg40_t *reg;
	reiser4_plug_t *bplug;
	
	aal_assert("umka-2395", entity != NULL);

	reg = (reg40_t *)entity;
	
	/* There is nothing to convert */
	if (reg40_size(entity) == 0 || new_size == 0)
		return 0;
	
	if (!(bplug = reg40_bplug(entity, new_size))) {
		aal_exception_error("Can't get enw body plugin.");
		return -EINVAL;
	}

	/* Comparing new plugin and old one. If they are the same, conversion if
	   not needed. */
	if (plug_equal(bplug, reg->body.plug))
		return 0;

	/* New plugin is tail one. Converting to extents */
	if (bplug->id.group == TAIL_ITEM)
		return reg40_extent2tail(entity);

	/* New plugin is extent one. Converting to tails */
	return reg40_tail2extent(entity);
}

/* Writes passed data to the file. Returns amount of data written to the
   tree. That is if we insert tail, we will have the same as passed data
   size. In the case of insert an extent, we will have its meta data size. */
int32_t reg40_put(object_entity_t *entity, void *buff, uint32_t n) {
	errno_t res;
	reg40_t *reg;

	int32_t bytes;
	uint32_t written;
	uint32_t maxspace;

	reg = (reg40_t *)entity;
	maxspace = reg40_chunk(reg);
	
	for (bytes = 0, written = 0; written < n; ) {
		uint32_t level;
		uint64_t offset;

		trans_hint_t hint;
		key_entity_t maxkey;

		/* Preparing hint key */
		plug_call(reg->offset.plug->o.key_ops,
			  assign, &hint.key, &reg->offset);

		/* Setting up @hint */
		if ((hint.count = n - written) > maxspace)
			hint.count = maxspace;

		/* Preparing insert hint */
		hint.offset = 0;
		hint.specific = buff;
		hint.tree = reg->obj.info.tree;
		hint.plug = reg40_bplug(entity, reg40_offset(entity) + n);

		/* Lookup place data will be inserted at */
		switch (obj40_lookup(&reg->obj, &hint.key,
				     LEAF_LEVEL, INST, &reg->body))
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
		if ((res = obj40_write(&reg->obj, &reg->body,
				       &hint, level)))
		{
			return res;
		}

		reg40_seek(entity, reg40_offset(entity) +
			   hint.count);

		/* @buff may be NULL for inserting holes */
		if (buff) {
			buff += hint.count;
		}

		bytes += hint.bytes;

		/* Updating counters and offset */
		written += hint.count;
	}

	return bytes;
}

static int32_t reg40_cut(object_entity_t *entity,
			 uint32_t n)
{
	errno_t res;
	reg40_t *reg;
	uint64_t size;

	int32_t bytes = 0;
	trans_hint_t hint;
	
	reg = (reg40_t *)entity;
	size = reg40_size(entity);

	while (n > 0) {
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
		if ((obj40_lookup(&reg->obj, &key, LEAF_LEVEL,
				  READ, &place) != PRESENT))
		{
			return -EINVAL;
		}

		/* Check if we should remove whole item at @place or just some
		   units from it. */
		if (place.len <= n) {
			hint.count = 1;
			place.pos.unit = MAX_UINT32;
			
			if ((res = obj40_remove(&reg->obj, &place, &hint)))
				return res;

			n -= place.len;
			bytes += hint.len;
			size -= place.len;
		} else {
			hint.count = n;
			place.pos.unit = place.len - n;

			if ((res = obj40_remove(&reg->obj, &place, &hint)))
				return res;

			bytes += hint.len;
		}
	}

	return bytes;
}

/* Writes "n" bytes from "buff" to passed file */
static int32_t reg40_write(object_entity_t *entity, 
			   void *buff, uint32_t n) 
{
	int32_t res;
	reg40_t *reg;

	int32_t bytes;
	uint64_t size;
	uint64_t offset;
	
	aal_assert("umka-2280", buff != NULL);
	aal_assert("umka-2281", entity != NULL);

	reg = (reg40_t *)entity;
	size = reg40_size(entity);
	offset = reg40_offset(entity);

	if (reg40_convert(entity, size + n)) {
		aal_exception_error("Can't perform tail "
				    "conversion.");
		return -EINVAL;
	}
		
	/* Inserting holes if it is needed */
	if (offset > size) {
		if ((bytes = reg40_put(entity, NULL,
				       offset - size)) < 0)
		{
			return bytes;
		}
	} else {
		bytes = 0;
	}

	/* Putting data to tree */
	if ((res = reg40_put(entity, buff, n)) < 0)
		return res;

	bytes += obj40_get_bytes(&reg->obj) + res;

	/* Updating stat data fields */
	return obj40_touch(&reg->obj, size + n,
			   bytes, time(NULL));
}

/* Truncates file to passed size */
static errno_t reg40_truncate(object_entity_t *entity,
			      uint64_t n)
{
	reg40_t *reg;
	int32_t bytes;
	uint64_t size;
	uint64_t offset;

	reg = (reg40_t *)entity;
	
	size = reg40_size(entity);
	offset = reg40_offset(entity);

	if (size == n)
		return 0;

	/* Converting body */
	if (reg40_convert(entity, n)) {
		aal_exception_error("Can't perform tail "
				    "conversion.");
		return -EINVAL;
	}
		
	if (n > size) {
		/* Inserting holes */
		if ((bytes = reg40_put(entity, NULL,
				       n - size)) < 0)
		{
			return bytes;
		}
		
		/* Updating stat data fields */
		bytes += obj40_get_bytes(&reg->obj);
		return obj40_touch(&reg->obj, n, bytes, time(NULL));
	} else {
		/* Cutting items/units */
		if ((bytes = reg40_cut(entity,
				       size - n)) < 0)
		{
			return bytes;
		}

		/* Updating stat data fields */
		bytes = obj40_get_bytes(&reg->obj) - bytes;
		return obj40_touch(&reg->obj, n, bytes, time(NULL));
	}

	/* Updating offset */
	reg40_seek(entity, n);
}

/* Removes all file items. */
static errno_t reg40_clobber(object_entity_t *entity) {
	errno_t res;
	reg40_t *reg;
	trans_hint_t hint;
	
	aal_assert("umka-2299", entity != NULL);

	reg = (reg40_t *)entity;
	
	if ((res = reg40_truncate(entity, 0)))
		return res;

	if ((res = obj40_update(&reg->obj)))
		return res;

	hint.count = 1;

	return obj40_remove(&reg->obj,
			    STAT_PLACE(&reg->obj), &hint);
}

struct layout_hint {
	void *data;
	object_entity_t *entity;
	region_func_t region_func;
};

typedef struct layout_hint layout_hint_t;

static errno_t callback_item_layout(void *place, blk_t start,
				    count_t width, void *data)
{
	layout_hint_t *hint = (layout_hint_t *)data;

	return hint->region_func(hint->entity, start,
				 width, hint->data);
}

/* Traverses all blocks belong to the passed file and calls passed @block_func
   for each of them. It is needed for calculating fragmentation, printing,
   etc. */
static errno_t reg40_layout(object_entity_t *entity,
			    region_func_t region_func,
			    void *data)
{
	errno_t res;
	reg40_t *reg;
	uint64_t size;
	uint64_t offset;

	layout_hint_t hint;
	key_entity_t maxkey;
		
	aal_assert("umka-1471", entity != NULL);
	aal_assert("umka-1472", region_func != NULL);

	reg = (reg40_t *)entity;
	
	if (!(size = reg40_size(entity)))
		return 0;

	hint.data = data;
	hint.entity = entity;
	hint.region_func = region_func;
	
	while (reg40_offset(entity) < size) {
		place_t *place = &reg->body;
		
		/* Updating place fo current body item */
		if ((res = reg40_update(entity)))
			return res;

		/* Calling enumerator funtions */
		if (place->plug->o.item_ops->layout) {
			if ((res = plug_call(place->plug->o.item_ops, layout,
					     place, callback_item_layout,
					     &hint)))
			{
				return res;
			}
		} else {
			blk_t blk = place->block->nr;
			
			if ((res = callback_item_layout(place, blk,
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

		/* Calling per-place callback function */
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

extern object_entity_t *reg40_recognize(object_info_t *info);

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
	.recognize	= reg40_recognize,
	.check_struct   = reg40_check_struct,

	.update		= NULL,
	.add_entry      = NULL,
	.rem_entry      = NULL,
	.attach         = NULL,
	.detach         = NULL,
	
	.fake		= NULL,
	.check_attach 	= NULL,
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
	.id    = {OBJECT_REG40_ID, FILE_OBJECT, OBJECT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "reg40",
	.desc  = "Regular file for reiser4, ver. " VERSION,
#endif
	.o = {
		.object_ops = &reg40_ops
	}
};

static reiser4_plug_t *reg40_start(reiser4_core_t *c) {
	rcore = c;
	return &reg40_plug;
}

plug_register(reg40, reg40_start, NULL);
