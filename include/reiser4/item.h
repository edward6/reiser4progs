/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   item.h -- common item functions. */

#ifndef REISER4_ITEM_H
#define REISER4_ITEM_H

#include <reiser4/types.h>

#ifndef ENABLE_STAND_ALONE
extern rid_t reiser4_item_type(reiser4_place_t *place);
extern bool_t reiser4_item_statdata(reiser4_place_t *place);

extern int reiser4_item_mergeable(reiser4_place_t *place1,
				  reiser4_place_t *place2);

extern errno_t reiser4_item_update_key(reiser4_place_t *place,
				       reiser4_key_t *key);

extern errno_t reiser4_item_maxreal_key(reiser4_place_t *place,
					reiser4_key_t *key);

extern errno_t reiser4_item_update_link(reiser4_place_t *place,
					blk_t blk);

extern uint16_t reiser4_item_overhead(reiser4_plug_t *plug);

extern void reiser4_item_set_flag(reiser4_place_t *place, 
				  uint16_t flag);

extern void reiser4_item_clear_flag(reiser4_place_t *place, 
				    uint16_t flag);

extern void reiser4_item_clear_flags(reiser4_place_t *place);

extern bool_t reiser4_item_test_flag(reiser4_place_t *place, 
				     uint16_t flag);

extern void reiser4_item_dup_flags(reiser4_place_t *place, 
			           uint16_t flags);

extern uint16_t reiser4_item_get_flags(reiser4_place_t *place);

#endif

extern uint32_t reiser4_item_units(reiser4_place_t *place);
extern blk_t reiser4_item_down_link(reiser4_place_t *place);
extern bool_t reiser4_item_branch(reiser4_plug_t *plug);

extern errno_t reiser4_item_get_key(reiser4_place_t *place,
				    reiser4_key_t *key);

extern errno_t reiser4_item_maxposs_key(reiser4_place_t *place,
					reiser4_key_t *key);

#endif
