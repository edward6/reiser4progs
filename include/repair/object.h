/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/object.h -- common structures and methods for object recovery. */

#ifndef REPAIR_OBJECT_H
#define REPAIR_OBJECT_H

#include <repair/repair.h>

typedef enum repair_object_flag {
	OF_CHECKED	= 0x0,
	OF_ATTACHED 	= 0x1,
	OF_TRAVERSED 	= 0x2,
	OF_ATTACHING 	= 0x3,
	OF_LAST
} repair_object_flag_t;

extern errno_t repair_object_check_struct(reiser4_object_t *object,
					  place_func_t place_func,
					  uint8_t mode, void *data);

extern reiser4_object_t *repair_object_obtain(reiser4_tree_t *tree,
					      reiser4_object_t *parent,
					      reiser4_key_t *key);

extern reiser4_object_t *repair_object_open(reiser4_tree_t *tree,
					    reiser4_object_t *parent,
					    reiser4_place_t *place);

extern errno_t repair_object_check_attach(reiser4_object_t *object, 
					  reiser4_object_t *parent, 
					  place_func_t place_func,
					  void *data, uint8_t mode);

extern reiser4_object_t *repair_object_fake(reiser4_tree_t *tree, 
					    reiser4_object_t *parent,
					    reiser4_key_t *key,
					    reiser4_plug_t *plug);

extern errno_t repair_object_mark(reiser4_object_t *object, uint16_t flag);
extern errno_t repair_object_clear(reiser4_object_t *object, uint16_t flag);
extern int repair_object_test(reiser4_object_t *object, uint16_t flag);
extern errno_t repair_object_refresh(reiser4_object_t *object);

extern void repair_object_print(reiser4_object_t *object,
				aal_stream_t *stream);

#endif
