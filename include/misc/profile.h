/*
  profile.h -- headers of methods for working with profiles in reiser4 programs.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef MISC_PROFILE_H
#define MISC_PROFILE_H

#include <reiser4/types.h>

extern void misc_profile_list(void);
extern void misc_profile_print(reiser4_profile_t *profile);

extern reiser4_profile_t *misc_profile_default(void);
extern reiser4_profile_t *misc_profile_find(const char *name);

extern errno_t misc_profile_override(reiser4_profile_t *profile,
				     char *override);
#endif
