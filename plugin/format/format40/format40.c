/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format40.c -- default disk-layout plugin for reiser4. */

#include "format40.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE
#  include <time.h>
#  include <stdlib.h>
#endif

extern reiser4_plugin_t format40_plugin;

#define SUPER(entity) (&((format40_t *)entity)->super)

static uint64_t format40_get_root(object_entity_t *entity) {
	aal_assert("umka-400", entity != NULL);
	return get_sb_root_block(SUPER(entity));
}

static uint16_t format40_get_height(object_entity_t *entity) {
	aal_assert("umka-1123", entity != NULL);
	return get_sb_tree_height(SUPER(entity));
}

#ifndef ENABLE_STAND_ALONE
static uint64_t format40_get_len(object_entity_t *entity) {
	aal_assert("umka-401", entity != NULL);
	return get_sb_block_count(SUPER(entity));
}

static uint64_t format40_get_free(object_entity_t *entity) {
	aal_assert("umka-402", entity != NULL);
	return get_sb_free_blocks(SUPER(entity));
}

static uint32_t format40_get_stamp(object_entity_t *entity) {
	aal_assert("umka-1122", entity != NULL);
	return get_sb_mkfs_id(SUPER(entity));
}

static uint16_t format40_get_policy(object_entity_t *entity) {
	aal_assert("vpf-831", entity != NULL);
	return get_sb_tail_policy(SUPER(entity));
}

static uint64_t format40_begin(object_entity_t *entity) {
	format40_t *format = (format40_t *)entity;
	
	aal_assert("vpf-462", format != NULL);
	aal_assert("vpf-463", format->device != NULL);
	
	return FORMAT40_OFFSET / format->blocksize;
}

static int format40_isdirty(object_entity_t *entity) {
	aal_assert("umka-2078", entity != NULL);
	return ((format40_t *)entity)->dirty;
}

static void format40_mkdirty(object_entity_t *entity) {
	aal_assert("umka-2079", entity != NULL);
	((format40_t *)entity)->dirty = 1;
}

static void format40_mkclean(object_entity_t *entity) {
	aal_assert("umka-2080", entity != NULL);
	((format40_t *)entity)->dirty = 0;
}

static errno_t format40_skipped(object_entity_t *entity,
				block_func_t func,
				void *data) 
{
	blk_t blk, offset;
	format40_t *format = (format40_t *)entity;
        
	aal_assert("umka-1086", func != NULL);
	aal_assert("umka-1085", entity != NULL);
    
	offset = REISER4_MASTER_OFFSET / format->blocksize;
    
	for (blk = 0; blk < offset; blk++) {
		errno_t res;
		
		if ((res = func(entity, blk, data)))
			return res;
	}
    
	return 0;
}

static errno_t format40_layout(object_entity_t *entity,
			       block_func_t func,
			       void *data) 
{
	errno_t res;
	blk_t blk, offset;
	format40_t *format = (format40_t *)entity;
        
	aal_assert("umka-1042", entity != NULL);
	aal_assert("umka-1043", func != NULL);
    
	blk = REISER4_MASTER_OFFSET / format->blocksize;
	offset = FORMAT40_OFFSET / format->blocksize;
    
	for (; blk <= offset; blk++) {
		if ((res = func(entity, blk, data)))
			return res;
	}
    
	return 0;
}

static errno_t format40_super_check(format40_t *format,
				    format40_super_t *super)
{
	blk_t offset;
	blk_t dev_len;
    
	dev_len = aal_device_len(format->device) /
		(format->blocksize / format->device->blocksize);
	
	if (get_sb_block_count(super) > dev_len) {
		aal_exception_error("Superblock has an invalid block "
				    "count %llu for device length %llu "
				    "blocks.", get_sb_block_count(super),
				    dev_len);
		return -EINVAL;
	}
    
	offset = FORMAT40_OFFSET / format->blocksize;

	if (get_sb_root_block(super) <= offset ||
	    get_sb_root_block(super) >= dev_len)
	{
		aal_exception_error("Superblock has an invalid root block "
				    "%llu for device length %llu blocks.",
				    get_sb_root_block(super), dev_len);
		return -EINVAL;
	}
	
	return 0;
}

static aal_device_t *format40_device(object_entity_t *entity) {
	return ((format40_t *)entity)->device;
}
#endif

static int format40_magic(format40_super_t *super) {
	return aal_strncmp(super->sb_magic, FORMAT40_MAGIC, 
			   aal_strlen(FORMAT40_MAGIC)) == 0;
}

static errno_t format40_super_open(format40_t *format) {
	errno_t res = 0;
	aal_block_t *block;
    
	if (!(block = aal_block_read(format->device, format->blocksize,
				     (FORMAT40_OFFSET / format->blocksize))))
	{
		aal_exception_error("Can't read format40 super block.");
		return -EIO;
	}

	if (!format40_magic((format40_super_t *)block->data)) {
		res = -EINVAL;
		goto error_free_block;
	}

	aal_memcpy(&format->super, block->data,
		   sizeof(format->super));

 error_free_block:
	aal_block_free(block);
	return res;
}

static object_entity_t *format40_open(aal_device_t *device,
				      uint32_t blocksize) {
	format40_t *format;

	aal_assert("umka-393", device != NULL);

	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

#ifndef ENABLE_STAND_ALONE
	format->dirty = 0;
#endif

	format->device = device;
	format->blocksize = blocksize;
	format->plugin = &format40_plugin;
    
	if (format40_super_open(format))
		goto error_free_format;
    
	return (object_entity_t *)format;

 error_free_format:
	aal_free(format);
	return NULL;
}

static void format40_close(object_entity_t *entity) {
	aal_assert("umka-398", entity != NULL);
	aal_free(entity);
}

#ifndef ENABLE_STAND_ALONE
static errno_t callback_clobber_block(object_entity_t *entity, 
				      blk_t blk, void *data) 
{
	errno_t res = 0;
	aal_block_t *block;
	format40_t *format;

	format = (format40_t *)entity;
    
	if (!(block = aal_block_create(format->device,
				       format->blocksize,
				       blk, 0)))
	{
		return -ENOMEM;
	}
    
	if (aal_block_write(block)) {
		aal_exception_error("Can't write block %llu to device. "
				    "%s.", blk, format->device->error);
		res = -EIO;
	}
    
	aal_block_free(block);
	return res;
}

/* This function should create super block and update all copies */
static object_entity_t *format40_create(aal_device_t *device, 
					uint64_t blocks,
					uint32_t blocksize,
					uint16_t tail)
{
	format40_t *format;
	format40_super_t *super;
    
	aal_assert("umka-395", device != NULL);
    
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

	format->dirty = 1;
	format->device = device;
	format->blocksize = blocksize;
	format->plugin = &format40_plugin;

	super = (format40_super_t *)&format->super;
    
	aal_memcpy(super->sb_magic, FORMAT40_MAGIC, 
		   aal_strlen(FORMAT40_MAGIC));

	set_sb_root_block(super, INVAL_BLK);
	set_sb_block_count(super, blocks);
	set_sb_tail_policy(super, tail);
	set_sb_tree_height(super, 2);
	set_sb_flushes(super, 0);

	srandom(time(0));
	set_sb_mkfs_id(super, random());

	/* Clobbering skipped area */
	if (format40_skipped((object_entity_t *)format, 
			     callback_clobber_block, NULL))
	{
		aal_exception_error("Can't clobber skipped area.");
		goto error_free_format;
	}
    
	return (object_entity_t *)format;

 error_free_format:
	aal_free(format);
	return NULL;
}

/* This function should update all copies of the super block */
static errno_t format40_sync(object_entity_t *entity) {
	blk_t offset;
	errno_t res = 0;
	format40_t *format;
	aal_block_t *block;
    
	aal_assert("umka-394", entity != NULL);
   
	format = (format40_t *)entity;
	offset = FORMAT40_OFFSET / format->blocksize;

	if (!(block = aal_block_create(format->device,
				       format->blocksize,
				       offset, 0)))
	{
		return -ENOMEM;
	}

	aal_memcpy(block->data, &format->super,
		   sizeof(format->super));
	
	if (aal_block_write(block)) {
		aal_exception_error("Can't write format40 super "
				    "block to %llu. %s.", offset,
				    format->device->error);
		res = -EIO;
	}
    
	format->dirty = 0;
	aal_block_free(block);
	
	return res;
}

static int format40_confirm(aal_device_t *device) {
	object_entity_t *entity;

	aal_assert("umka-733", device != NULL);

	if (!(entity = format40_open(device, REISER4_BLKSIZE)))
		return 0;
		
	format40_close(entity);
	return 0;
}

static errno_t format40_valid(object_entity_t *entity) {
	format40_t *format;
    
	aal_assert("umka-397", entity != NULL);
    
	format = (format40_t *)entity;
	return format40_super_check(format, SUPER(entity));
}

#endif

static void format40_oid(object_entity_t *entity, 
			 void **oid_start,
			 uint32_t *oid_len) 
{
	aal_assert("umka-732", entity != NULL);
	
	*oid_start = &(SUPER(entity)->sb_oid);
	
	*oid_len = &(SUPER(entity)->sb_file_count) -
		&(SUPER(entity)->sb_oid);
}

static rid_t format40_oid_pid(object_entity_t *entity) {
	return OID_REISER40_ID;
}

#ifndef ENABLE_STAND_ALONE
static const char *formats[] = {"format40"};

static const char *format40_name(object_entity_t *entity) {
	return formats[0];
}

static rid_t format40_journal_pid(object_entity_t *entity) {
	return JOURNAL_REISER40_ID;
}

static rid_t format40_alloc_pid(object_entity_t *entity) {
	return ALLOC_REISER40_ID;
}

static void format40_set_root(object_entity_t *entity, 
			      uint64_t root) 
{
	aal_assert("umka-403", entity != NULL);

	((format40_t *)entity)->dirty = 1;
	set_sb_root_block(SUPER(entity), root);
}

static void format40_set_len(object_entity_t *entity, 
			     uint64_t blocks) 
{
	aal_assert("umka-404", entity != NULL);

	((format40_t *)entity)->dirty = 1;
	set_sb_block_count(SUPER(entity), blocks);
}

static void format40_set_free(object_entity_t *entity, 
			      uint64_t blocks) 
{
	aal_assert("umka-405", entity != NULL);

	((format40_t *)entity)->dirty = 1;
	set_sb_free_blocks(SUPER(entity), blocks);
}

static void format40_set_height(object_entity_t *entity, 
				uint16_t height) 
{
	aal_assert("umka-555", entity != NULL);

	((format40_t *)entity)->dirty = 1;
	set_sb_tree_height(SUPER(entity), height);
}

static void format40_set_stamp(object_entity_t *entity, 
			       uint32_t mkfsid) 
{
	aal_assert("umka-1121", entity != NULL);

	((format40_t *)entity)->dirty = 1;
	set_sb_mkfs_id(SUPER(entity), mkfsid);
}

static void format40_set_policy(object_entity_t *entity, 
			       uint16_t tail)
{
	aal_assert("vpf-830", entity != NULL);

	((format40_t *)entity)->dirty = 1;
	set_sb_tail_policy(SUPER(entity), tail);
}

errno_t format40_print(object_entity_t *entity,
		       aal_stream_t *stream,
		       uint16_t options) 
{
	blk_t offset;
	format40_t *format;
	format40_super_t *super;
    
	aal_assert("vpf-246", entity != NULL);
	aal_assert("umka-1290", stream != NULL);

	format = (format40_t *)entity;
	super = &format->super;
    
	aal_stream_format(stream, "Format super block:\n");
	
	aal_stream_format(stream, "plugin:\t\t%s\n",
			  entity->plugin->h.label);
	
	aal_stream_format(stream, "description:\t%s\n",
			  entity->plugin->h.desc);

	offset = (FORMAT40_OFFSET / format->blocksize);
	
	aal_stream_format(stream, "offset:\t\t%lu\n",
			  offset);
    
	aal_stream_format(stream, "magic:\t\t%s\n",
			  super->sb_magic);
	
	aal_stream_format(stream, "flushes:\t%llu\n",
			  get_sb_flushes(super));
	
	aal_stream_format(stream, "stamp:\t\t0x%x\n",
			  get_sb_mkfs_id(super));
    
	aal_stream_format(stream, "blocks:\t\t%llu\n",
			  get_sb_block_count(super));
	
	aal_stream_format(stream, "free blocks:\t%llu\n",
			  get_sb_free_blocks(super));
	
	aal_stream_format(stream, "root block:\t%llu\n",
			  get_sb_root_block(super));
	
	aal_stream_format(stream, "tail policy:\t%u\n",
			  get_sb_tail_policy(super));
	
	aal_stream_format(stream, "next oid:\t0x%llx\n",
			  get_sb_oid(super));
	
	aal_stream_format(stream, "file count:\t%llu\n",
			  get_sb_file_count(super));
	
	aal_stream_format(stream, "tree height:\t%u\n",
			  get_sb_tree_height(super));
    
	return 0;
}

extern errno_t format40_update(object_entity_t *entity);

extern errno_t format40_check_struct(object_entity_t *entity, uint8_t mode);
#endif

static reiser4_format_ops_t format40_ops = {
#ifndef ENABLE_STAND_ALONE
	.device		= format40_device,
	.valid		= format40_valid,
	.check_struct	= format40_check_struct,
	.sync		= format40_sync,
	.isdirty        = format40_isdirty,
	.mkdirty        = format40_mkdirty,
	.mkclean        = format40_mkclean,
	.create		= format40_create,
	.print		= format40_print,
	.layout	        = format40_layout,
	.skipped        = format40_skipped,
	.confirm	= format40_confirm,
	.update		= format40_update,
	.start		= format40_begin,
	.name		= format40_name,
#endif
	.open		= format40_open,
	.oid	        = format40_oid,
	.close		= format40_close,

	.get_root	= format40_get_root,
	.get_height	= format40_get_height,
		
#ifndef ENABLE_STAND_ALONE
	.get_len	= format40_get_len,
	.get_free	= format40_get_free,
	.get_stamp	= format40_get_stamp,
	.get_policy	= format40_get_policy,
		
	.set_root	= format40_set_root,
	.set_len	= format40_set_len,
	.set_free	= format40_set_free,
	.set_height	= format40_set_height,
	.set_stamp	= format40_set_stamp,
	.set_policy	= format40_set_policy,
	.journal_pid	= format40_journal_pid,
	.alloc_pid	= format40_alloc_pid,
#endif
	.oid_pid	= format40_oid_pid
};

static reiser4_plugin_t format40_plugin = {
	.h = {
		.class = CLASS_INIT,
		.id = FORMAT_REISER40_ID,
		.group = 0,
		.type = FORMAT_PLUGIN_TYPE,
#ifndef ENABLE_STAND_ALONE
		.label = "format40",
		.desc = "Disk-format for reiser4, ver. " VERSION
#endif
	},
	.o = {
		.format_ops = &format40_ops
	}
};

static reiser4_plugin_t *format40_start(reiser4_core_t *core) {
	return &format40_plugin;
}

plugin_register(format40, format40_start, NULL);

