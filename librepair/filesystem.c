/*
    librepair/filesystem.c - methods are needed mostly by fsck for work 
    with broken filesystems.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/librepair.h>

/* Opens the filesystem - master, format, block and oid allocators - without 
 * opening a journal. */
errno_t repair_fs_open(repair_data_t *repair, aal_device_t *host_device, 
    aal_device_t *journal_device, reiser4_profile_t *profile)
{
    void *oid_area_start, *oid_area_end;
    errno_t error = 0;

    aal_assert("vpf-851", repair != NULL);
    aal_assert("vpf-159", host_device != NULL);
    aal_assert("vpf-172", profile != NULL);
 
    /* Allocating memory and initializing fields */
    if (!(repair->fs = aal_calloc(sizeof(*repair->fs), 0)))
	return -ENOMEM;

    repair->fs->device = host_device;
    repair->fs->profile = profile;
    
    error |= repair_master_open(repair->fs, repair->mode);    
    if (repair_error_exists(error)) {
	aal_exception_fatal("Failed to open the master super block.");
	goto error_fs_free;
    }
    
    error |= repair_format_open(repair->fs, repair->mode);	
    if (repair_error_exists(error)) {
	aal_exception_fatal("Failed to open the format.");
	goto error_master_close;
    }
    
    error |= repair_journal_handle(repair->fs, journal_device, repair->mode);
    if (repair_error_exists(error)) {
	aal_exception_fatal("Failed to replay the journal.");
	goto error_format_close;
    }
    
    error |= repair_format_update(repair->fs->format);
    if (repair_error_exists(error)) {
	aal_exception_fatal("Failed to update the format after journal "
	    "replaying.");
	goto error_format_close;
    }
    
    /* Block and oid allocator plugins are specified by format plugin 
     * unambiguously, so there is nothing to be checked additionally here. */
    if ((repair->fs->alloc = reiser4_alloc_open(repair->fs, 
	reiser4_format_get_len(repair->fs->format))) == NULL) 
    {
	aal_exception_fatal("Failed to open a block allocator.");
	error = -EINVAL;
	goto error_format_close;
    }

    if ((repair->fs->oid = reiser4_oid_open(repair->fs)) == NULL) {	
	aal_exception_fatal("Failed to open an object id allocator.");
	error = -EINVAL;
	goto error_alloc_close;
    }
    
    return 0;

error_alloc_close:
    reiser4_alloc_close(repair->fs->alloc);
    
error_format_close:
    reiser4_format_close(repair->fs->format);
    
error_master_close:
    reiser4_master_close(repair->fs->master);

error_fs_free:
    aal_free(repair->fs);
    repair->fs = NULL;

    if (error > 0) {
	if (error & REPAIR_FATAL)
	    repair->fatal++;
	else if (error & REPAIR_FIXABLE)
	    repair->fixable++;

	error = 0;
    }
    
    return error;
}

