/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format40_repair.c -- repair methods for the default disk-layout plugin for 
   reiserfs 4.0. */

#ifndef ENABLE_STAND_ALONE

#include "format40.h"
#include <repair/plugin.h>

errno_t format40_check_struct(generic_entity_t *entity, uint8_t mode) {
	format40_t *format = (format40_t *)entity;
	format40_super_t *super;
	count_t count;
	errno_t res = 0;
	
	aal_assert("vpf-160", entity != NULL);
	
	super = &format->super;
	
	count = aal_device_len(format->device) /
		(format->blksize / format->device->blksize);
	
	/* Check the fs size. */
	if (count < get_sb_block_count(super)) {
		/* Device is smaller then fs size. */
		if (mode == RM_BUILD) {
			if (aal_exception_throw(EXCEPTION_TYPE_FATAL,
						EXCEPTION_OPT_YESNO,
						"Number of blocks found in the "
						"superblock (%llu) is not equal "
						"to the size of the partition "
						"(%llu).\n Are you sure that "
						"the partition size is correct?", 
						get_sb_block_count(super), 
						count) == EXCEPTION_OPT_NO)
			{
				/* This is not the repair code problem. */
				return -EINVAL;
			}
			
			set_sb_block_count(super, count);
			format->state |= (1 << ENTITY_DIRTY);
		} else {
			aal_fatal("Number of blocks found in the "
				  "superblock (%llu) is not equal to "
				  "the size of the partition (%llu).\n "
				  "Check the partition size first.", 
				  get_sb_block_count(super), count);
			return RE_FATAL;
		}
	} else if (count > get_sb_block_count(super)) {
		/* Device is larger then fs size. */
		aal_fatal("Number of blocks found in the superblock "
			  "(%llu) is not equal to the size of the "
			  "partition (%llu). %s", get_sb_block_count(super),
			  count, mode != RM_CHECK ? "Fixed.": "");
		
		set_sb_block_count(super, count);
		
		if (mode != RM_CHECK)
			format->state |= (1 << ENTITY_DIRTY);
		else 
			res |= RE_FIXABLE;
	}
	
	/* Check the free block count. */
	if (get_sb_free_blocks(super) > get_sb_block_count(super)) {
		aal_error("Invalid free block count (%llu) found in the "
			  "superblock. %s", get_sb_free_blocks(super), 
			  mode == RM_CHECK ? 
			  "" : "Will be fixed later.");
		
		if (mode == RM_CHECK)
			res |= RE_FIXABLE;
	}
	
	return res;
}

/* Update from the device only those fields which can be changed while 
   replaying. */
errno_t format40_update(generic_entity_t *entity) {
	format40_t *format = (format40_t *)entity;
	format40_super_t *super;
	aal_block_t block;
	errno_t res;
	blk_t blk;
	
	aal_assert("vpf-828", format != NULL);
	aal_assert("vpf-828", format->device != NULL);
	
	blk = FORMAT40_BLOCKNR(format->blksize);

	if ((res = aal_block_init(&block, format->device,
				  format->blksize, blk)))
		return res;
	
	if ((res = aal_block_read(&block))) {
		aal_error("Failed to read the block (%llu).", blk);
		goto error_free_block;
	}
	
	super = (format40_super_t *)block.data;

	format->super.sb_oid = super->sb_oid;
	format->super.sb_free_blocks = super->sb_free_blocks;
	format->super.sb_root_block = super->sb_root_block;
	format->super.sb_file_count = super->sb_file_count;
	format->super.sb_tree_height = super->sb_tree_height;
	format->super.sb_flushes = super->sb_flushes;
	
 error_free_block:
	aal_block_fini(&block);
	return res;
}

errno_t format40_pack(generic_entity_t *entity,
		      aal_stream_t *stream)
{
	rid_t pid;
	uint32_t size;
	format40_t *format;
	
	aal_assert("umka-2600", entity != NULL);
	aal_assert("umka-2601", stream != NULL);

	format = (format40_t *)entity;

	/* Write plugin id. */
	pid = entity->plug->id.id;
	aal_stream_write(stream, &pid, sizeof(pid));

	/* Write data size. */
	size = sizeof(format->super);
	aal_stream_write(stream, &size, sizeof(size));

	/* Write format data to @stream. */
	aal_stream_write(stream, &format->super, size);

	return 0;
}

generic_entity_t *format40_unpack(fs_desc_t *desc,
				  aal_stream_t *stream)
{
	uint32_t size;
	format40_t *format;
	
	aal_assert("umka-2650", desc != NULL);
	aal_assert("umka-2603", stream != NULL);

	/* Initializing format instance. */
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

	format->plug = &format40_plug;
	format->device = desc->device;
	format->blksize = desc->blksize;

	/* Read size nad check for validness. */
	if (aal_stream_read(stream, &size, sizeof(size)) != sizeof(size))
		goto error_eostream;

	if (size != sizeof(format->super)) {
		aal_error("Invalid size %u is detected in stream.", size);
		goto error_free_format;
	}

	/* Read format data from @stream. */
	if (aal_stream_read(stream, &format->super, size) != (int32_t)size)
		goto error_eostream;

	format->state |= (1 << ENTITY_DIRTY);
	return (generic_entity_t *)format;

 error_eostream:
	aal_error("Can't unpack the disk format40. Stream is over?");
	
 error_free_format:
	aal_free(format);
	return NULL;
}

void format40_print(generic_entity_t *entity,
		    aal_stream_t *stream,
		    uint16_t options) 
{
	rid_t tail_pid;
	format40_t *format;
	format40_super_t *super;
	reiser4_plug_t *tail_plug;
    
	aal_assert("vpf-246", entity != NULL);
	aal_assert("umka-1290", stream != NULL);

	format = (format40_t *)entity;
	super = &format->super;
    
	tail_pid = get_sb_tail_policy(super);

	if (!(tail_plug = format40_core->factory_ops.ifind(POLICY_PLUG_TYPE,
							   tail_pid)))
	{
		aal_error("Can't find tail policy plugin by its id 0x%x.",
			  tail_pid);
	}
		
	aal_stream_format(stream, "Format super block (%lu):\n",
			  FORMAT40_BLOCKNR(format->blksize));
	
	aal_stream_format(stream, "plugin:\t\t%s\n",
			  entity->plug->label);
	
	aal_stream_format(stream, "description:\t%s\n",
			  entity->plug->desc);

	aal_stream_format(stream, "magic:\t\t%s\n",
			  super->sb_magic);
	
	aal_stream_format(stream, "flushes:\t%llu\n",
			  get_sb_flushes(super));
	
	aal_stream_format(stream, "mkfs id:\t0x%x\n",
			  get_sb_mkfs_id(super));
    
	aal_stream_format(stream, "blocks:\t\t%llu\n",
			  get_sb_block_count(super));
	
	aal_stream_format(stream, "free blocks:\t%llu\n",
			  get_sb_free_blocks(super));
	
	aal_stream_format(stream, "root block:\t%llu\n",
			  get_sb_root_block(super));

	aal_stream_format(stream, "tail policy:\t0x%x (%s)\n",
			  tail_pid, tail_plug ? tail_plug->label:
			  "absent");
	
	aal_stream_format(stream, "next oid:\t0x%llx\n",
			  get_sb_oid(super));
	
	aal_stream_format(stream, "file count:\t%llu\n",
			  get_sb_file_count(super));
	
	aal_stream_format(stream, "tree height:\t%u\n",
			  get_sb_tree_height(super));

	if (aal_test_bit(&super->sb_flags, 0))
		aal_stream_format(stream, "key policy:\tLARGE\n");
	else
		aal_stream_format(stream, "key policy:\tSHORT\n");
}

#endif
