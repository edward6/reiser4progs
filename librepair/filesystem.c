/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/filesystem.c - methods are needed mostly by fsck for work 
   with broken filesystems. */

#include <repair/librepair.h>

/* Opens the filesystem - master, format, block and oid allocators - without 
   opening a journal. */
errno_t repair_fs_open(repair_data_t *repair, 
		       aal_device_t *host_device,
		       aal_device_t *journal_device)
{
	uint64_t len;
	errno_t res;

	aal_assert("vpf-851", repair != NULL);
	aal_assert("vpf-159", host_device != NULL);
 
	/* Allocating memory and initializing fields */
	if (!(repair->fs = aal_calloc(sizeof(*repair->fs), 0)))
		return -ENOMEM;

	repair->fs->device = host_device;
    
	if ((res = repair_master_open(repair->fs, repair->mode))) {
		aal_exception_fatal("Failed to open the master super block.");
		goto error_fs_free;
	}
    
	if ((res = repair_format_open(repair->fs, repair->mode))) {
		aal_exception_fatal("Failed to open the format.");
		goto error_master_close;
	}
    
	if ((res = repair_journal_open(repair->fs, journal_device, 
				       repair->mode))) 
	{
		aal_exception_fatal("Failed to open the journal.");
		goto error_format_close;
	}

	if ((res = repair_journal_replay(repair->fs->journal, 
					 repair->fs->device))) 
	{			
		aal_exception_fatal("Failed to replay the journal.");
		goto error_journal_close;
	}
    
	if ((res = repair_format_update(repair->fs->format))) {
		aal_exception_fatal("Failed to update the format after journal "
				    "replaying.");
		goto error_journal_close;
	}
	
	len = reiser4_format_get_len(repair->fs->format);
    
	/* Block and oid allocator plugins are specified by format plugin 
	 * unambiguously, so there is nothing to be checked here anymore. */
	if (!(repair->fs->alloc = reiser4_alloc_open(repair->fs, len))) {
		aal_exception_fatal("Failed to open a block allocator.");
		res = -EINVAL;
		goto error_journal_close;
	}
	
	if ((res = repair_alloc_check_struct(repair->fs->alloc, repair->mode)))
		goto error_alloc_close;
	
	if ((repair->fs->oid = reiser4_oid_open(repair->fs)) == NULL) {	
		aal_exception_fatal("Failed to open an object id allocator.");
		res = -EINVAL;
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

	if (res > 0) {
		if (res & RE_FATAL)
			repair->fatal++;
		else if (res & RE_FIXABLE)
			repair->fixable++;

		res = 0;
	}
    
	return res;
}

/* Close the journal and the filesystem. */
void repair_fs_close(reiser4_fs_t *fs) {
	aal_assert("vpf-909", fs != NULL);
	aal_assert("vpf-910", fs->journal != NULL);
	
	reiser4_journal_close(fs->journal);
	reiser4_fs_close(fs);    
}
