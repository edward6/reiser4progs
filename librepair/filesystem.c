/*
    librepair/filesystem.c - methods are needed mostly by fsck for work 
    with broken filesystems.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include <repair/librepair.h>

errno_t repair_fs_check(reiser4_fs_t *fs) {
    repair_check_t data;
    reiser4_joint_t *joint;
    errno_t res;

    aal_assert("vpf-180", fs != NULL, return -1);
    aal_assert("vpf-181", fs->format != NULL, return -1);
    aal_assert("vpf-182", fs->format->device != NULL, return -1);

    aal_memset(&data, 0, sizeof(data));
    
    if (repair_filter_setup(fs, &data))
	return -1;    

    if ((res = repair_filter_joint_open(&joint, 
	reiser4_format_get_root(fs->format), &data))) 
    {
	repair_set_flag(&data, REPAIR_NOT_FIXED);
    } else {
	traverse_hint_t hint = {TO_BACKWARD, LEAF_LEVEL};
	    
	/* Cut the corrupted, unrecoverable parts of the tree off. */ 
	if ((res = reiser4_joint_traverse(joint, &hint, &data, 
	    repair_filter_joint_open,      repair_filter_joint_check, 
	    repair_filter_before_traverse, repair_filter_setup_traverse, 
	    repair_filter_update_traverse, repair_filter_after_traverse)) < 0)
	    return res;
    }

    if (repair_filter_update(fs, &data))
	return -1;

    if (joint) {
	traverse_hint_t hint = {TO_BACKWARD, LEAF_LEVEL};
	    
	/* Solve overlapped problem within the tree. */
	if ((res = reiser4_joint_traverse(joint, &hint, &data,
	    repair_filter_joint_open, repair_scan_node_check,
	    NULL, NULL, NULL, NULL)) < 0)
	    return res;
    }
      
    return 0;
}

static errno_t repair_master_check(reiser4_fs_t *fs, 
    callback_ask_user_t ask_blocksize) 
{
    uint16_t blocksize = 0;
    int error = 0;
    reiser4_plugin_t *plugin;
   
    aal_assert("vpf-161", fs != NULL, return -1);
    aal_assert("vpf-163", ask_blocksize != NULL, return -1);
    aal_assert("vpf-164", repair_data(fs) != NULL, return -1);
    aal_assert("vpf-170", repair_data(fs)->host_device != NULL, return -1);
    
    if (!fs->master) {
	/* Master SB was not opened. Create a new one. */
	if (aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_YESNO, 
	    "Master super block cannot be found. Do you want to build a new "
	    "one on (%s)?", aal_device_name(repair_data(fs)->host_device)) == 
	    EXCEPTION_NO) 
	    return -1;

        if (!(blocksize = ask_blocksize(fs, &error)) && error)
	    return -1;

	/* 
	    FIXME-VITALY: What should be done here with uuid and label? 
	    At least not here as uiud and label seem to be on the wrong place.
	    Move them to specific SB.
	*/
	
	/* Create a new master SB. */
	if (!(fs->master = reiser4_master_create(repair_data(fs)->host_device, 
	    FAKE_PLUGIN, blocksize, NULL, NULL))) 
	{
	    aal_exception_fatal("Cannot create a new master super block.");
	    return -1;
	} else if (repair_verbose(repair_data(fs)))
	    aal_exception_info("A new master superblock was created on (%s).", 
		aal_device_name(repair_data(fs)->host_device));
    } else {
	/* Master SB was opened. Check it for validness. */

	/* Check the blocksize. */
	if (!aal_pow_of_two(reiser4_master_blocksize(fs->master))) {
	    aal_exception_fatal("Invalid blocksize found in the master super "
		"block (%u).", reiser4_master_blocksize(fs->master));
	    
	    if (!(blocksize = ask_blocksize(fs, &error)) && error)
		return -1;

	    set_mr_blocksize(fs->master->super, blocksize);
	} 
    }

    /* Setting actual used block size from master super block */
    if (aal_device_set_bs(repair_data(fs)->host_device, 
	reiser4_master_blocksize(fs->master))) 
    {
        aal_exception_fatal("Invalid block size was specified (%u). It must "
	    "be power of two.", reiser4_master_blocksize(fs->master));
	return -1;
    }
    
    return 0;
}    

static errno_t repair_alloc_check(reiser4_fs_t *fs) {
    return 0;
}

static errno_t repair_oid_check(reiser4_fs_t *fs) {
    return 0;
}

reiser4_fs_t *repair_fs_open(repair_data_t *data, 
    callback_ask_user_t ask_blocksize) 
{
    reiser4_fs_t *fs;
    void *oid_area_start, *oid_area_end;

    aal_assert("vpf-159", data != NULL, return NULL);
    aal_assert("vpf-172", data->host_device != NULL, return NULL);
    
    /* Allocating memory and initializing fields */
    if (!(fs = aal_calloc(sizeof(*fs), 0)))
	return NULL;
    
    fs->data = data;
    
    /* Try to open master and rebuild if needed. */
    fs->master = reiser4_master_open(data->host_device);
	
    /* Check opened master or build a new one. */
    if (repair_master_check(fs, ask_blocksize))
	goto error_free_master;
    
    /* Try to open the disk format. */
    fs->format = reiser4_format_open(data->host_device, 
	reiser4_master_format(fs->master));
    
    /* Check the opened disk format or rebuild it if needed. */
    if (repair_format_check(fs))
	goto error_free_format;
    
    fs->alloc = reiser4_alloc_open(fs->format, 
	reiser4_format_get_len(fs->format));
    
    if (repair_alloc_check(fs))
	goto error_free_alloc;

    /* Initializes oid allocator */
    fs->oid = reiser4_oid_open(fs->format);
  
    if (repair_oid_check(fs))
	goto error_free_oid;
    
    return fs;
    
error_free_oid:
    if (fs->oid)
	reiser4_oid_close(fs->oid);
error_free_alloc:
    if (fs->alloc)
	reiser4_alloc_close(fs->alloc);
error_free_format:
    if (fs->format)
	reiser4_format_close(fs->format);
error_free_master:
    if (fs->master)
	reiser4_master_close(fs->master);
error_free_fs:
    aal_free(fs);
error:
    return NULL;
}

errno_t repair_fs_sync(reiser4_fs_t *fs) 
{
    aal_assert("vpf-173", fs != NULL, return -1);
    
    /* Synchronizing block allocator */
    if (reiser4_alloc_sync(fs->alloc))
	return -1;
    
    /* Synchronizing the object allocator */
    if (reiser4_oid_sync(fs->oid))
	return -1;

    /* Synchronizing the disk format */
    if (reiser4_format_sync(fs->format))
	return -1;

    if (reiser4_master_confirm(fs->format->device)) {
	if (reiser4_master_sync(fs->master))
	    return -1;
    }
    
 return 0;
}

/* 
    Closes all filesystem's entities. Calls plugins' "done" routine for every 
    plugin and frees all assosiated memory. 
*/
void repair_fs_close(reiser4_fs_t *fs) 
{
    aal_assert("vpf-174", fs != NULL, return);

    reiser4_oid_close(fs->oid);

    reiser4_alloc_close(fs->alloc);
    reiser4_format_close(fs->format);
    reiser4_master_close(fs->master);

    /* Freeing memory occupied by fs instance */
    aal_free(fs);
}

