/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.h -- reiser4 profile functions. */

#ifndef REISER4_PROFILE_H
#define REISER4_PROFILE_H

#include <reiser4/types.h>

extern reiser4_profile_t default_profile;

#ifndef ENABLE_STAND_ALONE
extern errno_t reiser4_profile_override(const char *type,
					const char *name);
#endif

extern uint64_t reiser4_profile_value(const char *type); 
extern reiser4_pid_t *reiser4_profile_pid(const char *type);

#endif
