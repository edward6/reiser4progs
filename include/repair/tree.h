/*
    repair/tree.h -- reiserfs tree recovery structures and macros.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_TREE_H
#define REPAIR_TREE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_tree_insert(reiser4_tree_t *, reiser4_coord_t *);
extern errno_t repair_tree_attach(reiser4_tree_t *, reiser4_node_t *);

#endif
