/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.h -- reiser4 profile functions. */

#ifndef REISER4_PROFILE_H
#define REISER4_PROFILE_H

#ifndef ENABLE_STAND_ALONE
#include <reiser4/types.h>

extern void reiser4_profile_init();
extern void reiser4_profile_print(aal_stream_t *stream);

extern rid_t reiser4_profile_index(char *name);
extern errno_t reiser4_profile_set(rid_t type, rid_t id);
extern inline reiser4_plug_t *reiser4_profile_plug(rid_t index);
extern errno_t reiser4_profile_override(rid_t index, const char *pname);

extern void reiser4_profile_set_flag(rid_t index, uint8_t flag);
extern bool_t reiser4_profile_get_flag(rid_t index, uint8_t flag);

#endif
#endif
