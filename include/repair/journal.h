/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/journal.h -- reiserfs journal recovery structures and macros. */

#ifndef REPAIR_JOURNAL_H
#define REPAIR_JOURNAL_H

#include <repair/repair.h>

extern errno_t repair_journal_print(reiser4_journal_t *journal,
				    aal_stream_t *stream);

extern errno_t repair_journal_open(reiser4_fs_t *fs, 
				   aal_device_t *journal_device, 
				   uint8_t mode);

extern errno_t repair_journal_replay(reiser4_journal_t *journal, 
				     aal_device_t *device);
#endif
