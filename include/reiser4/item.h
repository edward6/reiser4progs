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

extern errno_t reiser4_item_get_key(reiser4_coord_t *coord,
				    reiser4_key_t *key);

#ifndef ENABLE_ALONE

extern errno_t reiser4_item_set_key(reiser4_coord_t *coord,
				    reiser4_key_t *key);

extern errno_t reiser4_item_print(reiser4_coord_t *coord,
				  aal_stream_t *stream);

extern errno_t reiser4_item_estimate(reiser4_coord_t *coord,
				     reiser4_item_hint_t *hint);

#endif

extern errno_t reiser4_item_maxposs_key(reiser4_coord_t *coord,
					reiser4_key_t *key);

extern errno_t reiser4_item_utmost_key(reiser4_coord_t *coord,
				       reiser4_key_t *key);

extern errno_t reiser4_item_gap_key(reiser4_coord_t *coord, 
				    reiser4_key_t *key);

extern bool_t reiser4_item_statdata(reiser4_coord_t *coord);
extern bool_t reiser4_item_filebody(reiser4_coord_t *coord);
extern bool_t reiser4_item_filename(reiser4_coord_t *coord);
extern bool_t reiser4_item_branch(reiser4_coord_t *coord);

extern bool_t reiser4_item_extent(reiser4_coord_t *coord);
extern bool_t reiser4_item_nodeptr(reiser4_coord_t *coord);

extern rpid_t reiser4_item_type(reiser4_coord_t *coord);

extern uint32_t reiser4_item_len(reiser4_coord_t *coord);
extern uint32_t reiser4_item_units(reiser4_coord_t *coord);
extern rbody_t *reiser4_item_body(reiser4_coord_t *coord);
extern reiser4_plugin_t *reiser4_item_plugin(reiser4_coord_t *coord);

#endif

