/*
    libprogs/format.c - methods are needed for handle the fs format.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include <repair/librepair.h>

/*
errno_t callback_data_block_check(object_entity_t *format, blk_t blk, 
    void *data) 
{
    blk_t passed_blk = *(blk_t *)data;

    if (passed_blk >= plugin_call(return -1, format->plugin->format_ops, 
	get_len, format))
	return -1;

    return passed_blk == blk ? 1 : 0;
}
*/

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
    reiser4_plugin_t *plugin;
   
    aal_assert("vpf-167", profile != NULL, return NULL);
    aal_assert("vpf-169", host_device != NULL, return NULL);
    
    if (!(plugin = reiser4_master_guess(host_device))) {
	/* Format was not detected on the partition. */
	aal_exception_fatal("Cannot detect an on-disk format on (%s).", 
	    aal_device_name(host_device));
	
	if (!(plugin = libreiser4_factory_ifind(FORMAT_PLUGIN_TYPE, 
	    profile->format))) 
	{
	    aal_exception_fatal("Cannot find the format plugin (%d) specified "
		"in the profile.", profile->format);
	    return NULL;	    
	}

	if (aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_YESNO, 
	    "Do you want to build the on-disk format (%s) specified in the "
	    "profile?", plugin->h.label) == EXCEPTION_NO)
	    return NULL;
    } else {
	/* Format was detected on the partition. */
	if (profile->format != plugin->h.sign.id)
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

static errno_t repair_format_check(reiser4_format_t **format, 
    aal_device_t *host_device, reiser4_profile_t *profile) 
{
    reiser4_plugin_t *plugin = NULL;

    aal_assert("vpf-165", format != NULL, return -1);
    aal_assert("vpf-166", *format == NULL || profile != NULL, 
	return -1);
    aal_assert("vpf-171", host_device != NULL, return -1);
    aal_assert("vpf-480", profile != NULL, return -1);
    
    if (*format == NULL) {
	/* Format was not opened. */
	aal_exception_fatal("Cannot open the on-disk format on (%s)", 
	    aal_device_name(host_device));
	
	if (!(plugin = __choose_format(profile, host_device)))
	    return -1;

	/* Create the format of the fake plugin number and fix it later. */
	if (!(*format = reiser4_format_create(host_device, 0, FAKE_PLUGIN, 
	    plugin->h.sign.id))) 
	{
	    aal_exception_fatal("Cannot create a filesystem of the format "
		"(%s).", plugin->h.label);
	    return -1;
	}
    } 
    
    /* Format was either opened or created. Check it and fix it. */
    if (plugin_call(return -1, (*format)->entity->plugin->format_ops, check, 
	(*format)->entity)) 
    {
	aal_exception_error("Failed to recover the on-disk format (%s) on "
	    "(%s).", plugin->h.label, aal_device_name(host_device));
	return -1;
    }
    
    return 0;
}

reiser4_format_t *repair_format_open(reiser4_master_t *master, 
    reiser4_profile_t *profile) 
{
    reiser4_format_t *format = NULL;
    aal_device_t *host_device;
    int res;

    aal_assert("vpf-398", master != NULL, return NULL);
    aal_assert("vpf-396", master->block != NULL, return NULL);
    aal_assert("vpf-479", master->block->device != NULL, return NULL);
    
    host_device = master->block->device;
    
    /* Try to open the disk format. */
    format = reiser4_format_open(host_device, reiser4_master_format(master));
    
    /* Check the opened disk format or rebuild it if needed. */
    if (repair_format_check(&format, host_device, profile))
	goto error_free_format;

    aal_assert("vpf-478", format != NULL, res = -1; goto error);
	
    return format;

error_free_format:
    if (format)
	reiser4_format_close(format);
error:
    return NULL;
}

void repair_format_print(reiser4_fs_t *fs, FILE *stream, uint16_t options) {
    char buf[4096];

    aal_assert("vpf-245", fs != NULL, return);
    aal_assert("vpf-175", fs->format != NULL, return);

    if (!stream)
	return;

    aal_memset(buf, 0, 4096);

    plugin_call(return, fs->format->entity->plugin->format_ops, print, 
	fs->format->entity, buf, 4096, options);
    
    fprintf(stream, "%s", buf);
}
