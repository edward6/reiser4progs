/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/filesystem.c - methods are needed mostly by fsck for work 
   with broken filesystems. */

#include <repair/librepair.h>

/* Opens the filesystem - master, format, block and oid allocators - without 
   opening a journal. */
errno_t repair_fs_open(repair_data_t *repair, 
		       aal_device_t *host_device,
		       aal_device_t *journal_device, 
		       reiser4_profile_t *profile)
{
	void *oid_area_start, *oid_area_end;
	errno_t error = REPAIR_OK;

	aal_assert("vpf-851", repair != NULL);
	aal_assert("vpf-159", host_device != NULL);
	aal_assert("vpf-172", profile != NULL);
 
	/* Allocating memory and initializing fields */
	if (!(repair->fs = aal_calloc(sizeof(*repair->fs), 0)))
		return -ENOMEM;

	repair->fs->device = host_device;
	repair->fs->profile = profile;
    
	if ((error = repair_master_open(repair->fs, repair->mode))) {
		aal_exception_fatal("Failed to open the master super block.");
		goto error_fs_free;
	}
    
	if ((error = repair_format_open(repair->fs, repair->mode))) {
		aal_exception_fatal("Failed to open the format.");
		goto error_master_close;
	}
    
	if ((error = repair_journal_open(repair->fs, journal_device, 
					 repair->mode))) 
	{
		aal_exception_fatal("Failed to open the journal.");
		goto error_format_close;
	}

	if ((error = repair_journal_replay(repair->fs->journal, 
					   repair->fs->device))) 
	{			
		aal_exception_fatal("Failed to replay the journal.");
		goto error_journal_close;
	}
    
	if ((error = repair_format_update(repair->fs->format))) {
		aal_exception_fatal("Failed to update the format after journal "
				    "replaying.");
		goto error_journal_close;
	}
    
	/* Block and oid allocator plugins are specified by format plugin 
	 * unambiguously, so there is nothing to be checked here anymore. */
	if ((repair->fs->alloc = reiser4_alloc_open(repair->fs, 
			       reiser4_format_get_len(repair->fs->format))) 
	    == NULL) 
	{
		aal_exception_fatal("Failed to open a block allocator.");
		error = -EINVAL;
		goto error_journal_close;
	}
	
	error = repair_alloc_check_struct(repair->fs->alloc, repair->mode);
	if (error)
		goto error_alloc_close;
	
	if ((repair->fs->oid = reiser4_oid_open(repair->fs)) == NULL) {	
		aal_exception_fatal("Failed to open an object id allocator.");
		error = -EINVAL;
		goto error_alloc_close;
	}
    
	return 0;

 error_alloc_close:
	reiser4_alloc_close(repair->fs->alloc);
	repair->fs->alloc = NULL;
    
 error_journal_close:
	reiser4_journal_close(repair->fs->journal);
	repair->fs->journal = NULL;
    
 error_format_close:
	reiser4_format_close(repair->fs->format);
	repair->fs->format = NULL;
    
 error_master_close:
	reiser4_master_close(repair->fs->master);
	repair->fs->master = NULL;

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

/* Close the journal and the filesystem. */
void repair_fs_close(reiser4_fs_t *fs) {
	aal_assert("vpf-909", fs != NULL);
	aal_assert("vpf-910", fs->journal != NULL);
	
	reiser4_journal_close(fs->journal);
	reiser4_fs_close(fs);    
}
