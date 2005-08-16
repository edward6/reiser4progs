/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format40_repair.c -- repair methods for the default disk-layout plugin for 
   reiserfs 4.0. */

#ifndef ENABLE_MINIMAL

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
	format40_mkdirty(format);
	
 error_free_block:
	aal_block_fini(&block);
	return res;
}

errno_t format40_check_struct(generic_entity_t *entity, 
			      backup_hint_t *hint, 
			      format_hint_t *desc, 
			      uint8_t mode)
{
	format40_backup_t *backup;
	format40_super_t *super;
	format40_t *format;
	errno_t res = 0;
	rid_t pid;
	
	aal_assert("vpf-1739", entity != NULL);
	aal_assert("vpf-1740", desc != NULL);
	
	format = (format40_t *)entity;
	super = (format40_super_t *)&format->super;
	backup = hint ? (format40_backup_t *)
		(hint->block.data + hint->off[BK_FORMAT]) : NULL;
	
	if (backup) {
		aal_assert("vpf-1741", format->blksize == hint->block.size);
	} else {
		aal_assert("vpf-1742", format->blksize == desc->blksize);
	}
	
	/* Check magic */
	if (aal_strncmp(super->sb_magic, FORMAT40_MAGIC, 
			sizeof(FORMAT40_MAGIC))) 
	{
		fsck_mess("The on-disk format magic (%s) does not match the "
			  "format40 plugin one (%s).%s", super->sb_magic, 
			  FORMAT40_MAGIC, mode == RM_BUILD ? " Fixed." : "");

		if (mode == RM_BUILD) {
			aal_strncpy(super->sb_magic, FORMAT40_MAGIC, 
				    sizeof(FORMAT40_MAGIC));

			format40_mkdirty(format);
		} else {
			res |= RE_FATAL;
		}
	}

	/* Check the policy. */
	if (desc->mask & (1 << PM_POLICY)) {
		/* Policy must be set to @desc->policy if differs. */
		if (desc->policy != get_sb_policy(super)) {
			fsck_mess("The on-disk formatting plugin id (0x%x) "
				  "does not match the specified one (0x%x).%s",
				  get_sb_policy(super), desc->policy,
				  mode != RM_CHECK ? " Fixed." : "");
			
			if (mode == RM_CHECK) {
				res |= RE_FIXABLE;
			} else {
				set_sb_policy(super, desc->policy);
				format40_mkdirty(format);
			}
		}
	} else if (backup) {
		/* Check policy by the backup. */
		if (get_sb_policy(super) != get_sb_policy(backup)) {
			fsck_mess("The on-disk formatting plugin id (0x%x) "
				  "does not match the backup one (0x%x).%s",
				  get_sb_policy(super), get_sb_policy(backup),
				  mode != RM_CHECK ? " Fixed." : "");

			if (mode == RM_CHECK) {
				res |= RE_FIXABLE;
			} else {
				set_sb_policy(super, get_sb_policy(backup));
				format40_mkdirty(format);
			} 
		}
	} else {
		/* Check that on-disk policy is correct, otherwise set one from 
		   the profile. */

		if (!(format40_core->factory_ops.ifind(POLICY_PLUG_TYPE, 
						       get_sb_policy(super))))
		{
			fsck_mess("Can't find the formatting policy "
				  "plugin by the on-disk id 0x%x.", 
				  get_sb_policy(super));

			if (mode != RM_CHECK) {
				fsck_mess("Using the default formatting "
					  "policy 0x%x.", desc->policy);
				set_sb_policy(super, desc->policy);
				format40_mkdirty(format);
			} else {
				res |= RE_FIXABLE;
			}
		}
	}
	
	/* Check the key. */
	if (desc->mask & (1 << PM_KEY)) {
		/* Key must be set to the @desc->key if differ. */
		pid = format40_get_key(entity);
		if (desc->key != pid) {
			fsck_mess("The on-disk format key plugin id (0x%x) does"
				  " not match the specified one (0x%x).%s", pid,
				  desc->key, mode == RM_BUILD ? " Fixed." : "");

			if (mode == RM_BUILD)
				format40_set_key(entity, desc->key);
			else
				res |= RE_FATAL;
		}
	} else if (backup) {
		/* Check key by the backup. */
		if (get_sb_flags(super) != get_sb_flags(backup)) {
			fsck_mess("The on-disk format flags (0x%llx) does "
				  "not match the backup one (0x%llx).%s",
				  get_sb_flags(super), get_sb_flags(backup),
				  mode == RM_BUILD ? " Fixed." : "");

			if (mode == RM_BUILD) {
				set_sb_flags(super, get_sb_flags(backup));
				format40_mkdirty(format);
			} else {
				res |= RE_FATAL;
			}
		}
	} else {
		/* On-disk key is always correct as it is just 1 bit. */
	}
	
	/* Check the block count. */
	if (backup) {
		/* Check the block count. */
		if (get_sb_block_count(super) != get_sb_block_count(backup)) {
			fsck_mess("The on-disk format block count (%llu) does "
				  "not match the backup one (%llu).%s",
				  get_sb_block_count(super), 
				  get_sb_block_count(backup), 
				  mode == RM_BUILD ? " Fixed." : "");

			if (mode == RM_BUILD) {
				set_sb_block_count(super, 
						   get_sb_block_count(backup));

				format40_mkdirty(format);
			} else {
				res |= RE_FATAL;
			}
		}
	} else {
		/* Check the block count. */
		if (get_sb_block_count(super) != desc->blocks) {
			fsck_mess("The on-disk format block count (%llu) does "
				  "not match the specified one (%llu).%s",
				  get_sb_block_count(super), desc->blocks,
				  mode == RM_BUILD ? " Fixed." : "");

			if (mode == RM_BUILD) {
				set_sb_block_count(super, desc->blocks);
				format40_mkdirty(format);
			} else {
				res |= RE_FATAL;
			}
		}
	}
	
	/* Check the mkfs id */
	if (backup) {
		if (get_sb_mkfs_id(super) != get_sb_mkfs_id(backup)) {
			fsck_mess("The on-disk format mkfs id (0x%x) does "
				  "not match the backup one (0x%x).%s",
				  get_sb_mkfs_id(super), get_sb_mkfs_id(backup),
				  mode == RM_BUILD ? " Fixed." : "");

			if (mode == RM_BUILD) {
				set_sb_mkfs_id(super, get_sb_mkfs_id(backup));
				format40_mkdirty(format);
			} else {
				res |= RE_FATAL;
			}
		}
	} 
	
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

generic_entity_t *format40_unpack(aal_device_t *device, 
				  uint32_t blksize,
				  aal_stream_t *stream)
{
	uint32_t size;
	format40_t *format;
	
	aal_assert("umka-2650", device != NULL);
	aal_assert("umka-2603", stream != NULL);

	/* Initializing format instance. */
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

	format->plug = &format40_plug;
	format->device = device;
	format->blksize = blksize;

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

	format40_mkdirty(format);
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

errno_t format40_check_backup(backup_hint_t *hint) {
	format40_backup_t *backup;
	
	aal_assert("vpf-1733", hint != NULL);

	backup = (format40_backup_t *)
		(hint->block.data + hint->off[BK_FORMAT]);
	
	hint->off[BK_FORMAT + 1] = hint->off[BK_FORMAT] + sizeof(*backup);
	
	/* Check the MAGIC. */
	if (aal_memcmp(backup->sb_magic, FORMAT40_MAGIC, 
		       sizeof(FORMAT40_MAGIC)))
	{
		return RE_FATAL;
	}
	
	/* Block count must be greater than the read block number. */
	if (get_sb_block_count(backup) <= hint->block.nr)
		return RE_FATAL;

	/* The is no need to check the block count as it will be checked
	   in repair_format_check_struct, just set blocks into the hint. */
	
	/* Some policy plugin must be found. */
	if (!format40_core->factory_ops.ifind(POLICY_PLUG_TYPE, 
					      get_sb_policy(backup))) 
	{
		return RE_FATAL;
	}
	
	/* Save the block count in the hint for the futher use. */
	hint->blocks = get_sb_block_count(backup);
	
	return 0;
}

/* Regenerate the format instance by the backup. */
generic_entity_t *format40_regenerate(aal_device_t *device, 
				      backup_hint_t *hint) 
{
	format40_backup_t *backup;
	format40_super_t *super;
	format40_t *format;
	
	aal_assert("vpf-1737", hint != NULL);
	aal_assert("vpf-1738", device != NULL);

	/* Initializing format instance. */
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

	backup = (format40_backup_t *)
		(hint->block.data + hint->off[BK_FORMAT]);
	
	format->plug = &format40_plug;
	format->device = device;
	format->blksize = hint->block.size;
	format40_mkdirty(format);
	
	/* Initializing super block fields. */
	super = (format40_super_t *)&format->super;

	/* Initialize fields by the backup or leave them uninitialized. */
	aal_strncpy(super->sb_magic, backup->sb_magic, sizeof(super->sb_magic));
	set_sb_block_count(super, get_sb_block_count(backup));
	set_sb_flags(super, get_sb_flags(backup));
	set_sb_mkfs_id(super, get_sb_mkfs_id(backup));
	set_sb_policy(super, get_sb_policy(backup));
	
	set_sb_tree_height(super, 2);
	set_sb_root_block(super, INVAL_BLK);

	return (generic_entity_t *)format;
}

#endif
