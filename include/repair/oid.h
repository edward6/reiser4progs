/*
    repair/oid.h -- reiser4 oid allocator recovery structures and macros.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#ifndef REPAIR_OID_H
#define REPAIR_OID_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_oid_check(reiser4_fs_t *fs);

#endif
