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

typedef errno_t (*repair_extent_func_t)(reiser4_ptr_hint_t *, void *);

extern reiser4_node_t *repair_node_open(reiser4_format_t *, blk_t);
extern errno_t repair_node_check(reiser4_node_t *, aux_bitmap_t *);
extern errno_t repair_node_dkeys_check(reiser4_node_t *, repair_data_t *);
extern errno_t repair_node_child_max_real_key(reiser4_coord_t *, 
    reiser4_key_t *);

#endif

