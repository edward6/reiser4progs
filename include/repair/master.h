/*
    repair/master.h -- reiserfs master superblock recovery structures and macros.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#ifndef REPAIR_MASTER_H
#define REPAIR_MASTER_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern reiser4_master_t *repair_master_open(aal_device_t *host_device);

#endif
