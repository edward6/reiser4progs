/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   item.h -- common item functions. */

#ifndef REISER4_ITEM_H
#define REISER4_ITEM_H

#include <reiser4/types.h>

#ifndef ENABLE_STAND_ALONE
extern errno_t reiser4_item_ukey(reiser4_place_t *place,
				 reiser4_key_t *key);

extern errno_t reiser4_item_print(reiser4_place_t *place,
				  aal_stream_t *stream);

extern errno_t reiser4_item_estimate(reiser4_place_t *place,
				     insert_hint_t *hint);

extern errno_t reiser4_item_maxreal_key(reiser4_place_t *place,
					reiser4_key_t *key);

extern rid_t reiser4_item_type(reiser4_place_t *place);
extern bool_t reiser4_item_statdata(reiser4_place_t *place);
extern errno_t reiser4_item_key(reiser4_place_t *place, reiser4_key_t *key);

#endif

extern errno_t reiser4_item_maxposs_key(reiser4_place_t *place,
					reiser4_key_t *key);

extern bool_t reiser4_item_branch(reiser4_plug_t *plug);
extern uint32_t reiser4_item_units(reiser4_place_t *place);


#endif

