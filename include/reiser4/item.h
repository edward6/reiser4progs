/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   item.h -- common item functions. */

#ifndef REISER4_ITEM_H
#define REISER4_ITEM_H

#include <reiser4/types.h>

#ifndef ENABLE_STAND_ALONE
extern rid_t reiser4_item_type(place_t *place);
extern bool_t reiser4_item_statdata(place_t *place);

extern errno_t reiser4_item_print(place_t *place,
				  aal_stream_t *stream);

extern errno_t reiser4_item_get_key(place_t *place,
				    reiser4_key_t *key);

extern errno_t reiser4_item_update_key(place_t *place,
				       reiser4_key_t *key);

extern errno_t reiser4_item_maxreal_key(place_t *place,
					reiser4_key_t *key);

extern errno_t reiser4_item_update_link(place_t *place,
					blk_t blk);
#endif

extern uint32_t reiser4_item_units(place_t *place);
extern blk_t reiser4_item_down_link(place_t *place);
extern bool_t reiser4_item_branch(reiser4_plug_t *plug);

extern errno_t reiser4_item_maxposs_key(place_t *place,
					reiser4_key_t *key);

#endif
