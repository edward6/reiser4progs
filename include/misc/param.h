/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   param.h -- methods for working with params reiser4
   programs. */

#ifndef MISC_PARAM_H
#define MISC_PARAM_H

#include <reiser4/types.h>

extern void misc_param_print(void);
extern errno_t misc_param_override(char *override);
#endif
