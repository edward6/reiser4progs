/*
    librepair/master.c - methods are needed for work with broken master 
    super block.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/librepair.h>

static int callback_bs_check (int64_t val, void * data) {
    if (!aal_pow_of_two(val))
	return 0;
    
    if (val < 512)
	return 0;

    return 1;
}

static errno_t repair_master_check(reiser4_fs_t *fs) {
    uint16_t blocksize = 0;
    int error = 0;
    reiser4_plugin_t *plugin;

    aal_assert("vpf-730", fs != NULL);
    aal_assert("vpf-161", fs->master != NULL || fs->device != NULL);
 
    if (fs->master == NULL) {
	/* Master SB was not opened. Create a new one. */
	if (aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_YESNO, 
	    "Master super block cannot be found. Do you want to build a new "
	    "one on (%s)?", aal_device_name(fs->device)) == EXCEPTION_NO)
	    return -1;

	blocksize = aal_ui_get_numeric(4096, callback_bs_check, NULL, 
	    "Which block size do you use?");

	/* 
	    FIXME-VITALY: What should be done here with uuid and label? 
	    At least not here as uiud and label seem to be on the wrong place.
	    Move them to specific SB.
	*/

	/* Create a new master SB. */
	if (!(fs->master = reiser4_master_create(fs->device, INVAL_PID, 
	    blocksize, NULL, NULL))) 
	{
	    aal_exception_fatal("Cannot create a new master super block.");
	    return -1;
	} else 
	    /* Will be printed if verbose. */
	    aal_exception_info("A new master superblock was created on (%s).", 
		aal_device_name(fs->device));
    } else {
	/* Master SB was opened. Check it for validness. */

	/* Check the blocksize. */
	if (!aal_pow_of_two(reiser4_master_blocksize(fs->master))) {
	    aal_exception_fatal("Invalid blocksize found in the master super "
		"block (%u).", reiser4_master_blocksize(fs->master));

	    blocksize = aal_ui_get_numeric(4096, callback_bs_check, NULL, 
		"Which block size do you use?");

	    set_mr_blocksize(fs->master->super, blocksize);
	} 
    }

    /* Setting actual used block size from master super block */
    if (aal_device_set_bs(fs->device, reiser4_master_blocksize(fs->master))) {
        aal_exception_fatal("Invalid block size was specified (%u). It must "
	    "be power of two.", reiser4_master_blocksize(fs->master));
	return -1;
    }
    
    return 0;
}

errno_t repair_master_open(reiser4_fs_t *fs) {
    aal_assert("vpf-399", fs != NULL);
    aal_assert("vpf-729", fs->device != NULL);

    /* Try to open master. */
    fs->master = reiser4_master_open(fs->device);

    /* Either check the opened master or build a new one. */
    if (repair_master_check(fs))
	goto error_master_free;
 
    return 0;

error_master_free:
    if (fs->master) {
	reiser4_master_close(fs->master);
	fs->master = NULL;
    }
 
    return -1;
}

