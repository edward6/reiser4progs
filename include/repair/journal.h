/*
    repair/journal.h -- reiserfs journal recovery structures and macros.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#ifndef REPAIR_JOURNAL_H
#define REPAIR_JOURNAL_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_journal_open(reiser4_fs_t *fs);

#endif

