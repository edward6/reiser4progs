/*
  format40.c -- default disk-layout plugin for reiser4.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "format40.h"
#include <aux/aux.h>

#ifndef ENABLE_COMPACT
#  include <stdlib.h>
#  include <time.h>
#endif

extern reiser4_plugin_t format40_plugin;

static reiser4_core_t *core = NULL;

static uint64_t format40_begin(object_entity_t *entity) {
	format40_t *format = (format40_t *)entity;
	
	aal_assert("vpf-462", format != NULL, return INVAL_BLK);
	aal_assert("vpf-463", format->device != NULL, return INVAL_BLK);
	
	return FORMAT40_OFFSET / format->device->blocksize;
}

static uint64_t format40_get_root(object_entity_t *entity) {
	format40_super_t *super;
    
	aal_assert("umka-400", entity != NULL, return INVAL_BLK);
    
	super = format40_super(((format40_t *)entity)->block);
	return get_sb_root_block(super);
}

static uint64_t format40_get_len(object_entity_t *entity) {
	format40_super_t *super;
    
	aal_assert("umka-401", entity != NULL, return INVAL_BLK);
    
	super = format40_super(((format40_t *)entity)->block);
	return get_sb_block_count(super);
}

static uint64_t format40_get_free(object_entity_t *entity) {
	format40_super_t *super;
    
	aal_assert("umka-402", entity != NULL, return INVAL_BLK);
    
	super = format40_super(((format40_t *)entity)->block);
	return get_sb_free_blocks(super);
}

static uint16_t format40_get_height(object_entity_t *entity) {
	format40_super_t *super;
    
	aal_assert("umka-1123", entity != NULL, return 0);
    
	super = format40_super(((format40_t *)entity)->block);
	return get_sb_tree_height(super);
}

static uint32_t format40_get_stamp(object_entity_t *entity) {
	format40_super_t *super;
    
	aal_assert("umka-1122", entity != NULL, return 0);
    
	super = format40_super(((format40_t *)entity)->block);
	return get_sb_mkfs_id(super);
}

static errno_t format40_skipped(object_entity_t *entity,
				block_func_t func,
				void *data) 
{
	blk_t blk, offset;
	format40_t *format = (format40_t *)entity;
        
	aal_assert("umka-1086", func != NULL, return -1);
	aal_assert("umka-1085", entity != NULL, return -1);
    
	offset = MASTER_OFFSET / format->device->blocksize;
    
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
	blk_t blk, offset;
	format40_t *format = (format40_t *)entity;
        
	aal_assert("umka-1042", entity != NULL, return -1);
	aal_assert("umka-1043", func != NULL, return -1);
    
	blk = MASTER_OFFSET / format->device->blocksize;
	offset = FORMAT40_OFFSET / format->device->blocksize;
    
	for (; blk <= offset; blk++) {
		if (func(entity, blk, data))
			return -1;
	}
    
	return 0;
}

static errno_t format40_super_check(format40_super_t *super, 
				    aal_device_t *device) 
{
	blk_t offset;
	blk_t dev_len = aal_device_len(device);
    
	if (get_sb_block_count(super) > dev_len) {
		aal_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,
				    "Superblock has an invalid block count %llu for device "
				    "length %llu blocks.", get_sb_block_count(super), dev_len);
		return -1;
	}
    
	offset = (FORMAT40_OFFSET / aal_device_get_bs(device));
	if (get_sb_root_block(super) < offset || get_sb_root_block(super) > dev_len) {
		aal_exception_error(
			"Superblock has an invalid root block %llu for device "
			"length %llu blocks.", get_sb_root_block(super), dev_len);
		return -1;
	}
	return 0;
}

static int format40_magic(format40_super_t *super) {
	return aal_strncmp(super->sb_magic, FORMAT40_MAGIC, 
			   aal_strlen(FORMAT40_MAGIC)) == 0;
}

static aal_device_t *format40_device(object_entity_t *entity) {
	return ((format40_t *)entity)->device;
}

static aal_block_t *format40_super_open(aal_device_t *device) {
	blk_t offset;
	aal_block_t *block;
    
	offset = (FORMAT40_OFFSET / aal_device_get_bs(device));
	
	if (!(block = aal_block_open(device, offset))) {
		aal_exception_error("Can't read block %llu. %s.", offset, 
				    aal_device_error(device));
		return NULL;
	}

	if (!format40_magic((format40_super_t *)block->data))
		return NULL;
    
	return block;
}

static object_entity_t *format40_open(aal_device_t *device) {
	format40_t *format;

	aal_assert("umka-393", device != NULL, return NULL);

	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

	format->device = device;
	format->plugin = &format40_plugin;
    
	if (!(format->block = format40_super_open(device)))
		goto error_free_format;
    
	return (object_entity_t *)format;

 error_free_format:
	aal_free(format);
 error:
	return NULL;
}

#ifndef ENABLE_COMPACT

static errno_t callback_clobber_block(object_entity_t *entity, 
				      blk_t blk, void *data) 
{
	aal_block_t *block;
	format40_t *format;

	format = (format40_t *)entity;
    
	if (!(block = aal_block_create(format->device, blk, 0))) {
		aal_exception_error("Can't clobber block %llu.", blk);
		return -1;
	}
    
	if (aal_block_sync(block)) {
		aal_exception_error("Can't write block %llu to device. %s.", blk, 
				    format->device->error);
		goto error_free_block;
	}
    
	aal_block_close(block);
	return 0;
    
 error_free_block:
	aal_block_close(block);
	return -1;
}

/* This function should create super block and update all copies */
static object_entity_t *format40_create(aal_device_t *device, 
					 uint64_t blocks, uint16_t tail)
{
	blk_t blk;
	format40_t *format;
	format40_super_t *super;
    
	aal_assert("umka-395", device != NULL, return NULL);
    
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;
    
	format->device = device;
	format->plugin = &format40_plugin;

	if (!(format->block = aal_block_create(device, (FORMAT40_OFFSET / 
							aal_device_get_bs(device)), 0))) 
	{
		aal_exception_error("Can't allocate superblock.");
		goto error_free_format;
	}
    
	super = (format40_super_t *)format->block->data;
    
	aal_memcpy(super->sb_magic, FORMAT40_MAGIC, 
		   aal_strlen(FORMAT40_MAGIC));

	set_sb_block_count(super, blocks);
	set_sb_tree_height(super, 2);
	set_sb_flushes(super, 0);
	set_sb_tail_policy(super, tail);

	srandom(time(0));
	set_sb_mkfs_id(super, random());

	/* Clobbering skipped area */
	if (format40_skipped((object_entity_t *)format, 
			     callback_clobber_block, NULL))
	{
		aal_exception_error("Can't clobber skipped area.");
		goto error_free_block;
	}
    
	return (object_entity_t *)format;

 error_free_block:
	aal_block_close(format->block);
 error_free_format:
	aal_free(format);
 error:
	return NULL;
}

/* This function should update all copies of the super block */
static errno_t format40_sync(object_entity_t *entity) {
	format40_t *format;
    
	aal_assert("umka-394", entity != NULL, return -1); 
   
	format = (format40_t *)entity;
    
	if (aal_block_sync(format->block)) {
		blk_t offset = aal_block_number(format->block);
	
		aal_exception_error("Can't write superblock to %llu. %s.", offset, 
				    aal_device_error(format->device));
	
		return -1;
	}
    
	return 0;
}

#endif

static errno_t format40_valid(object_entity_t *entity) {
	format40_t *format;
    
	aal_assert("umka-397", entity != NULL, return -1);
    
	format = (format40_t *)entity;
    
	return format40_super_check(format40_super(format->block), 
				    format->device);
}

static void format40_close(object_entity_t *entity) {
	aal_assert("umka-398", entity != NULL, return);
    
	aal_block_close(((format40_t *)entity)->block);
	aal_free(entity);
}

static int format40_confirm(aal_device_t *device) {
	aal_block_t *block;

	aal_assert("umka-733", device != NULL, return 0);
    
	if (!(block = format40_super_open(device)))
		return 0;
	
	aal_block_close(block);
	return 1;
}

static void format40_oid(object_entity_t *entity, 
			 void **oid_start, uint32_t *oid_len) 
{
	format40_super_t *super;
    
	aal_assert("umka-732", entity != NULL, return);
    
	super = format40_super(((format40_t *)entity)->block);
    
	*oid_start = &super->sb_oid;
	*oid_len = &super->sb_file_count - &super->sb_oid;
}

static const char *formats[] = {"4.0"};

static const char *format40_name(object_entity_t *entity) {
	return formats[0];
}

static rpid_t format40_journal_pid(object_entity_t *entity) {
	return JOURNAL_REISER40_ID;
}

static rpid_t format40_alloc_pid(object_entity_t *entity) {
	return ALLOC_REISER40_ID;
}

static rpid_t format40_oid_pid(object_entity_t *entity) {
	return OID_REISER40_ID;
}

#ifndef ENABLE_COMPACT

static void format40_set_root(object_entity_t *entity, 
			      uint64_t root) 
{
	format40_super_t *super;
    
	aal_assert("umka-403", entity != NULL, return);
    
	super = format40_super(((format40_t *)entity)->block);
	set_sb_root_block(super, root);
}

static void format40_set_len(object_entity_t *entity, 
			     uint64_t blocks) 
{
	format40_super_t *super;
    
	aal_assert("umka-404", entity != NULL, return);
    
	super = format40_super(((format40_t *)entity)->block);
	set_sb_block_count(super, blocks);
}

static void format40_set_free(object_entity_t *entity, 
			      uint64_t blocks) 
{
	format40_super_t *super;
    
	aal_assert("umka-405", entity != NULL, return);
    
	super = format40_super(((format40_t *)entity)->block);
	set_sb_free_blocks(super, blocks);
}

static void format40_set_height(object_entity_t *entity, 
				uint16_t height) 
{
	format40_super_t *super;
    
	aal_assert("umka-555", entity != NULL, return);

	super = format40_super(((format40_t *)entity)->block);
	set_sb_tree_height(super, height);
}

static void format40_set_stamp(object_entity_t *entity, 
			       uint32_t mkfsid) 
{
	format40_super_t *super;
    
	aal_assert("umka-1121", entity != NULL, return);

	super = format40_super(((format40_t *)entity)->block);
	set_sb_mkfs_id(super, mkfsid);
}

errno_t format40_print(object_entity_t *entity, aal_stream_t *stream,
		       uint16_t options) 
{
	aal_block_t *block;
	format40_super_t *super;
    
	aal_assert("vpf-246", entity != NULL, return -1);
	aal_assert("umka-1290", stream != NULL, return -1);
    
	block = ((format40_t *)entity)->block;
	super = format40_super(block);
    
	aal_stream_format(stream, "plugin:\t\t%s\n", entity->plugin->h.label);
	aal_stream_format(stream, "description:\t%s\n", entity->plugin->h.desc);
	aal_stream_format(stream, "offset:\t\t%llu\n", aal_block_number(block));
    
	aal_stream_format(stream, "magic:\t\t%s\n", super->sb_magic);
	aal_stream_format(stream, "flushes:\t%llu\n", get_sb_flushes(super));
	aal_stream_format(stream, "stamp:\t\t0x%x\n", get_sb_mkfs_id(super));
    
	aal_stream_format(stream, "length:\t\t%llu\n",get_sb_block_count(super));
	aal_stream_format(stream, "free blocks:\t%llu\n", get_sb_free_blocks(super));
	aal_stream_format(stream, "root block:\t%llu\n", get_sb_root_block(super));
	aal_stream_format(stream, "tail policy:\t%u\n", get_sb_tail_policy(super));
	aal_stream_format(stream, "next oid:\t0x%llx\n", get_sb_oid(super));
	aal_stream_format(stream, "file count:\t%llu\n", get_sb_file_count(super));
	aal_stream_format(stream, "tree height:\t%u\n", get_sb_tree_height(super));
    
	return 0;
}

extern errno_t format40_check(object_entity_t *entity);

#endif

static reiser4_plugin_t format40_plugin = {
	.format_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.id = FORMAT_REISER40_ID,
			.group = 0,
			.type = FORMAT_PLUGIN_TYPE,
			.label = "format40",
			.desc = "Disk-format for reiserfs 4.0, ver. " VERSION,
		},
		.open		= format40_open,
		.valid		= format40_valid,
		.device		= format40_device,
		
#ifndef ENABLE_COMPACT	
		.check		= format40_check,
		.sync		= format40_sync,
		.create		= format40_create,
		.print		= format40_print,
#else
		.check		= NULL,
		.sync		= NULL,
		.create		= NULL,
		.print		= NULL,
#endif
		.oid	        = format40_oid,
	
		.close		= format40_close,
		.confirm	= format40_confirm,
		.name		= format40_name,

		.start		= format40_begin,
		.get_root	= format40_get_root,
		.get_len	= format40_get_len,
		.get_free	= format40_get_free,
		.get_height	= format40_get_height,
		.get_stamp	= format40_get_stamp,

		.layout	        = format40_layout,
		.skipped        = format40_skipped,
	
#ifndef ENABLE_COMPACT	
		.set_root	= format40_set_root,
		.set_len	= format40_set_len,
		.set_free	= format40_set_free,
		.set_height	= format40_set_height,
		.set_stamp	= format40_set_stamp,
#else
		.set_root	= NULL,
		.set_len	= NULL,
		.set_free	= NULL,
		.set_height	= NULL,
		.set_stamp	= NULL,
#endif
		.journal_pid	= format40_journal_pid,
		.alloc_pid	= format40_alloc_pid,
		.oid_pid	= format40_oid_pid
	}
};

static reiser4_plugin_t *format40_start(reiser4_core_t *c) {
	core = c;
	return &format40_plugin;
}

plugin_register(format40_start, NULL);

