/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format40.c -- default disk-layout plugin for reiser4. */

#ifndef ENABLE_STAND_ALONE
#  include <time.h>
#  include <stdlib.h>
#endif

#include "format40.h"
#include "format40_repair.h"

reiser4_core_t *format40_core;

/* All these functions are standard object getters and setters. They are
   dedicated to modify object properties and get values from its fields. They
   are almost the same and we will not describe each one especially. */
static uint64_t format40_get_root(generic_entity_t *entity) {
	aal_assert("umka-400", entity != NULL);
	return get_sb_root_block(SUPER(entity));
}

static uint16_t format40_get_height(generic_entity_t *entity) {
	aal_assert("umka-1123", entity != NULL);
	return get_sb_tree_height(SUPER(entity));
}

#ifndef ENABLE_STAND_ALONE
static uint64_t format40_get_len(generic_entity_t *entity) {
	aal_assert("umka-401", entity != NULL);
	return get_sb_block_count(SUPER(entity));
}

static uint64_t format40_get_free(generic_entity_t *entity) {
	aal_assert("umka-402", entity != NULL);
	return get_sb_free_blocks(SUPER(entity));
}

static uint32_t format40_get_stamp(generic_entity_t *entity) {
	aal_assert("umka-1122", entity != NULL);
	return get_sb_mkfs_id(SUPER(entity));
}

static uint16_t format40_get_policy(generic_entity_t *entity) {
	aal_assert("vpf-831", entity != NULL);
	return get_sb_tail_policy(SUPER(entity));
}

static uint64_t format40_start(generic_entity_t *entity) {
	format40_t *format = (format40_t *)entity;
	
	aal_assert("vpf-462", format != NULL);
	aal_assert("vpf-463", format->device != NULL);
	
	return FORMAT40_BLOCKNR(format->blksize);
}

static uint32_t format40_get_state(generic_entity_t *entity) {
	aal_assert("umka-2651", entity != NULL);
	return ((format40_t *)entity)->state;
}

void format40_set_state(generic_entity_t *entity,
			uint32_t state)
{
	aal_assert("umka-2078", entity != NULL);
	((format40_t *)entity)->state = state;
}

static errno_t format40_layout(generic_entity_t *entity,
			       region_func_t region_func,
			       void *data) 
{
	blk_t blk;
	errno_t res;
	format40_t *format;
        
	aal_assert("umka-1042", entity != NULL);
	aal_assert("umka-1043", region_func != NULL);
    
	format = (format40_t *)entity;
	blk = FORMAT40_BLOCKNR(format->blksize);
	
	if ((res = region_func(entity, 0, blk, data)))
		return res;
	
	return region_func(entity, blk, 1, data);
}

static errno_t format40_check(generic_entity_t *entity,
			      format40_super_t *super)
{
	format40_t *format;
	blk_t max_format_len;

	format = (format40_t *)entity;
    
	max_format_len = aal_device_len(format->device) /
		(format->blksize / format->device->blksize);
	
	if (get_sb_block_count(super) > max_format_len) {
		aal_error("Superblock has an invalid block "
			  "count %llu for max possible length "
			  "%llu blocks.", get_sb_block_count(super),
			  max_format_len);
		return -EINVAL;
	}
    
	if (get_sb_root_block(super) <= format40_start(entity) ||
	    get_sb_root_block(super) >= max_format_len)
	{
		aal_error("Superblock has an invalid root block "
			  "%llu. It must lie between %llu and %llu "
			  "blocks.", get_sb_root_block(super),
			  format40_start(entity), max_format_len);
		return -EINVAL;
	}
	
	return 0;
}
#endif

static int format40_magic(format40_super_t *super) {
	return (!aal_strncmp(super->sb_magic, FORMAT40_MAGIC, 
			     sizeof(FORMAT40_MAGIC)));
}

static errno_t format40_super_open(format40_t *format) {
	errno_t res;
	blk_t offset;
	aal_block_t block;

	offset = FORMAT40_BLOCKNR(format->blksize);
		
	aal_block_init(&block, format->device,
		       format->blksize, offset);

	if ((res = aal_block_read(&block))) {
		aal_error("Can't read format40 super block. "
			  "%s.", format->device->error);
		goto error_free_block;
	}

	if (!format40_magic((format40_super_t *)block.data)) {
		res = -EINVAL;
		goto error_free_block;
	}

	aal_memcpy(&format->super, block.data,
		   sizeof(format->super));

 error_free_block:
	aal_block_fini(&block);
	return res;
}

static generic_entity_t *format40_open(fs_desc_t *desc) {
	format40_t *format;

	aal_assert("umka-393", desc != NULL);

	/* Initializing format instance. */
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

	format->state = 0;
	format->plug = &format40_plug;
	format->device = desc->device;
	format->blksize = desc->blksize;

	/* Initializing super block. */
	if (format40_super_open(format)) {
		aal_free(format);
		return NULL;
	}
    
	return (generic_entity_t *)format;
}

static void format40_close(generic_entity_t *entity) {
	aal_assert("umka-398", entity != NULL);
	aal_free(entity);
}

static uint64_t format40_get_flags(generic_entity_t *entity) {
	format40_t *format;
	
	aal_assert("umka-2343", entity != NULL);

	format = (format40_t *)entity;
	return format->super.sb_flags;
}

#ifndef ENABLE_STAND_ALONE
static errno_t format40_clobber_block(void *entity, blk_t start,
				      count_t width, void *data) 
{
	blk_t blk;
	errno_t res;
	aal_block_t block;
	format40_t *format;
	
	format = (format40_t *)entity;

	if ((res = aal_block_init(&block, format->device,
				  format->blksize, 0)))
	{
		return res;
	}
	
	aal_block_fill(&block, 0);

	for (blk = start; blk < start + width; blk++) {
		aal_block_move(&block, format->device, blk);
		
		if ((res = aal_block_write(&block)))
			goto error_free_block;
	}

 error_free_block:
	aal_block_fini(&block);
	return res;
}

/* Create format object instnace. Create on-disk format specific suber block
   structures. Return format instance to caller. */
static generic_entity_t *format40_create(fs_desc_t *desc,
					 uint64_t blocks)
{
	blk_t start;
	format40_t *format;
	format40_super_t *super;
    
	aal_assert("umka-395", desc != NULL);

	/* Initializing format instance. */
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

	format->plug = &format40_plug;
	format->device = desc->device;
	format->blksize = desc->blksize;
	format->state |= (1 << ENTITY_DIRTY);

	/* Initializing super block fields. */
	super = (format40_super_t *)&format->super;

	/* Setting up format40 magic. */
	aal_memcpy(super->sb_magic, FORMAT40_MAGIC, 
		   sizeof(FORMAT40_MAGIC));

	/* Number of flushed is zero. */
	set_sb_flushes(super, 0);

	/* Tree height is 2 -- minimal possible tree height in reiser4. All
	   needed nodes will be created later and this value will beused for
	   create them correctly (with level set right). */
	set_sb_tree_height(super, 2);

	/* Filesystem available blocks is set to @blocks. */
	set_sb_block_count(super, blocks);

	/* Root node pointer is set to invalid block numeber, and thus, it
	   shows, that filesyetem is flesh one, that is with not nodes. This
	   value will be used by tree to behave correctly. */
	set_sb_root_block(super, INVAL_BLK);

	/* Setting up tail policy to passed @desc->policy value. */
	set_sb_tail_policy(super, desc->policy->id.id);

	/* Initializing fsck related fields. */
	srandom(time(0));
	set_sb_mkfs_id(super, random());

	/* Clobbering format skipped area in order to let mount to detect
	   reiser4 correctly without specifying exact filesystem type. Skipped
	   area is [0-15] blocks. */
	start = MASTER_BLOCKNR(format->blksize);
	
	if (format40_clobber_block((generic_entity_t *)format,
				   0, start, NULL))
	{
		aal_error("Can't clobber format skipped area [%u-%llu].",
			  0, start - 1);
		aal_free(format);
		return NULL;
	}

	return (generic_entity_t *)format;
}

/* Saves all important permenent info about format40 to be backuped 
   somewhere on the fs. */
static errno_t format40_backup(generic_entity_t *entity, aal_stream_t *stream) {
	aal_assert("vpf-1396", entity != NULL);
	aal_assert("vpf-1397", stream != NULL);
	
	aal_stream_write(stream, SUPER(entity)->sb_magic,
			 MAGIC_SIZE);
	
	aal_stream_write(stream, &SUPER(entity)->sb_block_count, 
			 sizeof(SUPER(entity)->sb_block_count));
	
	aal_stream_write(stream, &SUPER(entity)->sb_mkfs_id, 
			 sizeof(SUPER(entity)->sb_mkfs_id));
	
	aal_stream_write(stream, &SUPER(entity)->sb_tail_policy, 
			 sizeof(SUPER(entity)->sb_tail_policy));
	
	/* Not default plugin ids should be written also in the future. */
	
	return 0;
}

/* This function should update all copies of the super block */
static errno_t format40_sync(generic_entity_t *entity) {
	errno_t res;
	blk_t offset;

	aal_block_t block;
	format40_t *format;
    
	aal_assert("umka-394", entity != NULL);
   
	format = (format40_t *)entity;
	offset = FORMAT40_BLOCKNR(format->blksize);

	if ((res = aal_block_init(&block, format->device,
				  format->blksize, offset)))
	{
		return res;
	}

	aal_memcpy(block.data, &format->super,
		   sizeof(format->super));
	
	if (!(res = aal_block_write(&block)))
		format->state &= ~(1 << ENTITY_DIRTY);
	
	aal_block_fini(&block);
	return res;
}

static errno_t format40_valid(generic_entity_t *entity) {
	aal_assert("umka-397", entity != NULL);
	return format40_check(entity, SUPER(entity));
}

static void format40_oid_area(generic_entity_t *entity, 
			      void **start, uint32_t *len) 
{
	aal_assert("umka-732", entity != NULL);
	
	*start = &(SUPER(entity)->sb_oid);
	
	*len = &(SUPER(entity)->sb_file_count) -
		&(SUPER(entity)->sb_oid);
}

static rid_t format40_oid_pid(generic_entity_t *entity) {
	return OID_REISER40_ID;
}

static rid_t format40_journal_pid(generic_entity_t *entity) {
	return JOURNAL_REISER40_ID;
}

static rid_t format40_alloc_pid(generic_entity_t *entity) {
	return ALLOC_REISER40_ID;
}

static void format40_set_root(generic_entity_t *entity, 
			      uint64_t root) 
{
	aal_assert("umka-403", entity != NULL);

	set_sb_root_block(SUPER(entity), root);
	((format40_t *)entity)->state |= (1 << ENTITY_DIRTY);
}

static void format40_set_len(generic_entity_t *entity, 
			     uint64_t blocks) 
{
	aal_assert("umka-404", entity != NULL);

	set_sb_block_count(SUPER(entity), blocks);
	((format40_t *)entity)->state |= (1 << ENTITY_DIRTY);
}

static void format40_set_free(generic_entity_t *entity, 
			      uint64_t blocks) 
{
	aal_assert("umka-405", entity != NULL);

	set_sb_free_blocks(SUPER(entity), blocks);
	((format40_t *)entity)->state |= (1 << ENTITY_DIRTY);
}

static void format40_set_height(generic_entity_t *entity, 
				uint16_t height) 
{
	aal_assert("umka-555", entity != NULL);

	set_sb_tree_height(SUPER(entity), height);
	((format40_t *)entity)->state |= (1 << ENTITY_DIRTY);
}

static void format40_set_flags(generic_entity_t *entity, 
			       uint64_t flags) 
{
	format40_t *format;
	
	aal_assert("umka-2340", entity != NULL);

	format = (format40_t *)entity;
	format->super.sb_flags |= flags;
	((format40_t *)entity)->state |= (1 << ENTITY_DIRTY);
}

static void format40_set_stamp(generic_entity_t *entity, 
			       uint32_t mkfsid) 
{
	aal_assert("umka-1121", entity != NULL);

	set_sb_mkfs_id(SUPER(entity), mkfsid);
	((format40_t *)entity)->state |= (1 << ENTITY_DIRTY);
}

static void format40_set_policy(generic_entity_t *entity, 
			       uint16_t tail)
{
	aal_assert("vpf-830", entity != NULL);

	set_sb_tail_policy(SUPER(entity), tail);
	((format40_t *)entity)->state |= (1 << ENTITY_DIRTY);
}
#endif

static reiser4_format_ops_t format40_ops = {
#ifndef ENABLE_STAND_ALONE
	.valid		= format40_valid,
	.sync		= format40_sync,
	.create		= format40_create,
	.backup		= format40_backup,
	.print		= format40_print,
	.layout	        = format40_layout,
	.update		= format40_update,
	.start		= format40_start,
	
	.pack           = format40_pack,
	.unpack         = format40_unpack,

	.get_len	= format40_get_len,
	.get_free	= format40_get_free,
	.get_stamp	= format40_get_stamp,
	.get_policy	= format40_get_policy,
		
	.set_flags	= format40_set_flags,
	.set_root	= format40_set_root,
	.set_len	= format40_set_len,
	.set_free	= format40_set_free,
	.set_height	= format40_set_height,
	.set_stamp	= format40_set_stamp,
	.set_policy	= format40_set_policy,
	.set_state      = format40_set_state,
	.get_state      = format40_get_state,
	.oid_pid	= format40_oid_pid,
	.oid_area       = format40_oid_area,
	.journal_pid	= format40_journal_pid,
	.alloc_pid	= format40_alloc_pid,
	.check_struct	= NULL,
#endif
	.open		= format40_open,
	.close		= format40_close,

	.get_flags      = format40_get_flags,
	.get_root	= format40_get_root,
	.get_height	= format40_get_height
};

reiser4_plug_t format40_plug = {
	.cl    = class_init,
	.id    = {FORMAT_REISER40_ID, 0, FORMAT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "format40",
	.desc  = "Disk-format for reiser4, ver. " VERSION,
#endif
	.o = {
		.format_ops = &format40_ops
	}
};

static reiser4_plug_t *format40_init(reiser4_core_t *c) {
	format40_core = c;
	return &format40_plug;
}

plug_register(format40, format40_init, NULL);

