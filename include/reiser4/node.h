/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node.h -- reiser4 formated node functions. */ 

#ifndef REISER4_NODE_H
#define REISER4_NODE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/types.h>

extern errno_t reiser4_node_load(reiser4_node_t *node);
extern errno_t reiser4_node_unload(reiser4_node_t *node);
extern uint8_t reiser4_node_get_level(reiser4_node_t *node);

extern reiser4_node_t *reiser4_node_open(aal_device_t *device,
					 uint32_t size, blk_t blk);

extern reiser4_node_t *reiser4_node_init(aal_device_t *device,
					 uint32_t size, blk_t blk,
					 rid_t pid);

#ifndef ENABLE_STAND_ALONE
extern errno_t reiser4_node_clone(reiser4_node_t *src,
				  reiser4_node_t *dst);

extern void reiser4_node_move(reiser4_node_t *node,
			      blk_t number);

extern errno_t reiser4_node_form(reiser4_node_t *node,
				 uint8_t level);

extern void reiser4_node_set_mstamp(reiser4_node_t *node,
				    uint32_t stamp);

extern void reiser4_node_set_fstamp(reiser4_node_t *node,
				    uint64_t stamp);

extern void reiser4_node_set_level(reiser4_node_t *node,
				   uint8_t level);

extern errno_t reiser4_node_uchildren(reiser4_node_t *node,
				      pos_t *start);

extern errno_t reiser4_node_print(reiser4_node_t *node,
				  aal_stream_t *stream);

extern errno_t reiser4_node_sync(reiser4_node_t *node);
extern errno_t reiser4_node_update(reiser4_node_t *node);

extern errno_t reiser4_node_ukey(reiser4_node_t *node,
				 pos_t *pos,
				 reiser4_key_t *key);

extern errno_t reiser4_node_insert(reiser4_node_t *node,
   				   pos_t *pos,
				   create_hint_t *hint);


extern errno_t reiser4_node_remove(reiser4_node_t *node,
				   pos_t *pos, uint32_t count);

extern errno_t reiser4_node_cut(reiser4_node_t *node,
				pos_t *start,
				pos_t *end);

extern errno_t reiser4_node_expand(reiser4_node_t *node, pos_t *pos,
				   uint32_t len, uint32_t count);

extern errno_t reiser4_node_shrink(reiser4_node_t *node, pos_t *pos,
				   uint32_t len, uint32_t count);

extern errno_t reiser4_node_shift(reiser4_node_t *node,
				  reiser4_node_t *neig,
				  shift_hint_t *hint);

extern bool_t reiser4_node_confirm(reiser4_node_t *node);
extern uint16_t reiser4_node_space(reiser4_node_t *node);
extern uint16_t reiser4_node_overhead(reiser4_node_t *node);
extern uint16_t reiser4_node_maxspace(reiser4_node_t *node);
extern uint32_t reiser4_node_get_mstamp(reiser4_node_t *node);
extern uint64_t reiser4_node_get_fstamp(reiser4_node_t *node);
#endif

extern errno_t reiser4_node_lkey(reiser4_node_t *node,
				 reiser4_key_t *key);

extern errno_t reiser4_node_realize(reiser4_node_t *node);

extern reiser4_node_t *reiser4_node_child(reiser4_node_t *node,
					  blk_t blk);

extern errno_t reiser4_node_connect(reiser4_node_t *node,
				    reiser4_node_t *child);

extern errno_t reiser4_node_disconnect(reiser4_node_t *node,
				       reiser4_node_t *child);

extern lookup_t reiser4_node_lookup(reiser4_node_t *node,
				    reiser4_key_t *key,
				    pos_t *pos);

extern errno_t reiser4_node_lock(reiser4_node_t *node);
extern errno_t reiser4_node_unlock(reiser4_node_t *node);
extern errno_t reiser4_node_close(reiser4_node_t *node);

extern uint32_t reiser4_node_items(reiser4_node_t *node);

#ifndef ENABLE_STAND_ALONE
extern bool_t reiser4_node_isdirty(reiser4_node_t *node);
extern void reiser4_node_mkdirty(reiser4_node_t *node);
extern void reiser4_node_mkclean(reiser4_node_t *node);
#endif

#define reiser4_node_lock(node) (node->counter++)
#define reiser4_node_unlock(node) (node->counter--)
#define reiser4_node_locked(node) (node->counter > 0)

#endif
