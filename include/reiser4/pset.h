/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   pset.h -- reiser4 plugin set functions. */

#ifndef REISER4_PSET_H
#define REISER4_PSET_H

#ifndef ENABLE_STAND_ALONE

extern void reiser4_opset_root(reiser4_opset_t *opset);

extern void reiser4_opset_diff(reiser4_tree_t *tree, reiser4_opset_t *opset);

#endif

extern errno_t reiser4_pset_init(reiser4_tree_t *tree);

extern reiser4_plug_t *reiser4_opset_plug(rid_t member, rid_t id);

extern errno_t reiser4_opset_init(reiser4_tree_t *tree, int check);

#endif
