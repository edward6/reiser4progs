/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   libprogs/format.c - methods are needed for handle the fs format. */

#include <repair/librepair.h>

static int callback_check_count(int64_t val, void *data) {
	reiser4_fs_t *fs = (reiser4_fs_t *)data;
	
	if (val < 0) 
		return 0;
	
	return reiser4_fs_check_len(fs, val) ? 0 : 1;
}

static errno_t repair_format_check_struct(reiser4_fs_t *fs, uint8_t mode) {
	count_t dev_len, fs_len;
	reiser4_plug_t *defplug;
	reiser4_plug_t *plug;
	errno_t res = 0;
	rid_t pid;

	/* Format was opened or detected. Check it and fix it. */
	if (fs->format->ent->plug->o.format_ops->check_struct) {
		res = plug_call(fs->format->ent->plug->o.format_ops,
				check_struct, fs->format->ent, 
				mode);

		if (repair_error_fatal(res))
			return res;
	}
		
	fs_len = reiser4_format_get_len(fs->format);
	
	/* Check that fs length is equal to device length. */
	dev_len = aal_device_len(fs->device) / 
		(reiser4_master_get_blksize(fs->master) / fs->device->blksize);
	
	if (dev_len != fs_len) {
		if (reiser4_fs_check_len(fs, fs_len)) {
			/* FS length is not valid. */
			if (mode != RM_BUILD)
				return RE_FATAL;
		} else {
			aal_fatal("Number of blocks found in the superblock "
				  "(%llu) is not equal to the size of the "
				  "partition (%llu).%s", fs_len, dev_len, 
				  mode != RM_BUILD ? " Assuming this is "
				  "correct.": "");
		}
		
		if (mode == RM_BUILD) {
			/* Confirm that size is correct. */
			fs_len = aal_ui_get_numeric(dev_len, 
						   callback_check_count,
						   fs, "Enter the correct "
						   "block count please");
			
			reiser4_format_set_len(fs->format, fs_len);
			reiser4_format_mkdirty(fs->format);
		}
	}

	defplug = reiser4_profile_plug(PROF_POLICY);
	pid = reiser4_format_get_policy(fs->format);

	if (pid != defplug->id.id) {
		if (reiser4_profile_overridden(PROF_POLICY)) {
			/* The policy was overridden. */
			aal_error("The specified reiser4 tail policy is '%s'. "
				  "Its id (0x%x) does not match the on-disk id "
				  "(0x%x). %s", defplug->label, defplug->id.id, 
				  pid, mode == RM_BUILD ? "Fixed." : "Has "
				  "effect in the BUILD mode only.");

			if (mode != RM_BUILD)
				return RE_FATAL;

			reiser4_format_set_policy(fs->format, defplug->id.id);
		} else {
			/* Was not overridden, try the on-disk one. */
			plug = reiser4_factory_ifind(POLICY_PLUG_TYPE, pid);

			if (!plug) {
				aal_error("Can't find the tail policy plugin "
					  "by the detected id 0x%x.", pid);

				if (mode != RM_BUILD)
					return RE_FATAL;

				/* This is not overridden in BUILD mode. */
				aal_error("Using the default tail policy '%s'.",
					  defplug->label);

				reiser4_format_set_policy(fs->format, 
							  defplug->id.id);
			}
		}
	}
	
	if (reiser4_profile_overridden(PROF_KEY)) {
		uint16_t flags;
		bool_t large;
		
		defplug = reiser4_profile_plug(PROF_KEY);
		flags = plug_call(fs->format->ent->plug->o.format_ops,
				  get_flags, fs->format->ent);

		large = flags & (1 << REISER4_LARGE_KEYS);

		if ((large && (defplug->id.id != KEY_LARGE_ID)) || 
		    (!large && (defplug->id.id != KEY_SHORT_ID)))
		{
			/* Key policy does ot match and was overridden. */
			aal_error("The specified key plugin '%s' does not match "
				  "to the on-disk one '%s'. %s", defplug->label,
				  large ? "LARGE" : "SHORT", mode == RM_BUILD ? 
				  "Fixed.":"Has effect in the BUILD mode only.");

			if (mode != RM_BUILD)
				return RE_FATAL;

			if (large)
				flags &= ~(1 << REISER4_LARGE_KEYS);
			else
				flags |= (1 << REISER4_LARGE_KEYS);

			plug_call(fs->format->ent->plug->o.format_ops, 
				  set_flags, fs->format->ent, flags);
		}
	}
	
	return res;
}

/* Checks the opened format, or build a new one if it was not openned. */
static errno_t repair_format_open_check(reiser4_fs_t *fs, uint8_t mode) {
	reiser4_plug_t *defplug, *plug, *policy;
	bool_t over;
	rid_t pid;
	
	aal_assert("vpf-165", fs != NULL);
	aal_assert("vpf-171", fs->device != NULL);
	aal_assert("vpf-834", fs->master != NULL);
	
	if (fs->format == NULL) {
		/* Format was not opened. */
		aal_fatal("Cannot open the on-disk format on (%s)",
			  fs->device->name);
		
		over = reiser4_profile_overridden(PROF_FORMAT);
		
		if (!over) {
			/* Format was not overridden, try to detect it. */
			if ((plug = reiser4_master_guess(fs->device))) {
				aal_info("The format '%s' is detected.%s",
					 plug->label, mode == RM_BUILD ? 
					 " Rebuilding with it." : "");
			}
		} else {
			plug = NULL;
		}
		
		if (mode != RM_BUILD)
			return RE_FATAL;

		if (plug) {
			/* Save in the master the format pid. */
			reiser4_master_set_format(fs->master, plug->id.id);
			reiser4_master_mkdirty(fs->master);

			/* Open the detected format. */
			if (!(fs->format = reiser4_format_open(fs))) {
				aal_fatal("Failed to open the detected "
					  "format '%s'.", plug->label);
				return -EINVAL;
			}
			
			return 0;
		}
		
		/* @format is still NULL: its id was overridden or the format
		   cannot be detected on the device. Try to find a profile by 
		   the on-disk id (the overridden pid was set there at the 
		   master check time), if fails take from the profile, and 
		   build a new profile. */
		defplug = reiser4_profile_plug(PROF_FORMAT);
		pid = reiser4_master_get_format(fs->master);

		if (pid != defplug->id.id && !over) {
			/* Not the same pid as in the profile and was not 
			   overridden. Try to find plugin by @pid. */
			plug = reiser4_factory_ifind(POLICY_PLUG_TYPE, pid);

			if (plug) {
				aal_error("Building the format '%s' by "
					  "the detected format id 0x%x.",
					  plug->label, pid);
			}
		}
		
		if (!plug) {
			plug = defplug;
			aal_error("No format was detected and no valid format "
				  "id was found. Building the default format "
				  "'%s'.", plug->label);
		}

		policy = reiser4_profile_plug(PROF_POLICY);

		/* Create the format from the scratch. */
		fs->format = reiser4_format_create(fs, plug, policy, 0);

		if (!fs->format) {
			aal_fatal("Failed to create a filesystem "
				  "of the format '%s' on '%s'.", 
				  plug->label, fs->device->name);
			return -EINVAL;
		} else {
			aal_fatal("The format '%s' was created on '%s'.",
				  plug->label, fs->device->name);
		}

		reiser4_format_set_stamp(fs->format, 0);
		reiser4_master_set_format(fs->master, plug->id.id);
		reiser4_master_mkdirty(fs->master);
		reiser4_format_mkdirty(fs->format);
	}

	return 0;
}

/* Try to open format and check it. */
errno_t repair_format_open(reiser4_fs_t *fs, uint8_t mode) {
	errno_t res;
	
	aal_assert("vpf-398", fs != NULL);
	
	/* Try to open the disk format. */
	fs->format = reiser4_format_open(fs);
	
	/* Check the opened disk format or rebuild it if needed. */
	res = repair_format_open_check(fs, mode);
	
	if (repair_error_fatal(res))
		goto error_format_close;
	
	res |= repair_format_check_struct(fs, mode);

	if (repair_error_fatal(res))
		goto error_format_close;
	
	return res;
	
 error_format_close:
	
	if (fs->format) {
		reiser4_format_close(fs->format);
		fs->format = NULL;
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
	fs_desc_t desc;
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

	desc.device = fs->device;
	desc.blksize = reiser4_master_get_blksize(fs->master);
	
	if (!(format->ent = plug_call(plug->o.format_ops,
				      unpack, &desc, stream)))
	{
		aal_error("Can't unpack disk-format.");
		goto error_free_format;
	}

	return format;

 error_free_format:
	aal_free(format);
	return NULL;
}

