/*
  item.h -- common item functions.
  Copyright 1996-2002 (C) Hans Reiser.
*/

#ifndef ITEM_H
#define ITEM_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/filesystem.h>

extern errno_t reiser4_item_print(reiser4_coord_t *coord,
				  char *buff, uint32_t n);

#ifndef ENABLE_COMPACT

extern errno_t reiser4_item_estimate(reiser4_coord_t *coord,
				     reiser4_item_hint_t *hint);

extern errno_t reiser4_item_update(reiser4_coord_t *coord,
				   reiser4_key_t *key);

#endif

extern errno_t reiser4_item_max_poss_key(reiser4_coord_t *coord,
					 reiser4_key_t *key);

extern errno_t reiser4_item_max_real_key(reiser4_coord_t *coord,
					 reiser4_key_t *key);

extern int reiser4_item_statdata(reiser4_coord_t *coord);
extern int reiser4_item_permissn(reiser4_coord_t *coord);
extern int reiser4_item_filebody(reiser4_coord_t *coord);
extern int reiser4_item_direntry(reiser4_coord_t *coord);
extern int reiser4_item_tail(reiser4_coord_t *coord);
extern int reiser4_item_extent(reiser4_coord_t *coord);
extern int reiser4_item_nodeptr(reiser4_coord_t *coord);

extern uint32_t reiser4_item_len(reiser4_coord_t *coord);
extern uint32_t reiser4_item_count(reiser4_coord_t *coord);
extern reiser4_body_t *reiser4_item_body(reiser4_coord_t *coord);
extern reiser4_plugin_t *reiser4_item_plugin(reiser4_coord_t *coord);
extern errno_t reiser4_item_key(reiser4_coord_t *coord, reiser4_key_t *key);

#endif

