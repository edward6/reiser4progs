/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   semantic.h -- reiser4 semantic tree functions. */

#ifndef REISER4_SEMANTIC_H
#define REISER4_SEMANTIC_H

#include <reiser4/types.h>

extern reiser4_object_t *reiser4_semantic_open(reiser4_tree_t *tree,
					       char *path, 
					       reiser4_key_t *from,
					       bool_t follow);

#endif
