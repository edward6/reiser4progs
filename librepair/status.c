/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   status.c -- repair status block functions. */

#include <repair/repair.h>
#include <fcntl.h>

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
		return RE_FIXABLE;
	} else
		aal_exception_error("Creating a new status block.");
	
	return 0;
}

void repair_status_clear(reiser4_status_t *status) {
	aal_assert("vpf-1342", status != NULL);
	
	aal_memset(STATUS(status), 0, sizeof(reiser4_status_sb_t));
	status->dirty = TRUE;
}

void repair_status_state(reiser4_status_t *status, uint64_t state) {
	aal_assert("vpf-1341", status != NULL);
	
	/* if the same as exists, return. */
	if (state == get_ss_status(STATUS(status))) 
		return;
	
	/* if some valuable state different from existent, clear all other 
	   stuff as it becomes obsolete. */
	if (!state) {
		/* If evth is ok, clear all info as obsolete. */
		aal_memset((char *)STATUS(status) + SS_MAGIC_SIZE, 0, 
			   sizeof(reiser4_status_sb_t) - SS_MAGIC_SIZE);
	} else {
		/* Set the state w/out clearing evth. */
		set_ss_status(STATUS(status), state);
	}
	
	status->dirty = TRUE;
}
