/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format40.c -- default disk-layout plugin for reiser4. */

#ifndef ENABLE_STAND_ALONE
#  include <time.h>
#  include <stdlib.h>
#endif

#include "format40.h"

extern reiser4_plug_t format40_plug;

#define SUPER(entity) (&((format40_t *)entity)->super)

static uint64_t format40_get_root(generic_entity_t *entity) {
	aal_assert("umka-400", entity != NULL);
	return get_sb_root_block(SUPER(entity));
}

static uint16_t format40_get_height(generic_entity_t *entity) {
	aal_assert("umka-1123", entity != NULL);
	return get_sb_tree_height(SUPER(entity));
}

static int format40_get_flag(generic_entity_t *entity,
			     uint8_t flag)
{
	uint64_t flags;

	aal_assert("umka-2339", entity != NULL);

	flags = get_sb_flags(SUPER(entity));
	return aal_test_bit(&flags, flag);
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

static uint64_t format40_begin(generic_entity_t *entity) {
	format40_t *format = (format40_t *)entity;
	
	aal_assert("vpf-462", format != NULL);
	aal_assert("vpf-463", format->device != NULL);
	
	return FORMAT40_BLOCKNR(format->blksize);
}

static int format40_isdirty(generic_entity_t *entity) {
	aal_assert("umka-2078", entity != NULL);
	return ((format40_t *)entity)->dirty;
}

void format40_mkdirty(generic_entity_t *entity) {
	aal_assert("umka-2079", entity != NULL);
	((format40_t *)entity)->dirty = 1;
}

static void format40_mkclean(generic_entity_t *entity) {
	aal_assert("umka-2080", entity != NULL);
	((format40_t *)entity)->dirty = 0;
}

static errno_t format40_skipped(generic_entity_t *entity,
				region_func_t region_func,
				void *data) 
{
	blk_t offset;
	format40_t *format;
        
	aal_assert("umka-1085", entity != NULL);
	aal_assert("umka-1086", region_func != NULL);

	format = (format40_t *)entity;
	offset = MASTER_BLOCKNR(format->blksize);
    
	return region_func(entity, 0, offset, data);
}

static errno_t format40_layout(generic_entity_t *entity,
			       region_func_t region_func,
			       void *data) 
{
	blk_t blk;
	format40_t *format;
        
	aal_assert("umka-1042", entity != NULL);
	aal_assert("umka-1043", region_func != NULL);
    
	format = (format40_t *)entity;
	blk = FORMAT40_BLOCKNR(format->blksize);
    
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
		aal_exception_error("Superblock has an invalid block "
				    "count %llu for max possible length "
				    "%llu blocks.", get_sb_block_count(super),
				    max_format_len);
		return -EINVAL;
	}
    
	if (get_sb_root_block(super) <= format40_begin(entity) ||
	    get_sb_root_block(super) >= max_format_len)
	{
		aal_exception_error("Superblock has an invalid root block "
				    "%llu. It must lie between %llu and %llu "
				    "blocks.", get_sb_root_block(super),
				    format40_begin(entity), max_format_len);
		return -EINVAL;
	}
	
	return 0;
}

static aal_device_t *format40_device(generic_entity_t *entity) {
	return ((format40_t *)entity)->device;
}
#endif

static int format40_magic(format40_super_t *super) {
	return aal_strncmp(super->sb_magic, FORMAT40_MAGIC, 
			   aal_strlen(super->sb_magic)) == 0;
}

static errno_t format40_super_open(format40_t *format) {
	errno_t res;
	blk_t offset;
	aal_block_t block;

	offset = FORMAT40_BLOCKNR(format->blksize);
		
	aal_block_init(&block, format->device,
		       format->blksize, offset);

	if ((res = aal_block_read(&block))) {
		aal_exception_error("Can't read format40 super block. "
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

static generic_entity_t *format40_open(aal_device_t *device,
				       uint32_t blksize)
{
	format40_t *format;

	aal_assert("umka-393", device != NULL);

	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

#ifndef ENABLE_STAND_ALONE
	format->dirty = 0;
#endif

	format->device = device;
	format->blksize = blksize;
	format->plug = &format40_plug;
    
	if (format40_super_open(format))
		goto error_free_format;
    
	return (generic_entity_t *)format;

 error_free_format:
	aal_free(format);
	return NULL;
}

static void format40_close(generic_entity_t *entity) {
	aal_assert("umka-398", entity != NULL);
	aal_free(entity);
}

#ifndef ENABLE_STAND_ALONE
static errno_t callback_clobber_block(void *entity, blk_t start,
				      count_t width, void *data) 
{
	blk_t blk;
	errno_t res;
	aal_block_t block;
	format40_t *format;
	
	format = (format40_t *)entity;

	if ((res = aal_block_init(&block, format->device,
				  format->blksize, blk)))
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

/* This function should create super block and update all copies */
static generic_entity_t *format40_create(aal_device_t *device, 
					 uint64_t blocks,
					 uint32_t blksize,
					 uint16_t tail)
{
	format40_t *format;
	format40_super_t *super;
    
	aal_assert("umka-395", device != NULL);
    
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

	format->dirty = 1;
	format->device = device;
	format->blksize = blksize;
	format->plug = &format40_plug;

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
	if (format40_skipped((generic_entity_t *)format,
			     callback_clobber_block, NULL))
	{
		aal_exception_error("Can't clobber skipped area.");
		goto error_free_format;
	}
    
	return (generic_entity_t *)format;

 error_free_format:
	aal_free(format);
	return NULL;
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
		format->dirty = 0;
	
	aal_block_fini(&block);
	return res;
}

static errno_t format40_valid(generic_entity_t *entity) {
	aal_assert("umka-397", entity != NULL);
	return format40_check(entity, SUPER(entity));
}
#endif

static void format40_oid(generic_entity_t *entity, 
			 void **oid_start,
			 uint32_t *oid_len) 
{
	aal_assert("umka-732", entity != NULL);
	
	*oid_start = &(SUPER(entity)->sb_oid);
	
	*oid_len = &(SUPER(entity)->sb_file_count) -
		&(SUPER(entity)->sb_oid);
}

static rid_t format40_oid_pid(generic_entity_t *entity) {
	return OID_REISER40_ID;
}

#ifndef ENABLE_STAND_ALONE
static const char *formats[] = {"format40"};

static const char *format40_name(generic_entity_t *entity) {
	return formats[0];
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

	((format40_t *)entity)->dirty = 1;
	set_sb_root_block(SUPER(entity), root);
}

static void format40_set_len(generic_entity_t *entity, 
			     uint64_t blocks) 
{
	aal_assert("umka-404", entity != NULL);

	((format40_t *)entity)->dirty = 1;
	set_sb_block_count(SUPER(entity), blocks);
}

static void format40_set_free(generic_entity_t *entity, 
			      uint64_t blocks) 
{
	aal_assert("umka-405", entity != NULL);

	((format40_t *)entity)->dirty = 1;
	set_sb_free_blocks(SUPER(entity), blocks);
}

static void format40_set_height(generic_entity_t *entity, 
				uint16_t height) 
{
	aal_assert("umka-555", entity != NULL);

	((format40_t *)entity)->dirty = 1;
	set_sb_tree_height(SUPER(entity), height);
}

static int format40_tst_flag(generic_entity_t *entity, 
			     uint8_t flag) 
{
	format40_t *format;
	
	aal_assert("umka-2343", entity != NULL);

	format = (format40_t *)entity;
	return aal_test_bit(&format->super.sb_flags, flag);
}

static void format40_set_flag(generic_entity_t *entity, 
			      uint8_t flag) 
{
	format40_t *format;
	
	aal_assert("umka-2340", entity != NULL);

	format = (format40_t *)entity;
	aal_set_bit(&format->super.sb_flags, flag);
	format->dirty = 1;
}

static void format40_clr_flag(generic_entity_t *entity, 
			      uint8_t flag) 
{
	format40_t *format;
	
	aal_assert("umka-2341", entity != NULL);

	format = (format40_t *)entity;
	aal_clear_bit(&format->super.sb_flags, flag);
	format->dirty = 1;
}

static void format40_set_stamp(generic_entity_t *entity, 
			       uint32_t mkfsid) 
{
	aal_assert("umka-1121", entity != NULL);

	((format40_t *)entity)->dirty = 1;
	set_sb_mkfs_id(SUPER(entity), mkfsid);
}

static void format40_set_policy(generic_entity_t *entity, 
			       uint16_t tail)
{
	aal_assert("vpf-830", entity != NULL);

	((format40_t *)entity)->dirty = 1;
	set_sb_tail_policy(SUPER(entity), tail);
}

errno_t format40_print(generic_entity_t *entity,
		       aal_stream_t *stream,
		       uint16_t options) 
{
	format40_t *format;
	format40_super_t *super;
    
	aal_assert("vpf-246", entity != NULL);
	aal_assert("umka-1290", stream != NULL);

	format = (format40_t *)entity;
	super = &format->super;
    
	aal_stream_format(stream, "Format super block:\n");
	
	aal_stream_format(stream, "plugin:\t\t%s\n",
			  entity->plug->label);
	
	aal_stream_format(stream, "description:\t%s\n",
			  entity->plug->desc);

	aal_stream_format(stream, "offset:\t\t%lu\n",
			  FORMAT40_BLOCKNR(format->blksize));
    
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

	if (aal_test_bit(&super->sb_flags, 0))
		aal_stream_format(stream, "key policy:\tLARGE\n");
	else
		aal_stream_format(stream, "key policy:\tSHORT\n");
    
	return 0;
}

extern errno_t format40_update(generic_entity_t *entity);

extern errno_t format40_check_struct(generic_entity_t *entity,
				     uint8_t mode);
#endif

static reiser4_format_ops_t format40_ops = {
#ifndef ENABLE_STAND_ALONE
	.device		= format40_device,
	.valid		= format40_valid,
	.sync		= format40_sync,
	.isdirty        = format40_isdirty,
	.mkdirty        = format40_mkdirty,
	.mkclean        = format40_mkclean,
	.create		= format40_create,
	.print		= format40_print,
	.layout	        = format40_layout,
	.skipped        = format40_skipped,
	.update		= format40_update,
	.start		= format40_begin,
	.name		= format40_name,
#endif
	.open		= format40_open,
	.oid	        = format40_oid,
	.close		= format40_close,

	.get_root	= format40_get_root,
	.get_flag	= format40_get_flag,
	.get_height	= format40_get_height,
		
#ifndef ENABLE_STAND_ALONE
	.get_len	= format40_get_len,
	.get_free	= format40_get_free,
	.get_stamp	= format40_get_stamp,
	.get_policy	= format40_get_policy,
		
	.set_flag	= format40_set_flag,
	.clr_flag	= format40_clr_flag,
	.tst_flag       = format40_tst_flag,
	
	.set_root	= format40_set_root,
	.set_len	= format40_set_len,
	.set_free	= format40_set_free,
	.set_height	= format40_set_height,
	.set_stamp	= format40_set_stamp,
	.set_policy	= format40_set_policy,
	.journal_pid	= format40_journal_pid,
	.alloc_pid	= format40_alloc_pid,
	.check_struct	= format40_check_struct,
#endif
	.oid_pid	= format40_oid_pid
};

static reiser4_plug_t format40_plug = {
	.cl    = CLASS_INIT,
	.id    = {FORMAT_REISER40_ID, 0, FORMAT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "format40",
	.desc  = "Disk-format for reiser4, ver. " VERSION,
#endif
	.o = {
		.format_ops = &format40_ops
	}
};

static reiser4_plug_t *format40_start(reiser4_core_t *core) {
	return &format40_plug;
}

plug_register(format40, format40_start, NULL);

