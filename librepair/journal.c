/*
    librepair/journal.c - methods are needed for the work with broken reiser4 journals.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/librepair.h>

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

    if (plugin_call(journal->entity->plugin->journal_ops, check, 
	journal->entity, callback_fs_check, journal->fs)) 
    {
	aal_exception_error("Failed to recover the journal (%s) on (%s).", 
	    journal->entity->plugin->h.label, aal_device_name(journal->device));
	return -1;
    }
    
    return 0;	    
}
/* Open the journal and check it. */
errno_t repair_journal_open(reiser4_fs_t *fs, aal_device_t *journal_device) {
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
	    return -1;
	}
    }
    
    /* Check the structure of the opened journal or rebuild it if needed. */
    if (repair_journal_check(fs->journal))
	goto error_journal_close;
    
    return 0;
    
error_journal_close:
    reiser4_journal_close(fs->journal);
    fs->journal = NULL;

    return -1;
}

/* Open, replay, close journal. */
errno_t repair_journal_handle(reiser4_fs_t *fs, aal_device_t *journal_device) {
    errno_t ret = 0;
 
    if (repair_journal_open(fs, journal_device))
	return -1;

    /* FIXME-VITALY: What if we do it on RO partition? */
    if (reiser4_journal_replay(fs->journal))
	ret = -1;

    reiser4_journal_close(fs->journal);
    fs->journal = NULL;

    return ret;
}

