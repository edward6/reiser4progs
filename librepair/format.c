/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   libprogs/format.c - methods are needed for handle the fs format. */

#include <repair/librepair.h>

/* Checks the opened format, or build a new one if it was not openned. */
static errno_t repair_format_check_struct(reiser4_fs_t *fs, uint8_t mode) {
	reiser4_plug_t *plug = NULL;
	rid_t policy, pid;
	errno_t res = 0;
	count_t count;
	
	aal_assert("vpf-165", fs != NULL);
	aal_assert("vpf-171", fs->device != NULL);
	aal_assert("vpf-834", fs->master != NULL);
	
	policy = reiser4_param_value("policy");
	
	if (policy >= TAIL_LAST_ID) {
		/* Tail id from the profile is wrong. */
		aal_exception_error("Invalid tail policy (%u) is specified in "
				    "the profile.", policy);
		return -EINVAL;
	}
	
	if (fs->format == NULL) {
		/* Format was not opened. */
		aal_exception_fatal("Cannot open the on-disk format on (%s)",
				    fs->device->name);
		
		if (mode != RM_BUILD)
			/* Fatal error in the format structure. */
			return RE_FATAL;
		
		pid = reiser4_param_value("format");
		
		/* Try to detect a format on the device. */
		if (!(plug = reiser4_master_guess(fs->device))) {
			/* Format was not detected on the device. */
			aal_exception_fatal("Cannot detect an on-disk format on (%s).",
					    fs->device->name);
			
			plug = reiser4_factory_ifind(FORMAT_PLUG_TYPE, pid);
			
			if (!plug) {
				aal_exception_fatal("Cannot find the format plugin "
						    "(0x%x) specified in the profile.", 
						    pid);
				return -EINVAL;
			}

			if (aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_YESNO,
						"Do you want to build the on-disk "
						"format (%s) specified in the profile?",
						plug->label) == EXCEPTION_NO)
				return -EINVAL;
			
			count = aal_device_len(fs->device);
			
			/* Create the format from the scratch. */
			fs->format = reiser4_format_create(fs, count, policy, pid);
			
			if (!fs->format) {
				aal_exception_fatal("Cannot create a filesystem of the "
						    "format (%s).", plug->label);
				return -EINVAL;
			} else {
				aal_exception_fatal("The format (%s) with tail policy "
						    "(%u) was created on the partition "
						    "(%s) of (%llu) block length.", 
						    plug->label, policy, 
						    fs->device->name, count);
			}
			
			reiser4_format_set_stamp(fs->format, 0);
			set_ms_format(SUPER(fs->master), pid);
			reiser4_master_mkdirty(fs->master);
			reiser4_format_mkdirty(fs->format);
			
			return 0;
		} else {
			/* Format was detected on the device. */
			aal_exception_fatal("The on-disk format (%s) was detected on "
					    "(%s). %s", plug->label, fs->device->name, 
					    pid != plug->id.id ? "It differs from "
					    "the one specified in the profile. Do not "
					    "forget to fix the profile." : "");
			
			set_ms_format(SUPER(fs->master), plug->id.id);
			reiser4_master_mkdirty(fs->master);
			
			if (!(fs->format = reiser4_format_open(fs))) {
				aal_exception_fatal("Failed to open the format (%s) "
						    "on the (%s).", plug->label, 
						    fs->device->name);
				return -EINVAL;
			}
		}
	}
	
	/* Format was opened or detected. Check it and fix it. */
	if ((res = plug_call(fs->format->entity->plug->o.format_ops, 
			     check_struct, fs->format->entity, mode)) < 0)
		return res;
	
	repair_error_check(res, mode);
	
	if (repair_error_fatal(res))
		return res;
	
	pid = reiser4_format_get_policy(fs->format);
	
	if (pid >= TAIL_LAST_ID) {
		/* Tail id from the profile is wrong. */
		aal_exception_error("Invalid tail policy (%u) detected in the format "
				    "on (%s). %s (%u) -- from the profile.", pid, 
				    fs->device->name, mode == RM_CHECK ? "Should be" :
				    "Fixed to", policy);
		
		reiser4_format_set_policy(fs->format, policy);

		if (mode != RM_CHECK)
			reiser4_format_mkdirty(fs->format);
		else
			res |= RE_FIXABLE;
	} else if (pid != policy) {
		aal_exception_fatal("The tail policy (%u) detected on (%s) differs "
				    "from the specified in the profile (%u). Do not "
				    "forget to fix the profile.", pid, fs->device->name,
				    policy);
	}
	
	return res;
}

/* Try to open format and check it. */
errno_t repair_format_open(reiser4_fs_t *fs, uint8_t mode) {
	errno_t res;
	
	aal_assert("vpf-398", fs != NULL);
	
	/* Try to open the disk format. */
	fs->format = reiser4_format_open(fs);
	
	/* Check the opened disk format or rebuild it if needed. */
	res = repair_format_check_struct(fs, mode);
	
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
