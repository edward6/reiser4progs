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

typedef struct repair_object {
    /* Realized plugin of the object. */
    reiser4_plugin_t *plugin;
    
    /* The start key of the object. */    
    reiser4_key_t *key;
    
    /* The place is found by the start key of the object. */
    reiser4_place_t place;
    
    /* Reiser4 storage tree pointer. */
    reiser4_tree_t *tree;

    /* Pointer to the parent object. */
    reiser4_object_t *parent;
} repair_object_t;

extern void repair_object_init(repair_object_t *hint, reiser4_tree_t *tree);
extern errno_t repair_object_launch(repair_object_t *hint, 
    reiser4_key_t *key);
extern errno_t repair_object_realize(repair_object_t *hint, 
    reiser4_place_t *place);

extern errno_t repair_object_check_struct(repair_object_t *hint, uint8_t mode);
extern errno_t repair_object_traverse(reiser4_object_t *hint);
extern reiser4_object_t *repair_object_open(repair_object_t *hint);

#endif
