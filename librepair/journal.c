/*
    librepair/jounrnal.c - methods are needed for the work with broken reiser4 journals.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include <repair/librepair.h>

static errno_t repair_journal_check(reiser4_fs_t *fs) {
    return 0;
}

errno_t repair_journal_open(reiser4_fs_t *fs) {
    errno_t res;

    aal_assert("vpf-445", fs != NULL, return -1);
    aal_assert("vpf-446", fs->data != NULL, return -1);
    
    /* Try to open the journal. */
    if ((fs->journal = reiser4_journal_open(fs->format, 
	repair_data(fs)->journal_device)) == NULL)
    {
	aal_exception_fatal("Failed to open a journal.");
	return -1;
    }
	
    /* Check the structure of the opened journal. */
    if ((res = repair_journal_check(fs)))
	goto error_close_journal;
	    
    return 0;
    
error_close_journal:
    reiser4_journal_close(fs->journal);
    
    return res;

}
