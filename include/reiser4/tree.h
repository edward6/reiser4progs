/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tree.h -- reiser4 balanced tree functions. */

#ifndef REISER4_TREE_H
#define REISER4_TREE_H

#include <reiser4/types.h>

extern void reiser4_tree_close(reiser4_tree_t *tree);
extern errno_t reiser4_tree_adjust(reiser4_tree_t *tree);
extern errno_t reiser4_tree_collapse(reiser4_tree_t *tree);
extern reiser4_tree_t *reiser4_tree_init(reiser4_fs_t *fs);

extern int64_t reiser4_tree_read(reiser4_tree_t *tree,
				 reiser4_place_t *place,
				 trans_hint_t *hint);

extern int64_t reiser4_tree_fetch(reiser4_tree_t *tree,
				  reiser4_place_t *place,
				  trans_hint_t *hint);

extern errno_t reiser4_tree_walk_node(reiser4_tree_t *tree,
				      reiser4_node_t *node,
#ifndef ENABLE_STAND_ALONE
				      walk_func_t pre_func,
				      walk_on_func_t on_func,
#endif
				      walk_func_t post_func);

extern errno_t reiser4_tree_next_place(reiser4_tree_t *tree, 
				       reiser4_place_t *place,
				       reiser4_place_t *next);

extern errno_t reiser4_tree_place_key(reiser4_tree_t *tree, 
				      reiser4_place_t *place,
				      reiser4_key_t *key);

extern errno_t reiser4_tree_connect_node(reiser4_tree_t *tree,
					 reiser4_node_t *parent,
					 reiser4_node_t *node);

extern errno_t reiser4_tree_realize_node(reiser4_tree_t *tree,
					 reiser4_node_t *node);

extern errno_t reiser4_tree_discard_node(reiser4_tree_t *tree,
					 reiser4_node_t *node);

extern errno_t reiser4_tree_disconnect_node(reiser4_tree_t *tree,
					    reiser4_node_t *node);

extern reiser4_node_t *reiser4_tree_child_node(reiser4_tree_t *tree,
					       reiser4_place_t *place);

extern inline uint32_t reiser4_tree_target_level(reiser4_tree_t *tree,
						 reiser4_plug_t *plug);

#ifndef ENABLE_STAND_ALONE
extern void reiser4_tree_fini(reiser4_tree_t *tree);
extern bool_t reiser4_tree_fresh(reiser4_tree_t *tree);
extern bool_t reiser4_tree_minimal(reiser4_tree_t *tree);
extern bool_t reiser4_tree_singular(reiser4_tree_t *tree);
extern errno_t reiser4_tree_sync(reiser4_tree_t *tree);
extern errno_t reiser4_tree_growup(reiser4_tree_t *tree);
extern errno_t reiser4_tree_dryout(reiser4_tree_t *tree);
extern errno_t reiser4_tree_compress(reiser4_tree_t *tree);

extern void reiser4_tree_set_root(reiser4_tree_t *tree,
				  blk_t blk);

extern void reiser4_tree_set_height(reiser4_tree_t *tree,
				    uint8_t height);

extern int64_t reiser4_tree_insert(reiser4_tree_t *tree,
				   reiser4_place_t *place,
				   trans_hint_t *hint,
				   uint8_t level);

extern int64_t reiser4_tree_write(reiser4_tree_t *tree,
				  reiser4_place_t *place,
				  trans_hint_t *hint,
				  uint8_t level);

extern errno_t reiser4_tree_remove(reiser4_tree_t *tree,
				   reiser4_place_t *place,
				   trans_hint_t *hint);

extern errno_t reiser4_tree_shift(reiser4_tree_t *tree,
				  reiser4_place_t *place,
				  reiser4_node_t *neig,
				  uint32_t flags);

extern errno_t reiser4_tree_shrink(reiser4_tree_t *tree,
				   reiser4_place_t *place);

extern int32_t reiser4_tree_expand(reiser4_tree_t *tree,
				   reiser4_place_t *place,
				   reiser4_place_t *parent,
				   uint32_t needed,
				   uint32_t flags);

extern errno_t reiser4_tree_trav(reiser4_tree_t *tree,
				 tree_open_func_t open_func,
				 tree_edge_func_t before_func,
				 tree_update_func_t update_func,
				 tree_edge_func_t after_func,
				 void *data);

extern errno_t reiser4_tree_update_keys(reiser4_tree_t *tree,
					reiser4_place_t *place,
					reiser4_key_t *key);

extern errno_t reiser4_tree_assign_root(reiser4_tree_t *tree,
					reiser4_node_t *node);

extern errno_t reiser4_tree_load_root(reiser4_tree_t *tree);

extern reiser4_node_t *reiser4_tree_alloc_node(reiser4_tree_t *tree,
					       uint8_t level);

extern errno_t reiser4_tree_unlock_node(reiser4_tree_t *tree,
					reiser4_node_t *node);

extern bool_t reiser4_tree_attached_node(reiser4_tree_t *tree,
					 reiser4_node_t *node);

extern errno_t reiser4_tree_attach_node(reiser4_tree_t *tree,
					reiser4_node_t *node,
					reiser4_place_t *place,
					uint32_t flags);

extern errno_t reiser4_tree_detach_node(reiser4_tree_t *tree,
					reiser4_node_t *node,
					uint32_t flags);

extern errno_t reiser4_tree_trav_node(reiser4_tree_t *tree,
				      reiser4_node_t *node,
				      tree_open_func_t open_func,
				      tree_edge_func_t before_func,
				      tree_update_func_t update_func,
				      tree_edge_func_t after_func,
				      void *data);

extern int64_t reiser4_tree_modify(reiser4_tree_t *tree, reiser4_place_t *place,
				   trans_hint_t *hint, uint8_t level, 
				   estimate_func_t estimate_func,
				   modify_func_t modify_func);

extern errno_t reiser4_tree_copy(reiser4_tree_t *src_tree,
				 reiser4_tree_t *dst_tree);

extern errno_t reiser4_tree_resize(reiser4_tree_t *tree,
				   count_t blocks);
#endif

extern blk_t reiser4_tree_get_root(reiser4_tree_t *tree);
extern uint8_t reiser4_tree_get_height(reiser4_tree_t *tree);
extern uint32_t reiser4_tree_get_blksize(reiser4_tree_t *tree);
extern aal_device_t *reiser4_tree_get_device(reiser4_tree_t *tree);

extern errno_t reiser4_tree_unload_node(reiser4_tree_t *tree,
					reiser4_node_t *node);

extern errno_t reiser4_tree_release_node(reiser4_tree_t *tree,
					 reiser4_node_t *node);

extern lookup_t reiser4_tree_collision(reiser4_tree_t *tree,
				       reiser4_place_t *place,
				       coll_hint_t *hint);

extern lookup_t reiser4_collision_handler(reiser4_place_t *place,
					  lookup_bias_t bias,
					  lookup_t lookup,
					  void *data);

extern reiser4_node_t *reiser4_tree_lookup_node(reiser4_tree_t *tree,
						blk_t blk);

extern reiser4_node_t *reiser4_tree_load_node(reiser4_tree_t *tree,
					      reiser4_node_t *parent,
					      blk_t blk);

extern lookup_t reiser4_tree_lookup(reiser4_tree_t *tree, lookup_hint_t *hint,
				    lookup_bias_t bias, reiser4_place_t *place);
#endif

