/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   semantic.h -- reiser4 semantic tree functions. */

#ifndef REISER4_SEMANTIC_H
#define REISER4_SEMANTIC_H

#include <reiser4/types.h>

extern reiser4_plug_t *reiser4_semantic_plug(reiser4_tree_t *tree,
					     reiser4_place_t *place);

extern object_entity_t *reiser4_semantic_resolve(reiser4_tree_t *tree,
						 char *path, reiser4_key_t *from,
						 bool_t follow);
#endif
