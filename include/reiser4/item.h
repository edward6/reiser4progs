/*
  item.h -- common item functions.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef REISER4_ITEM_H
#define REISER4_ITEM_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/types.h>

extern errno_t reiser4_item_realize(reiser4_place_t *place);

extern errno_t reiser4_item_get_key(reiser4_place_t *place,
				    reiser4_key_t *key);

#ifndef ENABLE_STAND_ALONE

extern errno_t reiser4_item_set_key(reiser4_place_t *place,
				    reiser4_key_t *key);

extern errno_t reiser4_item_print(reiser4_place_t *place,
				  aal_stream_t *stream);

extern bool_t reiser4_item_mergeable(reiser4_place_t *place1,
				     reiser4_place_t *place2);

extern errno_t reiser4_item_feel(reiser4_place_t *place,
				 reiser4_key_t *start,
				 reiser4_key_t *end,
				 feel_hint_t *hint);

extern errno_t reiser4_item_estimate(reiser4_place_t *place,
				     create_hint_t *hint);

extern errno_t reiser4_item_maxreal_key(reiser4_place_t *place,
					reiser4_key_t *key);

extern errno_t reiser4_item_gap_key(reiser4_place_t *place, 
				    reiser4_key_t *key);

extern rid_t reiser4_item_type(reiser4_place_t *place);
extern body_t *reiser4_item_body(reiser4_place_t *place);
extern bool_t reiser4_item_data(reiser4_plugin_t *plugin);
extern bool_t reiser4_item_statdata(reiser4_place_t *place);
extern bool_t reiser4_item_filebody(reiser4_place_t *place);
extern bool_t reiser4_item_filename(reiser4_place_t *place);

extern reiser4_plugin_t *reiser4_item_plugin(reiser4_place_t *place);
#endif

extern errno_t reiser4_item_maxposs_key(reiser4_place_t *place,
					reiser4_key_t *key);

extern uint32_t reiser4_item_len(reiser4_place_t *place);
extern bool_t reiser4_item_branch(reiser4_place_t *place);
extern uint32_t reiser4_item_units(reiser4_place_t *place);


#endif

