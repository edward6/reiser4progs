/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/node.h -- reiserfs node recovery structures and macros. */

#ifndef REPAIR_NODE_H
#define REPAIR_NODE_H

#include <repair/repair.h>

typedef errno_t (*repair_extent_func_t)(ptr_hint_t *, void *);
typedef errno_t (*node_func_t)(reiser4_place_t *, void *);

extern reiser4_node_t *repair_node_open(reiser4_tree_t *tree,
					blk_t blk, bool_t check);

extern errno_t repair_node_check_struct(reiser4_node_t *node,
					uint8_t mode);

extern errno_t repair_node_traverse(reiser4_node_t *node, 
				    node_func_t func, void *data);

extern errno_t repair_node_merge(reiser4_node_t *dst, pos_t *dst_pos, 
				 reiser4_node_t *src, pos_t *src_pos, 
				 merge_hint_t *hint);
#endif
