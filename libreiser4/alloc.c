/*
  alloc.c -- reiser4 block allocator common code.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_ALONE

#include <reiser4/reiser4.h>

/*
  Initializes block allocator structures and requests block allocator plugin for
  opening. Returns initialized instance of block allocator, which may be used in
  all further operations.
*/
reiser4_alloc_t *reiser4_alloc_open(
	reiser4_fs_t *fs,	/* fs allocator is going to be opened on */
	count_t count)		/* filesystem size in blocks */
{
	rid_t pid;
	reiser4_alloc_t *alloc;
	reiser4_plugin_t *plugin;
	
	aal_assert("umka-135", fs != NULL);
	aal_assert("umka-135", fs->format != NULL);

	/* Initializing instance of block allocator */
	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;

	alloc->fs = fs;
	alloc->dirty = FALSE;
	alloc->fs->alloc = alloc;
	
	if ((pid = reiser4_format_alloc_pid(fs->format)) == INVAL_PID) {
		aal_exception_error("Invalid block allocator plugin id has "
				    "been found.");
		goto error_free_alloc;
	}
    
	/* Finding block allocator plugin */
	if (!(plugin = libreiser4_factory_ifind(ALLOC_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find block allocator plugin by "
				    "its id 0x%x.", pid);
		goto error_free_alloc;
	}
    
	/* Calling "open" method from block allocator plugin */
	if (!(alloc->entity = plugin_call(plugin->alloc_ops, open,
					  fs->device, count)))
	{
		aal_exception_error("Can't initialize block allocator.");
		goto error_free_alloc;
	}
	
	return alloc;
	
 error_free_alloc:
	aal_free(alloc);
	return NULL;
}

/* 
   Creates new block allocator. Initializes all structures, calles block
   allocator plugin in order to initialize allocator instance and returns
   instance to caller.
*/
reiser4_alloc_t *reiser4_alloc_create(
	reiser4_fs_t *fs,           /* fs block allocator is going to be created on */
	count_t count)		    /* filesystem size in blocks */
{
	rid_t pid;
	reiser4_alloc_t *alloc;
	reiser4_plugin_t *plugin;
	
	aal_assert("umka-726", fs != NULL);
	aal_assert("umka-1694", fs->format != NULL);

	/* Allocating memory for the allocator instance */
	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;

	alloc->fs = fs;
	alloc->dirty = TRUE;
	alloc->fs->alloc = alloc;
	
	if ((pid = reiser4_format_alloc_pid(fs->format)) == INVAL_PID) {
		aal_exception_error("Invalid block allocator plugin id "
				    "has been found.");
		goto error_free_alloc;
	}
    
	/* Getting needed plugin from plugin factory by its id */
	if (!(plugin = libreiser4_factory_ifind(ALLOC_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find block allocator plugin by "
				    "its id 0x%x.", pid);
		goto error_free_alloc;
	}
    
	/* Query the block allocator plugin for creating allocator entity */
	if (!(alloc->entity = plugin_call(plugin->alloc_ops, create,
					  fs->device, count)))
	{
		aal_exception_error("Can't create block allocator.");
		goto error_free_alloc;
	}
	
	return alloc;
	
 error_free_alloc:
	aal_free(alloc);
	return NULL;
}

errno_t reiser4_alloc_assign(reiser4_alloc_t *alloc,
			     aux_bitmap_t *bitmap)
{
	errno_t res;
	
	aal_assert("vpf-582", alloc != NULL);
	aal_assert("umka-1848", bitmap != NULL);

	if ((res = plugin_call(alloc->entity->plugin->alloc_ops, 
			       assign, alloc->entity, bitmap)))
		return res;

	alloc->dirty = TRUE;
	return 0;
}

/* Make request to allocator plugin in order to save its data to device */
errno_t reiser4_alloc_sync(
	reiser4_alloc_t *alloc)	/* allocator to be synced */
{
	errno_t res;
	aal_assert("umka-139", alloc != NULL);

	if ((res = plugin_call(alloc->entity->plugin->alloc_ops,
			       sync, alloc->entity)))
		return res;

	alloc->dirty = FALSE;
	return 0;
}

errno_t reiser4_alloc_print(reiser4_alloc_t *alloc,
			    aal_stream_t *stream)
{
	aal_assert("umka-1566", alloc != NULL);
	aal_assert("umka-1567", stream != NULL);

	return plugin_call(alloc->entity->plugin->alloc_ops,
			   print, alloc->entity, stream, 0);
}

/* Close passed allocator instance */
void reiser4_alloc_close(
	reiser4_alloc_t *alloc)	/* allocator to be closed */
{
	aal_assert("umka-1504", alloc != NULL);

	alloc->fs->alloc = NULL;
	
	/* Calling the plugin for close its internal instance properly */
	plugin_call(alloc->entity->plugin->alloc_ops, 
		    close, alloc->entity);
    
	if (alloc->forbid)
		aux_bitmap_close(alloc->forbid);
	    
	aal_free(alloc);
}

/* Returns the number of free blocks in allocator */
count_t reiser4_alloc_unused(
	reiser4_alloc_t *alloc)	/* allocator to be realeased */
{
	aal_assert("umka-362", alloc != NULL);

	return plugin_call(alloc->entity->plugin->alloc_ops, 
			   unused, alloc->entity);
}

/* Returns the number of used blocks in allocator */
count_t reiser4_alloc_used(
	reiser4_alloc_t *alloc)	/* allocator used blocks will be obtained from */
{
	aal_assert("umka-499", alloc != NULL);

	return plugin_call(alloc->entity->plugin->alloc_ops, 
			   used, alloc->entity);
}

/* Marks specified blocks as used */
errno_t reiser4_alloc_occupy_region(
	reiser4_alloc_t *alloc,	/* allocator for working with */
	blk_t start, 		/* start block to be marked */
	count_t count)		/* count to be marked */
{
	aal_assert("umka-501", alloc != NULL);

	alloc->dirty = TRUE;
	
	plugin_call(alloc->entity->plugin->alloc_ops, 
		    occupy_region, alloc->entity, start, count);

	return 0;
}

/* Deallocs specified blocks */
errno_t reiser4_alloc_release_region(
	reiser4_alloc_t *alloc,	/* allocator for wiorking with */
	blk_t start, 		/* start block to be deallocated */
	count_t count)		/* count of blocks to be deallocated */
{
	aal_assert("umka-503", alloc != NULL);

	alloc->dirty = TRUE;
	
	return plugin_call(alloc->entity->plugin->alloc_ops, 
			   release_region, alloc->entity, start, count);
}

/* Makes request to plugin for allocating block */
count_t reiser4_alloc_allocate_region(
	reiser4_alloc_t *alloc, /* allocator for working with */
	blk_t *start,           /* start block */
	count_t count)          /* requested block count */
{
	aal_assert("umka-505", alloc != NULL);

	alloc->dirty = TRUE;
	
	return plugin_call(alloc->entity->plugin->alloc_ops, 
			   allocate_region, alloc->entity, start, count);
}

errno_t reiser4_alloc_valid(
	reiser4_alloc_t *alloc)	/* allocator to be checked */
{
	aal_assert("umka-833", alloc != NULL);

	return plugin_call(alloc->entity->plugin->alloc_ops, 
			   valid, alloc->entity);
}

/* Returns TRUE if specified blocks used. */
bool_t reiser4_alloc_used_region(
	reiser4_alloc_t *alloc,	/* allocator for working with */
	blk_t start, 		/* start block to be tested (used or not) */
	count_t count)		/* count of blocks to be tested */
{
	aal_assert("umka-662", alloc != NULL);

	return plugin_call(alloc->entity->plugin->alloc_ops, 
			   used_region, alloc->entity, start, count);
}

/* Returns TRUE if specified blocks unused. */
bool_t reiser4_alloc_unused_region(
	reiser4_alloc_t *alloc,	/* allocator for working with */
	blk_t start, 		/* start block to be tested (used or not) */
	count_t count)		/* count of blocks to be tested */
{
	aal_assert("umka-662", alloc != NULL);

	return plugin_call(alloc->entity->plugin->alloc_ops, 
			   unused_region, alloc->entity, start, count);
}

errno_t reiser4_alloc_related_region(reiser4_alloc_t *alloc,
				     blk_t blk, region_func_t func, 
				     void *data)
{
	aal_assert("vpf-557", alloc != NULL);
	aal_assert("umka-1685", func != NULL);

	return plugin_call(alloc->entity->plugin->alloc_ops, 
			   related_region, alloc->entity, blk, func, data);
}

errno_t reiser4_alloc_layout(reiser4_alloc_t *alloc, 
			     block_func_t func,
			     void *data)
{
	aal_assert("umka-1080", alloc != NULL);
	aal_assert("umka-1081", func != NULL);

	return plugin_call(alloc->entity->plugin->alloc_ops,
			   layout, alloc->entity, func, data);
}

errno_t reiser4_alloc_forbid(reiser4_alloc_t *alloc,
			     blk_t start,
			     count_t count) 
{
	aal_assert("vpf-584", alloc != NULL);

	if (!alloc->forbid) {
		count_t used = reiser4_alloc_used(alloc);
		count_t unused = reiser4_alloc_unused(alloc);
		alloc->forbid = aux_bitmap_create(unused + used);
	}
	
	aux_bitmap_mark_region(alloc->forbid, start, count);
	
	return 0;	
}

errno_t reiser4_alloc_permit(reiser4_alloc_t *alloc,
			     blk_t start, count_t count) 
{
	aal_assert("vpf-585", alloc != NULL);
	
	if (alloc->forbid) {
		aux_bitmap_clear_region(alloc->forbid,
					start, count);
	}
	
	return 0;
}

errno_t reiser4_alloc_assign_forb(reiser4_alloc_t *alloc, 
				  aux_bitmap_t *bitmap) 
{
	uint32_t i;

	aal_assert("vpf-583", alloc != NULL);
	aal_assert("umka-1849", bitmap != NULL);

	if (!alloc->forbid) {
		count_t used = reiser4_alloc_used(alloc);
		count_t unused = reiser4_alloc_unused(alloc);
		
		alloc->forbid = aux_bitmap_create(unused + used);
	}

	aal_assert("vpf-589", alloc->forbid->size == bitmap->size &&
		   alloc->forbid->total == bitmap->total);

	/* FIXME-UMKA: This should be optimized */
	for (i = 0; i < alloc->forbid->size; i++)
		alloc->forbid->map[i] |= bitmap->map[i];
	
	return 0;
}

errno_t reiser4_alloc_assign_perm(reiser4_alloc_t *alloc, 
	aux_bitmap_t *bitmap) 
{
	uint32_t i;

	aal_assert("vpf-587", alloc != NULL);
	aal_assert("umka-1850", bitmap != NULL);
	
	if (!alloc->forbid) 
		return 0;

	aal_assert("vpf-590", alloc->forbid->size == bitmap->size &&
		   alloc->forbid->total == bitmap->total);
	
	for (i = 0; i < alloc->forbid->size; i++)
		alloc->forbid->map[i] &= ~bitmap->map[i];

	return 0;
}

#endif
