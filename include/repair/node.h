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

extern errno_t repair_node_open(reiser4_node_t *node, blk_t blk, void *data);

extern errno_t repair_node_check(reiser4_node_t *node, repair_check_t *data);
extern errno_t repair_node_ld_key(reiser4_key_t *ld_key, repair_check_t *data, 
    uint8_t path_length);
extern errno_t repair_node_rd_key(reiser4_key_t *rd_key, repair_check_t *data, 
    uint8_t path_length);
extern errno_t repair_node_handle_pointers(reiser4_node_t *node, 
    repair_check_t *data);

#endif

