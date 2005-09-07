/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40.c -- reiser4 regular file plugin. */

#ifndef ENABLE_MINIMAL
#  include <unistd.h>
#endif

#include "reg40.h"
#include "reg40_repair.h"

/* Reads @n bytes to passed buffer @buff. Negative values are returned on
   errors. */
static int64_t reg40_read(reiser4_object_t *reg, 
			  void *buff, uint64_t n)
{
	int64_t read;
	uint64_t fsize;
	trans_hint_t hint;

	aal_assert("umka-2511", buff != NULL);
	aal_assert("umka-2512", reg != NULL);
	
	fsize = obj40_size(reg);

	/* Preparing hint to be used for calling read with it. Here we
	   initialize @count -- number of bytes to read, @specific -- pointer to
	   buffer data will be read into, and pointer to tree instance, file is
	   opened on. */ 
	aal_memset(&hint, 0, sizeof(hint));
	hint.count = n;
	hint.specific = buff;

	/* Initializing offset data must be read from. This is current file
	   offset, so we use @reg->position. */
	aal_memcpy(&hint.offset, &reg->position, sizeof(hint.offset));

	/* Correcting number of bytes to be read. It cannot be more then file
	   size value from stat data. That is because, body item itself does not
	   know reliably how long it is. For instnace, extent40. */
	if (obj40_offset(reg) + hint.count > fsize)
		hint.count = fsize - obj40_offset(reg);

	/* Reading data. */
	if ((read = obj40_read(reg, &hint)) < 0)
		return read;

	/* Updating file offset if needed. */
	if (read > 0)
		obj40_seek(reg, obj40_offset(reg) + read);
	
	return read;
}

/* Open regular file by passed initial info and return initialized
   instance. This @info struct contains information about the obejct, like its
   statdata coord, etc. */
static errno_t reg40_open(reiser4_object_t *reg) {
	obj40_open(reg);

#ifndef ENABLE_MINIMAL
	{
		lookup_t lookup;

		/* Get the body plugin in use. */
		if ((lookup = obj40_find_item(reg, &reg->position, FIND_EXACT, 
					      NULL, NULL, &reg->body)) < 0)
		{
			return lookup;
		} else if (lookup > 0) {
			reg->body_plug = reg->body.plug;
		}
	}
#endif

	return 0;
}

#ifndef ENABLE_MINIMAL
/* Returns plugin (tail or extent) for next write operation basing on passed
   @size -- new file size. This function will use tail policy plugin to find
   what kind of next body item should be writen. */
reiser4_plug_t *reg40_policy_plug(reiser4_object_t *reg, uint64_t new_size) {
	reiser4_plug_t *policy;
	
	aal_assert("umka-2394", reg != NULL);

	policy = reg->info.opset.plug[OPSET_POLICY];
	
	aal_assert("umka-2393", policy != NULL);

	/* Calling formatting policy plugin to detect body plugin. */
	if (plug_call(policy->pl.policy, tails, new_size)) {
		/* Trying to get non-standard tail plugin from stat data. And if
		   it is not found, default one from params will be taken. */
		return reg->info.opset.plug[OPSET_TAIL];
	}
	
	/* The same for extent plugin */
	return reg->info.opset.plug[OPSET_EXTENT];
}

/* Makes tail2extent and extent2tail conversion. */
static errno_t reg40_convert(reiser4_object_t *reg, 
			     reiser4_plug_t *plug) 
{
	errno_t res;
	conv_hint_t hint;

	aal_assert("umka-2467", plug != NULL);
	aal_assert("umka-2466", reg != NULL);

	aal_memset(&hint, 0, sizeof(hint));
	
	/* Getting file data start key. We convert file starting from the zero
	   offset until end is reached. */
	aal_memcpy(&hint.offset, &reg->position, sizeof(hint.offset));
	
	plug_call(reg->position.plug->pl.key, set_offset,
		  &hint.offset, 0);
	
	/* Prepare convert hint. */
	hint.plug = plug;

	hint.count = obj40_size(reg);
	hint.place_func = NULL;

	/* Converting file data. */
	if ((res = obj40_convert(reg, &hint)))
		return res;

	/* Updating stat data fields. */
	return obj40_touch(reg, MAX_UINT64, hint.bytes);
}

/* Make sure, that file body is of particular plugin type, that depends on tail
   policy plugin. If no - convert it to plugin told by tail policy
   plugin. Called from all modifying calls like write(), truncate(), etc. */
static errno_t reg40_check_body(reiser4_object_t *reg,
				uint64_t new_size)
{
	reiser4_plug_t *plug;
	
	aal_assert("umka-2395", reg != NULL);

	/* There is nothing to convert? */
	if (!new_size)
		return 0;

	/* Getting item plugin that should be used according to 
	   the current tail policy plugin. */
	if (!(plug = reg40_policy_plug(reg, new_size))) {
		aal_error("Can't get body plugin for new "
			  "file size %llu.", new_size);
		return -EIO;
	}

	if (!reg->body_plug) {
		reg->body_plug = plug;
		return 0;
	}
		
	/* Comparing new plugin and old one. If they are the same, conversion if
	   not needed. */
	if (plug_equal(plug, reg->body_plug))
		return 0;

	/* Convert file. */
	reg->body_plug = plug;
	return reg40_convert(reg, plug);
}

/* Writes passed data to the file. Returns amount of data on disk, that is
   @bytes value, which should be counted in stat data. */
int64_t reg40_put(reiser4_object_t *reg, void *buff, 
		  uint64_t n, place_func_t place_func) 
{
	int64_t written;
	trans_hint_t hint;

	/* Preparing hint to be used for calling write method. This is
	   initializing @count - number of bytes to write, @specific - buffer to
	   write into and @offset -- file offset data must be written at. */
	aal_memset(&hint, 0, sizeof(hint));
	hint.count = n;

	hint.specific = buff;
	hint.shift_flags = SF_DEFAULT;
	hint.place_func = place_func;
	hint.plug = reg->body_plug;

	aal_memcpy(&hint.offset, &reg->position, sizeof(hint.offset));

	/* Write data to tree. */
	if ((written = obj40_write(reg, &hint)) < 0)
		return written;

	/* Updating file offset. */
	obj40_seek(reg, obj40_offset(reg) + written);
	
	return hint.bytes;
}

/* Cuts some amount of data and makes file length of passed @n value. */
static int64_t reg40_cut(reiser4_object_t *reg, uint64_t offset) {
	errno_t res;
	uint64_t size;
	trans_hint_t hint;
	
	size = obj40_size(reg);

	aal_assert("umka-3076", size > offset);

	aal_memset(&hint, 0, sizeof(hint));
	
	/* Preparing key of the data to be truncated. */
	aal_memcpy(&hint.offset, &reg->position, sizeof(hint.offset));
		
	plug_call(reg->info.object.plug->pl.key,
		  set_offset, &hint.offset, offset);

	/* Removing data from the tree. */
	hint.count = MAX_UINT64;
	hint.shift_flags = SF_DEFAULT;
	hint.data = reg->info.tree;
	hint.plug = reg40_policy_plug(reg, offset);

	if ((res = obj40_truncate(reg, &hint)) < 0)
		return res;

	return hint.bytes;
}

/* Writes @n bytes from @buff to passed file */
static int64_t reg40_write(reiser4_object_t *reg, 
			   void *buff, uint64_t n) 
{
	int64_t res;

	int64_t bytes;
	uint64_t size;
	uint64_t offset;
	uint64_t new_size;

	aal_assert("umka-2281", reg != NULL);

	size = obj40_size(reg);
	offset = obj40_offset(reg);
	new_size = offset + n > size ? offset + n : size;
	
	/* Convert body items if needed. */
	if ((res = reg40_check_body(reg, new_size))) {
		aal_error("Can't perform tail conversion.");
		return res;
	}
		
	/* Inserting holes if needed. */
	if (offset > size) {
		uint64_t hole = offset - size;

		/* Seek back to size of hole, as reg40_put() uses 
		   @reg->position as data write offset. */
		obj40_seek(reg, size);

		/* Put a hole of size @hole. */
		if ((bytes = reg40_put(reg, NULL, hole, NULL)) < 0)
			return bytes;
	} else {
		bytes = 0;
	}

	/* Putting data to tree. */
	if ((res = reg40_put(reg, buff, n, NULL)) < 0)
		return res;

	bytes += res;

	/* Updating the SD place and update size, bytes there. */
	if ((res = obj40_update(reg)))
		return res;

	/* Calculating new @bytes and updating stat data fields. */
	bytes += obj40_get_bytes(reg);
	if ((res = obj40_touch(reg, new_size, bytes)))
		return res;
	
	return bytes;
}

/* Truncates file to passed size @n. */
static errno_t reg40_truncate(reiser4_object_t *reg, uint64_t n) {
	errno_t res;
	int64_t bytes;
	uint64_t size;

	size = obj40_size(reg);

	if (size == n)
		return 0;

	if (n > size) {
		/* Converting body if needed. */
		if ((res = reg40_check_body(reg, n))) {
			aal_error("Can't perform tail conversion.");
			return res;
		}

		obj40_seek(reg, size);
		if ((bytes = reg40_put(reg, NULL, n - size, NULL)) < 0)
			return bytes;
		
		/* Updating stat data fields. */
		if ((res = obj40_update(reg )))
			return res;
		
		bytes += obj40_get_bytes(reg );
		return obj40_touch(reg , n, bytes);
	} else {
		/*
		if (reg->body_plug->id.group == EXTENT_ITEM) {
			uint32_t blksize;
			
			blksize = place_blksize(STAT_PLACE(reg ));
			size = (n + blksize - 1) / blksize * blksize;
		} else */
			size = n;
			
		/* Cutting items/units */
		if ((bytes = reg40_cut(reg, size)) < 0)
			return bytes;

		/* Updating stat data fields. */
		if ((res = obj40_update(reg )))
			return res;

		bytes = obj40_get_bytes(reg ) - bytes;

		if ((res = obj40_touch(reg , n, bytes)))
			return res;

		/* Converting body if needed. */
		if ((res = reg40_check_body(reg, n))) {
			aal_error("Can't perform tail conversion.");
			return res;
		}

		return 0;
	}
}

/* Removes file body items and file stat data item. */
static errno_t reg40_clobber(reiser4_object_t *reg) {
	errno_t res;
	
	aal_assert("umka-2299", reg != NULL);

	if ((res = reg40_truncate(reg, 0)))
		return res;

	return obj40_clobber(reg);
}

/* File data enumeration related stuff. */
typedef struct layout_hint {
	void *data;
	reiser4_object_t *reg;
	region_func_t region_func;
} layout_hint_t;

static errno_t cb_item_layout(blk_t start, count_t width, void *data) {
	layout_hint_t *hint = (layout_hint_t *)data;
	return hint->region_func(start, width, hint->data);
}

/* Enumerates all blocks belong to file and calls passed @region_func for each
   of them. It is needed for calculating fragmentation, printing, etc. */
static errno_t reg40_layout(reiser4_object_t *reg,
			    region_func_t region_func,
			    void *data)
{
	errno_t res;
	uint64_t size;

	layout_hint_t hint;
	reiser4_key_t maxkey;
		
	aal_assert("umka-1471", reg != NULL);
	aal_assert("umka-1472", region_func != NULL);

	if (!(size = obj40_size(reg)))
		return 0;

	/* Initializing layout_hint. */
	hint.data = data;
	hint.reg = reg;
	hint.region_func = region_func;

	/* Loop though the all file items. */
	while (obj40_offset(reg) < size) {
		reiser4_place_t *place = &reg->body;
		
		/* Update current body coord. */
		if ((res = obj40_find_item(reg, &reg->position, FIND_EXACT, 
					   NULL, NULL, &reg->body)) < 0)
		{
			return res;
		}
		
		/* Check if file stream is over. */
		if (res == ABSENT)
			return 0;

		/* Calling item enumerator funtion for current body item. */
		if (place->plug->pl.item->object->layout) {
			if ((res = plug_call(place->plug->pl.item->object,
					     layout, place, cb_item_layout,
					     &hint)))
			{
				return res;
			}
		} else {
			blk_t blk = place_blknr(place);
			
			if ((res = cb_item_layout(blk, 1, &hint)))
				return res;
		}

		/* Getting current item max real key inside, in order to know
		   how much to increase file offset. */
		plug_call(place->plug->pl.item->balance, maxreal_key,
			  place, &maxkey);

		/* Updating file offset. */
		obj40_seek(reg, plug_call(maxkey.plug->pl.key,
					  get_offset, &maxkey) + 1);
	}
	
	return 0;
}

/* Implements metadata() function. It traverses items belong to file. This is
   needed for printing, getting metadata, etc. */
static errno_t reg40_metadata(reiser4_object_t *reg,
			      place_func_t place_func,
			      void *data)
{
	errno_t res;
	uint64_t size;
	
	aal_assert("umka-2386", reg != NULL);
	aal_assert("umka-2387", place_func != NULL);

	/* Counting stat data item. */
	if ((res = obj40_metadata(reg , place_func, data)))
		return res;

	/* Loop thougj the all file items. */
	if (!(size = obj40_size(reg)))
		return 0;

	while (obj40_offset(reg) < size) {
		reiser4_key_t maxkey;
		
		/* Update body place. */
		if ((res = obj40_find_item(reg, &reg->position, FIND_EXACT, 
					   NULL, NULL, &reg->body)) < 0)
		{
			return res;
		}
		
		/* Check if file stream is over. */
		if (res == ABSENT)
			return 0;

		/* Calling per-place callback function */
		if ((res = place_func(&reg->body, data)))
			return res;

		/* Getting current item max real key inside, in order to know
		   how much increase file offset. */
		plug_call(reg->body.plug->pl.item->balance, maxreal_key,
			  &reg->body, &maxkey);

		/* Updating file offset */
		obj40_seek(reg, plug_call(maxkey.plug->pl.key,
					  get_offset, &maxkey) + 1);
	}
	
	return 0;
}
#endif

/* Regular file operations. */
static reiser4_object_plug_t reg40 = {
#ifndef ENABLE_MINIMAL
	.create	        = obj40_create,
	.write	        = reg40_write,
	.truncate       = reg40_truncate,
	.layout         = reg40_layout,
	.metadata       = reg40_metadata,
	.convert        = reg40_convert,
	.update         = obj40_save_stat,
	.link           = obj40_link,
	.unlink         = obj40_unlink,
	.linked         = obj40_linked,
	.clobber        = reg40_clobber,
	.recognize	= obj40_recognize,
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
		
	.stat           = obj40_load_stat,
	.open	        = reg40_open,
	.close	        = NULL,
	.reset	        = obj40_reset,
	.seek	        = obj40_seek,
	.offset	        = obj40_offset,
	.read	        = reg40_read,

#ifndef ENABLE_MINIMAL
	.sdext_mandatory = (1 << SDEXT_LW_ID),
	.sdext_unknown   = (1 << SDEXT_SYMLINK_ID)
#endif
};

/* Regular file plugin. */
reiser4_plug_t reg40_plug = {
	.cl    = class_init,
	.id    = {OBJECT_REG40_ID, REG_OBJECT, OBJECT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
	.label = "reg40",
	.desc  = "Regular file plugin.",
#endif
	.pl = {
		.object = &reg40
	}
};

/* Plugin factory related stuff. This method will be called during plugin
   initializing in plugin factory. */
static reiser4_plug_t *reg40_start(reiser4_core_t *c) {
	obj40_core = c;
	return &reg40_plug;
}

plug_register(reg40, reg40_start, NULL);
