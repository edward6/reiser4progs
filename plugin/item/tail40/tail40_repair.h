/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tail40_repair.h -- reiser4 tail plugin repair functions. */

#ifndef TAIL40_REPAIR_H
#define TAIL40_REPAIR_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

extern errno_t tail40_merge(place_t *place, trans_hint_t *hint);
extern errno_t tail40_prep_merge(place_t *place, trans_hint_t *hint);

extern errno_t tail40_pack(place_t *place, aal_stream_t *stream);
extern errno_t tail40_unpack(place_t *place, aal_stream_t *stream);

#endif
