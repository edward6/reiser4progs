/*
  joint.h -- functions which work with joint structure.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef JOINT_H
#define JOINT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/filesystem.h>

extern void reiser4_joint_close(reiser4_joint_t *joint);

extern reiser4_joint_t *reiser4_joint_create(reiser4_node_t *node);

extern errno_t reiser4_joint_dup(reiser4_joint_t *joint,
				 reiser4_joint_t *dup);

extern errno_t reiser4_joint_pos(reiser4_joint_t *joint, 
				 reiser4_pos_t *pos);

extern reiser4_joint_t *reiser4_joint_find(reiser4_joint_t *joint, 
					   reiser4_key_t *key);

extern errno_t reiser4_joint_attach(reiser4_joint_t *joint, 
				    reiser4_joint_t *child);

extern void reiser4_joint_detach(reiser4_joint_t *joint, 
				 reiser4_joint_t *child);

#ifndef ENABLE_COMPACT

extern errno_t reiser4_joint_sync(reiser4_joint_t *joint);

extern errno_t reiser4_joint_insert(reiser4_joint_t *joint, reiser4_pos_t *pos,
				    reiser4_item_hint_t *hint);

extern errno_t reiser4_joint_remove(reiser4_joint_t *joint,
				    reiser4_pos_t *pos);

extern errno_t reiser4_joint_move(reiser4_joint_t *dst_joint, reiser4_pos_t *dst_pos,
				  reiser4_joint_t *src_joint, reiser4_pos_t *src_pos);

extern errno_t reiser4_joint_update(reiser4_joint_t *joint, reiser4_pos_t *pos,
				    reiser4_key_t *key);

extern errno_t reiser4_joint_traverse(reiser4_joint_t *joint, traverse_hint_t *hint,
				      reiser4_open_func_t open_func,
				      reiser4_edge_func_t before_func,
				      reiser4_setup_func_t setup_func, 
				      reiser4_setup_func_t update_func,
				      reiser4_edge_func_t after_func);

#endif

extern reiser4_joint_t *reiser4_joint_left(reiser4_joint_t *joint);
extern reiser4_joint_t *reiser4_joint_right(reiser4_joint_t *joint);
extern errno_t reiser4_joint_lock(reiser4_joint_t *joint);
extern errno_t reiser4_joint_unlock(reiser4_joint_t *joint);

#endif

