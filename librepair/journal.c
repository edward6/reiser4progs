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
    errno_t ret;
    
    aal_assert("vpf-460", journal != NULL);
    aal_assert("vpf-736", journal->fs != NULL);

    if ((ret = plugin_call(journal->entity->plugin->journal_ops, check, 
	journal->entity, callback_fs_check, journal->fs)))
    {
	aal_exception_error("Failed to recover the journal (%s) on (%s).", 
	    journal->entity->plugin->h.label, aal_device_name(journal->device));
	return ret;
    }
    
    return 0;	    
}
/* Open the journal and check it. */
errno_t repair_journal_open(reiser4_fs_t *fs, aal_device_t *journal_device) {
    errno_t ret = 0;
    
    aal_assert("vpf-445", fs != NULL);
    aal_assert("vpf-446", fs->format != NULL);
    aal_assert("vpf-476", journal_device != NULL);

    /* Try to open the journal. */
    if ((fs->journal = reiser4_journal_open(fs, journal_device)) == NULL) {
	/* failed to open a journal. Build a new one. */
	aal_exception_fatal("Failed to open a journal by its id (0x%x). "
	    "Try to build a new one.", reiser4_format_journal_pid(fs->format));
	
	if (!(fs->journal = reiser4_journal_create(fs, journal_device, NULL))) 
	{
	    aal_exception_fatal("Cannot create a journal by its id (0x%x).", 
		reiser4_format_journal_pid(fs->format));
	    return -EINVAL;
	}
    }
    
    /* Check the structure of the opened journal or rebuild it if needed. */
    if ((ret = repair_journal_check(fs->journal)))
	goto error_journal_close;
    
    return 0;
    
error_journal_close:
    reiser4_journal_close(fs->journal);
    fs->journal = NULL;

    return ret;
}

/* Open, replay, close journal. */
errno_t repair_journal_handle(reiser4_fs_t *fs, aal_device_t *journal_device) {    
    errno_t ret = 0;
    int flags;
 
    if ((ret = repair_journal_open(fs, journal_device)))
	return ret;
    
    flags = journal_device->flags;
    if (aal_device_reopen(journal_device, journal_device->blocksize, O_RDWR))
	return -EIO;
    
    if ((ret = reiser4_journal_replay(fs->journal)))
	ret = ret;

    /* FIXME-UMKA->VITALY: Here should be also reopening format and master super
     * blocks due to them might be in replayed transactions and we should keep
     * them uptodate */
    
    if (aal_device_reopen(journal_device, journal_device->blocksize, flags))
	return -EIO;
    
    reiser4_journal_close(fs->journal);
    fs->journal = NULL;

    return 0;
}

