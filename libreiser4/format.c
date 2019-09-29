/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format.c -- disk format common code. This code is wrapper for disk-format
   plugin. It is used by filesystem code (filesystem.c) for working with
   different disk-format plugins in independent maner. */

#include <reiser4/libreiser4.h>

#ifndef ENABLE_MINIMAL
bool_t reiser4_format_isdirty(reiser4_format_t *format) {
	uint32_t state;
	
	aal_assert("umka-2106", format != NULL);

	state = reiser4call(format, get_state);
	return (state & (1 << ENTITY_DIRTY));
}

void reiser4_format_mkdirty(reiser4_format_t *format) {
	uint32_t state;
	
	aal_assert("umka-2107", format != NULL);

	state = reiser4call(format, get_state);
	state |= (1 << ENTITY_DIRTY);
	reiser4call(format, set_state, state);
}

void reiser4_format_mkclean(reiser4_format_t *format) {
	uint32_t state;
	
	aal_assert("umka-2108", format != NULL);

	state = reiser4call(format, get_state);
	state &= ~(1 << ENTITY_DIRTY);
	reiser4call(format, set_state, state);
}
#endif

/* Opens disk-format on specified device. Actually it just calls specified by
   "pid" disk-format plugin and that plugin makes all dirty work. */
reiser4_format_t *reiser4_format_open(reiser4_fs_t *fs) {
	rid_t pid;
	uint32_t blksize;
	reiser4_plug_t *plug;
	reiser4_format_t *format;
	
	aal_assert("umka-104", fs != NULL);
	aal_assert("umka-1700", fs->master != NULL);
    
	/* Initializing format instance. */
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;
    
	format->fs = fs;

	pid = reiser4_master_get_format(fs->master);
	blksize = reiser4_master_get_blksize(fs->master);
    
	/* Finding needed disk-format plugin by its plugin id. */
	if (!(plug = reiser4_factory_ifind(FORMAT_PLUG_TYPE, pid))) {
		aal_error("Can't find disk-format plugin by "
			  "its id 0x%x.", pid);
		goto error_free_format;
	}
    
	/* Initializing disk-format entity by calling plugin */
	if (!(format->ent = plugcall(((reiser4_format_plug_t *)plug), 
				     open, fs->device, blksize)))
	{
		aal_fatal("Can't open disk-format %s.",  plug->label);
		goto error_free_format;
	}

	return format;
	
 error_free_format:
	aal_free(format);
	return NULL;
}

#ifndef ENABLE_MINIMAL
/* Creates disk-format structures on specified device */
reiser4_format_t *reiser4_format_create(
	reiser4_fs_t *fs,		/* fs instance */
	reiser4_format_plug_t *plug,	/* format plugin */
	rid_t policy,			/* policy plug id */
	rid_t key,			/* key plug id */
	rid_t key_alloc,		/* key allocation scheme */
	rid_t node,                     /* node plug id */
	count_t blocks,			/* block count */
	long int mkfs_id,               /* identifier for fsck */
	uint64_t subvol_id,             /* internal id */
	uint64_t num_subvols,           /* in the logical volume */
	uint8_t num_segments_bits)
{
	reiser4_format_t *format;
	format_hint_t desc;
		
	aal_assert("umka-105", fs != NULL);
	aal_assert("vpf-1595", plug != NULL);

	/* Initializing format instance. */
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

	format->fs = fs;

	/* Initializing filesystem descriptor. */
	desc.blksize = reiser4_master_get_blksize(fs->master);
	desc.blocks = blocks;
	desc.policy = policy;
	desc.key = key;
	desc.key_alloc = key_alloc;
	desc.node = node;
	desc.mkfs_id = mkfs_id;
	desc.subvol_id = subvol_id;
	desc.num_subvols = num_subvols;
	desc.num_sgs_bits = num_segments_bits;
	/*
	 * Initializing entity of disk-format by means of calling "create"
	 * method from found plugin. Plugin "create" method will be creating
	 * all disk structures, namely, format-specific super block
	 */
	if (!(format->ent = plugcall(plug, create, fs->device, &desc))) {
		aal_error("Can't create format %s on %s.",
			  plug->p.label, fs->device->name);
    
		aal_free(format);
		return NULL;
	}
	
	return format;
}

errno_t reiser4_format_backup(reiser4_format_t *format,
			      backup_hint_t *hint)
{
	aal_assert("vpf-1390", format != NULL);
	aal_assert("vpf-1391", hint != NULL);
	
	return reiser4call(format, backup, hint);
}

/* Saves passed format on its device */
errno_t reiser4_format_sync(
	reiser4_format_t *format)	/* disk-format to be saved */
{
	aal_assert("umka-107", format != NULL);
	
	if (!reiser4_format_isdirty(format))
		return 0;
	
	return reiser4call(format, sync);
}

count_t reiser4_format_len(aal_device_t *device, uint32_t blksize) {
	return (aal_device_len(device) * device->blksize / 
		FS_LEN_ADJUST * FS_LEN_ADJUST / blksize);
}

errno_t reiser4_format_check_len(aal_device_t *device, 
				 uint32_t blksize, 
				 count_t blocks) 
{
	count_t dev_len;

	aal_assert("vpf-1564", device != NULL);
	
	dev_len = reiser4_format_len(device, blksize);
	
	if (blocks > dev_len) {
		aal_error("Device %s is too small (%llu) for filesystem %llu "
			  "blocks long.", device->name, dev_len, blocks);
		return -EINVAL;
	}

	if (blocks < REISER4_FS_MIN_SIZE(blksize)) {
		aal_error("Requested filesystem size (%llu) is too small. "
			  "Reiser4 required minimal size %u blocks long.",
			  blocks, REISER4_FS_MIN_SIZE(blksize));
		return -EINVAL;
	}

	return 0;
}

/* Checks passed disk-format for validness */
errno_t reiser4_format_valid(reiser4_format_t *format) {
	aal_assert("umka-829", format != NULL);
	
	return reiser4call(format, valid);
}
#endif

/* Closes passed disk-format */
void reiser4_format_close(
	reiser4_format_t *format)	/* format to be closed */
{
	aal_assert("umka-1505", format != NULL);

	format->fs->format = NULL;
	reiser4call(format, close);
	aal_free(format);
}

#ifndef ENABLE_MINIMAL
/* Returns root block from passed disk-format */
blk_t reiser4_format_get_root(
	reiser4_format_t *format)	/* format to be used */
{
	aal_assert("umka-113", format != NULL);

	return reiser4call(format, get_root);
}

/* Returns string described used disk-format */
const char *reiser4_format_name(
	reiser4_format_t *format)	/* disk-format to be inspected */
{
	aal_assert("umka-111", format != NULL);
	return format->ent->plug->p.label;
}

blk_t reiser4_format_start(reiser4_format_t *format) {
	aal_assert("umka-1693", format != NULL);
	
	return reiser4call(format, start);
}

/* Returns filesystem length in blocks from passed disk-format */
count_t reiser4_format_get_len(
	reiser4_format_t *format)	/* disk-format to be inspected */
{
	aal_assert("umka-360", format != NULL);
    
	return reiser4call(format, get_len);
}

/* Returns number of free blocks */
count_t reiser4_format_get_free(
	reiser4_format_t *format)	/* format to be used */
{
	aal_assert("umka-426", format != NULL);
    
	return reiser4call(format, get_free);
}

/* Returns tree height */
uint16_t reiser4_format_get_height(
	reiser4_format_t *format)	/* format to be inspected */
{
	aal_assert("umka-557", format != NULL);
    
	return reiser4call(format, get_height);
}

/* Returns current mkfs id from the format-specific super-block */
uint32_t reiser4_format_get_stamp(
	reiser4_format_t *format)	/* format to be inspected */
{
	aal_assert("umka-1124", format != NULL);
    
	return reiser4call(format, get_stamp);
}

/* Returns current tail policy id from the format-specific super-block */
uint16_t reiser4_format_get_policy(
	reiser4_format_t *format)	/* format to be inspected */
{
	aal_assert("vpf-836", format != NULL);
    
	return reiser4call(format, get_policy);
}

/* Returns node plugin id from the format-specific super-block */
rid_t reiser4_format_node_pid(
	reiser4_format_t *format)	/* format to be inspected */
{
	aal_assert("edward-19", format != NULL);

	return reiser4call(format, node_pid);
}

/* Sets new root block */
void reiser4_format_set_root(
	reiser4_format_t *format,	/* format new root blocks will be set in */
	blk_t root)			/* new root block */
{
	aal_assert("umka-420", format != NULL);

	reiser4call(format, set_root, root);
}

/* Sets new filesystem length */
void reiser4_format_set_len(
	reiser4_format_t *format,	/* format instance to be used */
	count_t blocks)		        /* new length in blocks */
{
	aal_assert("umka-422", format != NULL);
    
	reiser4call(format, set_len, blocks);
}

/* Sets free block count */
void reiser4_format_set_free(
	reiser4_format_t *format,	/* format to be used */
	count_t blocks)		        /* new free block count */
{
	aal_assert("umka-424", format != NULL);
    
	reiser4call(format, set_free, blocks);
}

/* Sets data room size in blocks */
void reiser4_format_set_data_capacity(
	reiser4_format_t *format,	/* format to be used */
	count_t blocks)		        /* new free block count */
{
	aal_assert("edward-32", format != NULL);

	reiser4call(format, set_data_capacity, blocks);
}

/* Sets minimal occupied number of blocks on a partition */
void reiser4_format_set_min_occup(
	reiser4_format_t *format,	/* format to be used */
	count_t blocks)		        /* value to set */
{
	aal_assert("edward-34", format != NULL);

	reiser4call(format, set_min_occup, blocks);
}

/* Sets new tree height */
void reiser4_format_set_height(
	reiser4_format_t *format,	/* format to be used */
	uint8_t height)		        /* new tree height */
{
	aal_assert("umka-559", format != NULL);
    
	reiser4call(format, set_height, height);
}

/* Updates mkfsid in super block */
void reiser4_format_set_stamp(
	reiser4_format_t *format,	/* format to be used */
	uint32_t stamp)		        /* new tree height */
{
	aal_assert("umka-1125", format != NULL);
    
	reiser4call(format, set_stamp, stamp);
}

/* Sets tail policy in the super block */
void reiser4_format_set_policy(
	reiser4_format_t *format,	/* format to be used */
	uint16_t policy)		/* new policy */
{
	aal_assert("vpf-835", format != NULL);
    
	reiser4call(format, set_policy, policy);
}

/* Returns journal plugin id in use */
rid_t reiser4_format_journal_pid(
	reiser4_format_t *format)	/* format journal pid will be taken
					   from */
{
	aal_assert("umka-115", format != NULL);
	
	return reiser4call(format, journal_pid);
}

/* Returns block allocator plugin id in use */
rid_t reiser4_format_alloc_pid(
	reiser4_format_t *format)	/* format allocator pid will be taken
					   from */
{
	aal_assert("umka-117", format != NULL);
	
	return reiser4call(format, alloc_pid);
}

errno_t reiser4_format_layout(reiser4_format_t *format, 
			      region_func_t region_func,
			      void *data)
{
	aal_assert("umka-1076", format != NULL);
	aal_assert("umka-1077", region_func != NULL);

	return reiser4call(format, layout, region_func, data);
}

/* Returns oid allocator plugin id in use */
rid_t reiser4_format_oid_pid(
	reiser4_format_t *format)	/* format oid allocator pid will be
					   taken from */
{
	aal_assert("umka-491", format != NULL);
	
	return reiser4call(format, oid_pid);
}

errno_t reiser4_format_inc_free(reiser4_format_t *format, uint64_t count) {
	uint64_t saved;
	
	aal_assert("vpf-1722", format != NULL);

	if (count == 0)
		return 0;
	
	saved = reiser4_format_get_free(format);
	reiser4_format_set_free(format, saved + count);

	return 0;
}

errno_t reiser4_format_dec_free(reiser4_format_t *format, uint64_t count) {
	uint64_t saved;
	
	aal_assert("vpf-1722", format != NULL);

	if (count == 0)
		return 0;
	
	saved = reiser4_format_get_free(format);

	if (saved < count) {
		aal_error("Format does not have enough (%llu) blocks "
			  "to allocate (%llu).", saved, count);
		return -ENOSPC;
	}

	reiser4_format_set_free(format, saved - count);
	return 0;
}
#endif
