/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   crc40.h -- reiser4 crypto compression regular file plugin declarations. */

#ifndef CRC40_H
#define CRC40_H
#ifndef ENABLE_MINIMAL

#include <aal/libaal.h>
#include "reiser4/plugin.h"
#include "plugin/object/obj40/obj40.h"

extern errno_t crc40_check_struct(reiser4_object_t *crc, 
				  place_func_t func,
				  void *data, uint8_t mode);

#define reiser4_cluster_size(id) (4096 << ((uint32_t)id))

#endif
#endif
