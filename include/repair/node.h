/*
    repair/node.h -- reiserfs node recovery structures and macros.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_NODE_H
#define REPAIR_NODE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

typedef errno_t (*repair_extent_func_t)(reiser4_ptr_hint_t *, void *);
typedef errno_t (*traverse_item_func_t)(reiser4_place_t *, void *);

extern reiser4_node_t *repair_node_open(reiser4_fs_t *fs, blk_t blk);
extern errno_t repair_node_check(reiser4_node_t *node, aux_bitmap_t *bitmap);
extern errno_t repair_node_dkeys_check(reiser4_node_t *node, repair_data_t *rd);
extern errno_t repair_node_traverse(reiser4_node_t *node, 
    traverse_item_func_t func, void *data);
extern errno_t repair_node_rd_key(reiser4_node_t *node, reiser4_key_t *rd_key);

#endif

