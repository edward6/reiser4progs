/*
    repair/cut_tree.h -- the structures and methods needed for fsck pass1. 
    Copyright (C) 1996 - 2002 Hans Reiser
*/

#ifndef REPAIR_CUT_TREE_H
#define REPAIR_CUT_TREE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_cut_tree_node_open(reiser4_node_t *node, blk_t blk, 
    void *data);
extern errno_t repair_cut_tree_before_traverse(reiser4_node_t *node, 
    reiser4_item_t *item, void *data);
extern errno_t repair_cut_tree_after_traverse(reiser4_node_t *node, 
    reiser4_item_t *item, void *data);
extern errno_t repair_cut_tree_setup_traverse(reiser4_node_t *node, 
    reiser4_item_t *item, void *data);
extern errno_t repair_cut_tree_update_traverse(reiser4_node_t *node, 
    reiser4_item_t *item, void *data);
extern errno_t repair_cut_tree_node_check(reiser4_node_t *node, 
    void *data);
extern errno_t repair_cut_tree_setup(reiser4_fs_t *fs, repair_check_t *data);
extern errno_t repair_cut_tree_update(reiser4_fs_t *fs, repair_check_t *data);

#endif
