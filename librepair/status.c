/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   status.c -- repair status block functions. */

#include <repair/repair.h>

/* Try to open status and check it. */
errno_t repair_status_open(reiser4_fs_t *fs, uint8_t mode) {
	uint32_t blksize;
	
	aal_assert("vpf-398", fs != NULL);
	
	blksize = reiser4_master_get_blksize(fs->master);

	/* Try to open the disk format. */
	if ((fs->status = reiser4_status_open(fs->device, blksize)))
		return 0;
	
	
	/* Status open failed. Create a new one. */
	if (!(fs->status = reiser4_status_create(fs->device, blksize)))
		return -EINVAL;

	if (mode == RM_CHECK) {
		fs->status->dirty = FALSE;
		blksize = reiser4_master_get_blksize(fs->master);
	}
		
	return 0;
}

errno_t repair_status_clear(reiser4_status_t *status) {
	set_ss_status(STATUS(status), 0);
	set_ss_extended(STATUS(status), 0);
	aal_memset(STATUS(status)->ss_stack, 0, sizeof(d64_t) * SS_STACK_SIZE);
	aal_memset(STATUS(status)->ss_message, 0, SS_MESSAGE_SIZE);
	
	status->dirty = TRUE;
	
	return 0;
}

