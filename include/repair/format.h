/*  Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
    
    repair/format.h -- reiserfs filesystem recovery structures and
    macros. */

#ifndef REPAIR_FORMAT_H
#define REPAIR_FORMAT_H

#include <reiser4/filesystem.h>

extern errno_t repair_format_open(reiser4_fs_t *fs, uint8_t mode);
extern errno_t repair_format_update(reiser4_format_t *format);

#endif
