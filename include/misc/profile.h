/*
  profile.h -- headers of methods for working with profiles in reiser4 programs.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef PROGS_PROFILE_H
#define PROGS_PROFILE_H

#include <reiser4/types.h>

extern void progs_profile_list(void);
extern void progs_profile_print(reiser4_profile_t *profile);

extern reiser4_profile_t *progs_profile_default(void);
extern reiser4_profile_t *progs_profile_find(const char *name);

extern errno_t progs_profile_override(reiser4_profile_t *profile,
				      char *override);

#endif
