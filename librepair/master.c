/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by 
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

	if (val > REISER4_MAX_BLKSIZE)
		return 0;

	return 1;
}

/* Checks the opened master, builds a new one on the base of user profile if no 
   one was opened. */
static errno_t repair_master_check(reiser4_fs_t *fs, uint8_t mode) {
	reiser4_plug_t *plug;
	uint16_t blksize;
	rid_t pid;
	
	aal_assert("vpf-730", fs != NULL);
	aal_assert("vpf-161", fs->master != NULL || fs->device != NULL);
	
	if (fs->master == NULL) {
		if (mode != RM_BUILD)
			return RE_FATAL;
		
		/* Master SB was not opened. Create a new one. */
		if (aal_yesno("Master super block cannot be found. Do"
			      " you want to build a new one on (%s)?",
			      fs->device->name) == EXCEPTION_OPT_NO)
		{
			return -EINVAL;
		}
		
		blksize = aal_ui_get_numeric(4096, callback_bs_check, NULL,
					     "Which block size do you use?");
		
		/* Create a new master SB. */
		if (!(fs->master = reiser4_master_create(fs->device, blksize)))
		{
			aal_error("Failed to create a new master super block.");
			return -EINVAL;
		}

		aal_warn("A new master superblock is created on (%s).", 
			 fs->device->name);
		
		reiser4_master_set_uuid(fs->master, NULL);
		reiser4_master_set_label(fs->master, NULL);
		reiser4_master_set_format(fs->master, INVAL_PID);
	} else {
		/* Master SB was opened. Check it for validness. */
		
		/* Check the blocksize. */
		if (!callback_bs_check(reiser4_master_get_blksize(fs->master), 
				       NULL))
		{
			aal_error("Invalid blocksize found in the "
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
		aal_error("Invalid block size was specified (%u). It "
			  "must be power of two.",
			  reiser4_master_get_blksize(fs->master));
		return -EINVAL;
	}
	
	plug = reiser4_profile_plug(PROF_FORMAT);
	pid = reiser4_master_get_format(fs->master);
	
	/* If the format is overridden, fix master accordingly to the specified 
	   value. */ 
	if (reiser4_profile_overridden(PROF_FORMAT) && pid != plug->id.id) {
		/* The @plug is the correct one. */
		aal_error("The specified reiser4 format on '%s' is '%s'. Its "
			  "id (0x%x) does not match the on-disk id (0x%x).%s", 
			  fs->device->name, plug->label, plug->id.id, pid, 
			  mode == RM_BUILD ? " Fixed." : " Has effect in BUILD "
			  "mode only.");

		if (mode != RM_BUILD)
			return RE_FATAL;

		reiser4_master_set_format(fs->master, plug->id.id);
		reiser4_master_mkdirty(fs->master);
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
	reiser4_master_t *master;
	uint32_t size;
    
	aal_assert("umka-981", device != NULL);
	aal_assert("umka-2611", stream != NULL);

	/* Read size and check for validness. */
	if (aal_stream_read(stream, &size, sizeof(size)) != sizeof(size)) {
		aal_error("Can't unpack master super block. Stream is over?");
		return NULL;
	}

	/* Allocating the memory for master super block struct */
	if (!(master = aal_calloc(sizeof(*master), 0)))
		return NULL;
	
	if (size != sizeof(master->ent)) {
		aal_error("Invalid size %u is detected in stream.",
			  size);
		goto error_free_master;
	}

	/* Read master data from @stream. */
	if (aal_stream_read(stream, &master->ent, size) != (int32_t)size) {
		aal_error("Can't unpack master super block. Stream is over?");
		goto error_free_master;
	}

	master->dirty = 1;
	master->device = device;
	
	return master;
	
 error_free_master:
	aal_free(master);
	return NULL;
}

void repair_master_print(reiser4_master_t *master,
			 aal_stream_t *stream,
			 uuid_unparse_t unparse)
{
	rid_t format_pid;
	uint32_t blksize;
	reiser4_plug_t *format_plug;
	
	aal_assert("umka-1568", master != NULL);
	aal_assert("umka-1569", stream != NULL);

	blksize = get_ms_blksize(SUPER(master));
	format_pid = reiser4_master_get_format(master);
	
	if (!(format_plug = reiser4_factory_ifind(FORMAT_PLUG_TYPE,
						  format_pid)))
	{
		aal_error("Can't find format plugin "
			  "by its id 0x%x.", format_pid);
	}
	
	aal_stream_format(stream, "Master super block (%lu):\n",
			  REISER4_MASTER_OFFSET / blksize);
	
	aal_stream_format(stream, "magic:\t\t%s\n",
			  reiser4_master_get_magic(master));
	
	aal_stream_format(stream, "blksize:\t%u\n",
			  get_ms_blksize(SUPER(master)));

	aal_stream_format(stream, "format:\t\t0x%x (%s)\n",
			  format_pid, format_plug ?
			  format_plug->label : "absent");

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	if (*master->ent.ms_uuid != '\0') {
		char uuid[37];
		
		uuid[36] = '\0';
		unparse(reiser4_master_get_uuid(master), uuid);
		aal_stream_format(stream, "uuid:\t\t%s\n", uuid);
	} else {
		aal_stream_format(stream, "uuid:\t\t<none>\n");
	}
#endif
	
	if (*master->ent.ms_label != '\0') {
		aal_stream_format(stream, "label:\t\t%s\n",
				  reiser4_master_get_label(master));
	} else {
		aal_stream_format(stream, "label:\t\t<none>\n");
	}
}

