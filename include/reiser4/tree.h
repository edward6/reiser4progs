/*
  tree.h -- reiser4 balanced tree functions.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef TREE_H
#define TREE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/plugin.h>
#include <reiser4/filesystem.h>

extern void reiser4_tree_close(reiser4_tree_t *tree);
extern reiser4_tree_t *reiser4_tree_open(reiser4_fs_t *fs);

#ifndef ENABLE_COMPACT

extern reiser4_tree_t *reiser4_tree_create(reiser4_fs_t *fs, 
					   reiser4_profile_t *profile);

extern errno_t reiser4_tree_sync(reiser4_tree_t *tree);
extern errno_t reiser4_tree_flush(reiser4_tree_t *tree);

extern errno_t reiser4_tree_grow(reiser4_tree_t *tree);
extern errno_t reiser4_tree_dryup(reiser4_tree_t *tree);

extern errno_t reiser4_tree_attach(reiser4_tree_t *tree,
				   reiser4_node_t *node);

extern errno_t reiser4_tree_detach(reiser4_tree_t *tree,
				   reiser4_node_t *node);

extern errno_t reiser4_tree_insert(reiser4_tree_t *tree,
				   reiser4_coord_t *coord,
				   reiser4_item_hint_t *hint);

extern errno_t reiser4_tree_write(reiser4_tree_t *tree,
				  reiser4_coord_t *dst,
				  reiser4_coord_t *src,
				  uint32_t count);

extern errno_t reiser4_tree_cut(reiser4_tree_t *tree,
				reiser4_coord_t *start,
				reiser4_coord_t *end);

extern errno_t reiser4_tree_remove(reiser4_tree_t *tree,
				   reiser4_coord_t *coord,
				   uint32_t count);

extern errno_t reiser4_tree_shift(reiser4_tree_t *tree,
				  reiser4_coord_t *coord,
				  reiser4_node_t *node,
				  uint32_t flags);

extern errno_t reiser4_tree_shrink(reiser4_tree_t *tree,
				   reiser4_coord_t *coord);

extern errno_t reiser4_tree_expand(reiser4_tree_t *tree,
				   reiser4_coord_t *coord,
				   uint32_t needed);

extern errno_t reiser4_tree_traverse(reiser4_tree_t *tree,
				     traverse_hint_t *hint,
				     traverse_open_func_t open_func,
				     traverse_edge_func_t before_func,
				     traverse_setup_func_t setup_func,
				     traverse_setup_func_t update_func,
				     traverse_edge_func_t after_func);
#endif

extern int reiser4_tree_lookup(reiser4_tree_t *tree,
			       reiser4_key_t *key,
			       uint8_t stop,
			       reiser4_coord_t *coord);

extern blk_t reiser4_tree_root(reiser4_tree_t *tree);
extern uint8_t reiser4_tree_height(reiser4_tree_t *tree);

extern errno_t reiser4_tree_split(reiser4_tree_t *tree, 
				  reiser4_coord_t *coord, 
				  uint8_t level) ;

extern void reiser4_tree_release(reiser4_tree_t *tree,
				 reiser4_node_t *node);

extern reiser4_node_t *reiser4_tree_load(reiser4_tree_t *tree,
					 reiser4_node_t *parent,
					 blk_t blk);

extern reiser4_node_t *reiser4_tree_allocate(reiser4_tree_t *tree,
					     uint8_t level);

#endif

