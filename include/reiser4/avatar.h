/*
    avatar.h -- functions which work with avatar structure.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#ifndef AVATAR_H
#define AVATAR_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/filesystem.h>

extern reiser4_avatar_t *reiser4_avatar_create(reiser4_node_t *node);
extern void reiser4_avatar_close(reiser4_avatar_t *avatar);

extern errno_t reiser4_avatar_dup(reiser4_avatar_t *avatar,
    reiser4_avatar_t *dup);

extern errno_t reiser4_avatar_pos(reiser4_avatar_t *avatar, 
    reiser4_pos_t *pos);

extern reiser4_avatar_t *reiser4_avatar_find(reiser4_avatar_t *avatar, 
    reiser4_key_t *key);

extern errno_t reiser4_avatar_attach(reiser4_avatar_t *avatar, 
    reiser4_avatar_t *child);

extern void reiser4_avatar_detach(reiser4_avatar_t *avatar, 
    reiser4_avatar_t *child);

#ifndef ENABLE_COMPACT

extern errno_t reiser4_avatar_sync(reiser4_avatar_t *avatar);

extern errno_t reiser4_avatar_insert(reiser4_avatar_t *avatar,
    reiser4_pos_t *pos, reiser4_item_hint_t *hint);

extern errno_t reiser4_avatar_remove(reiser4_avatar_t *avatar,
    reiser4_pos_t *pos);

extern errno_t reiser4_avatar_move(reiser4_avatar_t *dst_avatar,
    reiser4_pos_t *dst_pos, reiser4_avatar_t *src_avatar,
    reiser4_pos_t *src_pos);

extern errno_t reiser4_avatar_update_key(reiser4_avatar_t *avatar, 
    reiser4_pos_t *pos, reiser4_key_t *key);

#endif

extern errno_t reiser4_avatar_realize(reiser4_avatar_t *avatar);

#endif

