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

extern errno_t repair_joint_ld_key(reiser4_joint_t*joint, reiser4_key_t *ld_key, 
    repair_check_t *data);
extern errno_t repair_joint_rd_key(reiser4_joint_t *joint, reiser4_key_t *rd_key, 
    repair_check_t *data);

extern errno_t repair_node_handle_pointers(reiser4_node_t *node, 
    repair_check_t *data);

#endif

