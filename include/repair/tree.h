/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/tree.h -- reiserfs tree recovery structures and macros. */

#ifndef REPAIR_TREE_H
#define REPAIR_TREE_H

#include <repair/repair.h>

extern errno_t repair_tree_parent_rkey(reiser4_tree_t *tree,
				       reiser4_node_t *node, 
				       reiser4_key_t *rd_key);

extern errno_t repair_tree_parent_lkey(reiser4_tree_t *tree,
				       reiser4_node_t *node, 
				       reiser4_key_t *ld_key); 

extern reiser4_node_t *repair_tree_load_node(reiser4_tree_t *tree, 
				     reiser4_node_t *parent,
				     blk_t blk, uint32_t mkid);

extern errno_t repair_tree_dknode_check(reiser4_tree_t *tree, 
					reiser4_node_t *node,
					uint8_t mode);

extern errno_t repair_tree_insert(reiser4_tree_t *tree,
				  reiser4_place_t *place,
				  region_func_t func, void *data);

extern errno_t repair_tree_attach_node(reiser4_tree_t *tree,
				       reiser4_node_t *node);

extern bool_t repair_tree_data_level(uint8_t level);
extern bool_t repair_tree_legal_level(reiser4_plug_t *plug, uint8_t level);

#endif
