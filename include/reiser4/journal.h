/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   journal.h -- reiser4 filesystem journal functions. */

#ifndef REISER4_JOURNAL_H
#define REISER4_JOURNAL_H

#ifndef ENABLE_STAND_ALONE
#include <reiser4/types.h>

extern reiser4_journal_t *reiser4_journal_open(reiser4_fs_t *fs,
					       aal_device_t *device);

extern errno_t reiser4_journal_mark(reiser4_journal_t *journal);

extern reiser4_journal_t *reiser4_journal_create(reiser4_fs_t *fs,
						 aal_device_t *device,
						 void *hint);

extern errno_t reiser4_journal_sync(reiser4_journal_t *journal);
extern errno_t reiser4_journal_replay(reiser4_journal_t *journal);

extern errno_t reiser4_journal_print(reiser4_journal_t *journal,
				     aal_stream_t *stream);

extern void reiser4_journal_close(reiser4_journal_t *journal);
extern void reiser4_journal_mkdirty(reiser4_journal_t *journal);
extern void reiser4_journal_mkclean(reiser4_journal_t *journal);
extern errno_t reiser4_journal_valid(reiser4_journal_t *journal);
extern bool_t reiser4_journal_isdirty(reiser4_journal_t *journal);

extern errno_t reiser4_journal_layout(reiser4_journal_t *journal, 
				      block_func_t func,
				      void *data);

#endif

#endif

