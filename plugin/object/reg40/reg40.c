/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40.c -- reiser4 regular file plugin. */

#ifndef ENABLE_STAND_ALONE
#  include <unistd.h>
#endif

#include "reg40.h"
#include "reg40_repair.h"

reiser4_core_t *reg40_core = NULL;

/* Returns regular file current offset. */
uint64_t reg40_offset(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;
	
	aal_assert("umka-1159", entity != NULL);

	return plug_call(reg->position.plug->o.key_ops,
			 get_offset, &reg->position);
}

/* Returns regular file size. */
static uint64_t reg40_size(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;

	aal_assert("umka-2278", entity != NULL);
	
	if (obj40_update(&reg->obj))
		return -EINVAL;

	return obj40_get_size(&reg->obj);
}

/* Position regular file to passed @offset. */
errno_t reg40_seek(object_entity_t *entity, 
		   uint64_t offset) 
{
	reg40_t *reg = (reg40_t *)entity;
	
	aal_assert("umka-1968", entity != NULL);

	plug_call(reg->position.plug->o.key_ops,
		  set_offset, &reg->position, offset);

	return 0;
}

/* Closes file. */
static void reg40_close(object_entity_t *entity) {
	aal_assert("umka-1170", entity != NULL);
	aal_free(entity);
}

/* Updates body place in correspond to file offset. */
lookup_t reg40_update_body(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;

	aal_assert("umka-1161", entity != NULL);
	
	/* Getting the next body item from the tree */
	return obj40_lookup(&reg->obj, &reg->position,
			    LEAF_LEVEL, FIND_EXACT,
			    &reg->body);
}

/* Resets file position. As fire position is stored inside @reg->position, it
   just builds new zero offset key.*/
errno_t reg40_reset(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;
	
	aal_assert("umka-1963", entity != NULL);
	
	plug_call(STAT_KEY(&reg->obj)->plug->o.key_ops, build_generic,
		  &reg->position, KEY_FILEBODY_TYPE, obj40_locality(&reg->obj),
		  obj40_ordering(&reg->obj), obj40_objectid(&reg->obj), 0);

	return 0;
}

/* Reads @n bytes to passed buffer @buff. Negative values are returned on
   errors. */
static int64_t reg40_read(object_entity_t *entity, 
			  void *buff, uint64_t n)
{
	reg40_t *reg;
	int64_t read;
	uint64_t fsize;
	trans_hint_t hint;

	aal_assert("umka-2511", buff != NULL);
	aal_assert("umka-2512", entity != NULL);
	
	reg = (reg40_t *)entity;
	fsize = reg40_size(entity);

	/* Preparing hint to be used for calling read with it. Here we
	   initialize @count -- number of bytes to read, @specific -- pointer to
	   buffer data will be read into, and pointer to tree instance, file is
	   opened on. */ 
	hint.count = n;
	hint.specific = buff;
	hint.tree = reg->obj.info.tree;

	/* Initializing offset data must be read from. This is current file
	   offset, so we use @reg->position. */
	plug_call(reg->position.plug->o.key_ops,
		  assign, &hint.offset, &reg->position);

	/* Correcting number of bytes to be read. It cannot be more then file
	   size value from stat data. That is because, body item itself does not
	   know reliably how long it is. For instnace, extent40. */
	if (reg40_offset(entity) + hint.count > fsize)
		hint.count = fsize - reg40_offset(entity);

	/* Reading data. */
	if ((read = obj40_read(&reg->obj, &hint)) < 0)
		return read;

	/* Updating file offset if needed. */
	if (read > 0)
		reg40_seek(entity, reg40_offset(entity) + read);
	
	return read;
}

/* Open regular file by passed initial info and return initialized
   instance. This @info struct contains information about the obejct, like its
   statdata coord, etc. */
static object_entity_t *reg40_open(object_info_t *info) {
	reg40_t *reg;

	aal_assert("umka-1163", info != NULL);
	aal_assert("umka-1164", info->tree != NULL);

	/* Checking if passed info contains correct stat data coord. What if
	   file has not stat data? */
	if (info->start.plug->id.group != STATDATA_ITEM)
		return NULL;

	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;

	/* Initializing file handle. It is needed for working with file stat
	   data item. */
	obj40_init(&reg->obj, &reg40_plug, reg40_core, info);

	/* Checking if stat data contains points to object, which can be handled
	   by current plugin. */
	if (obj40_pid(&reg->obj, OBJECT_PLUG_TYPE,
		      "regular") !=  reg40_plug.id.id)
	{
		goto error_free_reg;
	}

	/* Initializing tail policy plugin. */
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

/* Loads stat data to passed @hint */
static errno_t reg40_stat(object_entity_t *entity,
			  statdata_hint_t *hint)
{
	reg40_t *reg;
	
	aal_assert("umka-2561", entity != NULL);
	aal_assert("umka-2562", hint != NULL);

	reg = (reg40_t *)entity;
	return obj40_load_stat(&reg->obj, hint);
}

#ifndef ENABLE_STAND_ALONE
/* Create the file described by pased @hint. That is create files stat data
   item. */
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
	
	/* Initializing file handle. */
	obj40_init(&reg->obj, &reg40_plug, reg40_core, info);

	/* Getting tail policy plugin id. */
	if (hint->body.reg.policy == INVAL_PID) {
		/* Getting default tail policy from param if passed hint
		   contains no valid tail policy plugin id. */
		hint->body.reg.policy = reg40_core->param_ops.value("policy");

		if (hint->body.reg.policy == INVAL_PID) {
			aal_exception_error("Invalid default tail policy "
					    "plugin id has been detected.");
			goto error_free_reg;
		}
	}

	/* Initializing tail policy plugin. */
	if (!(reg->policy = reg40_core->factory_ops.ifind(POLICY_PLUG_TYPE,
						     hint->body.reg.policy)))
	{
		aal_exception_error("Can't find tail policy plugin by "
				    "its id 0x%x.", hint->body.reg.policy);
		goto error_free_reg;
	}

	/* Create stat data item with size, bytes, nlinks equal to zero and mask
	   equal to @mask. */
	mask = (1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID);

	if (obj40_create_stat(&reg->obj, hint->label.statdata,
			      mask, 0, 0, 0, 0, S_IFREG, NULL))
	{
		goto error_free_reg;
	}

	/* Copy new created stat data coord to @info->start. It is needed on
	   higher levels of abstraction and for fsck. */
	aal_memcpy(&info->start, STAT_PLACE(&reg->obj),
		   sizeof(info->start));

	/* Reset file. */
	reg40_reset((object_entity_t *)reg);
	return (object_entity_t *)reg;

 error_free_reg:
	aal_free(reg);
	return NULL;
}

/* Returns number of links. Needed to let higher API levels know, that file has
   zero links and may be clobbered. */
static uint32_t reg40_links(object_entity_t *entity) {
	reg40_t *reg;
	
	aal_assert("umka-2296", entity != NULL);

	reg = (reg40_t *)entity;
	return obj40_links(&reg->obj);
}

/* Increments link number in stat data. */
static errno_t reg40_link(object_entity_t *entity) {
	reg40_t *reg;
	
	aal_assert("umka-1912", entity != NULL);

	reg = (reg40_t *)entity;
	return obj40_link(&reg->obj);
}

/* Decrements link number in stat data. */
static errno_t reg40_unlink(object_entity_t *entity) {
	reg40_t *reg;
	
	aal_assert("umka-1911", entity != NULL);

	reg = (reg40_t *)entity;
	return obj40_unlink(&reg->obj);
}

/* Returns plugin (tail or extent) for next write operation basing on passed
   @size -- new file size. This function will use tail policy plugin to find
   what kind of next body item should be writen. */
reiser4_plug_t *reg40_policy_plug(reg40_t *reg, uint64_t new_size) {
	aal_assert("umka-2394", reg != NULL);
	aal_assert("umka-2393", reg->policy != NULL);

	/* FIXME-UMKA: Here is not enough to have only plugin type to get it
	   from stat data, as for tails and extents plugin type is the same and
	   namely ITEM_PLUG_TYPE. */
	
	/* Calling tail policy plugin to detect body plugin. */
	if (plug_call(reg->policy->o.policy_ops, tails, new_size)) {
		/* Trying to get non-standard tail plugin from stat data. And if
		   it is not found, default one from params will be taken. */
		return obj40_plug(&reg->obj, ITEM_PLUG_TYPE, "tail");
	} else {
		/* The same for extent plugin */
		return obj40_plug(&reg->obj, ITEM_PLUG_TYPE, "extent");
	}
}

/* Makes tail2extent and extent2tail conversion. */
static errno_t reg40_convert(object_entity_t *entity, 
			     reiser4_plug_t *plug) 
{
	errno_t res;
	reg40_t *reg;
	uint64_t fsize;
	conv_hint_t hint;

	aal_assert("umka-2467", plug != NULL);
	aal_assert("umka-2466", entity != NULL);

	reg = (reg40_t *)entity;
	fsize = reg40_size(entity);

	/* Getting file data start key. We convert file starting from the zero
	   offset until end is reached. */
	plug_call(reg->position.plug->o.key_ops, assign,
		  &hint.offset, &reg->position);
	
	plug_call(reg->position.plug->o.key_ops, set_offset,
		  &hint.offset, 0);
	
	/* Prepare convert hint. */
	hint.chunk = 0;
	hint.bytes = 0;
	hint.plug = plug;
	hint.count = fsize;

	/* Converting file data. */
	if ((res = obj40_convert(&reg->obj, &hint)))
		return res;

	/* Updating stat data fields. */
	return obj40_touch(&reg->obj, fsize, hint.bytes);
}

/* Make sure, that file body is of particular plugin type, that depends on tail
   policy plugin. If no - convert it to plugin told by tail policy
   plugin. Called from all modifying calls like write(), truncate(), etc. */
static errno_t reg40_check_body(object_entity_t *entity,
				uint64_t new_size)
{
	reg40_t *reg;
	uint64_t fsize;

	reiser4_plug_t *body_plug;
	reiser4_plug_t *policy_plug;
	
	aal_assert("umka-2395", entity != NULL);

	reg = (reg40_t *)entity;
	fsize = reg40_size(entity);
	
	/* There is nothing to convert? */
	if (!fsize || !new_size)
		return 0;

	/* Getting policy plugin. It will be used for new file body items and
	   old body items will be converted to. */
	if (!(policy_plug = reg40_policy_plug(reg, new_size))) {
		aal_exception_error("Can't get body plugin "
				    "for new file size %llu.",
				    new_size);
		return -EIO;
	}

	/* Getting old file body plugin. This is needed for comparing with new
	   body plugin, told us by tail policy plugin. */
	if (!(body_plug = reg40_policy_plug(reg, fsize))) {
		aal_exception_error("Can't get body plugin "
				    "for old file size %llu.",
				    fsize);
		return -EIO;
	}
		
	/* Comparing new plugin and old one. If they are the same, conversion if
	   not needed. */
	if (plug_equal(policy_plug, body_plug))
		return 0;

	/* Convert file. */
	return reg40_convert(entity, policy_plug);
}

/* Writes passed data to the file. Returns amount of data on disk, that is
   @bytes value, which should be counted in stat data. */
int64_t reg40_put(object_entity_t *entity, void *buff, uint64_t n) {
	reg40_t *reg;
	int64_t written;
	trans_hint_t hint;
	uint64_t new_offset;

	reg = (reg40_t *)entity;

	/* Preparing hint to be used for calling write method. This is
	   initializing @count -- number iof bytes to write, @specific -- buffer
	   to write into and @offset -- file offset data must be written at. */
	hint.bytes = 0;
	hint.count = n;
	
	hint.specific = buff;
	hint.tree = reg->obj.info.tree;
	
	plug_call(reg->position.plug->o.key_ops,
		  assign, &hint.offset, &reg->position);

	/* Getting file plugin for new offset. */
	new_offset = reg40_offset(entity) + n;
	
	if (!(hint.plug = reg40_policy_plug(reg, new_offset)))
		return -EIO;

	/* Write data to tree. */
	if ((written = obj40_write(&reg->obj, &hint)) < 0)
		return written;

	/* Updating file offset. */
	reg40_seek(entity, reg40_offset(entity) + written);
	
	return hint.bytes;
}

/* Cuts some amount of data and makes file length of passed @n value. */
static int64_t reg40_cut(object_entity_t *entity, uint64_t n) {
	errno_t res;
	reg40_t *reg;
	uint64_t size;
	trans_hint_t hint;
	
	reg = (reg40_t *)entity;
	size = reg40_size(entity);

	/* Preparing key of the data to be truncated. */
	plug_call(STAT_KEY(&reg->obj)->plug->o.key_ops,
		  assign, &hint.offset, &reg->position);
		
	plug_call(STAT_KEY(&reg->obj)->plug->o.key_ops,
		  set_offset, &hint.offset, n);

	/* Removing data from the tree. */
	hint.count = size - n;
	hint.data = reg->obj.info.tree;
	hint.plug = reg40_policy_plug(reg, n);

	if ((res = obj40_truncate(&reg->obj, &hint)) < 0)
		return res;

	return hint.bytes;
}

/* Writes @n bytes from @buff to passed file */
static int64_t reg40_write(object_entity_t *entity, 
			   void *buff, uint64_t n) 
{
	int64_t res;
	reg40_t *reg;

	int64_t bytes;
	uint64_t size;
	uint64_t offset;
	
	aal_assert("umka-2280", buff != NULL);
	aal_assert("umka-2281", entity != NULL);

	reg = (reg40_t *)entity;
	size = reg40_size(entity);
	offset = reg40_offset(entity);

	/* Convert body items if needed. */
	if ((res = reg40_check_body(entity, size + n))) {
		aal_exception_error("Can't perform tail "
				    "conversion.");
		return res;
	}
		
	/* Inserting holes if needed. */
	if (offset > size) {
		if ((bytes = reg40_put(entity, NULL,
				       offset - size)) < 0)
		{
			return bytes;
		}
	} else {
		bytes = 0;
	}

	/* Putting data to tree. */
	if ((res = reg40_put(entity, buff, n)) < 0)
		return res;

	/* Updating stat data fields */
	bytes += obj40_get_bytes(&reg->obj) + res;

	return obj40_touch(&reg->obj, size + n, bytes);
}

/* Truncates file to passed size @n. */
static errno_t reg40_truncate(object_entity_t *entity,
			      uint64_t n)
{
	errno_t res;
	reg40_t *reg;
	int64_t bytes;
	uint64_t size;

	reg = (reg40_t *)entity;
	size = reg40_size(entity);

	if (size == n)
		return 0;

	if (n > size) {
		/* Converting body if needed. */
		if ((res = reg40_check_body(entity, n))) {
			aal_exception_error("Can't perform tail "
					    "conversion.");
			return res;
		}
		
		/* Inserting holes. */
		if ((bytes = reg40_put(entity, NULL,
				       n - size)) < 0)
		{
			return bytes;
		}
		
		/* Updating stat data fields */
		bytes += obj40_get_bytes(&reg->obj);
		return obj40_touch(&reg->obj, n, bytes);
	} else {
		/* Cutting items/units */
		if ((bytes = reg40_cut(entity, size - n)) < 0)
			return bytes;

		/* Updating stat data fields */
		bytes = obj40_get_bytes(&reg->obj) - bytes;

		if ((res = obj40_touch(&reg->obj, n, bytes)))
			return res;

		/* Converting body if needed. */
		if ((res = reg40_check_body(entity, n))) {
			aal_exception_error("Can't perform tail "
					    "conversion.");
			return res;
		}

		return 0;
	}
}

/* Removes file body items and file stat data item. */
static errno_t reg40_clobber(object_entity_t *entity) {
	errno_t res;
	
	aal_assert("umka-2299", entity != NULL);

	if ((res = reg40_truncate(entity, 0)))
		return res;

	return obj40_clobber(&((reg40_t *)entity)->obj);
}

/* File data enumeration related stuff. */
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

/* Enumerates all blocks belong to file and calls passed @region_func for each
   of them. It is needed for calculating fragmentation, printing, etc. */
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

	/* Initializing layout_hint. */
	hint.data = data;
	hint.entity = entity;
	hint.region_func = region_func;

	/* Loop though the all file items. */
	while (reg40_offset(entity) < size) {
		place_t *place = &reg->body;
		
		/* Update current body coord. */
		if ((res = reg40_update_body(entity)) < 0)
			return res;

		/* Check if file stream is over. */
		if (res == ABSENT)
			return 0;

		/* Calling item enumerator funtion for current body item. */
		if (place->plug->o.item_ops->object->layout) {
			if ((res = plug_call(place->plug->o.item_ops->object,
					     layout, place, callback_item_layout,
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
		   how much to increase file offset. */
		plug_call(reg->body.plug->o.item_ops->balance, maxreal_key,
			  &reg->body, &maxkey);

		/* Updating file offset. */
		offset = plug_call(maxkey.plug->o.key_ops, get_offset, &maxkey);
		reg40_seek(entity, reg40_offset(entity) + offset + 1);
	}
	
	return 0;
}

/* Implements metadata() function. It traverses items belong to file. This is
   needed for printing, getting metadata, etc. */
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

	/* Counting stat data item. */
	if ((res = obj40_metadata(&reg->obj, place_func, data)))
		return res;

	/* Loop thougj the all file items. */
	if (!(size = reg40_size(entity)))
		return 0;

	while (reg40_offset(entity) < size) {
		uint64_t offset;
		key_entity_t maxkey;
		
		/* Update body place. */
		if ((res = reg40_update_body(entity)) < 0)
			return res;

		/* Check if file stream is over. */
		if (res == ABSENT)
			return 0;

		/* Calling per-place callback function */
		if ((res = place_func(entity, &reg->body, data)))
			return res;

		/* Getting current item max real key inside, in order to know
		   how much increase file offset. */
		plug_call(reg->body.plug->o.item_ops->balance, maxreal_key,
			  &reg->body, &maxkey);

		/* Updating file offset */
		offset = plug_call(maxkey.plug->o.key_ops, get_offset, &maxkey);
		reg40_seek(entity, reg40_offset(entity) + offset + 1);
	}
	
	return 0;
}

/* Updates stat data from passed @hint */
static errno_t reg40_update(object_entity_t *entity,
			    statdata_hint_t *hint)
{
	reg40_t *reg;
	
	aal_assert("umka-2559", entity != NULL);
	aal_assert("umka-2560", hint != NULL);

	reg = (reg40_t *)entity;
	return obj40_save_stat(&reg->obj, hint);
}
#endif

/* Regular file operations. */
static reiser4_object_ops_t reg40_ops = {
#ifndef ENABLE_STAND_ALONE
	.create	        = reg40_create,
	.write	        = reg40_write,
	.truncate       = reg40_truncate,
	.layout         = reg40_layout,
	.metadata       = reg40_metadata,
	.convert        = reg40_convert,
	.form		= reg40_form,
	.update         = reg40_update,
	.link           = reg40_link,
	.unlink         = reg40_unlink,
	.links          = reg40_links,
	.clobber        = reg40_clobber,
	.recognize	= reg40_recognize,
	.check_struct   = reg40_check_struct,
	
	.add_entry      = NULL,
	.rem_entry      = NULL,
	.build_entry    = NULL,
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
		
	.stat           = reg40_stat,
	.open	        = reg40_open,
	.close	        = reg40_close,
	.reset	        = reg40_reset,
	.seek	        = reg40_seek,
	.offset	        = reg40_offset,
	.read	        = reg40_read,
};

/* Regular file plugin. */
reiser4_plug_t reg40_plug = {
	.cl    = class_init,
	.id    = {OBJECT_REG40_ID, REG_OBJECT, OBJECT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "reg40",
	.desc  = "Regular file for reiser4, ver. " VERSION,
#endif
	.o = {
		.object_ops = &reg40_ops
	}
};

/* Plugin factory related stuff. This method will be called during plugin
   initializing in plugin factory. */
static reiser4_plug_t *reg40_start(reiser4_core_t *c) {
	reg40_core = c;
	return &reg40_plug;
}

plug_register(reg40, reg40_start, NULL);
