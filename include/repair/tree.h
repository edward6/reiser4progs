/* repair/tree.h -- reiserfs tree recovery structures and macros.
   
   Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING. */

#ifndef REPAIR_TREE_H
#define REPAIR_TREE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_tree_copy(reiser4_tree_t *, reiser4_place_t *);
extern errno_t repair_tree_attach(reiser4_tree_t *, reiser4_node_t *);

extern bool_t repair_tree_legal_level(reiser4_item_group_t group,
				      uint8_t level);

extern bool_t repair_tree_data_level(uint8_t level);

#endif
