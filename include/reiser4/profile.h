/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.h -- reiser4 profile functions. */

#ifndef REISER4_PROFILE_H
#define REISER4_PROFILE_H

extern uint64_t reiser4_profile_value(reiser4_profile_t *profile, 
				      const char *name); 

extern reiser4_pid_t *reiser4_profile_pid(reiser4_profile_t *profile,
					  const char *name);

#ifndef ENABLE_STAND_ALONE
extern errno_t reiser4_profile_override(reiser4_profile_t *profile, 
					const char *type, const char *name);
#endif

#endif
