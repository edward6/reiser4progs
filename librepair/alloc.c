/* Copyright 2001-2005 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   alloc.c -- repair block allocator code. */

#include <repair/librepair.h>

errno_t repair_alloc_check_struct(reiser4_alloc_t *alloc, uint8_t mode) {
	aal_assert("vpf-1659", alloc != NULL);

	return reiser4call(alloc, check_struct, mode);
}

errno_t repair_alloc_layout_bad(reiser4_alloc_t *alloc, 
				region_func_t func, void *data)
{
	aal_assert("vpf-1322", alloc != NULL);
	
	return reiser4call(alloc, layout_bad, func, data);
}

void repair_alloc_print(reiser4_alloc_t *alloc, aal_stream_t *stream) {
	aal_assert("umka-1566", alloc != NULL);
	aal_assert("umka-1567", stream != NULL);

	reiser4call(alloc, print, stream, 0);
}

/* Fetches block allocator data to @stream. */
errno_t repair_alloc_pack(reiser4_alloc_t *alloc, aal_stream_t *stream) {
	rid_t pid;
	
	aal_assert("umka-2614", alloc != NULL);
	aal_assert("umka-2615", stream != NULL);

	pid = alloc->ent->plug->p.id.id;
	aal_stream_write(stream, &pid, sizeof(pid));

	return reiser4call(alloc, pack, stream);
}

/* Loads block allocator data from @stream to alloc entity. */
reiser4_alloc_t *repair_alloc_unpack(reiser4_fs_t *fs, aal_stream_t *stream) {
	reiser4_alloc_t *alloc;
	reiser4_plug_t *plug;
	uint32_t blksize;
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
	blksize = reiser4_master_get_blksize(fs->master);
	
	/* Query the block allocator plugin for creating allocator entity */
	if (!(alloc->ent = plugcall((reiser4_alloc_plug_t *)plug, unpack,
				    fs->device, blksize, stream)))
	{
		aal_error("Can't unpack block allocator.");
		goto error_free_alloc;
	}
	
	return alloc;
	
 error_free_alloc:
	aal_free(alloc);
	return NULL;
}

errno_t repair_alloc_open(reiser4_fs_t *fs, uint8_t mode) {
	uint64_t len;

	len = reiser4_format_get_len(fs->format);
	
	if (!(fs->alloc = reiser4_alloc_open(fs, len)))
		return -EINVAL;

	return repair_alloc_check_struct(fs->alloc, mode);
}
