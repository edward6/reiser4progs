/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   body40.h -- file body item plugins common code. */

#ifndef BODY40_H
#define BODY40_H

typedef uint64_t (*trans_func_t) (item_entity_t *,
				  uint32_t);

extern errno_t body40_get_key(item_entity_t *item,
				uint32_t pos,
				key_entity_t *key,
				trans_func_t trans_func);

extern errno_t body40_maxreal_key(item_entity_t *item,
				    key_entity_t *key,
				    trans_func_t trans_func);

extern errno_t body40_maxposs_key(item_entity_t *item,
				    key_entity_t *key);

extern lookup_t body40_lookup(item_entity_t *item,
				key_entity_t *key,
				uint64_t *pos,
				trans_func_t trans_func);

#ifndef ENABLE_STAND_ALONE

extern int body40_mergeable(item_entity_t *item1,
			      item_entity_t *item2);
#endif

#endif
