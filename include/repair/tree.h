/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/tree.h -- reiserfs tree recovery structures and macros. */

#ifndef REPAIR_TREE_H
#define REPAIR_TREE_H

#include <repair/repair.h>

extern errno_t repair_tree_parent_rkey(reiser4_tree_t *tree, node_t *node, 
				       reiser4_key_t *rd_key);

extern errno_t repair_tree_parent_lkey(reiser4_tree_t *tree, node_t *node, 
				       reiser4_key_t *ld_key); 

extern errno_t repair_tree_dknode_check(reiser4_tree_t *tree, 
					node_t *node, uint8_t mode);

extern errno_t repair_tree_insert(reiser4_tree_t *tree, place_t *place);
extern errno_t repair_tree_attach(reiser4_tree_t *tree, node_t *node);

extern bool_t repair_tree_legal_level(reiser4_item_group_t group,
				      uint8_t level);

extern bool_t repair_tree_data_level(uint8_t level);

#endif
