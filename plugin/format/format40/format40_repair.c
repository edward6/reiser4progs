/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format40_repair.c -- repair methods for the default disk-layout plugin for 
   reiserfs 4.0. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "format40.h"
#include <repair/plugin.h>

errno_t format40_check_struct(object_entity_t *entity, uint8_t mode) {
	format40_t *format = (format40_t *)entity;
	format40_super_t *super;
	count_t count;
	errno_t result = RE_OK;
	
	aal_assert("vpf-160", entity != NULL);
	
	super = &format->super;
	count = aal_device_len(format->device);
	
	/* Check the fs size. */
	if (count < get_sb_block_count(super)) {
		/* Device is smaller then fs size. */
		if (mode == RM_BUILD) {
			if (aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_YESNO,
						"Number of blocks found in the "
						"superblock (%llu) is not equal "
						"to the size of the partition "
						"(%llu).\n Are you sure that "
						"the partition size is correct?", 
						get_sb_block_count(super), 
						count) == EXCEPTION_NO)
			{
				/* This is not the repair code problem. */
				return -EINVAL;
			}
			
			set_sb_block_count(super, count);
			result |= RE_FIXED;	    
		} else {
			aal_exception_fatal("Number of blocks found in the "
					    "superblock (%llu) is not equal to "
					    "the size of the partition (%llu).\n "
					    "Check the partition size first.", 
					    get_sb_block_count(super), count);
			return RE_FATAL;
		}
	} else if (count > get_sb_block_count(super)) {
		/* Device is larger then fs size. */
		aal_exception_fatal("Number of blocks found in the superblock "
				    "(%llu) is not equal to the size of the "
				    "partition (%llu). %s", get_sb_block_count(super),
				    count, mode != RM_CHECK ? "Fixed.": "");
		
		if (mode != RM_CHECK) {
			set_sb_block_count(super, count);
			result |= RE_FIXED;
		} else 
			result |= RE_FIXABLE;
	}
	
	/* Check the free block count. */
	if (get_sb_free_blocks(super) > get_sb_block_count(super)) {
		aal_exception_error("Invalid free block count (%llu) found in the "
				    "superblock. %s", get_sb_free_blocks(super), 
				    mode == RM_CHECK ? 
				    "" : "Will be fixed later.");
		
		if (mode == RM_CHECK)
			result |= RE_FIXABLE;
	}
	
	count = (FORMAT40_OFFSET / aal_device_get_bs(format->device));
	
	/* Check the root block number. */
	if (get_sb_root_block(super) >= get_sb_block_count(super) || 
	    get_sb_root_block(super) <= count)
	{
		aal_exception_error("Invalid root block number (%llu) found in "
				    "the superblock.", get_sb_root_block(super));
		
		if (mode != RM_BUILD)
			result |= RE_FATAL;
		else 
			set_sb_root_block(super, INVAL_BLK);
	}
	
	return result;
}

/* Update from the device only those fields which can be changed while 
 * replaying. */
errno_t format40_update(object_entity_t *entity) {
	format40_t *format = (format40_t *)entity;
	format40_super_t *super;
	aal_block_t block;
	errno_t res;
	blk_t blk;
	
	aal_assert("vpf-828", format != NULL);
	aal_assert("vpf-828", format->device != NULL);
	
	blk = (FORMAT40_OFFSET / format->blksize);

	if ((res = aal_block_init(&block, format->device,
				  format->blksize, blk)))
	{
		return res;
	}
	
	if ((res = aal_block_read(&block))) {
		aal_exception_error("Failed to read the block "
				    "(%llu).", blk);
		goto error_free_block;
	}
	
	super = (format40_super_t *)block.data;

	format->super.sb_free_blocks = super->sb_free_blocks;
	format->super.sb_root_block = super->sb_root_block;
	format->super.sb_oid = super->sb_oid;
	format->super.sb_file_count = super->sb_file_count;
	format->super.sb_tree_height = super->sb_tree_height;
	format->super.sb_flushes = super->sb_flushes;
	
 error_free_block:
	aal_block_fini(&block);
	return res;
}

#endif

