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

extern errno_t reiser4_item_init(reiser4_item_t *item, 
    reiser4_entity_t *entity, reiser4_pos_t *pos);

extern errno_t reiser4_item_open(reiser4_item_t *item, 
    reiser4_entity_t *entity, reiser4_pos_t *pos);

extern uint32_t reiser4_item_count(reiser4_item_t *item);

extern errno_t reiser4_item_print(reiser4_item_t *item, 
    char *buff, uint32_t n);

#ifndef ENABLE_COMPACT

extern errno_t reiser4_item_estimate(reiser4_item_t *item,
    reiser4_item_hint_t *hint);

#endif

extern errno_t reiser4_item_get_key(reiser4_item_t *item, 
    reiser4_key_t *key);

extern errno_t reiser4_item_set_key(reiser4_item_t *item, 
    reiser4_key_t *key);

extern errno_t reiser4_item_max_poss_key(reiser4_item_t *item, 
    reiser4_key_t *key);

extern errno_t reiser4_item_max_real_key(reiser4_item_t *item, 
    reiser4_key_t *key);

extern uint32_t reiser4_item_len(reiser4_item_t *item);
extern reiser4_body_t *reiser4_item_body(reiser4_item_t *item);
extern reiser4_plugin_t *reiser4_item_plugin(reiser4_item_t *item);
extern uint16_t reiser4_item_detect(reiser4_item_t *item);

extern int reiser4_item_statdata(reiser4_item_t *item);
extern int reiser4_item_permissn(reiser4_item_t *item);
extern int reiser4_item_filebody(reiser4_item_t *item);
extern int reiser4_item_direntry(reiser4_item_t *item);
extern int reiser4_item_tail(reiser4_item_t *item);
extern int reiser4_item_extent(reiser4_item_t *item);
extern int reiser4_item_nodeptr(reiser4_item_t *item);

#endif

