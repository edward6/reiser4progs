/*
    repair/node.h -- reiserfs node recovery structures and macros.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#ifndef REPAIR_NODE_H
#define REPAIR_NODE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_joint_check(reiser4_joint_t *joint, repair_check_t *data);
extern errno_t repair_joint_dkeys_check(reiser4_joint_t *joint, 
    repair_check_t *data);

#endif

