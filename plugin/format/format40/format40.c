/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format40.c -- disk-layout plugin for reiser4 simple volumes. */

#include "format40.h"
#include "format40_repair.h"
#include <misc/misc.h>

reiser4_core_t *format40_core = NULL;

/* All these functions are standard object getters and setters. They are
   dedicated to modify object properties and get values from its fields. They
   are almost the same and we will not describe each one especially. */
uint64_t format40_get_root(reiser4_format_ent_t *entity) {
	aal_assert("umka-400", entity != NULL);
	return get_sb_root_block(SUPER(entity));
}

uint16_t format40_get_height(reiser4_format_ent_t *entity) {
	aal_assert("umka-1123", entity != NULL);
	return get_sb_tree_height(SUPER(entity));
}

#ifndef ENABLE_MINIMAL
uint64_t format40_get_len(reiser4_format_ent_t *entity) {
	aal_assert("umka-401", entity != NULL);
	return get_sb_block_count(SUPER(entity));
}

uint64_t format40_get_free(reiser4_format_ent_t *entity) {
	aal_assert("umka-402", entity != NULL);
	return get_sb_free_blocks(SUPER(entity));
}

uint32_t format40_get_stamp(reiser4_format_ent_t *entity) {
	aal_assert("umka-1122", entity != NULL);
	return get_sb_mkfs_id(SUPER(entity));
}

rid_t format40_get_policy(reiser4_format_ent_t *entity) {
	aal_assert("vpf-831", entity != NULL);
	return get_sb_policy(SUPER(entity));
}

uint32_t format40_node_pid(reiser4_format_ent_t *entity) {
	aal_assert("edward-18", entity != NULL);
	return get_sb_node_pid(SUPER(entity));
}

uint64_t format40_start(reiser4_format_ent_t *entity) {
	format40_t *format = (format40_t *)entity;
	
	aal_assert("vpf-462", format != NULL);
	aal_assert("vpf-463", format->device != NULL);
	
	return FORMAT40_BLOCKNR(format->blksize);
}

uint32_t format40_get_state(reiser4_format_ent_t *entity) {
	aal_assert("umka-2651", entity != NULL);
	return ((format40_t *)entity)->state;
}

void format40_set_state(reiser4_format_ent_t *entity, uint32_t state)
{
	aal_assert("umka-2078", entity != NULL);
	((format40_t *)entity)->state = state;
}

errno_t format40_layout(reiser4_format_ent_t *entity,
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
	
	if ((res = region_func(0, blk, data)))
		return res;
	
	return region_func(blk, 1, data);
}

#endif

static int format40_magic(format40_super_t *super) {
	return (!aal_strncmp(super->sb_magic, FORMAT40_MAGIC, 
			     sizeof(FORMAT40_MAGIC)));
}

errno_t check_super_format40(format40_super_t *super)
{
	if (!format40_magic(super))
		return -EINVAL;
	return 0;
}

errno_t format40_super_open_common(format40_t *format,
				   errno_t (*check)(format40_super_t *s))
{
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
	res = check((format40_super_t *)block.data);
	if (res)
		goto error_free_block;

	aal_memcpy(&format->super, block.data,
		   sizeof(format->super));
 error_free_block:
	aal_block_fini(&block);
	return res;
}

static errno_t format40_super_open(format40_t *format)
{
	return format40_super_open_common(format, check_super_format40);
}

reiser4_format_ent_t *format40_open_common(aal_device_t *device,
					   uint32_t blksize,
					   reiser4_format_plug_t *plug,
					   errno_t (*super_open)(format40_t *f))
{
	format40_t *format;

	aal_assert("umka-393", device != NULL);

	/* Initializing format instance. */
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

	format->state = 0;
	format->plug = plug;
	format->device = device;
	format->blksize = blksize;

	/* Initializing super block. */
	if (super_open(format)) {
		aal_free(format);
		return NULL;
	}
	return (reiser4_format_ent_t *)format;
}

static reiser4_format_ent_t *format40_open(aal_device_t *device,
					   uint32_t blksize)
{
	return format40_open_common(device, blksize,
				    &format40_plug, format40_super_open);
}

void format40_close(reiser4_format_ent_t *entity) {
	aal_assert("umka-398", entity != NULL);
	aal_free(entity);
}

#ifndef ENABLE_MINIMAL

errno_t format40_clobber_block(void *entity, blk_t start,
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

void set_sb_format40(format40_super_t *super, format_hint_t *desc)
{
	uint64_t flags;

	/* Setting up format40 magic. */
	aal_memcpy(super->sb_magic, FORMAT40_MAGIC, 
		   sizeof(FORMAT40_MAGIC));

	/* Number of flushed is zero. */
	set_sb_flushes(super, 0);
	/*
	 * Tree height is 2 -- minimal possible tree height in reiser4. All
	 * needed nodes will be created later and this value will beused for
	 * create them correctly (with level set right)
	 */
	set_sb_tree_height(super, 2);
	set_sb_block_count(super, desc->blocks);
	set_sb_mkfs_id(super, desc->mkfs_id);

	/* Root node pointer is set to invalid block numeber, and thus, it
	   shows, that filesyetem is flesh one, that is with not nodes. This
	   value will be used by tree to behave correctly. */
	set_sb_root_block(super, INVAL_BLK);

	/* Setting up tail policy to passed @desc->policy value. */
	set_sb_policy(super, desc->policy);

	/* Set node plugin id */
	set_sb_node_pid(super, desc->node);

	/* Set the flags. */
	/* FIXME: Hardcoded plugin ids for 2 cases. */
	flags = (desc->key == KEY_LARGE_ID) ? (1 << FORMAT40_LARGE_KEYS) : 0;
	/* Newly created brick always has a data room */
	flags |= (1 << FORMAT40_HAS_DATA_ROOM);
	set_sb_flags(super, flags);

	/* Set version values. */
	set_sb_version(super, get_release_number_minor());
}

/*
 * Create format object instnace. Create on-disk format specific suber block
 * structures. Return format instance to caller
 */
reiser4_format_ent_t *format40_create_common(aal_device_t *device,
					     format_hint_t *desc,
					     void (*set_sb)(format40_super_t *s,
						            format_hint_t *dsc))
{
	blk_t start;
	format40_t *format;
	format40_super_t *super;

	aal_assert("umka-395", device != NULL);
	aal_assert("vpf-1735", desc != NULL);

	if (desc->key >= KEY_LAST_ID) {
		aal_error("Wrong key plugin id (%u) is specified.", desc->key);
		return NULL;
	}

	/* Initializing format instance. */
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

	format->plug = &format40_plug;
	format->device = device;
	format->blksize = desc->blksize;
	format40_mkdirty(format);

	/* Initializing super block fields. */
	super = (format40_super_t *)&format->super;
	set_sb(super, desc);

	/* Clobbering format skipped area in order to let mount to detect
	   reiser4 correctly without specifying exact filesystem type. 
	   Skipped area is [0-15] blocks. Clobber the master block also to 
	   avoid mounting of the previous reiser4 if this mkfs attempt fails. */
	start = FORMAT40_BLOCKNR(format->blksize);
	   
	if (format40_clobber_block((reiser4_format_ent_t *)format,
				   0, start, NULL))
	{
		aal_error("Can't clobber format skipped area [%u-%llu].",
			  0, start - 1);
		aal_free(format);
		return NULL;
	}

	return (reiser4_format_ent_t *)format;
}

static reiser4_format_ent_t *format40_create(aal_device_t *device,
					     format_hint_t *desc)
{
	return format40_create_common(device, desc, set_sb_format40);
}

/* All important permanent format40 data get backuped into @hint. */
errno_t format40_backup(reiser4_format_ent_t *entity, backup_hint_t *hint) {
	format40_backup_t *backup;
	
	aal_assert("vpf-1396", entity != NULL);
	aal_assert("vpf-1397", hint != NULL);
	
	backup = (format40_backup_t *)
		(hint->block.data + hint->off[BK_FORMAT]);
	
	hint->off[BK_FORMAT + 1] = hint->off[BK_FORMAT] + sizeof(*backup);
	
	aal_memcpy(backup->sb_magic, SUPER(entity)->sb_magic, 
		   sizeof(SUPER(entity)->sb_magic));
	backup->sb_block_count = SUPER(entity)->sb_block_count;
	backup->sb_mkfs_id = SUPER(entity)->sb_mkfs_id;
	backup->sb_policy = SUPER(entity)->sb_policy;
	backup->sb_flags = SUPER(entity)->sb_flags;
	backup->sb_reserved = 0;
	
	/* Get rid of UPDATE_BACKUP flag. */
	set_sb_version(backup, get_sb_version(SUPER(entity)));
	
	hint->version = get_sb_version(SUPER(entity));
	
	return 0;
}

/* This function should update all copies of the super block */
errno_t format40_sync(reiser4_format_ent_t *entity) {
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

	aal_block_fill(&block, 0);
	aal_memcpy(block.data, &format->super,
		   sizeof(format->super));
	
	if (!(res = aal_block_write(&block)))
		format40_mkclean(format);
	
	aal_block_fini(&block);
	return res;
}

errno_t format40_valid(reiser4_format_ent_t *entity) {
	format40_t *format;
	blk_t max_format_len;

	aal_assert("umka-397", entity != NULL);
	
	format = (format40_t *)entity;
    
	max_format_len = aal_device_len(format->device) /
		(format->blksize / format->device->blksize);
	
	if (get_sb_block_count(SUPER(format)) > max_format_len) {
		aal_error("Superblock has an invalid block count "
			  "%llu for max possible length %llu blocks.", 
			  get_sb_block_count(SUPER(format)), max_format_len);
		return -EINVAL;
	}
    
	if (get_sb_root_block(SUPER(format)) <= format40_start(entity) ||
	    get_sb_root_block(SUPER(format)) >= max_format_len)
	{
		aal_error("Superblock has an invalid root block "
			  "%llu. It must lie between %llu and %llu "
			  "blocks.", get_sb_root_block(SUPER(format)),
			  format40_start(entity), max_format_len);
		return -EINVAL;
	}

	if (get_sb_version(SUPER(format)) > get_release_number_minor()) {
		aal_error("Format version (%u) of the partition is "
			  "greater than format release number (%u) "
			  "of reiser4progs. Please upgrade reiser4progs, "
			  "or run fsck.reiser4 --build-sb to fix the fs "
			  "consistency.",
			  get_sb_version(SUPER(format)),
			  get_release_number_minor());

		return -EINVAL;
	}

	return 0;
}

void format40_oid_area(reiser4_format_ent_t *entity,
		       void **start, uint32_t *len)
{
	aal_assert("umka-732", entity != NULL);
	
	*start = &(SUPER(entity)->sb_oid);
	
	*len = sizeof(SUPER(entity)->sb_oid);
}

rid_t format40_oid_pid(reiser4_format_ent_t *entity) {
	return OID_REISER40_ID;
}

rid_t format40_journal_pid(reiser4_format_ent_t *entity) {
	return JOURNAL_REISER40_ID;
}

rid_t format40_alloc_pid(reiser4_format_ent_t *entity) {
	return ALLOC_REISER40_ID;
}

void format40_set_root(reiser4_format_ent_t *entity, uint64_t root) {
	aal_assert("umka-403", entity != NULL);

	set_sb_root_block(SUPER(entity), root);
	format40_mkdirty(entity);
}

void format40_set_len(reiser4_format_ent_t *entity, uint64_t blocks) {
	aal_assert("umka-404", entity != NULL);

	set_sb_block_count(SUPER(entity), blocks);
	format40_mkdirty(entity);
}

void format40_set_free(reiser4_format_ent_t *entity, uint64_t blocks) {
	aal_assert("umka-405", entity != NULL);

	if (get_sb_free_blocks(SUPER(entity)) == blocks)
		return;

	set_sb_free_blocks(SUPER(entity), blocks);
	format40_mkdirty(entity);
}

void format40_set_height(reiser4_format_ent_t *entity, uint16_t height) {
	aal_assert("umka-555", entity != NULL);

	set_sb_tree_height(SUPER(entity), height);
	format40_mkdirty(entity);
}

void format40_set_stamp(reiser4_format_ent_t *entity, uint32_t mkfsid) {
	aal_assert("umka-1121", entity != NULL);

	set_sb_mkfs_id(SUPER(entity), mkfsid);
	format40_mkdirty(entity);
}

void format40_set_policy(reiser4_format_ent_t *entity, rid_t tail) {
	aal_assert("vpf-830", entity != NULL);

	set_sb_policy(SUPER(entity), tail);
	format40_mkdirty(entity);
}

void format40_set_key(reiser4_format_ent_t *entity, rid_t key) {
	uint64_t flags;
	
	aal_assert("vpf-830", entity != NULL);
	
	flags = get_sb_flags(SUPER(entity));
	flags &= ~(1 << FORMAT40_LARGE_KEYS);
	flags |= ((key == KEY_LARGE_ID) ? (1 << FORMAT40_LARGE_KEYS) : 0);
	set_sb_flags(SUPER(entity), flags);
	format40_mkdirty(entity);
}

uint32_t format40_version(reiser4_format_ent_t *entity) {
	aal_assert("vpf-1913", entity != NULL);
	return get_sb_version(SUPER(entity));
}
#endif

rid_t format40_get_key(reiser4_format_ent_t *entity) {
	uint64_t flags;
	
	aal_assert("vpf-1736", entity != NULL);
	
	flags = get_sb_flags(SUPER(entity));
	
	return (flags & (1 << FORMAT40_LARGE_KEYS)) ?
		KEY_LARGE_ID : KEY_SHORT_ID;
}

reiser4_format_plug_t format40_plug = {
	.p = {
		.id	= {FORMAT_REISER40_ID, 0, FORMAT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "format40",
		.desc  = "Disk-format plugin.",
#endif
	},

#ifndef ENABLE_MINIMAL
	.valid		= format40_valid,
	.sync		= format40_sync,
	.create		= format40_create,
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
	.node_pid       = format40_node_pid,
	.backup		= format40_backup,
	.check_backup	= format40_check_backup,
	.regenerate     = format40_regenerate,
	.check_struct	= format40_check_struct,
	.version	= format40_version,
#endif
	.open		= format40_open,
	.close		= format40_close,

	.get_root	= format40_get_root,
	.get_height	= format40_get_height,
	.key_pid        = format40_get_key,
};

/*
  Local variables:
  c-indentation-style: "K&R"
  mode-name: "LC"
  c-basic-offset: 8
  tab-width: 8
  fill-column: 80
  scroll-step: 1
  End:
*/
