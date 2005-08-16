/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   alloc.c -- reiser4 block allocator common code. */

#ifndef ENABLE_MINIMAL
#include <reiser4/libreiser4.h>

typedef enum alloc_init {
	ALLOC_OPEN,
	ALLOC_CREATE
} alloc_init_t;

bool_t reiser4_alloc_isdirty(reiser4_alloc_t *alloc) {
	uint32_t state;
	
	aal_assert("umka-2655", alloc != NULL);

	state = plug_call(alloc->ent->plug->o.alloc_ops,
			  get_state, alloc->ent);
	
	return (state & (1 << ENTITY_DIRTY));
}

void reiser4_alloc_mkdirty(reiser4_alloc_t *alloc) {
	uint32_t state;
	
	aal_assert("umka-2656", alloc != NULL);

	state = plug_call(alloc->ent->plug->o.alloc_ops,
			  get_state, alloc->ent);

	state |= (1 << ENTITY_DIRTY);
	
	plug_call(alloc->ent->plug->o.alloc_ops,
		  set_state, alloc->ent, state);
}

void reiser4_alloc_mkclean(reiser4_alloc_t *alloc) {
	uint32_t state;
	
	aal_assert("umka-2657", alloc != NULL);

	state = plug_call(alloc->ent->plug->o.alloc_ops,
			  get_state, alloc->ent);

	state &= ~(1 << ENTITY_DIRTY);
	
	plug_call(alloc->ent->plug->o.alloc_ops,
		  set_state, alloc->ent, state);
}

/* Common block allocator init function. It is used for open, 
   create and unpack block allocator instance. */
static reiser4_alloc_t *reiser4_alloc_init(reiser4_fs_t *fs,
					   count_t blocks,
					   alloc_init_t init)
{
	rid_t pid;
	uint32_t blksize;
	reiser4_plug_t *plug;
	reiser4_alloc_t *alloc;
	
	/* Initializing instance of block allocator */
	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;

	alloc->fs = fs;
	
	if ((pid = reiser4_format_alloc_pid(fs->format)) == INVAL_PID) {
		aal_error("Invalid block allocator plugin id has "
			  "been found.");
		goto error_free_alloc;
	}
    
	/* Finding block allocator plugin */
	if (!(plug = reiser4_factory_ifind(ALLOC_PLUG_TYPE, pid))) {
		aal_error("Can't find block allocator plugin by "
			  "its id 0x%x.", pid);
		goto error_free_alloc;
	}

	blksize = reiser4_master_get_blksize(fs->master);

	/* Initializing block allocator entity. */
	switch (init) {
	case ALLOC_OPEN:
		alloc->ent = plug_call(plug->o.alloc_ops, open, 
				       fs->device, blksize, blocks);
		break;
	case ALLOC_CREATE:
		alloc->ent = plug_call(plug->o.alloc_ops, create, 
				       fs->device, blksize, blocks);
		break;
	}

	if (!alloc->ent) {
		aal_error("Can't initialize block allocator.");
		goto error_free_alloc;
	}

	return alloc;

 error_free_alloc:
	aal_free(alloc);
	return NULL;
}

/* Initializes block allocator structures and make request to block allocator
   plugin for opening. Returns initialized instance, which may be used in all
   further operations. */
reiser4_alloc_t *reiser4_alloc_open(
	reiser4_fs_t *fs,	/* fs allocator is going to be opened on */
	count_t blocks)		/* filesystem size in blocks */
{
	aal_assert("umka-135", fs != NULL);
	aal_assert("umka-135", fs->format != NULL);

	return reiser4_alloc_init(fs, blocks, ALLOC_OPEN);
}

/* Creates block allocator instance. Initializes all structures, calles block
   allocator plugin in order to initialize allocator instance and returns
   instance to caller. */
reiser4_alloc_t *reiser4_alloc_create(
	reiser4_fs_t *fs,    /* fs block allocator is going to be created on */
	count_t blocks)	     /* filesystem size in blocks */
{
	aal_assert("umka-726", fs != NULL);
	aal_assert("umka-1694", fs->format != NULL);

	return reiser4_alloc_init(fs, blocks, ALLOC_CREATE);
}


errno_t reiser4_alloc_assign(reiser4_alloc_t *alloc,
			     reiser4_bitmap_t *bitmap)
{
	aal_assert("vpf-582", alloc != NULL);
	aal_assert("umka-1848", bitmap != NULL);

	return plug_call(alloc->ent->plug->o.alloc_ops, 
			 assign, alloc->ent, bitmap);
}

errno_t reiser4_alloc_extract(reiser4_alloc_t *alloc,
			      reiser4_bitmap_t *bitmap)
{
	aal_assert("umka-2191", alloc != NULL);
	aal_assert("umka-2192", bitmap != NULL);

	return plug_call(alloc->ent->plug->o.alloc_ops, 
			 extract, alloc->ent, bitmap);
}

/* Make request to allocator plugin in order to save its data to device */
errno_t reiser4_alloc_sync(
	reiser4_alloc_t *alloc)	/* allocator to be synced */
{
	aal_assert("umka-139", alloc != NULL);

	if (!reiser4_alloc_isdirty(alloc))
		return 0;
	
	return plug_call(alloc->ent->plug->o.alloc_ops,
			 sync, alloc->ent);
}

/* Close passed allocator instance */
void reiser4_alloc_close(
	reiser4_alloc_t *alloc)	/* allocator to be closed */
{
	aal_assert("umka-1504", alloc != NULL);

	alloc->fs->alloc = NULL;
	
	/* Calling the plugin for close its internal instance properly */
	plug_call(alloc->ent->plug->o.alloc_ops, 
		  close, alloc->ent);
    
	aal_free(alloc);
}

/* Returns the number of free blocks in allocator */
count_t reiser4_alloc_free(
	reiser4_alloc_t *alloc)	/* allocator to be realeased */
{
	aal_assert("umka-362", alloc != NULL);

	return plug_call(alloc->ent->plug->o.alloc_ops, 
			 free, alloc->ent);
}

/* Returns the number of used blocks in allocator */
count_t reiser4_alloc_used(
	reiser4_alloc_t *alloc)	/* allocator used blocks will be obtained
				   from */
{
	aal_assert("umka-499", alloc != NULL);

	return plug_call(alloc->ent->plug->o.alloc_ops, 
			 used, alloc->ent);
}

/* Marks specified blocks as used */
errno_t reiser4_alloc_occupy(
	reiser4_alloc_t *alloc,	/* allocator for working with */
	blk_t start, 		/* start block to be marked */
	count_t count)		/* count to be marked */
{
	aal_assert("umka-501", alloc != NULL);

	plug_call(alloc->ent->plug->o.alloc_ops, 
		  occupy, alloc->ent, start, count);

	if (alloc->hook.alloc)
		alloc->hook.alloc(alloc, start, count, alloc->hook.data);

	return 0;
}

/* Deallocs specified blocks */
errno_t reiser4_alloc_release(
	reiser4_alloc_t *alloc,	/* allocator for wiorking with */
	blk_t start, 		/* start block to be deallocated */
	count_t count)		/* count of blocks to be deallocated */
{
	errno_t res;
	
	aal_assert("umka-503", alloc != NULL);

	if ((res = plug_call(alloc->ent->plug->o.alloc_ops, 
			     release, alloc->ent, start, count)))
	{
		return res;
	}

	if (alloc->hook.release) {
		alloc->hook.release(alloc, start, count,
				    alloc->hook.data);
	}

	return 0;
}

/* Makes request to plugin for allocating block */
count_t reiser4_alloc_allocate(
	reiser4_alloc_t *alloc, /* allocator for working with */
	blk_t *start,           /* start block */
	count_t count)          /* requested block count */
{
	count_t blocks;
	
	aal_assert("umka-505", alloc != NULL);

	*start = 0;
	
	blocks = plug_call(alloc->ent->plug->o.alloc_ops, 
			   allocate, alloc->ent, start, count);
	
	if (blocks && alloc->hook.alloc)
		alloc->hook.alloc(alloc, *start, blocks, alloc->hook.data);
		
	return blocks;
}

errno_t reiser4_alloc_valid(
	reiser4_alloc_t *alloc)	/* allocator to be checked */
{
	aal_assert("umka-833", alloc != NULL);

	return plug_call(alloc->ent->plug->o.alloc_ops, 
			 valid, alloc->ent);
}

/* Returns TRUE if specified blocks are used. */
bool_t reiser4_alloc_occupied(
	reiser4_alloc_t *alloc,	/* allocator for working with */
	blk_t start, 		/* start block to be tested (used or not) */
	count_t count)		/* count of blocks to be tested */
{
	aal_assert("umka-662", alloc != NULL);

	return plug_call(alloc->ent->plug->o.alloc_ops, 
			 occupied, alloc->ent, start, count);
}

/* Returns TRUE if specified blocks are unused. */
bool_t reiser4_alloc_available(
	reiser4_alloc_t *alloc,	/* allocator for working with */
	blk_t start, 		/* start block to be tested (used or not) */
	count_t count)		/* count of blocks to be tested */
{
	aal_assert("umka-662", alloc != NULL);

	return plug_call(alloc->ent->plug->o.alloc_ops, 
			 available, alloc->ent, start, count);
}

errno_t reiser4_alloc_layout(reiser4_alloc_t *alloc, 
			     region_func_t region_func,
			     void *data)
{
	aal_assert("umka-1080", alloc != NULL);
	aal_assert("umka-1081", region_func != NULL);

	return plug_call(alloc->ent->plug->o.alloc_ops,
			 layout, alloc->ent, region_func,
			 data);
}

errno_t reiser4_alloc_region(reiser4_alloc_t *alloc, blk_t blk, 
			     region_func_t func, void *data)
{
	aal_assert("vpf-557", alloc != NULL);
	
	return plug_call(alloc->ent->plug->o.alloc_ops, region, 
			 alloc->ent, blk, func, data);
}

#endif
