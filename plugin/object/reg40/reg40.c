/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40.c -- reiser4 regular file plugin. */

#ifndef ENABLE_MINIMAL
#  include <unistd.h>
#endif

#include <aal/libaal.h>
#include "reiser4/plugin.h"
#include "plugin/object/obj40/obj40.h"
#include "reg40_repair.h"

/* Reads @n bytes to passed buffer @buff. Negative values are returned on
   errors. */
static int64_t reg40_read(reiser4_object_t *reg, 
			  void *buff, uint64_t n)
{
	errno_t res;
	int64_t read;
	uint64_t off;
	uint64_t fsize;
	trans_hint_t hint;

	aal_assert("umka-2511", buff != NULL);
	aal_assert("umka-2512", reg != NULL);
	
	if ((res = obj40_update(reg)))
		return res;
	
	fsize = obj40_get_size(reg);
	off = obj40_offset(reg);
	if (off > fsize)
		return 0;

	/* Correcting number of bytes to be read. It cannot be more then file
	   size value from stat data. That is because, body item itself does 
	   not know reliably how long it is. For instnace, extent40. */
	if (n > fsize - off)
		n = fsize - off;
	
	/* Reading data. */
	if ((read = obj40_read(reg, &hint, buff, off, n)) < 0)
		return read;

	/* Updating file offset if needed. */
	if (read > 0)
		obj40_seek(reg, off + read);
	
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

	if ((res = obj40_update(reg)))
		return res;
	
	hint.count = obj40_get_size(reg);
	hint.place_func = NULL;

	/* Converting file data. */
	if ((res = obj40_convert(reg, &hint)))
		return res;
	
	/* Updating stat data place */
	if ((res = obj40_update(reg)))
		return res;
	
	/* Updating stat data fields. */
	if (hint.bytes != obj40_get_bytes(reg))
		return obj40_set_bytes(reg, hint.bytes);

	return 0;
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

/* Writes @n bytes from @buff to passed file */
static int64_t reg40_write(reiser4_object_t *reg, 
			   void *buff, uint64_t n) 
{
	trans_hint_t hint;
	uint64_t fsize;
	int64_t count;
	int64_t bytes;
	uint64_t off;
	int64_t res;

	aal_assert("umka-2281", reg != NULL);
	
	if ((res = obj40_update(reg)))
		return res;
	
	fsize = obj40_get_size(reg);
	off = obj40_offset(reg);
	bytes = 0;
	
	/* Inserting holes if needed. */
	if (off > fsize) {
		count = off - fsize;
		
		/* Fill the hole with zeroes. */
		if ((res = obj40_write(reg, &hint, NULL, fsize, count,
				       reg->body_plug, NULL)) < 0)
		{
			return res;
		}
		
		/* If not enough bytes are written, the hole is not 
		   filled yet, cannot continue, return 0. */
		if (res != count) {
			if ((res = obj40_touch(reg, res, hint.bytes)) < 0)
				return res;
			
			return 0;
		}
		
		bytes = hint.bytes;
	} 
	
	/* Putting data to tree. */
	if ((count = obj40_write(reg, &hint, buff, off, n,
				 reg->body_plug, NULL)) < 0)
	{
		return count;
	}
	
	bytes += hint.bytes;
	off += count;
	
	obj40_seek(reg, off);
	
	off = (off > fsize ? off - fsize : 0);
	
	/* Updating the SD place and update size, bytes there. */
	if ((res = obj40_touch(reg, off, bytes)) < 0)
		return res;
	
	fsize += off;
	
	/* Convert body items if needed. */
	if ((res = reg40_check_body(reg, fsize))) {
		aal_error("Can't perform tail conversion.");
		return res;
	}
	
	return count;
}

/* Truncates file to passed size @n. */
static errno_t reg40_truncate(reiser4_object_t *reg, uint64_t n) {
	trans_hint_t hint;
	uint64_t size;
	errno_t res;

	if ((res = obj40_update(reg)))
		return res;
	
	size = obj40_get_size(reg);

	if (size == n)
		return 0;

	if (n > size) {
		return obj40_touch(reg, n - size, 0);
	}
	
	/* Cutting items/units */
	if ((res = obj40_truncate(reg, &hint, size, reg->body_plug)) < 0)
		return res;
	
	/* Updating stat data fields. */
	if ((res = obj40_touch(reg, n - size, -hint.bytes)))
		return res;

	/* Converting body if needed. */
	if ((res = reg40_check_body(reg, n))) {
		aal_error("Can't perform tail conversion.");
		return res;
	}

	return 0;
}

/* Removes file body items and file stat data item. */
static errno_t reg40_clobber(reiser4_object_t *reg) {
	errno_t res;
	
	aal_assert("umka-2299", reg != NULL);

	if ((res = reg40_truncate(reg, 0)))
		return res;

	return obj40_clobber(reg);
}

/* Enumerates all blocks belong to file and calls passed @region_func for each
   of them. It is needed for calculating fragmentation, printing, etc. */
static errno_t reg40_layout(reiser4_object_t *reg,
			    region_func_t func,
			    void *data)
{
	obj40_reset(reg);
	return obj40_layout(reg, func, NULL, data);
}

/* Implements metadata() function. It traverses items belong to file. This is
   needed for printing, getting metadata, etc. */
static errno_t reg40_metadata(reiser4_object_t *reg,
			      place_func_t place_func,
			      void *data)
{
	obj40_reset(reg);
	return obj40_traverse(reg, place_func, NULL, data);
}
#endif

/* Regular file operations. */
static reiser4_object_plug_t reg40 = {
#ifndef ENABLE_MINIMAL
	.inherit	= obj40_inherit,
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
	.sdext_unknown   = (1 << SDEXT_SYMLINK_ID |
			    1 << SDEXT_CLUSTER_ID),
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
