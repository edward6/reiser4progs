/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/node.h -- reiserfs node recovery structures and macros. */

#ifndef REPAIR_NODE_H
#define REPAIR_NODE_H

#include <repair/repair.h>

extern reiser4_node_t *repair_node_open(reiser4_tree_t *tree,
					blk_t blk, uint32_t mkid);

extern errno_t repair_node_check_level(reiser4_node_t *node,
				       uint8_t mode);

extern errno_t repair_node_check_struct(reiser4_node_t *node, 
					place_func_t func,
					uint8_t mode,
					void *data);

extern errno_t repair_node_clear_flags(reiser4_node_t *node);

extern errno_t repair_node_pack(reiser4_node_t *node,
				aal_stream_t *stream);

extern reiser4_node_t *repair_node_unpack(reiser4_tree_t *tree,
					  aal_stream_t *stream);

extern void repair_node_print(reiser4_node_t *node,
			      aal_stream_t *stream);

#endif
