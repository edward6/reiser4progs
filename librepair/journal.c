/*
    librepair/jounrnal.c - methods are needed for the work with broken reiser4 journals.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include <repair/librepair.h>

static errno_t repair_journal_check(reiser4_journal_t *journal) {
    aal_assert("vpf-460", journal != NULL, return -1);

    /* FIXME-UMKA->VITALY: Where we should get filesystem layout function?
     * Actually we have one in fs API. But we have not fs access here. */
    if (plugin_call(journal->entity->plugin->journal_ops, check, 
	journal->entity, NULL))
    {
	aal_exception_error("Failed to recover the journal (%s) on (%s).", 
	    journal->entity->plugin->h.label, aal_device_name(journal->device));
	return -1;
    }
	    
    return 0;	    
}

reiser4_journal_t *repair_journal_open(reiser4_format_t *format, 
    aal_device_t *journal_device) 
{
    reiser4_journal_t *journal = NULL;

    aal_assert("vpf-446", format != NULL, return NULL);
    aal_assert("vpf-476", journal_device != NULL, return NULL);

    /* FIXME-UMKA->VITALY */
    
    /* Try to open the journal. */
    if ((journal = reiser4_journal_open(NULL/* Here should be fs instance*/, journal_device)) == NULL) {
	/* failed to open a journal. Build a new one. */
	aal_exception_fatal("Failed to open a journal by its id (0x%x). "
	    "Try to build a new one.", reiser4_format_journal_pid(format));
	
	if (!(journal = reiser4_journal_create(NULL/* Here should be fs instance*/, journal_device, NULL))) {
	    aal_exception_fatal("Cannot create a journal by its id (0x%x).", 
		reiser4_format_journal_pid(format));
	    return NULL;
	}
    }
    
    aal_assert("vpf-482", journal != NULL, goto error);
    
    /* Check the structure of the opened journal or rebuild it if needed. */
    if (repair_journal_check(journal))
	goto error_journal_close;
    
    return journal;
    
error_journal_close:
    reiser4_journal_close(journal);
error:
    return NULL;
}

/* Open, replay, close journal. */
errno_t repair_journal_handle(reiser4_format_t *format, 
    aal_device_t *journal_device) 
{
    reiser4_journal_t *journal = NULL;

    aal_assert("vpf-445", format != NULL, return -1);
    aal_assert("vpf-444", journal_device != NULL, return -1);
    
    if ((journal = repair_journal_open(format, journal_device)) == NULL)
	return -1;

    /* FIXME-VITALY: What if we do it on RO partition? */
    if (reiser4_journal_replay(journal))
	goto error_journal_close;

    reiser4_journal_close(journal);

    return 0;
    
error_journal_close:
    reiser4_journal_close(journal);

    return -1;
}
