/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40_repair.h -- reiser4 hashed directory plugin repair functions. */

#ifndef DIR40_REPAIR_H
#define DIR40_REPAIR_H

#include "dir40.h"
#include "plugin/object/obj40/obj40_repair.h"

extern errno_t dir40_form(reiser4_object_t *object);
extern errno_t dir40_fake(reiser4_object_t *dir);
extern errno_t dir40_recognize(reiser4_object_t *dir);


extern errno_t dir40_check_attach(reiser4_object_t *object, 
				  reiser4_object_t *parent,
				  place_func_t place_func, 
				  void *data, uint8_t mode);

extern errno_t dir40_check_struct(reiser4_object_t *object, 
				  place_func_t place_func,
				  void *data, uint8_t mode);
#endif
