/*
    librepair/jounrnal.c - methods are needed for the work with broken reiser4 journals.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include <repair/librepair.h>

static errno_t repair_journal_check(reiser4_journal_t *journal) {
    aal_assert("vpf-460", journal != NULL, return -1);
    
    return plugin_call(return -1, journal->entity->plugin->journal_ops, check, 
	journal->entity);
}

static reiser4_journal_t *repair_journal_open(reiser4_format_t *format, 
    aal_device_t *journal_device) 
{
    reiser4_journal_t *journal;
    errno_t res;

    aal_assert("vpf-446", format != NULL, return NULL);
    aal_assert("vpf-476", journal_device != NULL, return NULL);

    /* Try to open the journal. */
    if ((journal = reiser4_journal_open(format, journal_device)) == NULL) {
	aal_exception_fatal("Failed to open a journal.");
	return NULL;
    }
	
    /* Check the structure of the opened journal. */
    if (repair_journal_check(journal))
	goto error_close_journal;
	    
    return journal;
    
error_close_journal:
    reiser4_journal_close(journal);
    
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
	goto error_close_journal;

    reiser4_journal_close(journal);

    return 0;
    
error_close_journal:
    reiser4_journal_close(journal);

    return -1;
}
