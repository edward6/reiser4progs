/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node.h -- reiser4 formated node functions. */ 

#ifndef REISER4_NODE_H
#define REISER4_NODE_H

#include <reiser4/types.h>

#ifndef ENABLE_STAND_ALONE
extern errno_t reiser4_node_sync(reiser4_node_t *node);
extern uint16_t reiser4_node_space(reiser4_node_t *node);
extern uint16_t reiser4_node_overhead(reiser4_node_t *node);
extern uint16_t reiser4_node_maxspace(reiser4_node_t *node);
extern uint32_t reiser4_node_get_mstamp(reiser4_node_t *node);
extern uint64_t reiser4_node_get_fstamp(reiser4_node_t *node);

extern errno_t reiser4_node_update_ptr(reiser4_node_t *node);
extern void reiser4_node_move(reiser4_node_t *node, blk_t nr);

extern errno_t callback_node_insert(reiser4_node_t *node, pos_t *pos,
				    trans_hint_t *hint);

extern errno_t callback_node_write(reiser4_node_t *node, pos_t *pos,
				   trans_hint_t *hint);

extern reiser4_node_t *reiser4_node_create(reiser4_tree_t *tree,
				   blk_t nr, rid_t pid,
				   uint8_t level);

extern int64_t reiser4_node_modify(reiser4_node_t *node, pos_t *pos,
				   trans_hint_t *hint,
				   modify_func_t modify_func);

extern int64_t reiser4_node_write(reiser4_node_t *node, pos_t *pos,
				  trans_hint_t *hint);

extern int64_t reiser4_node_trunc(reiser4_node_t *node, pos_t *pos,
				  trans_hint_t *hint);

extern errno_t reiser4_node_insert(reiser4_node_t *node, pos_t *pos,
				   trans_hint_t *hint);

extern errno_t reiser4_node_remove(reiser4_node_t *node, pos_t *pos,
				   trans_hint_t *hint);

extern errno_t reiser4_node_expand(reiser4_node_t *node, pos_t *pos,
				   uint32_t len, uint32_t count);

extern errno_t reiser4_node_shrink(reiser4_node_t *node, pos_t *pos,
				   uint32_t len, uint32_t count);

extern errno_t reiser4_node_shift(reiser4_node_t *node, reiser4_node_t *neig,
				  shift_hint_t *hint);

extern errno_t reiser4_node_fuse(reiser4_node_t *node, pos_t *pos1, pos_t *pos2);

extern errno_t reiser4_node_update_key(reiser4_node_t *node, pos_t *pos,
				       reiser4_key_t *key);

extern errno_t reiser4_node_fresh(reiser4_node_t *node, uint8_t level);

extern void reiser4_node_set_mstamp(reiser4_node_t *node, uint32_t stamp);
extern void reiser4_node_set_fstamp(reiser4_node_t *node, uint64_t stamp);
extern void reiser4_node_set_level(reiser4_node_t *node, uint8_t level);
#endif

extern uint8_t reiser4_node_get_level(reiser4_node_t *node);

extern reiser4_node_t *reiser4_node_open(reiser4_tree_t *tree,
				 blk_t nr);

extern errno_t reiser4_node_leftmost_key(reiser4_node_t *node,
					 reiser4_key_t *key);

extern lookup_t reiser4_node_lookup(reiser4_node_t *node,
				    reiser4_key_t *key,
				    bias_t bias, pos_t *pos);

extern errno_t reiser4_node_fini(reiser4_node_t *node);
extern errno_t reiser4_node_close(reiser4_node_t *node);
extern uint32_t reiser4_node_items(reiser4_node_t *node);

#ifndef ENABLE_STAND_ALONE
extern bool_t reiser4_node_isdirty(reiser4_node_t *node);
extern void reiser4_node_mkdirty(reiser4_node_t *node);
extern void reiser4_node_mkclean(reiser4_node_t *node);
#endif

#define node_blocknr(node) ((node)->block->nr)

extern void reiser4_node_lock(reiser4_node_t *node);
extern void reiser4_node_unlock(reiser4_node_t *node);
extern bool_t reiser4_node_locked(reiser4_node_t *node);

#endif
