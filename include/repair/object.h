/*
    repair/object.h -- common structures and methods for object recovery.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_OBJECT_H
#define REPAIR_OBJECT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern bool_t repair_object_can_begin(reiser4_place_t *place);

#endif
