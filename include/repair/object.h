/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   repair/object.h -- common structures and methods for object recovery. */

#ifndef REPAIR_OBJECT_H
#define REPAIR_OBJECT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

typedef errno_t (*traverse_func_t) (reiser4_object_t *parent, 
				    reiser4_object_t **object, 
				    entry_hint_t *entry, void *data);

extern reiser4_plugin_t *repair_object_realize(reiser4_object_t *object);

extern errno_t repair_object_launch(reiser4_object_t *object);

extern inline void repair_object_init(reiser4_object_t *object,
				      reiser4_tree_t *tree, 
				      reiser4_place_t *place,
				      reiser4_key_t *parent, 
				      reiser4_key_t *key);

extern errno_t repair_object_check_struct(reiser4_object_t *object, 
					  reiser4_plugin_t *plugin, 
					  place_func_t func,
					  uint8_t mode, void *data);

extern errno_t repair_object_check_link(reiser4_object_t *object, 
					reiser4_object_t *parent, 
					uint8_t mode);

extern errno_t repair_object_traverse(reiser4_object_t *object, 
				      traverse_func_t func, 
				      void *data);

#endif
