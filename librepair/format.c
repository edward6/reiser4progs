/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   libprogs/format.c - methods are needed for handle the fs format. */

#include <repair/librepair.h>

static int cb_check_plugname(char *name, void *data) {
	reiser4_plug_t **plug = (reiser4_plug_t **)data;
	
	if (!name) return 0;
	
	*plug = reiser4_factory_nfind(name);
	if ((*plug)->id.type != KEY_PLUG_TYPE)
		*plug = NULL;
	
	return *plug ? 1 : 0;
}

static int cb_check_count(int64_t val, void *data) {
	reiser4_fs_t *fs = (reiser4_fs_t *)data;
	uint32_t blksize;
	
	if (val < 0) 
		return 0;
	
	blksize = reiser4_master_get_blksize(fs->master);
	return reiser4_fs_check_len(fs->device, blksize, val) ? 0 : 1;
}

/* Try to open format if not yet and check it. */
errno_t repair_format_check_struct(reiser4_fs_t *fs, uint8_t mode) {
	generic_entity_t *fent;
	reiser4_plug_t *plug; 
	format_hint_t hint;
	count_t blocks;
	bool_t over;
	errno_t res;
	rid_t pid;
	
	aal_assert("vpf-165", fs != NULL);
	aal_assert("vpf-171", fs->device != NULL);
	aal_assert("vpf-834", fs->master != NULL);
	
	pid = reiser4_master_get_format(fs->master);

	/* If format is not opened but the master has been changed, try
	 to open format again -- probably the master format plug id has 
	 been changed. */
	if (!fs->format && pid < FORMAT_LAST_ID && 
	    reiser4_master_isdirty(fs->master))
	{
		fs->format = reiser4_format_open(fs);
	}

	/* If format is still not opened, return the error not in 
	   the BUILD mode. */
	if (!fs->format && mode != RM_BUILD)
		return RE_FATAL;
	
	/* Prepare the format hint for the futher checks. */
	aal_memset(&hint, 0, sizeof(hint));

	/* If the policy/key/etc plugin is overridden in the profile or there 
	   is no opened backup not format, policy is taken from the profile. 
	   Otherwise from the backup if opened or from the format. */
	over = reiser4_profile_overridden(PROF_POLICY);
	plug = reiser4_profile_plug(PROF_POLICY);
	hint.policy = plug->id.id;
	hint.mask |= over ? (1 << PM_POLICY) : 0;

	over = reiser4_profile_overridden(PROF_KEY);
	plug = reiser4_profile_plug(PROF_KEY);
	
	if (over) {
		hint.mask |= (1 << PM_KEY);
	} else if (!fs->backup && mode == RM_BUILD) {
		char buff[256];

		plug = reiser4_profile_plug(PROF_KEY);
		aal_memset(buff, 0, sizeof(buff));
		aal_memcpy(buff, plug->label, 
			   aal_strlen(plug->label));

		aal_ui_get_alpha(buff, cb_check_plugname, &plug, 
				 "Enter the key plugin name");
		hint.mask |= (1 << PM_KEY);
	}
	hint.key = plug->id.id;
	
	hint.blksize = reiser4_master_get_blksize(fs->master);
	hint.blocks = aal_device_len(fs->device) / 
		(hint.blksize / fs->device->blksize);

	/* Check the block count if the backup is not opened. */
	if (!fs->backup) {
		if (fs->format) {
			blocks = reiser4_format_get_len(fs->format);

			if (reiser4_fs_check_len(fs->device, 
						 hint.blksize, blocks))
			{
				/* FS length is not valid. */
				if (mode != RM_BUILD)
					return RE_FATAL;

				blocks = MAX_UINT64;
			} else if (blocks != hint.blocks) {
				aal_warn("Number of blocks found in the super "
					 "block (%llu) is not equal to the size"
					 " of the partition (%llu).%s", blocks,
					 hint.blocks, mode != RM_BUILD ? 
					 " Assuming this is correct.": "");
			}
		} else {
			blocks = MAX_UINT64;
		}

		if (blocks != hint.blocks && mode == RM_BUILD) {
			/* Confirm that size is correct. */
			blocks = aal_ui_get_numeric(blocks == MAX_UINT64 ? 
						    hint.blocks : blocks, 
						    cb_check_count, fs, 
						    "Enter the correct "
						    "block count please");
			
			hint.blocks = blocks;
		}
	}

	/* If there still is no format opened, create a new one or regenerate 
	   it from the backup if exists. */
	if (!fs->format) {
		if (!(plug = reiser4_factory_ifind(FORMAT_PLUG_TYPE, pid))) {
			aal_fatal("Failed to find a format plugin "
				  "by its on-disk id (%u).", pid);
			return -EINVAL;
		}
		
		if (fs->backup) {
			fent = plug_call(plug->o.format_ops, regenerate,
					 fs->device, &fs->backup->hint);
		} else {
			fent = plug_call(plug->o.format_ops, create, 
					 fs->device, &hint);
		}

		if (!fent) {
			aal_error("Failed to %s the format '%s' on '%s'.",
				  fs->backup ? "regenerate" : "create", 
				  plug->label, fs->device->name);
			return -EINVAL;
		} else {
			aal_warn("The format '%s' is %s on '%s'.", plug->label,
				 fs->backup ? "regenerated from backup" : 
				 "created", fs->device->name);
		}
	} else {
		fent = fs->format->ent;
	}
	
	/* Check the format structure. If there is no backup and format, then 
	   @fent has been just created, nothing to check anymore. */
	if (fs->backup || fs->format) {
		res = plug_call(fent->plug->o.format_ops, check_struct, 
				fent, fs->backup ? &fs->backup->hint : NULL,
				&hint, mode);
	} else {
		res = 0;
	}
	
	if (!fs->format) {
		if (!(fs->format = aal_calloc(sizeof(reiser4_format_t), 0))) {
			aal_error("Can't allocate the format.");
			plug_call(plug->o.format_ops, close, fent);
			return -ENOMEM;
		}

		fs->format->fs = fs;
		fs->format->ent = fent;
	}
	
	return res;
}

errno_t repair_format_update(reiser4_format_t *format) {
	aal_assert("vpf-829", format != NULL);

	if (format->ent->plug->o.format_ops->update == NULL)
		return 0;
    
	return format->ent->plug->o.format_ops->update(format->ent);
}

/* Fetches format data to @stream. */
errno_t repair_format_pack(reiser4_format_t *format, aal_stream_t *stream) {
	aal_assert("umka-2604", format != NULL);
	aal_assert("umka-2605", stream != NULL);

	return plug_call(format->ent->plug->o.format_ops,
			 pack, format->ent, stream);
}

/* Prints @format to passed @stream */
void repair_format_print(reiser4_format_t *format, aal_stream_t *stream) {
	aal_assert("umka-1560", format != NULL);
	aal_assert("umka-1561", stream != NULL);

	plug_call(format->ent->plug->o.format_ops,
		  print, format->ent, stream, 0);
}

/* Loads format data from @stream to format entity. */
reiser4_format_t *repair_format_unpack(reiser4_fs_t *fs, aal_stream_t *stream) {
	rid_t pid;
	uint32_t blksize;
	reiser4_plug_t *plug;
	reiser4_format_t *format;
	
	aal_assert("umka-2606", fs != NULL);
	aal_assert("umka-2607", stream != NULL);

	if (aal_stream_read(stream, &pid, sizeof(pid)) != sizeof(pid)) {
		aal_error("Can't unpack disk format. Stream is over?");
		return NULL;
	}

	/* Getting needed plugin from plugin factory */
	if (!(plug = reiser4_factory_ifind(FORMAT_PLUG_TYPE, pid)))  {
		aal_error("Can't find disk-format plugin by "
			  "its id 0x%x.", pid);
		return NULL;
	}
    
	/* Allocating memory for format instance. */
	if (!(format = aal_calloc(sizeof(*format), 0)))
		return NULL;

	format->fs = fs;
	format->fs->format = format;

	blksize = reiser4_master_get_blksize(fs->master);
	
	if (!(format->ent = plug_call(plug->o.format_ops, unpack, 
				      fs->device, blksize, stream)))
	{
		aal_error("Can't unpack disk-format.");
		goto error_free_format;
	}

	return format;

 error_free_format:
	aal_free(format);
	return NULL;
}

errno_t repair_format_check_backup(aal_device_t *device, backup_hint_t *hint) {
	reiser4_master_sb_t *master;
	reiser4_plug_t *plug;
	errno_t res;

	aal_assert("vpf-1732", hint != NULL);
	
	master = (reiser4_master_sb_t *)hint->el[BK_MASTER];
	
	if (!(plug = reiser4_factory_ifind(FORMAT_PLUG_TYPE, 
					   get_ms_format(master))))
	{
		return RE_FATAL;
	}

	if ((res = plug_call(plug->o.format_ops, check_backup, hint)))
		return res;

	return (reiser4_fs_check_len(device, get_ms_blksize(master), 
				     hint->blocks)) ? RE_FATAL : 0;
}

