/*
    format40_repair.c -- repair methods for the default disk-layout plugin for reiserfs 4.0.
  
    Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_ALONE

#include "format40.h"

static int callback_len_check(int64_t len, void *data) {
    if (len > *(int64_t *)data) {
	aal_exception_error("Invalid partition size was specified (%lld)", 
	    *(int64_t *)data);
	return 0;
    }
    
    return 1;
}

static int callback_tail_check(int64_t tail, void *data) {
    if (tail >= *(int *)data) {
	aal_exception_error("Invalid tail policy was specified (%ld)", 
	    *(int *)data);
	return 0;
    }
    
    return 1;
}

errno_t format40_check(object_entity_t *entity) {
    format40_super_t *super;
    format40_t *format = (format40_t *)entity;
    
    char *answer = NULL;
    long int result;
    int error, n = 0;
    
    aal_assert("vpf-160", entity != NULL);
    
    super = &format->super;
    
    /* Check the fs size. */
    if (aal_device_len(format->device) != get_sb_block_count(super)) {
	result = aal_device_len(format->device);
	    
	if (aal_exception_throw(EXCEPTION_ERROR, EXCEPTION_YESNO, 
	    "Number of blocks found in the superblock (%llu) is not equal to "
	    "the size of the partition (%llu).\nHave you used resizer?", 
	    get_sb_block_count(super), aal_device_len(format->device)) == 
	    EXCEPTION_NO) 
	{
	    aal_exception_error("Size of the partition was fixed to (%llu).", 
		aal_device_len(format->device));
	} else {
	    uint64_t len = aal_device_len(format->device);
		
	    result = aal_ui_get_numeric(aal_device_len(format->device), 
		callback_len_check, &len, "Enter the number of blocks on your "
		"partition");
	}
	set_sb_block_count(super, result);
    }

    /* Check the free block count. */
    if (get_sb_free_blocks(super) > get_sb_block_count(super)) {
	aal_exception_error("Invalid free block count (%llu) found in the "
	    "superblock. Zeroed.", get_sb_free_blocks(super));
	set_sb_free_blocks(super, 0);
    }
    
    /* Check the root block number. */
    if (get_sb_root_block(super) > get_sb_block_count(super)) {
	if (get_sb_root_block(super) == INVAL_BLK) {
	    aal_exception_error("FAKE root block number (%llu) found in the "
		"superblock.", get_sb_root_block(super));
	} else {
	    aal_exception_error("Invalid root block (%llu) found in the superblock."
		" Set to FAKE blocknumber.", get_sb_root_block(super));
	    set_sb_root_block(super, INVAL_BLK);
	}
    }
    
    /* Some extra check for root block? */    

    /* Check the tail policy. */
    if (get_sb_tail_policy(super) >= TAIL_LAST_ID) {
	int tail_id = TAIL_LAST_ID;
	aal_exception_error("Invalid tail policy (%u) found in the superblock.", 
	    get_sb_tail_policy(super));
	
	result = aal_ui_get_numeric(0, callback_tail_check, &tail_id, "Enter "
	    "the preferable tail policy (0-%u)",  TAIL_LAST_ID - 1);
	
	set_sb_tail_policy(super, result);
    }
    return 0;
}

#endif
