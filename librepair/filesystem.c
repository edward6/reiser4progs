/*
    librepair/filesystem.c - methods are needed mostly by fsck for work 
    with broken filesystems.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include <repair/librepair.h>

/* FIXME-VITALY: This should be the tree method. */
errno_t repair_fs_check(reiser4_fs_t *fs, repair_data_t *repair_data) {
    traverse_hint_t hint;
    reiser4_joint_t *joint = NULL;
    errno_t res = 0;

    aal_assert("vpf-180", fs != NULL, return -1);
    aal_assert("vpf-181", fs->format != NULL, return -1);
    aal_assert("vpf-493", repair_data != NULL, return -1);

    hint.data = repair_data;
    hint.cleanup = 1;
    
    if (repair_filter_setup(&hint))
	return -1;    

    if ((res = repair_filter_joint_open(&joint, 
	reiser4_format_get_root(fs->format), &hint)) < 0)
	return res;

    if (res == 0 && joint != NULL) {
	/* Cut the corrupted, unrecoverable parts of the tree off. */ 
	if ((res = reiser4_joint_traverse(joint, &hint, repair_filter_joint_open,
	    repair_filter_joint_check,	    repair_filter_setup_traverse,  
	    repair_filter_update_traverse,  repair_filter_after_traverse)) < 0)
	    goto error_free_joint;
    } else 
	repair_set_flag(repair_data, REPAIR_NOT_FIXED);

    if ((res = repair_filter_update(&hint)))
	goto error_free_joint;

    if (reiser4_format_get_root(fs->format) != FAKE_BLK) {
	/* repair_data->pass.scan.(format_layout|used) are initialized from 
	 * repair_data->pass.filter.(format_layout|formatted) due to repair_data->pass 
	 * unit structure. */

	/* Solve overlapped problem within the tree. */
	if ((res = reiser4_joint_traverse(joint, &hint, repair_filter_joint_open,
	    repair_scan_node_check, NULL, NULL, NULL)) < 0)
	    goto error_free_joint;
    }

    if (joint)
	reiser4_joint_close(joint);

    return 0;
    
error_free_joint:
    if (joint)
	reiser4_joint_close(joint);

    return res;
}

reiser4_fs_t *repair_fs_open(aal_device_t *host_device, 
    reiser4_profile_t *profile) 
{
    reiser4_fs_t *fs;
    void *oid_area_start, *oid_area_end;

    aal_assert("vpf-159", host_device != NULL, return NULL);
    aal_assert("vpf-172", profile != NULL, return NULL);
 
    /* Allocating memory and initializing fields */
    if (!(fs = aal_calloc(sizeof(*fs), 0)))
	return NULL;

    if ((fs->master = repair_master_open(host_device)) == NULL)
	goto error_free_fs;
	
    if ((fs->format = repair_format_open(fs->master, profile)) == NULL)
	goto error_close_master;
    
    /* Block and oid allocator plugins are specified by format plugin unambiguously, 
     * so there is nothing to be checked additionally here. */
    if ((fs->alloc = reiser4_alloc_open(fs->format, 
	reiser4_format_get_len(fs->format))) == NULL) 
    {
	aal_exception_fatal("Failed to open a block allocator.");
	goto error_close_format;
    }

    if ((fs->oid = reiser4_oid_open(fs->format)) == NULL) {	
	aal_exception_fatal("Failed to open an object id allocator.");
	goto error_close_alloc;
    }
    
    return fs;

error_close_alloc:
    reiser4_alloc_close(fs->alloc);
    
error_close_format:
    reiser4_format_close(fs->format);
    
error_close_master:
    reiser4_master_close(fs->master);

error_free_fs:
    aal_free(fs);

    return NULL;
}

errno_t repair_fs_sync(reiser4_fs_t *fs) {
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
void repair_fs_close(reiser4_fs_t *fs) {
    aal_assert("vpf-174", fs != NULL, return);

    reiser4_oid_close(fs->oid);

    reiser4_alloc_close(fs->alloc);
    reiser4_format_close(fs->format);
    reiser4_master_close(fs->master);

    /* Freeing memory occupied by fs instance */
    aal_free(fs);
}

