/*
  format.c -- disk format common code. This code is wrapper for disk-format
  plugin. It is used by filesystem code (filesystem.c) for working with
  different disk-format plugins in independent maner.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>
#include <reiser4/reiser4.h>

#ifndef ENABLE_STAND_ALONE
bool_t reiser4_format_isdirty(reiser4_format_t *format) {
	aal_assert("umka-2106", format != NULL);

	return plugin_call(format->entity->plugin->o.format_ops,
			   isdirty, format->entity);
}

void reiser4_format_mkdirty(reiser4_format_t *format) {
	aal_assert("umka-2107", format != NULL);

	plugin_call(format->entity->plugin->o.format_ops,
		    mkdirty, format->entity);
}

void reiser4_format_mkclean(reiser4_format_t *format) {
	aal_assert("umka-2108", format != NULL);

	plugin_call(format->entity->plugin->o.format_ops,
		    mkclean, format->entity);
}
#endif

/* 
  Opens disk-format on specified device. Actually it just calls specified by
  "pid" disk-format plugin and that plugin makes all dirty work.
*/
reiser4_format_t *reiser4_format_open(
	reiser4_fs_t *fs)	/* fs the format will be opened on */
{
	rid_t pid;
	uint32_t blocksize;
	reiser4_format_t *format;
	reiser4_plugin_t *plugin;
	
	aal_assert("umka-104", fs != NULL);
	aal_assert("umka-1700", fs->master != NULL);
    
	/* Allocating memory for instance of disk-format */
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;
    
	format->fs = fs;
	format->fs->format = format;

	pid = reiser4_master_format(fs->master);
	blocksize = reiser4_master_blksize(fs->master);
    
	/* Finding needed disk-format plugin by its plugin id */
	if (!(plugin = libreiser4_factory_ifind(FORMAT_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find disk-format plugin by "
				    "its id 0x%x.", pid);
		goto error_free_format;
	}
    
	/* Initializing disk-format entity by calling plugin */
	if (!(format->entity = plugin_call(plugin->o.format_ops, open,
					   fs->device, blocksize)))
	{
		aal_exception_fatal("Can't open disk-format %s.",
				    plugin->h.label);
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
	count_t len,		/* filesystem length in blocks */
	uint16_t tail,		/* tail policy to be used */
	rid_t pid)		/* disk-format plugin id to be used */
{
	uint32_t blocksize;
	reiser4_format_t *format;
	reiser4_plugin_t *plugin;
		
	aal_assert("umka-105", fs != NULL);

	/* Getting needed plugin from plugin factory */
	if (!(plugin = libreiser4_factory_ifind(FORMAT_PLUGIN_TYPE, pid)))  {
		aal_exception_error("Can't find disk-format plugin by "
				    "its id 0x%x.", pid);
		return NULL;
	}
    
	/* Allocating memory */
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

	format->fs = fs;
	format->fs->format = format;
	
	blocksize = reiser4_master_blksize(fs->master);
	
	/* 
	   Initializing entity of disk-format by means of calling "create" method 
	   from found plugin. Plugin "create" method will be creating all disk
	   structures, namely, format-specific super block.
	*/
	if (!(format->entity = plugin_call(plugin->o.format_ops, create,
					   fs->device, len, blocksize, tail))) 
	{
		aal_exception_error("Can't create disk-format %s on %s.", 
				    plugin->h.label, aal_device_name(fs->device));
    
		goto error_free_format;
	}
	
	return format;

 error_free_format:
	aal_free(format);
	return NULL;
}

/* Saves passed format on its device */
errno_t reiser4_format_sync(
	reiser4_format_t *format)	/* disk-format to be saved */
{
	aal_assert("umka-107", format != NULL);
	
	return plugin_call(format->entity->plugin->o.format_ops,
			   sync, format->entity);
}

/* Confirms disk-format (simple check) */
int reiser4_format_confirm(
	reiser4_format_t *format)	/* format to be checked */
{
	aal_assert("umka-832", format != NULL);

	return plugin_call(format->entity->plugin->o.format_ops, 
			   confirm, format->fs->device);
}

errno_t reiser4_format_print(reiser4_format_t *format,
			     aal_stream_t *stream)
{
	aal_assert("umka-1560", format != NULL);
	aal_assert("umka-1561", stream != NULL);

	return plugin_call(format->entity->plugin->o.format_ops,
			   print, format->entity, stream, 0);
}

/* Checks passed disk-format for validness */
errno_t reiser4_format_valid(
	reiser4_format_t *format)	/* format to be checked */
{
	aal_assert("umka-829", format != NULL);

	return plugin_call(format->entity->plugin->o.format_ops, 
			   valid, format->entity);
}

/* Reopens disk-format on specified device */
errno_t reiser4_format_reopen(
	reiser4_format_t *format)	/* format to be reopened */
{
	uint32_t blocksize;
	reiser4_plugin_t *plugin;
	
	aal_assert("umka-428", format != NULL);

	plugin = format->entity->plugin;
	plugin_call(plugin->o.format_ops, close, format->entity);
	
	blocksize = reiser4_master_blksize(format->fs->master);
	
	if (!(format->entity = plugin_call(plugin->o.format_ops, open,
					   format->fs->device, blocksize)))
	{
		aal_exception_fatal("Can't open disk-format %s.",
				    plugin->h.label);
		return -EINVAL;
	}
	
	return 0;
}

#endif

/* Closes passed disk-format */
void reiser4_format_close(
	reiser4_format_t *format)	/* format to be closed */
{
	aal_assert("umka-1505", format != NULL);

	format->fs->format = NULL;
	
	plugin_call(format->entity->plugin->o.format_ops,
		    close, format->entity);

	aal_free(format);
}

/* Returns root block from passed disk-format */
blk_t reiser4_format_get_root(
	reiser4_format_t *format)	/* format to be used */
{
	aal_assert("umka-113", format != NULL);

	return plugin_call(format->entity->plugin->o.format_ops, 
			   get_root, format->entity);
}

#ifndef ENABLE_STAND_ALONE
/* Returns string described used disk-format */
const char *reiser4_format_name(
	reiser4_format_t *format)	/* disk-format to be inspected */
{
	aal_assert("umka-111", format != NULL);
	
	return plugin_call(format->entity->plugin->o.format_ops,
			   name, format->entity);
}

blk_t reiser4_format_start(reiser4_format_t *format) {
	aal_assert("umka-1693", format != NULL);
	
	return plugin_call(format->entity->plugin->o.format_ops, 
			   start, format->entity);
}

/* Returns filesystem length in blocks from passed disk-format */
count_t reiser4_format_get_len(
	reiser4_format_t *format)	/* disk-format to be inspected */
{
	aal_assert("umka-360", format != NULL);
    
	return plugin_call(format->entity->plugin->o.format_ops, 
			   get_len, format->entity);
}

/* Returns number of free blocks */
count_t reiser4_format_get_free(
	reiser4_format_t *format)	/* format to be used */
{
	aal_assert("umka-426", format != NULL);
    
	return plugin_call(format->entity->plugin->o.format_ops, 
			   get_free, format->entity);
}
#endif

/* Returns tree height */
uint16_t reiser4_format_get_height(
	reiser4_format_t *format)	/* format to be inspected */
{
	aal_assert("umka-557", format != NULL);
    
	return plugin_call(format->entity->plugin->o.format_ops, 
			   get_height, format->entity);
}

#ifndef ENABLE_STAND_ALONE
/* Returns current mkfs id from the format-specific super-block */
uint32_t reiser4_format_get_stamp(
	reiser4_format_t *format)	/* format to be inspected */
{
	aal_assert("umka-1124", format != NULL);
    
	return plugin_call(format->entity->plugin->o.format_ops, 
			   get_stamp, format->entity);
}

/* Returns current tail policy id from the format-specific super-block */
uint16_t reiser4_format_get_policy(
	reiser4_format_t *format)	/* format to be inspected */
{
	aal_assert("vpf-836", format != NULL);
    
	return plugin_call(format->entity->plugin->o.format_ops, 
			   get_policy, format->entity);
}

/* Sets new root block */
void reiser4_format_set_root(
	reiser4_format_t *format,	/* format new root blocks will be set in */
	blk_t root)			/* new root block */
{
	aal_assert("umka-420", format != NULL);

	plugin_call(format->entity->plugin->o.format_ops, 
		    set_root, format->entity, root);
}

/* Sets new filesystem length */
void reiser4_format_set_len(
	reiser4_format_t *format,	/* format instance to be used */
	count_t blocks)		        /* new length in blocks */
{
	aal_assert("umka-422", format != NULL);
    
	plugin_call(format->entity->plugin->o.format_ops, 
		    set_len, format->entity, blocks);
}

/* Sets free block count */
void reiser4_format_set_free(
	reiser4_format_t *format,	/* format to be used */
	count_t blocks)		        /* new free block count */
{
	aal_assert("umka-424", format != NULL);
    
	plugin_call(format->entity->plugin->o.format_ops, 
		    set_free, format->entity, blocks);
}

/* Sets new tree height */
void reiser4_format_set_height(
	reiser4_format_t *format,	/* format to be used */
	uint8_t height)		        /* new tree height */
{
	aal_assert("umka-559", format != NULL);
    
	plugin_call(format->entity->plugin->o.format_ops, 
		    set_height, format->entity, height);
}

/* Updates mkfsid in super block */
void reiser4_format_set_stamp(
	reiser4_format_t *format,	/* format to be used */
	uint32_t stamp)		        /* new tree height */
{
	aal_assert("umka-1125", format != NULL);
    
	plugin_call(format->entity->plugin->o.format_ops, 
		    set_stamp, format->entity, stamp);
}

/* Sets tail policy in the super block */
void reiser4_format_set_policy(
	reiser4_format_t *format,	/* format to be used */
	uint16_t policy)		/* new policy */
{
	aal_assert("vpf-835", format != NULL);
    
	plugin_call(format->entity->plugin->o.format_ops, 
		    set_policy, format->entity, policy);
}

/* Returns journal plugin id in use */
rid_t reiser4_format_journal_pid(
	reiser4_format_t *format)	/* disk-format journal pid will be obtained from */
{
	aal_assert("umka-115", format != NULL);
	
	return plugin_call(format->entity->plugin->o.format_ops, 
			   journal_pid, format->entity);
}

/* Returns block allocator plugin id in use */
rid_t reiser4_format_alloc_pid(
	reiser4_format_t *format)	/* disk-format allocator pid will be obtained from */
{
	aal_assert("umka-117", format != NULL);
	
	return plugin_call(format->entity->plugin->o.format_ops, 
			   alloc_pid, format->entity);
}

errno_t reiser4_format_skipped(reiser4_format_t *format, 
			       block_func_t func,
			       void *data)
{
	aal_assert("umka-1083", format != NULL);
	aal_assert("umka-1084", func != NULL);

	return plugin_call(format->entity->plugin->o.format_ops,
			   skipped, format->entity, func, data);
}

errno_t reiser4_format_layout(reiser4_format_t *format, 
			      block_func_t func,
			      void *data)
{
	aal_assert("umka-1076", format != NULL);
	aal_assert("umka-1077", func != NULL);

	return plugin_call(format->entity->plugin->o.format_ops,
			   layout, format->entity, func, data);
}

/* Returns oid allocator plugin id in use */
rid_t reiser4_format_oid_pid(
	reiser4_format_t *format)	/* disk-format oid allocator pid will be obtained from */
{
	aal_assert("umka-491", format != NULL);
	
	return plugin_call(format->entity->plugin->o.format_ops, 
			   oid_pid, format->entity);
}
#endif
