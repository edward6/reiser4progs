/*
  alloc.c -- reiser4 block allocator common code.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/reiser4.h>

/*
  Initializes block allocator structures and requests block allocator plugin for
  opening. Returns initialized instance of block allocator, which may be used in
  all further operations.
*/
reiser4_alloc_t *reiser4_alloc_open(
	reiser4_fs_t *fs,	/* fs allocator is going to be opened on */
	count_t len)		/* filesystem size in blocks */
{
	rpid_t pid;
	reiser4_alloc_t *alloc;
	reiser4_plugin_t *plugin;
	
	aal_assert("umka-135", fs != NULL, return NULL);
	aal_assert("umka-135", fs->format != NULL, return NULL);

	/* Initializing instance of block allocator */
	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;

	alloc->fs = fs;
	
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
	if (!(alloc->entity = plugin_call(goto error_free_alloc, 
					  plugin->alloc_ops, open,
					  fs->device, len)))
	{
		aal_exception_error("Can't initialize block allocator.");
		goto error_free_alloc;
	}
	
	return alloc;
	
 error_free_alloc:
	aal_free(alloc);
	return NULL;
}

#ifndef ENABLE_COMPACT

/* 
   Creates new block allocator. Initializes all structures, calles block
   allocator plugin in order to initialize allocator instance and returns
   instance to caller.
*/
reiser4_alloc_t *reiser4_alloc_create(
	reiser4_fs_t *fs,           /* fs block allocator is going to be created on */
	count_t len)		    /* filesystem size in blocks */
{
	rpid_t pid;
	reiser4_alloc_t *alloc;
	reiser4_plugin_t *plugin;
	
	aal_assert("umka-726", fs != NULL, return NULL);
	aal_assert("umka-1694", fs->format != NULL, return NULL);

	/* Allocating memory for the allocator instance */
	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;

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
	if (!(alloc->entity = plugin_call(goto error_free_alloc, 
					  plugin->alloc_ops, create,
					  fs->device, len)))
	{
		aal_exception_error("Can't create block allocator.");
		goto error_free_alloc;
	}
	
	return alloc;
	
 error_free_alloc:
	aal_free(alloc);
	return NULL;
}

errno_t reiser4_alloc_assign(reiser4_alloc_t *alloc, aux_bitmap_t *bitmap) {
	aal_assert("vpf-582", alloc != NULL && bitmap != NULL, return -1);

	return plugin_call(return -1, alloc->entity->plugin->alloc_ops, 
			   assign, alloc->entity, bitmap);
}

/* Make request to allocator plugin in order to save its data to device */
errno_t reiser4_alloc_sync(
	reiser4_alloc_t *alloc)	/* allocator to be syncked */
{
	aal_assert("umka-139", alloc != NULL, return -1);

	return plugin_call(return -1, alloc->entity->plugin->alloc_ops, 
			   sync, alloc->entity);
}

errno_t reiser4_alloc_print(reiser4_alloc_t *alloc, aal_stream_t *stream) {
	aal_assert("umka-1566", alloc != NULL, return -1);
	aal_assert("umka-1567", stream != NULL, return -1);

	return plugin_call(return -1, alloc->entity->plugin->alloc_ops,
			   print, alloc->entity, stream, 0);
}

#endif

/* Close passed allocator instance */
void reiser4_alloc_close(
	reiser4_alloc_t *alloc)	/* allocator to be closed */
{
	aal_assert("umka-1504", alloc != NULL, return);

	/* Calling the plugin for close its internal instance properly */
	plugin_call(return, alloc->entity->plugin->alloc_ops, 
		    close, alloc->entity);
    
	if (alloc->forbid)
		aux_bitmap_close(alloc->forbid);
	    
	aal_free(alloc);
}

/* Returns the number of free blocks in allocator */
count_t reiser4_alloc_free(
	reiser4_alloc_t *alloc)	/* allocator to be realeased */
{
	aal_assert("umka-362", alloc != NULL, return INVAL_BLK);

	return plugin_call(return INVAL_BLK, alloc->entity->plugin->alloc_ops, 
			   free, alloc->entity);
}

/* Returns the number of used blocks in allocator */
count_t reiser4_alloc_used(
	reiser4_alloc_t *alloc)	/* allocator used blocks will be obtained from */
{
	aal_assert("umka-499", alloc != NULL, return INVAL_BLK);

	return plugin_call(return INVAL_BLK, alloc->entity->plugin->alloc_ops, 
			   used, alloc->entity);
}

#ifndef ENABLE_COMPACT

/* Marks specified block as used */
errno_t reiser4_alloc_mark(
	reiser4_alloc_t *alloc,	/* allocator for working with */
	blk_t blk)			/* block to be marked */
{
	aal_assert("umka-501", alloc != NULL, return -1);

	plugin_call(return -1, alloc->entity->plugin->alloc_ops, 
		    mark, alloc->entity, blk);

	return 0;
}

/* Deallocs specified block */
errno_t reiser4_alloc_release(
	reiser4_alloc_t *alloc,	/* allocator for wiorking with */
	blk_t blk)		/* block to be deallocated */
{
	aal_assert("umka-503", alloc != NULL, return -1);

	plugin_call(return -1, alloc->entity->plugin->alloc_ops, 
		    release, alloc->entity, blk);

	return 0;
}

/* Makes request to plugin for allocating block */
blk_t reiser4_alloc_allocate(
	reiser4_alloc_t *alloc)	/* allocator for working with */
{
	aal_assert("umka-505", alloc != NULL, return INVAL_BLK);

	return plugin_call(return INVAL_BLK, alloc->entity->plugin->alloc_ops, 
			   allocate, alloc->entity);
}

errno_t reiser4_alloc_valid(
	reiser4_alloc_t *alloc)	/* allocator to be checked */
{
	aal_assert("umka-833", alloc != NULL, return -1);

	return plugin_call(return -1, alloc->entity->plugin->alloc_ops, 
			   valid, alloc->entity);
}

#endif

/* 
   Checks whether specified block used or not. Returns TRUE if used and FALSE
   otherwise.
*/
int reiser4_alloc_test(
	reiser4_alloc_t *alloc,	/* allocator for working with */
	blk_t blk)		/* block to be tested (used or not ) */
{
	aal_assert("umka-662", alloc != NULL, return 0);

	return plugin_call(return 0, alloc->entity->plugin->alloc_ops, 
			   test, alloc->entity, blk);
}

errno_t reiser4_alloc_region(
	reiser4_alloc_t *alloc,
	blk_t blk,
	block_func_t func, 
	void *data)
{
	aal_assert("vpf-557", alloc != NULL, return 0);
	aal_assert("umka-1685", func != NULL, return 0);

	return plugin_call(return -1, alloc->entity->plugin->alloc_ops, 
			   region, alloc->entity, blk, func, data);
}

errno_t reiser4_alloc_layout(
	reiser4_alloc_t *alloc, 
	block_func_t func,
	void *data)
{
	aal_assert("umka-1080", alloc != NULL, return -1);
	aal_assert("umka-1081", func != NULL, return -1);

	return plugin_call(return -1, alloc->entity->plugin->alloc_ops,
			   layout, alloc->entity, func, data);
}

errno_t reiser4_alloc_forbid(reiser4_alloc_t *alloc, blk_t blk) {
	aal_assert("vpf-584", alloc != NULL, return -1);

	if (!alloc->forbid) 
	    aux_bitmap_create(reiser4_alloc_free(alloc) + 
			      reiser4_alloc_used(alloc));
	
	aux_bitmap_mark(alloc->forbid, blk);
	
	return 0;	
}

errno_t reiser4_alloc_permit(reiser4_alloc_t *alloc, blk_t blk) {
	aal_assert("vpf-585", alloc != NULL, return -1);
	
	if (!alloc->forbid) 
	    aux_bitmap_create(reiser4_alloc_free(alloc) + 
			      reiser4_alloc_used(alloc));
	
	aux_bitmap_clear(alloc->forbid, blk);
	
	return 0;
}

errno_t reiser4_alloc_assign_forb(reiser4_alloc_t *alloc, 
	aux_bitmap_t *bitmap) 
{
	uint32_t i;
	aal_assert("vpf-583", alloc != NULL && bitmap != NULL, return -1);

	if (!alloc->forbid) 
	    aux_bitmap_create(reiser4_alloc_free(alloc) + 
			      reiser4_alloc_used(alloc));

	aal_assert("vpf-589", alloc->forbid->size == bitmap->size &&
		   alloc->forbid->total == bitmap->total, return -1);
	
	for (i = 0; i < alloc->forbid->size; i++)
		alloc->forbid->map[i] |= bitmap->map[i];
	
	return 0;
}

errno_t reiser4_alloc_assign_perm(reiser4_alloc_t *alloc, 
	aux_bitmap_t *bitmap) 
{
	uint32_t i;

	aal_assert("vpf-587", alloc != NULL && bitmap != NULL, return -1);
	
	if (!alloc->forbid) 
	    return 0;

	aal_assert("vpf-590", alloc->forbid->size == bitmap->size &&
		   alloc->forbid->total == bitmap->total, return -1);
	
	for (i = 0; i < alloc->forbid->size; i++)
		alloc->forbid->map[i] &= ~bitmap->map[i];

	return 0;
}
