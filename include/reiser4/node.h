/*
  node.h -- reiser4 formated node functions.
  Copyright (C) 1996-2002 Hans Reiser.
*/ 

#ifndef NODE_H
#define NODE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>
#include <reiser4/key.h>
#include <reiser4/plugin.h>

extern reiser4_node_t *reiser4_node_open(aal_device_t *device, blk_t blk);
extern errno_t reiser4_node_close(reiser4_node_t *node);
extern errno_t reiser4_node_valid(reiser4_node_t *node);

#ifndef ENABLE_COMPACT

extern errno_t reiser4_node_split(reiser4_node_t *node, reiser4_node_t *right);
extern errno_t reiser4_node_remove(reiser4_node_t *node, reiser4_pos_t *pos);

extern errno_t reiser4_node_copy(reiser4_node_t *dst_node, reiser4_pos_t *dst_pos,
				 reiser4_node_t *src_node, reiser4_pos_t *src_pos);

extern errno_t reiser4_node_move(reiser4_node_t *dst_node, reiser4_pos_t *dst_pos,
				 reiser4_node_t *src_node, reiser4_pos_t *src_pos);

#endif

extern errno_t reiser4_node_lkey(reiser4_node_t *node, reiser4_key_t *key);

extern uint32_t reiser4_node_count(reiser4_node_t *node);

extern int reiser4_node_lookup(reiser4_node_t *node, 
			       reiser4_key_t *key, reiser4_pos_t *pos);

extern int reiser4_node_confirm(reiser4_node_t *node);

#ifndef ENABLE_COMPACT

extern reiser4_node_t *reiser4_node_create(aal_device_t *device, blk_t blk,
					   rpid_t pid, uint8_t level);

extern errno_t reiser4_node_sync(reiser4_node_t *node);
extern errno_t reiser4_node_flush(reiser4_node_t *node);

extern errno_t reiser4_node_insert(reiser4_node_t *node, reiser4_pos_t *pos,
				   reiser4_item_hint_t *hint);

#endif

extern uint16_t reiser4_node_overhead(reiser4_node_t *node);
extern uint16_t reiser4_node_maxspace(reiser4_node_t *node);
extern uint16_t reiser4_node_space(reiser4_node_t *node);
extern uint16_t reiser4_node_pid(reiser4_node_t *node);

#endif

