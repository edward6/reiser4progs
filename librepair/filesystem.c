/*
    librepair/filesystem.c - methods are needed mostly by fsck for work 
    with broken filesystems.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/librepair.h>

/* Opens the filesystem - master, format, block and oid allocators - without 
 * opening a journal. */
reiser4_fs_t *repair_fs_open(aal_device_t *host_device, 
    reiser4_profile_t *profile) 
{
    reiser4_fs_t *fs;
    void *oid_area_start, *oid_area_end;

    aal_assert("vpf-159", host_device != NULL);
    aal_assert("vpf-172", profile != NULL);
 
    /* Allocating memory and initializing fields */
    if (!(fs = aal_calloc(sizeof(*fs), 0)))
	return NULL;

    fs->device = host_device;
    fs->profile = profile;
    
    if (repair_master_open(fs))
	goto error_fs_free;
	
    if (repair_format_open(fs))
	goto error_master_close;
    
    /* Block and oid allocator plugins are specified by format plugin unambiguously, 
     * so there is nothing to be checked additionally here. */
    if ((fs->alloc = reiser4_alloc_open(fs, 
	reiser4_format_get_len(fs->format))) == NULL) 
    {
	aal_exception_fatal("Failed to open a block allocator.");
	goto error_format_close;
    }

    if ((fs->oid = reiser4_oid_open(fs)) == NULL) {	
	aal_exception_fatal("Failed to open an object id allocator.");
	goto error_alloc_close;
    }
    
    return fs;

error_alloc_close:
    reiser4_alloc_close(fs->alloc);
    
error_format_close:
    reiser4_format_close(fs->format);
    
error_master_close:
    reiser4_master_close(fs->master);

error_fs_free:
    aal_free(fs);

    return NULL;
}

/* Sync the filesystem - master, format, block and oid allocators. */
errno_t repair_fs_sync(reiser4_fs_t *fs) {
    errno_t res;
    
    aal_assert("vpf-173", fs != NULL);
    
    /* Synchronizing block allocator */
    if ((res = reiser4_alloc_sync(fs->alloc)))
	return res;
    
    /* Synchronizing the object allocator */
    if ((res = reiser4_oid_sync(fs->oid)))
	return res;

    /* Synchronizing the disk format */
    if ((res = reiser4_format_sync(fs->format)))
	return res;

    /* Synchronizing the master */
    if (reiser4_master_confirm(fs->device)) {
	if ((res = reiser4_master_sync(fs->master)))
	    return res;
    }
    
    return 0;
}

/* 
    Closes all filesystem's entities. Calls plugins' "done" routine for every 
    plugin and frees all assosiated memory. 
*/
void repair_fs_close(reiser4_fs_t *fs) {
    aal_assert("vpf-174", fs != NULL);

    reiser4_oid_close(fs->oid);

    reiser4_alloc_close(fs->alloc);
    reiser4_format_close(fs->format);
    reiser4_master_close(fs->master);

    /* Freeing memory occupied by fs instance */
    aal_free(fs);
}

/* Enumerates all filesystem areas (block alloc, journal, etc.) */
errno_t repair_fs_layout(reiser4_fs_t *fs, block_func_t func, void *data) {
    errno_t res;
    
    if ((res = reiser4_format_skipped(fs->format, func, data)))
	return res;
	
    if ((res = reiser4_format_layout(fs->format, func, data)))
	return res;
    
    return reiser4_alloc_layout(fs->alloc, func, data);
}
