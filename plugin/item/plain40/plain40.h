/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   plain40.h -- reiser4 plain tail item plugin functions. */

#ifndef PLAIN40_H
#define PLAIN40_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern reiser4_core_t *plain40_core;

extern errno_t plain40_prep_shift(reiser4_place_t *src_place,
				  reiser4_place_t *dst_place,
				  shift_hint_t *hint);

extern errno_t plain40_prep_write(reiser4_place_t *place, 
				  trans_hint_t *hint);

#endif
