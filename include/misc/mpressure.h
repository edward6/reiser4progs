/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   mpressure.c -- memory pressure detect functions common for all reiser4progs.
   Probably here should be more reliable method to determine memory pressure. */

#ifndef MISC_MPRESSURE_H
#define MISC_MPRESSURE_H

#include <reiser4/types.h>

extern void misc_mpressure_setup(uint32_t value);
extern int misc_mpressure_detect(reiser4_tree_t *tree);

#endif
