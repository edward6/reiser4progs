/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tree.h -- reiser4 balanced tree functions. */

#ifndef REISER4_TREE_H
#define REISER4_TREE_H

#include <reiser4/types.h>

extern void reiser4_tree_fini(reiser4_tree_t *tree);

extern errno_t reiser4_tree_walk(reiser4_tree_t *tree,
				 reiser4_node_t *node,
				 walk_func_t walk_func);

extern errno_t reiser4_tree_adjust(reiser4_tree_t *tree,
				   reiser4_node_t *node,
				   bool_t mpcheck);

extern reiser4_tree_t *reiser4_tree_init(reiser4_fs_t *fs,
					 mpc_func_t mpc_func);

extern errno_t reiser4_tree_connect(reiser4_tree_t *tree,
				     reiser4_node_t *parent,
				     reiser4_node_t *node);

extern errno_t reiser4_tree_disconnect(reiser4_tree_t *tree,
				       reiser4_node_t *parent,
				       reiser4_node_t *node);

extern reiser4_node_t *reiser4_tree_child(reiser4_tree_t *tree,
					  reiser4_place_t *place);

extern reiser4_node_t *reiser4_tree_neigh(reiser4_tree_t *tree,
					  reiser4_node_t *node,
					  aal_direction_t where);

#ifndef ENABLE_STAND_ALONE
extern void reiser4_tree_pack_set(reiser4_tree_t *tree,
				  pack_func_t func);

extern void reiser4_tree_pack_on(reiser4_tree_t *tree);
extern void reiser4_tree_pack_off(reiser4_tree_t *tree);

extern errno_t reiser4_tree_lroot(reiser4_tree_t *tree);

extern bool_t reiser4_tree_fresh(reiser4_tree_t *tree);
extern errno_t reiser4_tree_sync(reiser4_tree_t *tree);

extern errno_t reiser4_tree_growup(reiser4_tree_t *tree);
extern errno_t reiser4_tree_dryout(reiser4_tree_t *tree);

extern errno_t reiser4_tree_attach(reiser4_tree_t *tree,
				   reiser4_node_t *node);

extern errno_t reiser4_tree_detach(reiser4_tree_t *tree,
				   reiser4_node_t *node);

extern errno_t reiser4_tree_conv(reiser4_tree_t *tree,
				 reiser4_place_t *place,
				 reiser4_plug_t *plug);

extern int32_t reiser4_tree_fetch(reiser4_tree_t *tree,
				  reiser4_place_t *place,
				  trans_hint_t *hint);

extern errno_t reiser4_tree_insert(reiser4_tree_t *tree,
				   reiser4_place_t *place,
				   trans_hint_t *hint,
				   uint8_t level);

extern int32_t reiser4_tree_read(reiser4_tree_t *tree,
				 reiser4_place_t *place,
				 trans_hint_t *hint);

extern int32_t reiser4_tree_write(reiser4_tree_t *tree,
				  reiser4_place_t *place,
				  trans_hint_t *hint,
				  uint8_t level);

extern errno_t reiser4_tree_cutout(reiser4_tree_t *tree,
				   reiser4_place_t *place,
				   trans_hint_t *hint);

extern errno_t reiser4_tree_remove(reiser4_tree_t *tree,
				   reiser4_place_t *place,
				   trans_hint_t *hint);

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
				 tree_open_func_t open_func,
				 tree_edge_func_t before_func,
				 tree_update_func_t update_func,
				 tree_edge_func_t after_func,
				 void *data);

extern errno_t reiser4_tree_traverse(reiser4_tree_t *tree,
				     tree_open_func_t open_func,
				     tree_edge_func_t before_func,
				     tree_update_func_t update_func,
				     tree_edge_func_t after_func,
				     void *data);


extern void reiser4_tree_set_root(reiser4_tree_t *tree,
				  blk_t blk);

extern void reiser4_tree_set_height(reiser4_tree_t *tree,
				    uint8_t height);
#endif

extern blk_t reiser4_tree_get_root(reiser4_tree_t *tree);
extern uint8_t reiser4_tree_get_height(reiser4_tree_t *tree);

extern errno_t reiser4_tree_collapse(reiser4_tree_t *tree);

extern lookup_res_t reiser4_tree_lookup(reiser4_tree_t *tree,
					reiser4_key_t *key,
					uint8_t level,
					lookup_mod_t mode,
					reiser4_place_t *place);

extern reiser4_node_t *reiser4_tree_alloc(reiser4_tree_t *tree,
					  uint8_t level);

extern errno_t reiser4_tree_copy(reiser4_tree_t *src_tree,
				 reiser4_tree_t *dst_tree);

extern errno_t reiser4_tree_resize(reiser4_tree_t *tree,
				   count_t blocks);

extern errno_t reiser4_tree_release(reiser4_tree_t *tree,
				    reiser4_node_t *node);

extern errno_t reiser4_tree_unload(reiser4_tree_t *tree,
				   reiser4_node_t *node);

extern reiser4_node_t *reiser4_tree_load(reiser4_tree_t *tree,
					 reiser4_node_t *parent,
					 blk_t blk);
#endif

