/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   spl40_repair.h -- reiser4 special file plugin repair methods. */

#ifndef SPL40_REPAIR_H
#define SPL40_REPAIR_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

extern object_entity_t *spl40_recognize(object_info_t *info);
extern errno_t spl40_check_struct(object_entity_t *object,
				  place_func_t place_func,
				  void *data, uint8_t mode);


#endif
