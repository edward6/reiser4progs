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

extern reiser4_tree_t *reiser4_tree_open(reiser4_fs_t *fs);
extern void reiser4_tree_close(reiser4_tree_t *tree);

#ifndef ENABLE_COMPACT

extern reiser4_tree_t *reiser4_tree_create(reiser4_fs_t *fs, 
					   reiser4_profile_t *profile);

extern errno_t reiser4_tree_sync(reiser4_tree_t *tree);
extern errno_t reiser4_tree_flush(reiser4_tree_t *tree);

extern errno_t reiser4_tree_insert(reiser4_tree_t *tree, reiser4_item_hint_t *hint,
				   uint8_t level, reiser4_coord_t *coord);

extern errno_t reiser4_tree_remove(reiser4_tree_t *tree, reiser4_key_t *key,
				   uint8_t level);

extern errno_t reiser4_tree_move(reiser4_tree_t *tree, reiser4_coord_t *dst,
				 reiser4_coord_t *src);

extern errno_t reiser4_tree_mkspace(reiser4_tree_t *tree, reiser4_coord_t *old,
				    reiser4_coord_t *new, uint32_t needed);

#endif

extern int reiser4_tree_lookup(reiser4_tree_t *tree, reiser4_key_t *key,
			       reiser4_level_t *level, reiser4_coord_t *coord);

extern blk_t reiser4_tree_root(reiser4_tree_t *tree);
extern reiser4_key_t *reiser4_tree_key(reiser4_tree_t *tree);
extern uint8_t reiser4_tree_height(reiser4_tree_t *tree);

extern reiser4_joint_t *reiser4_tree_allocate(reiser4_tree_t *tree, uint8_t level);
extern void reiser4_tree_release(reiser4_tree_t *tree, reiser4_joint_t *joint);
extern reiser4_joint_t *reiser4_tree_load(reiser4_tree_t *tree, blk_t blk);

extern errno_t reiser4_tree_collector(reiser4_tree_t *tree);

#endif

