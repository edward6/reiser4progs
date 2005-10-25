/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   crc40.h -- reiser4 crypto compression regular file plugin declarations. */

#ifndef CRC40_H
#define CRC40_H
#ifndef ENABLE_MINIMAL

#include <aal/libaal.h>
#include "reiser4/plugin.h"
#include "plugin/object/obj40/obj40.h"

#define ccreg40_clstart(off, size) ((off) & ~((size) - 1))

#define ccreg40_clnext(off, size) (ccreg40_clstart(off, size) + (size))

#define ccreg40_clsame(off1, off2, size) \
	(ccreg40_clstart(off1, size) == ccreg40_clstart(off2, size))

#define ccreg40_clsize(crc) \
	(4096ll << crc->info.opset.plug[OPSET_CLUSTER]->id.id)

extern errno_t ccreg40_check_struct(reiser4_object_t *crc, 
				    place_func_t func,
				    void *data, uint8_t mode);

extern uint32_t ccreg40_get_cluster_size(reiser4_place_t *place);

extern errno_t ccreg40_set_cluster_size(reiser4_place_t *place, 
					uint32_t cluster);

#endif
#endif
