/*
    librepair/journal.c - methods are needed for the work with broken reiser4 journals.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/librepair.h>
#include <fcntl.h>

/* Callback for journal check method - check if a block, pointed from the 
 * journal, is of the special filesystem areas - skipped, block allocator, 
 * oid alocator, etc. */
static errno_t callback_fs_check(void *layout, block_func_t func, 
    void *data) 
{
    reiser4_fs_t *fs = (reiser4_fs_t *)layout;
    
    aal_assert("vpf-737", fs != NULL);

    return reiser4_fs_layout(fs, func, data);
}

/* Checks the opened journal. */
static errno_t repair_journal_check(reiser4_journal_t *journal) {
    aal_assert("vpf-460", journal != NULL);
    aal_assert("vpf-736", journal->fs != NULL);

    return plugin_call(journal->entity->plugin->o.journal_ops, check, 
	journal->entity, callback_fs_check, journal->fs);
}
/* Open the journal and check it. */
errno_t repair_journal_open(reiser4_fs_t *fs, aal_device_t *journal_device, 
    uint8_t mode) 
{
    reiser4_plugin_t *plugin;
    errno_t ret = REPAIR_OK;
    rid_t pid;
    
    aal_assert("vpf-445", fs != NULL);
    aal_assert("vpf-446", fs->format != NULL);
    aal_assert("vpf-476", journal_device != NULL);

    /* Try to open the journal. */
    if ((fs->journal = reiser4_journal_open(fs, journal_device)) == NULL) {
	/* failed to open a journal. Build a new one. */
	aal_exception_fatal("Failed to open a journal by its id (0x%x). %s", 
	    reiser4_format_journal_pid(fs->format));
	
	if (mode != REPAIR_REBUILD)
	    return REPAIR_FATAL;

	if ((pid = reiser4_format_journal_pid(fs->format)) == INVAL_PID) {
	    aal_exception_error("Invalid journal plugin id has been found.");
	    return -EINVAL;
	}
    
	if (!(plugin = libreiser4_factory_ifind(JOURNAL_PLUGIN_TYPE, pid)))  {
	    aal_exception_error("Cannot find journal plugin by its id 0x%x.", 
		pid);
	    return -EINVAL;
	}
	
	if (aal_exception_throw(EXCEPTION_ERROR, EXCEPTION_YESNO, "Do you want "
	    "to create a new journal (%s)?", plugin->h.label) == EXCEPTION_NO)
	    return -EINVAL;
	    
	if (!(fs->journal = reiser4_journal_create(fs, journal_device, NULL))) {
	    aal_exception_fatal("Cannot create a journal by its id (0x%x).", 
		reiser4_format_journal_pid(fs->format));
	    return -EINVAL;
	}
    } else {    
	/* Check the structure of the opened journal or rebuild it if needed. */
	ret = repair_journal_check(fs->journal);

	if (repair_error_exists(ret))
	    goto error_journal_close;

	if (ret & REPAIR_FIXED)
	    reiser4_journal_mkdirty(fs->journal);
    }
    
    return 0;
    
error_journal_close:
    reiser4_journal_close(fs->journal);
    fs->journal = NULL;

    return ret;
}

errno_t repair_journal_replay(reiser4_journal_t *journal, aal_device_t *device) 
{
    errno_t ret = REPAIR_OK;
    int j_flags, flags;
    
    aal_assert("vpf-906", journal != NULL);
    aal_assert("vpf-907", journal->device != NULL);
    aal_assert("vpf-908", device != NULL);
    
    if (repair_error_exists(ret))
	return ret;
    
    j_flags = journal->device->flags;
    flags = device->flags;

    if (aal_device_reopen(journal->device, device->blocksize, O_RDWR))
	return -EIO;
    
    if (aal_device_reopen(device, device->blocksize, O_RDWR))
	return -EIO;

    if (reiser4_journal_replay(journal))
	return -EINVAL;
    
    if (aal_device_reopen(device, device->blocksize, flags))
	return -EIO;
    
    if (aal_device_reopen(journal->device, device->blocksize, j_flags))
	return -EIO;

    return 0;
}

