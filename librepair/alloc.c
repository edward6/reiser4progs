/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   alloc.c -- repair block allocator code. */

#include <repair/librepair.h>

errno_t repair_alloc_layout_bad(reiser4_alloc_t *alloc, region_func_t func, 
				void *data) 
{
	aal_assert("vpf-1322", alloc != NULL);
	
	return plug_call(alloc->ent->plug->o.alloc_ops, 
			 layout_bad, alloc->ent, func, data);
}

void repair_alloc_print(reiser4_alloc_t *alloc, aal_stream_t *stream) {
	aal_assert("umka-1566", alloc != NULL);
	aal_assert("umka-1567", stream != NULL);

	plug_call(alloc->ent->plug->o.alloc_ops,
		  print, alloc->ent, stream, 0);
}

/* Fetches block allocator data to @stream. */
errno_t repair_alloc_pack(reiser4_alloc_t *alloc, aal_stream_t *stream) {
	aal_assert("umka-2614", alloc != NULL);
	aal_assert("umka-2615", stream != NULL);

	return plug_call(alloc->ent->plug->o.alloc_ops,
			 pack, alloc->ent, stream);
}

/* Loads block allocator data from @stream to alloc entity. */
reiser4_alloc_t *repair_alloc_unpack(reiser4_fs_t *fs, aal_stream_t *stream) {
	reiser4_alloc_t *alloc;
	reiser4_plug_t *plug;
	fs_desc_t desc;
	uint32_t read;
	rid_t pid;
	
	aal_assert("umka-2616", fs != NULL);
	aal_assert("umka-2617", stream != NULL);

	read = aal_stream_read(stream, &pid, sizeof(pid));
	if (read != sizeof(pid)) {
		aal_error("Can't unpack the block allocator. Stream is over?");
		return NULL;
	}
	
	/* Getting needed plugin from plugin factory by its id */
	if (!(plug = reiser4_factory_ifind(ALLOC_PLUG_TYPE, pid))) {
		aal_error("Can't find block allocator plugin "
			  "by its id 0x%x.", pid);
		return NULL;
	}
    
	/* Allocating memory for the allocator instance */
	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;

	alloc->fs = fs;
	alloc->fs->alloc = alloc;

	desc.device = fs->device;
	desc.blksize = reiser4_master_get_blksize(fs->master);
	
	/* Query the block allocator plugin for creating allocator entity */
	if (!(alloc->ent = plug_call(plug->o.alloc_ops, unpack,
				     &desc, stream)))
	{
		aal_error("Can't unpack block allocator.");
		goto error_free_alloc;
	}
	
	return alloc;
	
 error_free_alloc:
	aal_free(alloc);
	return NULL;
}

