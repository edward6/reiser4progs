/*
  node.h -- reiser4 formated node functions.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/ 

#ifndef REISER4_NODE_H
#define REISER4_NODE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/types.h>

extern reiser4_node_t *reiser4_node_open(aal_device_t *device,
					 blk_t blk);

extern uint8_t reiser4_node_get_level(reiser4_node_t *node);
extern uint32_t reiser4_node_get_mstamp(reiser4_node_t *node);
extern uint64_t reiser4_node_get_fstamp(reiser4_node_t *node);

#ifndef ENABLE_ALONE

extern void reiser4_node_set_mstamp(reiser4_node_t *node,
				    uint32_t stamp);

extern void reiser4_node_set_fstamp(reiser4_node_t *node,
				    uint64_t stamp);

extern void reiser4_node_set_level(reiser4_node_t *node,
				   uint8_t level);

extern reiser4_node_t *reiser4_node_create(aal_device_t *device,
					   blk_t blk, rpid_t pid,
					   uint8_t level);

extern errno_t reiser4_node_print(reiser4_node_t *node,
				  aal_stream_t *stream);

extern errno_t reiser4_node_sync(reiser4_node_t *node);

extern errno_t reiser4_node_ukey(reiser4_node_t *node,
				 rpos_t *pos,
				 reiser4_key_t *key);

extern errno_t reiser4_node_insert(reiser4_node_t *node,
   				   rpos_t *pos,
				   reiser4_item_hint_t *hint);


extern errno_t reiser4_node_remove(reiser4_node_t *node,
				   rpos_t *pos, uint32_t count);

extern errno_t reiser4_node_write(reiser4_node_t *dst_node,
				  rpos_t *dst_pos,
				  reiser4_node_t *src_node,
				  rpos_t *src_pos,
				  uint32_t count);

extern errno_t reiser4_node_cut(reiser4_node_t *node,
				rpos_t *start,
				rpos_t *end);

extern errno_t reiser4_node_copy(reiser4_node_t *src_node,
				 rpos_t *src_pos,
				 reiser4_node_t *dst_node,
				 rpos_t *dst_pos,
				 uint32_t count);

extern errno_t reiser4_node_expand(reiser4_node_t *node, rpos_t *pos,
				   uint32_t len, uint32_t count);

extern errno_t reiser4_node_shrink(reiser4_node_t *node, rpos_t *pos,
				   uint32_t len, uint32_t count);

extern errno_t reiser4_node_shift(reiser4_node_t *node,
				  reiser4_node_t *neig,
				  shift_hint_t *hint);

extern errno_t reiser4_node_traverse(reiser4_node_t *node,
				     traverse_hint_t *hint,
				     traverse_open_func_t open_func,
				     traverse_edge_func_t before_func,
				     traverse_setup_func_t setup_func,
				     traverse_setup_func_t update_func,
				     traverse_edge_func_t after_func);

#endif

extern errno_t reiser4_node_lkey(reiser4_node_t *node,
				 reiser4_key_t *key);

extern errno_t reiser4_node_pos(reiser4_node_t *node,
				rpos_t *pos);

extern reiser4_node_t *reiser4_node_cbp(reiser4_node_t *node,
					blk_t blk);

extern errno_t reiser4_node_connect(reiser4_node_t *node,
				    reiser4_node_t *child);

extern errno_t reiser4_node_disconnect(reiser4_node_t *node,
				       reiser4_node_t *child);

extern int reiser4_node_lookup(reiser4_node_t *node,
			       reiser4_key_t *key,
			       rpos_t *pos);

extern errno_t reiser4_node_lock(reiser4_node_t *node);
extern errno_t reiser4_node_unlock(reiser4_node_t *node);
extern errno_t reiser4_node_close(reiser4_node_t *node);
extern errno_t reiser4_node_release(reiser4_node_t *node);

extern bool_t reiser4_node_confirm(reiser4_node_t *node);
extern errno_t reiser4_node_valid(reiser4_node_t *node);
extern uint32_t reiser4_node_items(reiser4_node_t *node);
extern uint16_t reiser4_node_space(reiser4_node_t *node);
extern uint16_t reiser4_node_overhead(reiser4_node_t *node);
extern uint16_t reiser4_node_maxspace(reiser4_node_t *node);

#define reiser4_node_mkdirty(node) (node->flags |= NF_DIRTY)
#define reiser4_node_mkclean(node) (node->flags &= ~NF_DIRTY)

#define reiser4_node_isdirty(node) (node->flags & NF_DIRTY)
#define reiser4_node_isclean(node) (!reiser4_node_isdirty(node))

#define reiser4_node_lock(node) (node->counter++)
#define reiser4_node_unlock(node) (node->counter--)
#define reiser4_node_locked(node) (node->counter > 0)

#endif
