/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   ccreg40.h -- reiser4 crypto compression regular file plugin declarations. */

#ifndef CCREG40_H
#define CCREG40_H
#ifndef ENABLE_MINIMAL

#include <aal/libaal.h>
#include "reiser4/plugin.h"
#include "plugin/object/obj40/obj40.h"

#define MIN_VALID_CLUSTER_SHIFT (0xc)
#define MAX_VALID_CLUSTER_SHIFT (0x10)
#define UNPREPPED_CLUSTER_SHIFT (0xff)

#define ccreg40_cloff(off, size) ((off) & ((size) - 1))

#define ccreg40_clstart(off, size) ((off) & ~((size) - 1))

#define ccreg40_clnext(off, size) (ccreg40_clstart(off, size) + (size))

#define ccreg40_clsame(off1, off2, size) \
	(ccreg40_clstart(off1, size) == ccreg40_clstart(off2, size))

extern errno_t ccreg40_check_struct(reiser4_object_t *cc, 
				    place_func_t func,
				    void *data, uint8_t mode);

extern errno_t ccreg40_get_cluster_shift(reiser4_place_t *place,
					 uint8_t *shift);

extern errno_t ccreg40_set_cluster_size(reiser4_place_t *place, 
					uint32_t cluster);

#endif
#endif
