/*
    libprogs/format.c - methods are needed for handle the fs format.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/librepair.h>

errno_t callback_mark_format_block(object_entity_t *format, blk_t blk, 
    void *data) 
{
    aux_bitmap_t *format_layout = (aux_bitmap_t *)data;

    aux_bitmap_mark(format_layout, blk);
    
    return 0;
}

static reiser4_plugin_t *__choose_format(reiser4_profile_t *profile, 
    aal_device_t *host_device)
{
    rpid_t format;
    reiser4_plugin_t *plugin = NULL;
   
    aal_assert("vpf-167", profile != NULL);
    aal_assert("vpf-169", host_device != NULL);
    
    format = reiser4_profile_value(profile, "format");
    
    if (!(plugin = reiser4_master_guess(host_device))) {
	/* Format was not detected on the partition. */
	aal_exception_fatal("Cannot detect an on-disk format on (%s).", 
	    aal_device_name(host_device));

	if (!(plugin = libreiser4_factory_ifind(FORMAT_PLUGIN_TYPE, 
	    format))) 
	{
	    aal_exception_fatal("Cannot find the format plugin (0x%x) specified "
		"in the profile.", format);
	    return NULL;	    
	}

	if (aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_YESNO, 
	    "Do you want to build the on-disk format (%s) specified in the "
	    "profile?", plugin->h.label) == EXCEPTION_NO)
	    return NULL;
    } else {
	/* Format was detected on the partition. */
	if (format != plugin->h.id)
	    aal_exception_fatal("The detected on-disk format (%s) differs from "
		"the profile's one.\nDo not forget to specify the correct "
		"on-disk format in the profile next time.", plugin->h.label);
	else 
	    /* Will be printed if verbose. */
	    aal_exception_info("The on-disk format (%s) was detected on (%s).", 
		plugin->h.label, aal_device_name(host_device));	
    }
    
    return plugin;
}

static errno_t repair_format_check(reiser4_fs_t *fs, reiser4_profile_t *profile) 
{
    reiser4_plugin_t *plugin = NULL;

    aal_assert("vpf-165", fs != NULL);
    aal_assert("vpf-171", fs->device != NULL);
    aal_assert("vpf-166", fs->format != NULL || profile != NULL);

    if (fs->format == NULL) {
	/* Format was not opened. */
	aal_exception_fatal("Cannot open the on-disk format on (%s)", 
	    aal_device_name(fs->device));
	
	if (!(plugin = __choose_format(profile, fs->device)))
	    return -1;

	/* Create the format with fake tail plugin. */
	if (!(fs->format = reiser4_format_create(fs, 0, INVAL_PID, 
	    plugin->h.id)))
	{
	    aal_exception_fatal("Cannot create a filesystem of the format "
		"(%s).", plugin->h.label);
	    return -1;
	}
    } 
    
    /* Format was either opened or created. Check it and fix it. */
    if (plugin_call(fs->format->entity->plugin->format_ops, check, 
	fs->format->entity)) 
    {
	aal_exception_error("Failed to recover the on-disk format (%s) on "
	    "(%s).", plugin->h.label, aal_device_name(fs->device));
	return -1;
    }
    
    return 0;
}

errno_t repair_format_open(reiser4_fs_t *fs, reiser4_profile_t *profile) {
    aal_assert("vpf-398", fs != NULL);

    /* Try to open the disk format. */
    fs->format = reiser4_format_open(fs);

    /* Check the opened disk format or rebuild it if needed. */
    if (repair_format_check(fs, profile))
	goto error_format_close;

    return 0;

error_format_close:
    if (fs->format) {
	reiser4_format_close(fs->format);
	fs->format = NULL;
    }
    
    return -1;
}

void repair_format_print(reiser4_fs_t *fs, FILE *file, uint16_t options) {
    aal_stream_t stream;

    aal_assert("vpf-245", fs != NULL);
    aal_assert("vpf-175", fs->format != NULL);

    if (!file)
	return;

    aal_stream_init(&stream);

    plugin_call(fs->format->entity->plugin->format_ops, print, 
	fs->format->entity, &stream, options);
    
    fprintf(file, (char *)stream.data);
    
    aal_stream_fini(&stream);
}
