/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format.c -- disk format common code. This code is wrapper for disk-format
   plugin. It is used by filesystem code (filesystem.c) for working with
   different disk-format plugins in independent maner. */

#include <reiser4/libreiser4.h>

#ifndef ENABLE_STAND_ALONE
bool_t reiser4_format_isdirty(reiser4_format_t *format) {
	uint32_t state;
	
	aal_assert("umka-2106", format != NULL);

	state = plug_call(format->entity->plug->o.format_ops,
			  get_state, format->entity);
	
	return (state & (1 << ENTITY_DIRTY));
}

void reiser4_format_mkdirty(reiser4_format_t *format) {
	uint32_t state;
	
	aal_assert("umka-2107", format != NULL);

	state = plug_call(format->entity->plug->o.format_ops,
			  get_state, format->entity);

	state |= (1 << ENTITY_DIRTY);
	
	plug_call(format->entity->plug->o.format_ops,
		  set_state, format->entity, state);
}

void reiser4_format_mkclean(reiser4_format_t *format) {
	uint32_t state;
	
	aal_assert("umka-2108", format != NULL);

	state = plug_call(format->entity->plug->o.format_ops,
			  get_state, format->entity);

	state &= ~(1 << ENTITY_DIRTY);
	
	plug_call(format->entity->plug->o.format_ops,
		  set_state, format->entity, state);
}
#endif

/* Opens disk-format on specified device. Actually it just calls specified by
   "pid" disk-format plugin and that plugin makes all dirty work. */
reiser4_format_t *reiser4_format_open(
	reiser4_fs_t *fs)	/* fs the format will be opened on */
{
	rid_t pid;
	fs_desc_t desc;
	reiser4_plug_t *plug;
	reiser4_format_t *format;
	
	aal_assert("umka-104", fs != NULL);
	aal_assert("umka-1700", fs->master != NULL);
    
	/* Initializing format instance. */
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;
    
	format->fs = fs;
	format->fs->format = format;

	desc.device = fs->device;
	pid = reiser4_master_get_format(fs->master);
	desc.blksize = reiser4_master_get_blksize(fs->master);
    
	/* Finding needed disk-format plugin by its plugin id. */
	if (!(plug = reiser4_factory_ifind(FORMAT_PLUG_TYPE, pid))) {
		aal_error("Can't find disk-format plugin by "
			  "its id 0x%x.", pid);
		goto error_free_format;
	}
    
	/* Initializing disk-format entity by calling plugin */
	if (!(format->entity = plug_call(plug->o.format_ops, open, &desc))) {
		aal_fatal("Can't open disk-format %s.",  plug->label);
		goto error_free_format;
	}

	return format;
	
 error_free_format:
	aal_free(format);
	return NULL;
}

#ifndef ENABLE_STAND_ALONE
/* Creates disk-format structures on specified device */
reiser4_format_t *reiser4_format_create(
	reiser4_fs_t *fs,	/* fs the format will be created on */
	count_t blocks,		/* filesystem length in blocks */
	uint16_t policy,	/* tail policy to be used */
	rid_t pid)		/* disk-format plugin id to be used */
{
	fs_desc_t desc;
	reiser4_plug_t *plug;
	reiser4_format_t *format;
		
	aal_assert("umka-105", fs != NULL);

	/* Getting needed plugin from plugin factory */
	if (!(plug = reiser4_factory_ifind(FORMAT_PLUG_TYPE, pid)))  {
		aal_error("Can't find disk-format plugin by "
			  "its id 0x%x.", pid);
		return NULL;
	}
    
	/* Initializing format instance. */
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

	format->fs = fs;
	format->fs->format = format;

	/* Initializing filesystem descriptor. */
	desc.policy = policy;
	desc.device = fs->device;
	desc.blksize = reiser4_master_get_blksize(fs->master);
	
	/* Initializing entity of disk-format by means of calling "create"
	   method from found plugin. Plugin "create" method will be creating 
	   all disk structures, namely, format-specific super block. */
	if (!(format->entity = plug_call(plug->o.format_ops,
					 create, &desc, blocks))) 
	{
		aal_error("Can't create format %s on %s.",
			  plug->label, fs->device->name);
    
		aal_free(format);
		return NULL;
	}
	
	return format;
}

errno_t reiser4_format_backup(reiser4_format_t *format,
			      aal_stream_t *stream) 
{
	aal_assert("vpf-1390", format != NULL);
	aal_assert("vpf-1391", stream != NULL);

	return plug_call(format->entity->plug->o.format_ops,
			 backup, format->entity, stream);
}

/* Saves passed format on its device */
errno_t reiser4_format_sync(
	reiser4_format_t *format)	/* disk-format to be saved */
{
	aal_assert("umka-107", format != NULL);
	
	if (!reiser4_format_isdirty(format))
		return 0;
	
	return plug_call(format->entity->plug->o.format_ops,
			 sync, format->entity);
}

/* Checks passed disk-format for validness */
errno_t reiser4_format_valid(
	reiser4_format_t *format)	/* format to be checked */
{
	aal_assert("umka-829", format != NULL);

	return plug_call(format->entity->plug->o.format_ops, 
			 valid, format->entity);
}
#endif

/* Closes passed disk-format */
void reiser4_format_close(
	reiser4_format_t *format)	/* format to be closed */
{
	aal_assert("umka-1505", format != NULL);

	format->fs->format = NULL;
	
	plug_call(format->entity->plug->o.format_ops,
		  close, format->entity);

	aal_free(format);
}

/* Returns root block from passed disk-format */
blk_t reiser4_format_get_root(
	reiser4_format_t *format)	/* format to be used */
{
	aal_assert("umka-113", format != NULL);

	return plug_call(format->entity->plug->o.format_ops, 
			 get_root, format->entity);
}

#ifndef ENABLE_STAND_ALONE
/* Returns string described used disk-format */
const char *reiser4_format_name(
	reiser4_format_t *format)	/* disk-format to be inspected */
{
	aal_assert("umka-111", format != NULL);
	return format->entity->plug->label;
}

blk_t reiser4_format_start(reiser4_format_t *format) {
	aal_assert("umka-1693", format != NULL);
	
	return plug_call(format->entity->plug->o.format_ops, 
			 start, format->entity);
}

/* Returns filesystem length in blocks from passed disk-format */
count_t reiser4_format_get_len(
	reiser4_format_t *format)	/* disk-format to be inspected */
{
	aal_assert("umka-360", format != NULL);
    
	return plug_call(format->entity->plug->o.format_ops, 
			 get_len, format->entity);
}

/* Returns number of free blocks */
count_t reiser4_format_get_free(
	reiser4_format_t *format)	/* format to be used */
{
	aal_assert("umka-426", format != NULL);
    
	return plug_call(format->entity->plug->o.format_ops, 
			 get_free, format->entity);
}
#endif

/* Returns tree height */
uint16_t reiser4_format_get_height(
	reiser4_format_t *format)	/* format to be inspected */
{
	aal_assert("umka-557", format != NULL);
    
	return plug_call(format->entity->plug->o.format_ops, 
			 get_height, format->entity);
}

#ifndef ENABLE_STAND_ALONE
/* Returns current mkfs id from the format-specific super-block */
uint32_t reiser4_format_get_stamp(
	reiser4_format_t *format)	/* format to be inspected */
{
	aal_assert("umka-1124", format != NULL);
    
	return plug_call(format->entity->plug->o.format_ops, 
			 get_stamp, format->entity);
}

/* Returns current tail policy id from the format-specific super-block */
uint16_t reiser4_format_get_policy(
	reiser4_format_t *format)	/* format to be inspected */
{
	aal_assert("vpf-836", format != NULL);
    
	return plug_call(format->entity->plug->o.format_ops, 
			 get_policy, format->entity);
}

/* Sets new root block */
void reiser4_format_set_root(
	reiser4_format_t *format,	/* format new root blocks will be set in */
	blk_t root)			/* new root block */
{
	aal_assert("umka-420", format != NULL);

	plug_call(format->entity->plug->o.format_ops, 
		  set_root, format->entity, root);
}

/* Sets new filesystem length */
void reiser4_format_set_len(
	reiser4_format_t *format,	/* format instance to be used */
	count_t blocks)		        /* new length in blocks */
{
	aal_assert("umka-422", format != NULL);
    
	plug_call(format->entity->plug->o.format_ops, 
		  set_len, format->entity, blocks);
}

/* Sets free block count */
void reiser4_format_set_free(
	reiser4_format_t *format,	/* format to be used */
	count_t blocks)		        /* new free block count */
{
	aal_assert("umka-424", format != NULL);
    
	plug_call(format->entity->plug->o.format_ops, 
		  set_free, format->entity, blocks);
}

/* Sets new tree height */
void reiser4_format_set_height(
	reiser4_format_t *format,	/* format to be used */
	uint8_t height)		        /* new tree height */
{
	aal_assert("umka-559", format != NULL);
    
	plug_call(format->entity->plug->o.format_ops, 
		  set_height, format->entity, height);
}

/* Updates mkfsid in super block */
void reiser4_format_set_stamp(
	reiser4_format_t *format,	/* format to be used */
	uint32_t stamp)		        /* new tree height */
{
	aal_assert("umka-1125", format != NULL);
    
	plug_call(format->entity->plug->o.format_ops, 
		  set_stamp, format->entity, stamp);
}

/* Sets tail policy in the super block */
void reiser4_format_set_policy(
	reiser4_format_t *format,	/* format to be used */
	uint16_t policy)		/* new policy */
{
	aal_assert("vpf-835", format != NULL);
    
	plug_call(format->entity->plug->o.format_ops, 
		  set_policy, format->entity, policy);
}

/* Returns journal plugin id in use */
rid_t reiser4_format_journal_pid(
	reiser4_format_t *format)	/* format journal pid will be taken
					   from */
{
	aal_assert("umka-115", format != NULL);
	
	return plug_call(format->entity->plug->o.format_ops, 
			 journal_pid, format->entity);
}

/* Returns block allocator plugin id in use */
rid_t reiser4_format_alloc_pid(
	reiser4_format_t *format)	/* format allocator pid will be taken
					   from */
{
	aal_assert("umka-117", format != NULL);
	
	return plug_call(format->entity->plug->o.format_ops, 
			 alloc_pid, format->entity);
}

errno_t reiser4_format_layout(reiser4_format_t *format, 
			      region_func_t region_func,
			      void *data)
{
	aal_assert("umka-1076", format != NULL);
	aal_assert("umka-1077", region_func != NULL);

	return plug_call(format->entity->plug->o.format_ops,
			 layout, format->entity, region_func,
			 data);
}

/* Returns oid allocator plugin id in use */
rid_t reiser4_format_oid_pid(
	reiser4_format_t *format)	/* format oid allocator pid will be
					   taken from */
{
	aal_assert("umka-491", format != NULL);
	
	return plug_call(format->entity->plug->o.format_ops, 
			 oid_pid, format->entity);
}
#endif

/* Returns key plugin id */
rid_t reiser4_format_key_pid(reiser4_format_t *format) {
	uint64_t flags;
	
	aal_assert("umka-2347", format != NULL);

	flags = plug_call(format->entity->plug->o.format_ops,
			  get_flags, format->entity);
	
	if (flags & (1 << REISER4_LARGE_KEYS))
		return KEY_LARGE_ID;

	return KEY_SHORT_ID;
}
