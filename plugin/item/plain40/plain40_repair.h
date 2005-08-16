/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   plain40_repair.h -- reiser4 plain tail item plugin repair functions. */

#ifndef PLAIN40_REPAIR_H
#define PLAIN40_REPAIR_H

#ifndef ENABLE_MINIMAL
#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern errno_t plain40_prep_insert_raw(reiser4_place_t *place, 
				       trans_hint_t *hint);

#endif
#endif
