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

extern uint32_t crc40_get_cluster_size(reiser4_place_t *place);

extern errno_t crc40_set_cluster_size(reiser4_place_t *place, 
				      uint32_t cluster);

#endif
#endif
