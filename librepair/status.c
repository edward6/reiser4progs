/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
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
		fs->status->dirty = 0;
		return RE_FIXABLE;
	} else
		aal_error("Creating a new status block.");
	
	return 0;
}

void repair_status_clear(reiser4_status_t *status) {
	aal_assert("vpf-1342", status != NULL);
	
	aal_memset(STATUS(status), 0, sizeof(reiser4_status_sb_t));
	status->dirty = 1;
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
	
	status->dirty = 1;
}

errno_t repair_status_pack(reiser4_status_t *status,
			   aal_stream_t *stream)
{
	uint32_t size;
	
	aal_assert("umka-2612", status != NULL);
	aal_assert("umka-2613", stream != NULL);

	/* Write data size. */
	size = sizeof(status->ent);
	aal_stream_write(stream, &size, sizeof(size));

	/* Write status data to @stream. */
	aal_stream_write(stream, &status->ent, size);

	return 0;
}

reiser4_status_t *repair_status_unpack(aal_device_t *device,
				       uint32_t blksize,
				       aal_stream_t *stream)
{
	uint32_t size;
	reiser4_status_t *status;
    
	aal_assert("umka-981", device != NULL);
	aal_assert("umka-2611", stream != NULL);

	/* Read size and check for validness. */
	if (aal_stream_read(stream, &size, sizeof(size)) != sizeof(size)) {
		aal_error("Can't unpack the status block. Stream is over?");
		return NULL;
	}

	/* Allocating the memory for status super block struct */
	if (!(status = aal_calloc(sizeof(*status), 0)))
		return NULL;
	
	if (size != sizeof(status->ent)) {
		aal_error("Invalid size %u is detected in stream.", size);
		goto error_free_status;
	}

	/* Read status data from @stream. */
	if (aal_stream_read(stream, &status->ent, size) != (int32_t)size) {
		aal_error("Can't unpack the status block. Stream is over?");
		goto error_free_status;
	}

	status->dirty = 1;
	status->device = device;
	status->blksize = blksize;
	
	return status;
	
 error_free_status:
	aal_free(status);
	return NULL;
}

void repair_status_print(reiser4_status_t *status, aal_stream_t *stream) {
	uint64_t state, extended;
	int i;
	
	aal_assert("umka-2493", status != NULL);
	aal_assert("umka-2494", stream != NULL);

	aal_stream_format(stream, "FS status block (%lu):\n", 
			  REISER4_STATUS_BLOCK);

	state = get_ss_status(STATUS(status));
	extended = get_ss_extended(STATUS(status));

	if (!state) {
		aal_stream_format(stream, "FS marked consistent\n");
		return;
	}
	
	if (state & FS_CORRUPTED) {
		aal_stream_format(stream, "FS marked corruped\n");
		state &= ~FS_CORRUPTED;
	}

	if (state & (1 << FS_DAMAGED)) {
		aal_stream_format(stream, "FS marked damaged\n");
		state &= ~FS_DAMAGED;
	}

	if (state & FS_DESTROYED) {
		aal_stream_format(stream, "FS marked destroyed\n");
		state &= ~FS_DESTROYED;
	}

	if (state & FS_IO) {
		aal_stream_format(stream, "FS marked having io "
				  "problems\n");
		state &= ~FS_IO;
	}

	if (state) {
		aal_stream_format(stream, "Some unknown status "
				  "flags found: %0xllx\n", state);
	}

	if (extended) {
		aal_stream_format(stream, "Extended status: %0xllx\n",
				  get_ss_extended(STATUS(status)));
	}
	
	if (*status->ent.ss_message != '\0') {
		aal_stream_format(stream, "Status message:\t%s\n",
				  STATUS(status)->ss_message);
	} 
	
	if (!STATUS(status)->ss_stack[0])
		return;
	
	aal_stream_format(stream, "Status backtrace:\n");
	
	for (i = 0; i < SS_STACK_SIZE; i++) {
		if (!ss_stack(STATUS(status), i)) {
			aal_stream_format(stream, "\t%d: 0xllx\n", i, 
					  STATUS(status)->ss_stack[i]);
		}
	}
}
