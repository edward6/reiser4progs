/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/object.h -- common structures and methods for object recovery. */

#ifndef REPAIR_OBJECT_H
#define REPAIR_OBJECT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

typedef enum repair_object_flag {
	OF_CHECKED	= 0x1,
	OF_ATTACHED 	= 0x2,
	OF_ATTACHING 	= 0x3,
	OF_LAST	 	= 0x5
} repair_object_flag_t;

typedef reiser4_object_t *(*object_open_func_t) (reiser4_object_t *parent, 
						 entry_hint_t *entry, 
						 void *data);

extern errno_t repair_object_check_struct(reiser4_object_t *object,
					  place_func_t place_func,
					  uint8_t mode, void *data);

extern reiser4_object_t *repair_object_launch(reiser4_tree_t *tree, 
					      reiser4_key_t *key);

extern reiser4_object_t *repair_object_realize(reiser4_tree_t *tree, 
					       reiser4_place_t *place,
					       bool_t only);

extern errno_t repair_object_traverse(reiser4_object_t *object, 
				      object_open_func_t open_func,
				      void *data);

extern errno_t repair_object_check_attach(reiser4_object_t *object, 
					  reiser4_object_t *parent, 
					  uint8_t mode);

#endif
