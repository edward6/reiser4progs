/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node.h -- reiser4 formated node functions. */ 

#ifndef REISER4_NODE_H
#define REISER4_NODE_H

#include <reiser4/types.h>

#ifndef ENABLE_STAND_ALONE
extern errno_t reiser4_node_sync(node_t *node);
extern uint16_t reiser4_node_space(node_t *node);
extern uint16_t reiser4_node_overhead(node_t *node);
extern uint16_t reiser4_node_maxspace(node_t *node);
extern uint32_t reiser4_node_get_mstamp(node_t *node);
extern uint64_t reiser4_node_get_fstamp(node_t *node);

extern errno_t reiser4_node_update_ptr(node_t *node);
extern void reiser4_node_move(node_t *node, blk_t nr);

extern node_t *reiser4_node_create(reiser4_tree_t *tree,
				   blk_t nr, rid_t pid,
				   uint8_t level);

extern int64_t reiser4_node_modify(node_t *node, pos_t *pos,
				   trans_hint_t *hint,
				   modify_func_t modify_func);

extern int64_t reiser4_node_write(node_t *node, pos_t *pos,
				  trans_hint_t *hint);

extern int64_t reiser4_node_trunc(node_t *node, pos_t *pos,
				  trans_hint_t *hint);

extern errno_t reiser4_node_insert(node_t *node, pos_t *pos,
				   trans_hint_t *hint);

extern errno_t reiser4_node_remove(node_t *node, pos_t *pos,
				   trans_hint_t *hint);

extern errno_t reiser4_node_expand(node_t *node, pos_t *pos,
				   uint32_t len, uint32_t count);

extern errno_t reiser4_node_shrink(node_t *node, pos_t *pos,
				   uint32_t len, uint32_t count);

extern errno_t reiser4_node_shift(node_t *node, node_t *neig,
				  shift_hint_t *hint);

extern errno_t reiser4_node_update_key(node_t *node, pos_t *pos,
				       reiser4_key_t *key);

extern errno_t reiser4_node_clone(node_t *src, node_t *dst);
extern errno_t reiser4_node_fresh(node_t *node, uint8_t level);

extern void reiser4_node_set_mstamp(node_t *node, uint32_t stamp);
extern void reiser4_node_set_fstamp(node_t *node, uint64_t stamp);
extern void reiser4_node_set_level(node_t *node, uint8_t level);
#endif

extern uint8_t reiser4_node_get_level(node_t *node);

extern node_t *reiser4_node_open(reiser4_tree_t *tree,
				 blk_t nr);

extern errno_t reiser4_node_leftmost_key(node_t *node,
					 reiser4_key_t *key);

extern lookup_t reiser4_node_lookup(node_t *node, reiser4_key_t *key,
				    bias_t bias, pos_t *pos);

extern errno_t reiser4_node_fini(node_t *node);
extern errno_t reiser4_node_close(node_t *node);
extern uint32_t reiser4_node_items(node_t *node);

#ifndef ENABLE_STAND_ALONE
extern bool_t reiser4_node_isdirty(node_t *node);
extern void reiser4_node_mkdirty(node_t *node);
extern void reiser4_node_mkclean(node_t *node);
#endif

extern void reiser4_node_lock(node_t *node);
extern void reiser4_node_unlock(node_t *node);
extern bool_t reiser4_node_locked(node_t *node);

#define node_blocknr(node) \
        ((node)->entity->block->nr)

#endif
