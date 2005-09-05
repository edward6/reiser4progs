/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   spl40_repair.h -- reiser4 special file plugin repair methods. */

#ifndef SPL40_REPAIR_H
#define SPL40_REPAIR_H

#include "spl40.h"
#include "plugin/object/obj40/obj40_repair.h"

extern errno_t spl40_recognize(reiser4_object_t *spl);

extern errno_t spl40_check_struct(reiser4_object_t *spl,
				  place_func_t place_func,
				  void *data, uint8_t mode);


#endif
