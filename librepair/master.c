/* Copyright 2001-2005 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/master.c - methods are needed for work with broken master 
   super block. */

#include <repair/librepair.h>

/* Checks the blocksize. */
static int cb_bs_check (int64_t val, void * data) {
	if (val < REISER4_MIN_BLKSIZE)
		return 0;

	if (val > REISER4_MAX_BLKSIZE)
		return 0;
	
	if (!aal_pow2(val))
		return 0;

	return 1;
}

/* Checks the opened master, builds a new one on the base of user profile if no 
   one was opened. */
errno_t repair_master_check_struct(reiser4_fs_t *fs, 
				   uint8_t mode, 
				   uint32_t options) 
{
	reiser4_master_sb_t *ms;
	reiser4_plug_t *format;
	fs_hint_t hint;
	uint16_t size;
	int new = 0;
	rid_t pid;
	int over;
	char *s;
	
	aal_assert("vpf-730", fs != NULL);
	aal_assert("vpf-161", fs->master != NULL || fs->device != NULL);
	
	over = reiser4_profile_overridden(PROF_FORMAT);
	format = reiser4_profile_plug(PROF_FORMAT);

	if (fs->backup) {
		backup_hint_t *bk_hint = &fs->backup->hint;
		
		ms = (reiser4_master_sb_t *)
			(bk_hint->block.data + bk_hint->off[BK_MASTER]);
	} else {
		ms = NULL;
	}
	
	if (fs->master == NULL) {
		if (mode != RM_BUILD)
			return RE_FATAL;
		
		if (ms) {
			fsck_mess("Master super block cannot be found on '%s'.",
				 fs->device->name);

			if (get_ms_mirror_id(ms)) {
				fsck_mess("Refuse to restore master of replica. "
					  "Fsck works only with original subvolumes.");
				return -EINVAL;
			}
			size = get_ms_blksize(ms);
		} else {
			/* Master SB was not opened. Create a new one. */
			size = 4096;
			
			if (!(options & (1 << REPAIR_YES))) {
				int opt;
			
				opt = aal_yesno("Master super block cannot be "
						"found on (%s). Do you want to "
						"build a new one?", 
						fs->device->name);
				
				if (opt == EXCEPTION_OPT_NO)
					return -EINVAL;

				size = aal_ui_get_numeric(size, cb_bs_check, 
							  NULL, "Which block "
							  "size do you use?");
			} 
		}
		
		/* Create a new master SB. */
		aal_memset(&hint, 0, sizeof(hint));
		hint.blksize = size;
		
		if (!(fs->master = reiser4_master_create(fs->device, &hint))) {
			aal_fatal("Failed to create a new master super block.");
			return -EINVAL;
		}

		aal_warn("A new master superblock is %s on '%s'.", 
			 ms ? "regenerated from backup" : "created", 
			 fs->device->name);
		
		reiser4_master_set_volume_uuid(fs->master,
					       ms ? ms->ms_vol_uuid : NULL);

		reiser4_master_set_subvol_uuid(fs->master,
					       ms ? ms->ms_sub_uuid : NULL);

		reiser4_master_set_stripe_bits(fs->master,
					       ms ? ms->ms_stripe_bits : 0);

		reiser4_master_set_mirror_id(fs->master,
					     ms ? ms->ms_mirror_id : 0);

		reiser4_master_set_num_replicas(fs->master,
						ms ? ms->ms_num_replicas : 0);

		reiser4_master_set_label(fs->master, ms ?
					 ms->ms_label : NULL);

		pid = ms ? get_ms_format(ms) : INVAL_PID;
		reiser4_master_set_format(fs->master, pid);
		new = 1;
	} else if (ms) {
		/*
		 * Master SB & backup are opened. Fix accoring to backup.
		 */
		if (reiser4_master_is_replica(fs->master)) {
			fsck_mess("Refuse to check/repar replica. "
				  "Fsck works only with original subvolumes.");
			return -EINVAL;
		}
		size = reiser4_master_get_blksize(fs->master);
		
		if (size != get_ms_blksize(ms)) {
			fsck_mess("Blocksize (%u) found in the master "
				  "super block does not match the one "
				  "found in the backup (%u).%s", size,
				  get_ms_blksize(ms), mode == RM_BUILD ?
				  " Fixed." : "");

			if (mode != RM_BUILD)
				return RE_FATAL;

			size = get_ms_blksize(ms);
			reiser4_master_set_blksize(fs->master, size);
		}

		if (!over) {
			pid = reiser4_master_get_format(fs->master);

			if (pid != get_ms_format(ms)) {
				/* The @plug is the correct one. */
				fsck_mess("The reiser4 format plugin id (%u) "
					  "found in the master super block on "
					  "'%s' does not match the one from "
					  "the backup (%u).%s.", pid, 
					  fs->device->name, 
					  get_ms_format(ms),
					  mode == RM_BUILD ? 
					  " Fixed." : "");

				if (mode != RM_BUILD)
					return RE_FATAL;

				pid = get_ms_format(ms);
				reiser4_master_set_format(fs->master, pid);
			}
		}
		
		s = reiser4_master_get_volume_uuid(fs->master);
		if (aal_strncmp(s, ms->ms_vol_uuid, sizeof(ms->ms_vol_uuid))) {
			uint64_t *x = (uint64_t *)s;
			uint64_t *y = (uint64_t *)ms->ms_vol_uuid;
			fsck_mess("UUID (0x%llx%llx) found in the master super "
				  "block does not match the one found in the "
				  "backup (0x%llx%llx).%s",
				  x[0],
				  x[1],
				  y[0],
				  y[1],
				  mode != RM_CHECK ? " Fixed." : "");

			if (mode == RM_CHECK)
				return RE_FIXABLE;

			reiser4_master_set_volume_uuid(fs->master,
						       ms->ms_vol_uuid);
		}
		
		s = reiser4_master_get_label(fs->master);
		if (aal_strncmp(s, ms->ms_label, sizeof(ms->ms_label)))
		{
			fsck_mess("LABEL (%s) found in the master super block "
				  "does not match the one found in the backup "
				  "(%s).%s", s, ms->ms_label, 
				  mode != RM_CHECK ? " Fixed." : "");

			if (mode == RM_CHECK)
				return RE_FIXABLE;

			reiser4_master_set_label(fs->master, ms->ms_label);
		}
	} else {
		/*
		 * Master super-block was opened. Check it for validness.
		 */
		if (reiser4_master_is_replica(fs->master)) {
			fsck_mess("Refuse to check/repar replica. "
				  "Fsck works only with original subvolumes.");
			return -EINVAL;
		}
		/* Check the blocksize. */
		size = reiser4_master_get_blksize(fs->master);

		if (!cb_bs_check(size, NULL)) {
			fsck_mess("Invalid blocksize found in the "
				  "master super block (%u).",
				  reiser4_master_get_blksize(fs->master));
			
			if (mode != RM_BUILD)
				return RE_FATAL;
			
			size = 4096;
			
			if (!(options & (1 << REPAIR_YES))) {
				size = aal_ui_get_numeric(size, cb_bs_check, 
							  NULL, "Which block "
							  "size do you  use?");
			}
			
			reiser4_master_set_blksize(fs->master, size);
		}
	}

	/* Setting actual used block size from master super block */
	size = reiser4_master_get_blksize(fs->master);
	if (aal_device_set_bs(fs->device, size)) {
		fsck_mess("Invalid block size was specified (%u). "
			  "It must be power of two.", size);
		return -EINVAL;
	}
	
	pid = reiser4_master_get_format(fs->master);
	
	/* If the format is overridden, fix master according to the profile. */
	if (over && pid != format->id.id) {
		if (!new || ms) {
			/* Do not swear if the master has been just created. */
			fsck_mess("The specified disk format on '%s' is '%s'. "
				  "Its id (0x%x) does not match the on-disk id "
				  "(0x%x).%s", fs->device->name, format->label,
				  format->id.id, pid, mode == RM_BUILD ? 
				  " Fixed." :" Has effect in BUILD mode only.");
		}

		if (mode != RM_BUILD)
			return RE_FATAL;

		pid = format->id.id;
		reiser4_master_set_format(fs->master, pid);
	}

	/* If format is opened but the format plugin id has been changed, 
	   close the format. */
	if (fs->format && pid != fs->format->ent->plug->p.id.id) {
		reiser4_format_close(fs->format);
		fs->format = NULL;
	}

	if (!over && !ms && !fs->format && mode == RM_BUILD) {
		/* If there is no backup and format plug id is not overridden
		   in the profile, format plug id has not been changed in the 
		   master! 
		   
		   In the BUILD mode: a new master has been just created or a 
		   master was opened but the format was not. For both cases --
		   ask for the format plugin to be used, otherwise, leave it as
		   is. 
		   
		   WARNING: the default format plugin is used while there is 
		   the only format plugin. */
		
		if (pid != format->id.id) {
			if (!new) {
				fsck_mess("The on-disk format plugin id 0x%x "
					  "is not correct. Using the default "
					  "one 0x%x('%s').", pid, format->id.id,
					  format->label);
			}
			
			reiser4_master_set_format(fs->master, format->id.id);
		}
	}

	return 0;
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
		aal_error("Invalid size %u is detected in stream.", size);
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
	reiser4_plug_t *plug;
	uint32_t blksize;
	rid_t pid;
	
	aal_assert("umka-1568", master != NULL);
	aal_assert("umka-1569", stream != NULL);

	blksize = get_ms_blksize(SUPER(master));
	pid = reiser4_master_get_format(master);
	
	if (!(plug = reiser4_factory_ifind(FORMAT_PLUG_TYPE, pid)))
		aal_error("Can't find format plugin by its id 0x%x.", pid);
	
	aal_stream_format(stream, "Master super block (%lu):\n",
			  REISER4_MASTER_BLOCKNR(blksize));
	
	aal_stream_format(stream, "magic:\t\t%s\n",
			  reiser4_master_get_magic(master));
	
	aal_stream_format(stream, "blksize:\t%u\n",
			  get_ms_blksize(SUPER(master)));

	aal_stream_format(stream, "format:\t\t0x%x (%s)\n",
			  pid, plug ? plug->label : "absent");

	aal_stream_format(stream, "stripe bits:\t%u\n",
			  get_ms_stripe_bits(SUPER(master)));

	aal_stream_format(stream, "mirror id:\t%u\n",
			  get_ms_mirror_id(SUPER(master)));

	aal_stream_format(stream, "replicas:\t%u\n",
			  get_ms_num_replicas(SUPER(master)));

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	if (*master->ent.ms_vol_uuid != '\0') {
		char uuid[37];

		uuid[36] = '\0';
		unparse(reiser4_master_get_volume_uuid(master), uuid);
		aal_stream_format(stream, "volume uuid:\t%s\n", uuid);
	} else {
		aal_stream_format(stream, "volume uuid:\t<none>\n");
	}
	if (*master->ent.ms_sub_uuid != '\0') {
		char uuid[37];
		
		uuid[36] = '\0';
		unparse(reiser4_master_get_subvol_uuid(master), uuid);
		aal_stream_format(stream, "subvol uuid:\t%s\n", uuid);
	} else {
		aal_stream_format(stream, "subvol uuid:\t<none>\n");
	}
#endif
	
	if (*master->ent.ms_label != '\0') {
		aal_stream_format(stream, "label:\t\t%.16s\n",
				  reiser4_master_get_label(master));
	} else {
		aal_stream_format(stream, "label:\t\t<none>\n");
	}
}

errno_t repair_master_check_backup(backup_hint_t *hint) {
	reiser4_master_sb_t *master;
	
	aal_assert("vpf-1731", hint != NULL);

	master = (reiser4_master_sb_t *)
		(hint->block.data + hint->off[BK_MASTER]);
	
	/* Check the MAGIC. */
	if (aal_strncmp(master->ms_magic, REISER4_MASTER_MAGIC,
			sizeof(REISER4_MASTER_MAGIC)))
	{
		return RE_FATAL;
	}

	/* Check the blocksize. */
	if (get_ms_blksize(master) != hint->block.size)
		return RE_FATAL;
	
	hint->off[BK_MASTER + 1] = hint->off[BK_MASTER] + 
		sizeof(reiser4_master_sb_t) + 8 /* reserved */;
	
	return 0;
}
