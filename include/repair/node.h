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

typedef enum repair_item_flag {
    ITEM_REACHABLE  = 0x1,
    ITEM_CHECKED    = 0x2,
    ITEM_LAST_FLAG  = 0x3
} repair_item_flag_t;

typedef errno_t (*repair_extent_func_t)(ptr_hint_t *, void *);
typedef errno_t (*traverse_node_func_t)(reiser4_place_t *, void *);

extern reiser4_node_t *repair_node_open(reiser4_fs_t *fs, blk_t blk);
extern errno_t repair_node_check(reiser4_node_t *node, uint8_t mode);
extern errno_t repair_node_dkeys_check(reiser4_node_t *node, uint8_t mode);
extern errno_t repair_node_traverse(reiser4_node_t *node, 
    traverse_node_func_t func, void *data);
extern errno_t repair_node_rd_key(reiser4_node_t *node, reiser4_key_t *rd_key);
extern void repair_node_print(reiser4_node_t *node, uint32_t start, 
    uint32_t count, uint16_t options);
extern errno_t repair_node_copy(reiser4_node_t *dst, pos_t *dst_pos, 
    reiser4_node_t *src, pos_t *src_pos, copy_hint_t *hint);

extern void repair_node_set_flag(reiser4_node_t *node, uint32_t pos, 
    uint16_t flag);
extern void repair_node_clear_flag(reiser4_node_t *node, uint32_t pos, 
    uint16_t flag);
extern bool_t repair_node_test_flag(reiser4_node_t *node, uint32_t pos, 
    uint16_t flag);

#endif

