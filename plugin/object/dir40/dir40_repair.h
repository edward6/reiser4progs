/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40_repair.h -- reiser4 hashed directory plugin repair functions. */

#ifndef DIR40_REPAIR_H
#define DIR40_REPAIR_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern errno_t dir40_form(object_entity_t *object);
extern object_entity_t *dir40_fake(object_info_t *info);
extern object_entity_t *dir40_recognize(object_info_t *info);


extern errno_t dir40_check_attach(object_entity_t *object, 
				  object_entity_t *parent,
				  place_func_t place_func, 
				  void *data, uint8_t mode);

extern errno_t dir40_check_struct(object_entity_t *object, 
				  place_func_t place_func,
				  void *data, uint8_t mode);
#endif

