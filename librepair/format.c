/*
    libprogs/format.c - methods are needed for handle the fs format.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/librepair.h>

/* Checks the opened format, or build a new one if it was not openned. */
static errno_t repair_format_check(reiser4_fs_t *fs, uint8_t mode) {
    reiser4_plugin_t *plugin = NULL;
    errno_t ret = REPAIR_OK;
    count_t count;
    rid_t policy, pid;

    aal_assert("vpf-165", fs != NULL);
    aal_assert("vpf-833", fs->profile != NULL);
    aal_assert("vpf-171", fs->device != NULL);
    aal_assert("vpf-834", fs->master != NULL);
    
    policy = reiser4_profile_value(fs->profile, "policy");
    
    if (policy >= TAIL_LAST_ID) {
	/* Tail id from the profile is wrong. */
	aal_exception_error("Invalid tail policy (%u) is specified in the "
	    "profile.", policy);
	return -EINVAL;
    }

    if (fs->format == NULL) {
	/* Format was not opened. */
	aal_exception_fatal("Cannot open the on-disk format on (%s)", 
	    aal_device_name(fs->device));
	
	if (mode != REPAIR_REBUILD)
	    /* Fatal error in the format structure. */
	    return REPAIR_FATAL;
	
	pid = reiser4_profile_value(fs->profile, "format");

	/* Try to detect a format on the device. */
	if (!(plugin = reiser4_master_guess(fs->device))) {
	    /* Format was not detected on the device. */
	    aal_exception_fatal("Cannot detect an on-disk format on (%s).", 
		aal_device_name(fs->device));
	
	    if (!(plugin = libreiser4_factory_ifind(FORMAT_PLUGIN_TYPE, pid))) {
		aal_exception_fatal("Cannot find the format plugin (0x%x) specified "
		    "in the profile.", pid);
		return -EINVAL;
	    }

	    if (aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_YESNO, 
		"Do you want to build the on-disk format (%s) specified in the "
		"profile?", plugin->h.label) == EXCEPTION_NO)
		return -EINVAL;

	    count = aal_device_len(fs->device);
	    
	    /* Create the format from the scratch. */
	    if (!(fs->format = reiser4_format_create(fs, count, policy, pid))) {
		aal_exception_fatal("Cannot create a filesystem of the format "
		    "(%s).", plugin->h.label);
		return -EINVAL;
	    } else {
		aal_exception_fatal("The format (%s) with tail policy (%u) was "
		    "created on the partition (%s) of (%llu) block length.", 
		    plugin->h.label, policy, aal_device_name(fs->device), count);
	    }

	    reiser4_format_set_stamp(fs->format, 0);
	    set_ms_format(SUPER(fs->master), pid);
	    reiser4_master_mkdirty(fs->master);
	    
	    return REPAIR_FIXED;
	} else {
	    /* Format was detected on the device. */
	    aal_exception_fatal("The on-disk format (%s) was detected on (%s). %s", 
		plugin->h.label, aal_device_name(fs->device), pid != plugin->h.id ?
		"It differs from the one specified in the profile. Do not forget to"
		" fix the profile." : "");

	    if (pid != plugin->h.id) {
		set_ms_format(SUPER(fs->master), plugin->h.id);
		fs->master->dirty = TRUE;
	    }
	    
	    if (!(fs->format = reiser4_format_open(fs))) {
		aal_exception_fatal("Failed to open the format (%s) on the (%s).", 
		    plugin->h.label, aal_device_name(fs->device));
		return -EINVAL;
	    }
	}
    } 
    
    /* Format was opened or detected. Check it and fix it. */
    ret = plugin_call(fs->format->entity->plugin->o.format_ops, check, 
	fs->format->entity, mode);
    
    if (repair_error_fatal(ret))
	return ret;
    
    pid = reiser4_format_get_policy(fs->format);
    
    if (pid != policy) {
	aal_exception_error("Tail policy (%u) detected on (%s) differs from "
	    "the specified in the profile (%u). %s", pid, 
	    aal_device_name(fs->device), policy, mode != REPAIR_CHECK ? 
	    "Fixed." : "");
	
	if (mode != REPAIR_CHECK) {
	    reiser4_format_set_policy(fs->format, policy);
	    ret |= REPAIR_FIXED;
	} else
	    ret |= REPAIR_FIXABLE;
    }
    
    return ret;
}

/* Try to open format and check it. */
errno_t repair_format_open(reiser4_fs_t *fs, uint8_t mode) {
    errno_t error;
    
    aal_assert("vpf-398", fs != NULL);

    /* Try to open the disk format. */
    fs->format = reiser4_format_open(fs);

    /* Check the opened disk format or rebuild it if needed. */
    error = repair_format_check(fs, mode);

    if (repair_error_exists(error))
	goto error_format_close;

    if (error & REPAIR_FIXED)
	reiser4_format_mkdirty(fs->format);
    
    return 0;

error_format_close:
    if (fs->format) {
	reiser4_format_close(fs->format);
	fs->format = NULL;
    }
    
    return error;
}

errno_t repair_format_update(reiser4_format_t *format) {
    aal_assert("vpf-829", format != NULL);

    if (format->entity->plugin->o.format_ops->update == NULL)
	return 0;
    
    return format->entity->plugin->o.format_ops->update(format->entity);
}

/* Prints the opened format. */
void repair_format_print(reiser4_fs_t *fs, FILE *file, uint16_t options) {
    aal_stream_t stream;

    aal_assert("vpf-245", fs != NULL);
    aal_assert("vpf-175", fs->format != NULL);

    if (!file)
	return;

    aal_stream_init(&stream);

    plugin_call(fs->format->entity->plugin->o.format_ops, print, 
	fs->format->entity, &stream, options);
    
    fprintf(file, (char *)stream.data);
    
    aal_stream_fini(&stream);
}


