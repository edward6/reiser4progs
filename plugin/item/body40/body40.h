/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   body40.h -- file body item plugins common code. */

#ifndef BODY40_H
#define BODY40_H

#include <reiser4/plugin.h>

typedef uint64_t (*trans_func_t) (place_t *, uint32_t);

extern errno_t body40_get_key(place_t *item, uint32_t pos,
			      key_entity_t *key,
			      trans_func_t trans_func);

extern int body40_mergeable(place_t *place1, place_t *place2);

extern errno_t body40_maxreal_key(place_t *item, key_entity_t *key,
				  trans_func_t trans_func);

extern errno_t body40_maxposs_key(place_t *item, key_entity_t *key);
#endif
