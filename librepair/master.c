/*
    librepair/master.c - methods are needed for work with broken master 
    super block.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include <repair/librepair.h>

static int callback_bs_check (int64_t val, void * data) {
    if (!aal_pow_of_two(val))
	return 0;
    
    if (val < 512)
	return 0;

    return 1;
}

static errno_t repair_master_check(reiser4_master_t **master, aal_device_t *host_device) {
    uint16_t blocksize = 0;
    int error = 0;
    reiser4_plugin_t *plugin;
   
    aal_assert("vpf-161", master != NULL, return -1);
    aal_assert("vpf-164", *master != NULL || host_device != NULL, return -1);
    
    if (*master == NULL) {
	/* Master SB was not opened. Create a new one. */
	if (aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_YESNO, 
	    "Master super block cannot be found. Do you want to build a new "
	    "one on (%s)?", aal_device_name(host_device)) == 
	    EXCEPTION_NO) 
	    return -1;

	blocksize = aal_ui_get_numeric(4096, callback_bs_check, NULL, 
	    "Which block size do you use?");

	/* 
	    FIXME-VITALY: What should be done here with uuid and label? 
	    At least not here as uiud and label seem to be on the wrong place.
	    Move them to specific SB.
	*/
	
	/* Create a new master SB. */
	if (!(*master = reiser4_master_create(host_device, FAKE_PLUGIN, 
	    blocksize, NULL, NULL))) 
	{
	    aal_exception_fatal("Cannot create a new master super block.");
	    return -1;
	} else 
	    /* Will be printed if verbose. */
	    aal_exception_info("A new master superblock was created on (%s).", 
		aal_device_name(host_device));
    } else {
	/* Master SB was opened. Check it for validness. */

	/* Check the blocksize. */
	if (!aal_pow_of_two(reiser4_master_blocksize(*master))) {
	    aal_exception_fatal("Invalid blocksize found in the master super "
		"block (%u).", reiser4_master_blocksize(*master));
	    
	    blocksize = aal_ui_get_numeric(4096, callback_bs_check, NULL, 
		"Which block size do you use?");

	    set_mr_blocksize((*master)->super, blocksize);
	} 
    }

    /* Setting actual used block size from master super block */
    if (aal_device_set_bs(host_device, reiser4_master_blocksize(*master))) 
    {
        aal_exception_fatal("Invalid block size was specified (%u). It must "
	    "be power of two.", reiser4_master_blocksize(*master));
	return -1;
    }
    
    return 0;
}    

reiser4_master_t *repair_master_open(aal_device_t *host_device) {
    reiser4_master_t *master = NULL;
    int res;
    
    aal_assert("vpf-399", host_device != NULL, return NULL);
    
    /* Try to open master. */
    master = reiser4_master_open(host_device);
	
    /* Either check the opened master or build a new one. */
    if (repair_master_check(&master, host_device))
	goto error_free_master;
    
    aal_assert("vpf-477", master != NULL, res = -1; goto error);
    
    return master;
    
error_free_master:
    if (master)
	reiser4_master_close(master);
error:    
    return NULL;
}

