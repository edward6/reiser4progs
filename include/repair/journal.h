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

extern errno_t repair_journal_handle(reiser4_format_t *format, 
    aal_device_t *journal_device);

#endif

