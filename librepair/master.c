/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/master.c - methods are needed for work with broken master 
   super block. */

#include <repair/librepair.h>

/* Checks the blocksize. */
static int callback_bs_check (int64_t val, void * data) {
	if (!aal_pow2(val))
		return 0;
    
	if (val < 512)
		return 0;

	return 1;
}

/* Checks the opened master, builds a new one on the base of user profile if no 
   one was opened. */
static errno_t repair_master_check(reiser4_fs_t *fs, uint8_t mode) {
	uint16_t blksize = 0;
	count_t dev_len;
	
	aal_assert("vpf-730", fs != NULL);
	aal_assert("vpf-161", fs->master != NULL || fs->device != NULL);
	
	if (fs->master == NULL) {
		if (mode != RM_BUILD)
			return RE_FATAL;
		
		/* Master SB was not opened. Create a new one. */
		if (aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_YESNO,
					"Master super block cannot be found. "
					"Do you want to build a new one on "
					"(%s)?", fs->device->name) == EXCEPTION_NO)
			return -EINVAL;
		
		blksize = aal_ui_get_numeric(4096, callback_bs_check, NULL,
					     "Which block size do you use?");
		
		
		dev_len = aal_device_len(fs->device) /
			(blksize / fs->device->blksize);
		
		/* Checks whether filesystem size is enough big. */
		if (dev_len < (count_t)REISER4_FS_MIN_SIZE(blksize)) {
			aal_error("The device '%s' of %llu blocks is "
				  "too small for the %u block size "
				  "fs.", fs->device->name, dev_len,
				  REISER4_FS_MIN_SIZE(blksize));
			return -EINVAL;
		}

		/* FIXME-VITALY: What should be done with uuid and label? At 
		   least not here as uuid and label seem to be on the wrong 
		   place. Move them to specific SB. */
		
		/* Create a new master SB. */
		if (!(fs->master = reiser4_master_create(fs->device, blksize)))
		{
			aal_fatal("Failed to create a new master "
				  "super block.");
			return -EINVAL;
		}

		aal_info("A new master superblock is created"
			 "on (%s).", fs->device->name);
		
		reiser4_master_set_uuid(fs->master, NULL);
		reiser4_master_set_label(fs->master, NULL);
		reiser4_master_set_format(fs->master, INVAL_PID);
	} else {
		/* Master SB was opened. Check it for validness. */
		
		/* Check the blocksize. */
		if (!aal_pow2(reiser4_master_get_blksize(fs->master))) {			
			aal_fatal("Invalid blocksize found in the "
				  "master super block (%u).",
				  reiser4_master_get_blksize(fs->master));
			
			if (mode != RM_BUILD)
				return RE_FATAL;
			
			blksize = aal_ui_get_numeric(4096, callback_bs_check,
						     NULL, "Which block size "
						     "do you use?");

			reiser4_master_set_blksize(fs->master, blksize);
			reiser4_master_mkdirty(fs->master);
		}
	}

	/* Setting actual used block size from master super block */
	if (aal_device_set_bs(fs->device, reiser4_master_get_blksize(fs->master))) {
		aal_fatal("Invalid block size was specified (%u). It "
			  "must be power of two.",
			  reiser4_master_get_blksize(fs->master));
		return -EINVAL;
	}
	
	return 0;
}

/* Opens and checks the master. */
errno_t repair_master_open(reiser4_fs_t *fs, uint8_t mode) {
	errno_t res;
	
	aal_assert("vpf-399", fs != NULL);
	aal_assert("vpf-729", fs->device != NULL);
	
	/* Try to open master. */
	fs->master = reiser4_master_open(fs->device);
	
	/* Either check the opened master or build a new one. */
	res = repair_master_check(fs, mode);
	
	if (repair_error_fatal(res))
		goto error_master_free;
	
	return res;
	
 error_master_free:
	if (fs->master) {
		reiser4_master_close(fs->master);
		fs->master = NULL;
	}
	
	return res;
}

errno_t repair_master_pack(reiser4_master_t *master, aal_stream_t *stream) {
	uint32_t size;
	
	aal_assert("umka-2608", master != NULL);
	aal_assert("umka-2609", stream != NULL);

	/* Write master size. */
	size = sizeof(master->ent);
	aal_stream_write(stream, &size, sizeof(size));

	/* Write master data to @stream. */
	aal_stream_write(stream, &master->ent, size);

	return 0;
}

reiser4_master_t *repair_master_unpack(aal_device_t *device, 
				       aal_stream_t *stream)
{
	uint32_t size;
	reiser4_master_t *master;
    
	aal_assert("umka-981", device != NULL);
	aal_assert("umka-2611", stream != NULL);

	/* Read size and check for validness. */
	aal_stream_read(stream, &size, sizeof(size));

	/* Allocating the memory for master super block struct */
	if (!(master = aal_calloc(sizeof(*master), 0)))
		return NULL;
	
	if (size != sizeof(master->ent)) {
		aal_error("Invalid size %u is detected in stream.",
			  size);
		goto error_free_master;
	}

	/* Read master data from @stream. */
	aal_stream_read(stream, &master->ent, size);

	master->dirty = TRUE;
	master->device = device;
	
	return master;
	
 error_free_master:
	aal_free(master);
	return NULL;
}

