/*
    repair/object.h -- common structures and methods for object recovery.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_OBJECT_H
#define REPAIR_OBJECT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern reiser4_plugin_t *repair_object_realize(reiser4_object_t *object);
extern errno_t repair_object_check_struct(reiser4_object_t *object, 
    reiser4_plugin_t *plugin, uint8_t mode);
extern errno_t repair_object_check_link(reiser4_object_t *object, 
    reiser4_object_t *parent, uint8_t mode);
extern errno_t repair_object_traverse(reiser4_object_t *object);
extern errno_t repair_object_launch(reiser4_object_t *object);
extern inline void repair_object_init(reiser4_object_t *object,
    reiser4_tree_t *tree, reiser4_place_t *place,
    reiser4_key_t *parent, reiser4_key_t *key);

#endif
