/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/node.h -- reiserfs node recovery structures and macros. */

#ifndef REPAIR_NODE_H
#define REPAIR_NODE_H

#include <repair/repair.h>

typedef errno_t (*node_func_t)(place_t *, void *);

extern errno_t repair_node_print(node_t *node,
				 aal_stream_t *stream);

extern node_t *repair_node_open(reiser4_tree_t *tree,
				blk_t blk, bool_t check);

extern errno_t repair_node_check_struct(node_t *node,
					uint8_t mode);

extern errno_t repair_node_traverse(node_t *node,
				    node_func_t func,
				    void *data);

extern errno_t repair_node_clear_flags(node_t *node);

extern errno_t repair_node_pack(node_t *node,
				aal_stream_t *stream,
				int mode);

extern node_t *repair_node_unpack(reiser4_tree_t *tree,
				  aal_stream_t *stream,
				  int mode);

#endif
