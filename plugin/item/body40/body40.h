/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   body40.h -- file body item plugins common code. */

#ifndef BODY40_H
#define BODY40_H

#include <reiser4/plugin.h>

typedef uint64_t (*trans_func_t) (reiser4_place_t *,
				  uint32_t);

extern errno_t body40_get_key(reiser4_place_t *item,
			      uint32_t pos, reiser4_key_t *key,
			      trans_func_t trans_func);

extern int body40_mergeable(reiser4_place_t *place1,
			    reiser4_place_t *place2);

extern errno_t body40_maxreal_key(reiser4_place_t *item,
				  reiser4_key_t *key,
				  trans_func_t trans_func);

extern errno_t body40_maxposs_key(reiser4_place_t *item,
				  reiser4_key_t *key);
#endif
