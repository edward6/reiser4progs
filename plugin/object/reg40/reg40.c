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
lookup_t reg40_update(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;

	aal_assert("umka-1161", entity != NULL);
	
	/* Getting the next body item from the tree */
	return obj40_lookup(&reg->obj, &reg->offset,
			    LEAF_LEVEL, FIND_EXACT,
			    &reg->body);
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
	uint32_t read;
	uint64_t fsize;

	aal_assert("umka-1183", buff != NULL);
	aal_assert("umka-1182", entity != NULL);

	reg = (reg40_t *)entity;
	fsize = reg40_size(entity);

	for (read = 0; read < n && reg40_offset(entity) < fsize;) {
		int32_t chunk;
		trans_hint_t hint;

		/* Update body place. Check for error. */
		if ((res = reg40_update(entity)) < 0)
			return res;
		else {
			/* Check if file stream is over. */
			if (res == ABSENT)
				return 0;
		}

		/* Initializing trans hint */
		plug_call(reg->offset.plug->o.key_ops, assign,
			  &hint.offset, &reg->offset);

		hint.specific = buff;
		hint.count = n - read;

		/* Calcilating chunk size to read. */
		if (reg40_offset(entity) + hint.count > fsize)
			hint.count = fsize - reg40_offset(entity);
			
		hint.tree = reg->obj.info.tree;

		/* Calling body item's read() method */
		if ((chunk = plug_call(reg->body.plug->o.item_ops,
				       read, &reg->body, &hint)) < 0)
		{
			return chunk;
		} else {
			if (chunk == 0)
				return 0;
		}

		buff += chunk; read += chunk;
		
		/* Updating offset */
		reg40_seek(entity, reg40_offset(entity) + chunk);
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
reiser4_plug_t *reg40_bplug(reg40_t *reg, uint64_t new_size) {
	aal_assert("umka-2394", reg != NULL);
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

/* Makes tail2extent and extent2tail conversion */
errno_t reg40_convert(object_entity_t *entity, 
		      reiser4_plug_t *plug, 
		      uint64_t fsize, 
		      int update) 
{
	reg40_t *reg;
	place_t place;
	uint64_t fsize;
	conv_hint_t hint;
	key_entity_t key;

	aal_assert("umka-2467", plug != NULL);
	aal_assert("umka-2466", entity != NULL);

	reg = (reg40_t *)entity;

	plug_call(reg->offset.plug->o.key_ops, assign, &key, &reg->offset);

	plug_call(reg->offset.plug->o.key_ops, set_offset, &key, 0);
	
	/* Looking for item to be converted */
	switch (obj40_lookup(&reg->obj, &key, LEAF_LEVEL,
			     FIND_EXACT, &place))
	{
	case PRESENT:
		break;
	default:
		return -EIO;
	}
	
	/* Prepare convert hint. */
	hint.bytes = 0;
	hint.size = fsize;
	hint.plug = plug;
	hint.place = &place;

	/* Converting item. */
	if (rcore->tree_ops.conv(reg->obj.info.tree,
				 &hint))
	{
		aal_exception_error("Can't convert item "
				    "%s at %llu:%u to %s.",
				    place.plug->label,
				    place.block->nr,
				    place.pos.item,
				    plug->label);
		return -EIO;
	}
	
	return update ? obj40_touch(&reg->obj, fsize, bytes, time(NULL)) : 0;
}

static errno_t reg40_check_body(object_entity_t *entity, uint64_t new_size) {
	reg40_t *reg;
	uint64_t fsize;
	reiser4_plug_t *bplug;
	
	aal_assert("umka-2395", entity != NULL);

	reg = (reg40_t *)entity;
	
	fsize = reg40_size(entity);
	
	/* There is nothing to convert */
	if (!fsize || !new_size)
		return 0;
	
	if (!(bplug = reg40_bplug(reg, new_size))) {
		aal_exception_error("Can't get new body plugin.");
		return -EINVAL;
	}

	/* Comparing new plugin and old one. If they are the same, conversion if
	   not needed. */
	if (plug_equal(bplug, reg->body.plug))
		return 0;

	return reg40_convert(entity, bplug, fsize, 1);
}

/* Writes passed data to the file. Returns amount of data written to the
   tree. That is if we insert tail, we will have the same as passed data
   size. In the case of insert an extent, we will have its meta data size. */
int32_t reg40_put(object_entity_t *entity, void *buff, uint32_t n) {
	reg40_t *reg;
	int32_t bytes;
	uint32_t written;
	uint32_t maxspace;

	reg = (reg40_t *)entity;
	maxspace = reg40_chunk(reg);
	
	for (bytes = 0, written = 0; written < n; ) {
		lookup_t res;
		int32_t write;
		uint32_t level;
		trans_hint_t hint;

		/* Preparing hint key */
		plug_call(reg->offset.plug->o.key_ops,
			  assign, &hint.offset, &reg->offset);

		/* Setting up @hint */
		if ((hint.count = n - written) > maxspace)
			hint.count = maxspace;

		/* Preparing insert hint */
		hint.specific = buff;
		hint.tree = reg->obj.info.tree;
		hint.plug = reg40_bplug(reg, reg40_offset(entity) + n);

		/* Lookup place data will be inserted at */
		if ((res = obj40_lookup(&reg->obj, &hint.offset,
					LEAF_LEVEL, FIND_CONV,
					&reg->body) < 0))
		{
			return res;
		}

		/* Setting up target level */
		level = hint.plug->id.group == EXTENT_ITEM ?
			LEAF_LEVEL + 1 : LEAF_LEVEL;

		/* Inserting data to the tree */
		if ((write = obj40_write(&reg->obj, &reg->body,
					 &hint, level)) < 0)
		{
			return write;
		}

		reg40_seek(entity, reg40_offset(entity) + write);

		/* @buff may be NULL for inserting holes */
		if (buff) {
			buff += write;
		}

		bytes += hint.bytes;

		/* Updating counters and offset */
		written += write;
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
				  FIND_EXACT, &place) != PRESENT))
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

	if (reg40_check_body(entity, size + n)) {
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

	/* Converting body. */
	if (reg40_check_body(entity, n)) {
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
		
		/* Update body place. Check for error. */
		if ((res = reg40_update(entity)) < 0)
			return res;

		/* Check if file stream is over. */
		if (res == ABSENT)
			return 0;

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
			
			if ((res = callback_item_layout(place, blk, 1, &hint)))
				return res;
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
		
		/* Update body place. Check for error. */
		if ((res = reg40_update(entity)) < 0)
			return res;

		/* Check if file stream is over. */
		if (res == ABSENT)
			return 0;

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

extern errno_t reg40_form(object_entity_t *object);

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
	.convert        = reg40_convert,
	.link           = reg40_link,
	.unlink         = reg40_unlink,
	.links          = reg40_links,
	.clobber        = reg40_clobber,
	.recognize	= reg40_recognize,
	.check_struct   = reg40_check_struct,

	.form		= reg40_form,
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
