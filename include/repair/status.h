/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   status.h -- filesystem status block functions. */

#ifndef REPAIR_STATUS_H
#define REPAIR_STATUS_H

#include <repair/repair.h>

extern errno_t repair_status_open(reiser4_fs_t *fs, uint8_t mode);
extern errno_t repair_status_clear(reiser4_fs_t *fs, uint8_t mode);

#endif
