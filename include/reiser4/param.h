/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   param.h -- reiser4 parameter functions. */

#ifndef REISER4_PARAM_H
#define REISER4_PARAM_H

#ifndef ENABLE_STAND_ALONE
#include <reiser4/types.h>

extern reiser4_param_t default_param;

extern errno_t reiser4_param_override(const char *name,
				      const char *value);

extern uint64_t reiser4_param_value(const char *name); 
extern reiser4_pid_t *reiser4_param_pid(const char *name);

#endif
#endif
