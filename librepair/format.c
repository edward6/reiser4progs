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
	reiser4_plug_t *plug;
	rid_t policy, pid;
	errno_t res = 0;

	/* Format was opened or detected. Check it and fix it. */
	if (fs->format->entity->plug->o.format_ops->check_struct) {
		res = plug_call(fs->format->entity->plug->o.format_ops,
				check_struct, fs->format->entity, 
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

	policy = reiser4_param_value("policy");
	pid = reiser4_format_get_policy(fs->format);
	plug = NULL;

	if (pid != policy) {
		bool_t over;
		
		over = reiser4_param_get_flag("policy", PF_OVERRIDDEN);
		
		if (!over) {
			if (!(plug = reiser4_factory_ifind(POLICY_PLUG_TYPE,
							   pid)))
			{
				aal_error("Can't find the tail policy "
					  "plugin by its id 0x%x.", pid);

				if (mode != RM_BUILD)
					return RE_FATAL;
			}
		}

		if (!plug) {
			/* The policy was overridden or the on-disk one was 
			   not found. Find the one from the profile. */

			if (!(plug = reiser4_factory_ifind(POLICY_PLUG_TYPE,
							   policy))) 
			{
				aal_error("Can't find the tail policy plugin "
					  "by its id 0x%x.", policy);
				return -EINVAL;
			}

			if (over) {
				/* This is overridden not in BUILD mode. */
				aal_error("The specified reiser4 tail policy "
					  "is '%s'. Its id (0x%x) does not "
					  "match the on-disk id (0x%x).%s", 
					  plug->label, policy, pid, 
					  mode == RM_BUILD ? " Fixed." : 
					  " Has effect in BUILD mode only.");

				if (mode != RM_BUILD)
					return RE_FATAL;
			} else {
				/* This is not overridden in BUILD mode. */
				aal_error("Using the default tail policy '%s'.",
					  plug->label);
			}
		}
	}
	
	if (reiser4_param_get_flag("key", PF_OVERRIDDEN)) {
		uint16_t flags;
		bool_t large;
		
		pid = reiser4_param_value("key");
		flags = plug_call(fs->format->entity->plug->o.format_ops,
				  get_flags, fs->format->entity);
		
		large = flags & (1 << REISER4_LARGE_KEYS);
		
		if (large && (pid != KEY_LARGE_ID) || 
		    !large && (pid != KEY_SHORT_ID))
		{
			/* Wrong key plugin is specified. Fix it. */
			
			if (!(plug = reiser4_factory_ifind(KEY_PLUG_TYPE, pid)))
			{
				aal_error("Can't find the key plugin "
					  "by its id 0x%x.", pid);
				return -EINVAL;
			}
			
			aal_error("The specified key plugin '%s' does not "
				  "match '%s' specified in the format.%s",
				  plug->label, large ? "LARGE" : "SHORT",
				  mode == RM_BUILD ? " Fixed." : "");

			if (mode != RM_BUILD)
				return RE_FATAL;

			large = large ? 0 : (1 << REISER4_LARGE_KEYS);
			
			plug_call(fs->format->entity->plug->o.format_ops, 
				  set_flags, fs->format->entity, large);
		}
	}
	
	return res;
}

/* Checks the opened format, or build a new one if it was not openned. */
static errno_t repair_format_open_check(reiser4_fs_t *fs, uint8_t mode) {
	reiser4_plug_t *plug;
	rid_t policy, pid;
	count_t count;
	bool_t over;
	
	aal_assert("vpf-165", fs != NULL);
	aal_assert("vpf-171", fs->device != NULL);
	aal_assert("vpf-834", fs->master != NULL);
	
	if (fs->format == NULL) {
		/* Format was not opened. */
		aal_fatal("Cannot open the on-disk format on (%s)",
			  fs->device->name);
		
		over = reiser4_param_get_flag("format", PF_OVERRIDDEN);
		plug = NULL;
		
		if (!over) {
			/* Format was not overridden, try to detect it. */
			if ((plug = reiser4_master_guess(fs->device))) {
				aal_info("The format '%s' is detected on '%s'."
					 "%s", plug->label, fs->device->name, 
					 mode == RM_BUILD ? " Rebuilding with "
					 "it." : "");
			}
		}
		
		if (mode != RM_BUILD)
			return RE_FATAL;

		if (plug) {
			reiser4_master_set_format(fs->master, plug->id.id);
			reiser4_master_mkdirty(fs->master);

			if (!(fs->format = reiser4_format_open(fs))) {
				aal_fatal("Can't open the format '%s' "
					  "on the '%s'.", plug->label, 
					  fs->device->name);
				return -EINVAL;
			}
		}
		
		if (!fs->format) {
			/* If @format is still NULL, its id was overridden or 
			   the format cannot be detected on the device. Build 
			   a new one. */

			pid = reiser4_param_value("format");
			plug = reiser4_factory_ifind(FORMAT_PLUG_TYPE, pid);
			
			if (!plug) {
				aal_fatal("Can't find the %s format plugin by "
					  "its id (0x%x).", over ? "specified" :
					  "default", pid);
				return -EINVAL;
			}

			count = aal_device_len(fs->device);
			
			policy = reiser4_param_value("policy");
			
			/* Create the format from the scratch. */
			fs->format = reiser4_format_create(fs, 0, policy, pid);
			
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
			reiser4_master_set_format(fs->master, pid);
			reiser4_master_mkdirty(fs->master);
			reiser4_format_mkdirty(fs->format);
		}
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

	if (format->entity->plug->o.format_ops->update == NULL)
		return 0;
    
	return format->entity->plug->o.format_ops->update(format->entity);
}

/* Fetches format data to @stream. */
errno_t repair_format_pack(reiser4_format_t *format, aal_stream_t *stream) {
	aal_assert("umka-2604", format != NULL);
	aal_assert("umka-2605", stream != NULL);

	return plug_call(format->entity->plug->o.format_ops,
			 pack, format->entity, stream);
}

/* Prints @format to passed @stream */
void repair_format_print(reiser4_format_t *format, aal_stream_t *stream) {
	aal_assert("umka-1560", format != NULL);
	aal_assert("umka-1561", stream != NULL);

	plug_call(format->entity->plug->o.format_ops,
		  print, format->entity, stream, 0);
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
	
	if (!(format->entity = plug_call(plug->o.format_ops,
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

