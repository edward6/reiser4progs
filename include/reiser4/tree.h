/*
  tree.h -- reiser4 balanced tree functions.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef REISER4_TREE_H
#define REISER4_TREE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/types.h>

extern void reiser4_tree_close(reiser4_tree_t *tree);
extern reiser4_tree_t *reiser4_tree_init(reiser4_fs_t *fs);

extern errno_t reiser4_tree_walk(reiser4_tree_t *tree,
				 reiser4_node_t *node,
				 walk_func_t walk_func);

extern errno_t reiser4_tree_adjust(reiser4_tree_t *tree);

extern errno_t reiser4_tree_connect(reiser4_tree_t *tree,
				     reiser4_node_t *parent,
				     reiser4_node_t *node);

extern errno_t reiser4_tree_disconnect(reiser4_tree_t *tree,
				       reiser4_node_t *parent,
				       reiser4_node_t *node);

extern reiser4_node_t *reiser4_tree_nbr(reiser4_tree_t *tree,
					reiser4_node_t *node,
					aal_direction_t where);

extern reiser4_node_t *reiser4_tree_child(reiser4_tree_t *tree,
					  reiser4_place_t *place);

extern reiser4_node_t *reiser4_tree_ltrt(reiser4_tree_t *tree,
					 reiser4_node_t *node,
					 aal_direction_t where);

#ifndef ENABLE_STAND_ALONE
extern void reiser4_tree_pack_set(reiser4_tree_t *tree,
				  pack_func_t func);

extern void reiser4_tree_pack_on(reiser4_tree_t *tree);
extern void reiser4_tree_pack_off(reiser4_tree_t *tree);

extern bool_t reiser4_tree_fresh(reiser4_tree_t *tree);
extern errno_t reiser4_tree_sync(reiser4_tree_t *tree);

extern errno_t reiser4_tree_growup(reiser4_tree_t *tree);
extern errno_t reiser4_tree_dryout(reiser4_tree_t *tree);

extern errno_t reiser4_tree_attach(reiser4_tree_t *tree,
				   reiser4_node_t *node);

extern errno_t reiser4_tree_detach(reiser4_tree_t *tree,
				   reiser4_node_t *node);

extern errno_t reiser4_tree_collapse(reiser4_tree_t *tree,
				     reiser4_node_t *node);

extern errno_t reiser4_tree_insert(reiser4_tree_t *tree,
				   reiser4_place_t *place,
				   uint8_t level,
				   create_hint_t *hint);

extern errno_t reiser4_tree_cut(reiser4_tree_t *tree,
				reiser4_place_t *start,
				reiser4_place_t *end);

extern errno_t reiser4_tree_remove(reiser4_tree_t *tree,
				   reiser4_place_t *place,
				   uint32_t count);

extern errno_t reiser4_tree_shift(reiser4_tree_t *tree,
				  reiser4_place_t *place,
				  reiser4_node_t *neig,
				  uint32_t flags);

extern errno_t reiser4_tree_shrink(reiser4_tree_t *tree,
				   reiser4_place_t *place);

extern errno_t reiser4_tree_expand(reiser4_tree_t *tree,
				   reiser4_place_t *place,
				   uint32_t needed,
				   uint32_t flags);

extern errno_t reiser4_tree_ukey(reiser4_tree_t *tree,
				 reiser4_place_t *place,
				 reiser4_key_t *key);

extern errno_t reiser4_tree_down(reiser4_tree_t *tree,
				 reiser4_node_t *node,
				 traverse_hint_t *hint,
				 traverse_open_func_t open_func,
				 traverse_edge_func_t before_func,
				 traverse_setup_func_t setup_func,
				 traverse_setup_func_t update_func,
				 traverse_edge_func_t after_func);

extern errno_t reiser4_tree_traverse(reiser4_tree_t *tree,
				     traverse_hint_t *hint,
				     traverse_open_func_t open_func,
				     traverse_edge_func_t before_func,
				     traverse_setup_func_t setup_func,
				     traverse_setup_func_t update_func,
				     traverse_edge_func_t after_func);

#endif

extern lookup_t reiser4_tree_lookup(reiser4_tree_t *tree,
			       reiser4_key_t *key,
			       uint8_t level,
			       reiser4_place_t *place);

extern blk_t reiser4_tree_root(reiser4_tree_t *tree);

extern uint8_t reiser4_tree_height(reiser4_tree_t *tree);

extern reiser4_node_t *reiser4_tree_alloc(reiser4_tree_t *tree,
					  uint8_t level);

extern errno_t reiser4_tree_release(reiser4_tree_t *tree,
				    reiser4_node_t *node);

extern reiser4_node_t *reiser4_tree_load(reiser4_tree_t *tree,
					 reiser4_node_t *parent,
					 blk_t blk);

extern errno_t reiser4_tree_unload(reiser4_tree_t *tree,
				   reiser4_node_t *node);

#endif

