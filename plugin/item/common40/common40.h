/*
  common40.h -- item plugins common code.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef COMMON40_H
#define COMMON40_H

typedef uint64_t (*trans_func_t) (item_entity_t *,
				  uint32_t);

extern errno_t common40_get_key(item_entity_t *item,
				uint32_t pos,
				key_entity_t *key,
				trans_func_t func);

extern int common40_mergeable(item_entity_t *item1,
			      item_entity_t *item2);

extern errno_t common40_maxposs_key(item_entity_t *item,
				    key_entity_t *key);

#endif
