/*
    repair/add_missing.h -- the common structures and methods for insertion leaves
    and extent item from twigs unconnected from the tree.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef ADD_MISSING_H
#define ADD_MISSING_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

extern errno_t repair_add_missing_pass(repair_data_t *rd);
extern errno_t repair_add_missing_release(repair_data_t *rd);

#endif

