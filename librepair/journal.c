/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/journal.c - methods are needed for the work with broken reiser4
   journals. */

#include <repair/librepair.h>
#include <fcntl.h>

/* Callback for journal check method - check if a block, pointed from the 
   journal, is of the special filesystem areas - skipped, block allocator,
   oid alocator, etc. */

static errno_t cb_journal_check(void *object, region_func_t func, void *data) {
	reiser4_fs_t *fs = (reiser4_fs_t *)object;
	
	aal_assert("vpf-737", fs != NULL);
	return reiser4_fs_layout(fs, func, data);
}

/* Checks the opened journal. */
static errno_t repair_journal_check_struct(reiser4_journal_t *journal) {
	aal_assert("vpf-460", journal != NULL);
	aal_assert("vpf-736", journal->fs != NULL);
	
	return plug_call(journal->ent->plug->o.journal_ops, 
			 check_struct, journal->ent, 
			 cb_journal_check, journal->fs);
}

/* Open the journal and check it. */
errno_t repair_journal_open(reiser4_fs_t *fs, aal_device_t *journal_device,
			    uint8_t mode) 
{
	reiser4_plug_t *plug;
	errno_t res = 0;
	rid_t pid;
	
	aal_assert("vpf-445", fs != NULL);
	aal_assert("vpf-446", fs->format != NULL);
	aal_assert("vpf-476", journal_device != NULL);
	
	/* Try to open the journal. */
	if ((fs->journal = reiser4_journal_open(fs, journal_device)) == NULL) {
		/* failed to open a journal. Build a new one. */
		aal_error("Failed to open a journal by its id (0x%x).",
			  reiser4_format_journal_pid(fs->format));
		
		if (mode != RM_BUILD)
			return RE_FATAL;
		
		if ((pid = reiser4_format_journal_pid(fs->format)) == INVAL_PID) {
			aal_error("Invalid journal plugin id has been found.");
			return -EINVAL;
		}
		
		if (!(plug = reiser4_factory_ifind(JOURNAL_PLUG_TYPE, pid)))  {
			aal_error("Cannot find journal plugin by its id 0x%x.",
				  pid);
			return -EINVAL;
		}
		
		if (aal_yesno("Do you want to create a new journal (%s)?",
					plug->label) == EXCEPTION_OPT_NO)
		{
			return -EINVAL;
		}
	    
		if (!(fs->journal = reiser4_journal_create(fs, journal_device))) {
			aal_error("Cannot create a journal by its id (0x%x).",
				  reiser4_format_journal_pid(fs->format));
			return -EINVAL;
		}
	} else {    
		/* Check the structure of the opened journal or rebuild it if needed. */
		if ((res = repair_journal_check_struct(fs->journal)))
			goto error_journal_close;
	}
	
	return 0;
	
 error_journal_close:
	reiser4_journal_close(fs->journal);
	fs->journal = NULL;
	
	return res;
}

void repair_journal_invalidate(reiser4_journal_t *journal) {
	aal_assert("vpf-1555", journal != NULL);

	plug_call(journal->ent->plug->o.journal_ops,
		  invalidate, journal->ent);
}

void repair_journal_print(reiser4_journal_t *journal, aal_stream_t *stream) {
	aal_assert("umka-1564", journal != NULL);
	aal_assert("umka-1565", stream != NULL);

	plug_call(journal->ent->plug->o.journal_ops,
		  print, journal->ent, stream, 0);
}
