/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   flow.h -- declarations of functions for working with flow. */

#ifndef REISER4_FLOW_H
#define REISER4_FLOW_H

#include <reiser4/types.h>

extern int64_t reiser4_flow_read(reiser4_tree_t *tree,
				 trans_hint_t *hint);

#ifndef ENABLE_MINIMAL
extern int64_t reiser4_flow_write(reiser4_tree_t *tree,
				  trans_hint_t *hint);

extern errno_t reiser4_flow_convert(reiser4_tree_t *tree,
				    conv_hint_t *hint);

extern int64_t reiser4_flow_cut(reiser4_tree_t *tree,
				trans_hint_t *hint);
#endif

#endif
