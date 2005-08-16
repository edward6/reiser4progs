/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.h -- methods for working with profile reiser4 programs. */

#ifndef MISC_PROFILE_H
#define MISC_PROFILE_H

#include <reiser4/types.h>

extern void misc_profile_print(void);
extern errno_t misc_profile_override(char *override);

#endif
