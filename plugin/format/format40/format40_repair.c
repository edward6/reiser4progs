/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format40_repair.c -- repair methods for the default disk-layout plugin for 
   reiserfs 4.0. */

#ifndef ENABLE_STAND_ALONE

#include "format40.h"
#include <repair/plugin.h>

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

	/* oid, free blocks and file count are updated right into 
	   the format. */
	format->super.sb_root_block = super->sb_root_block;
	format->super.sb_tree_height = super->sb_tree_height;
	format->super.sb_flushes = super->sb_flushes;
	format->state |= (1 << ENTITY_DIRTY);
	
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
	format40_super_t *super;
	reiser4_plug_t *plug;
	format40_t *format;
	rid_t pid;
    
	aal_assert("vpf-246", entity != NULL);
	aal_assert("umka-1290", stream != NULL);

	format = (format40_t *)entity;
	super = &format->super;
    
	pid = get_sb_policy(super);

	if (!(plug = format40_core->factory_ops.ifind(POLICY_PLUG_TYPE, pid))) {
		aal_error("Can't find tail policy plugin by its id 0x%x.", pid);
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
			  pid, plug ? plug->label:
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
