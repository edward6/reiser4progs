/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
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
extern errno_t reiser4_param_set(const char *name, rid_t id);
extern void reiser4_param_set_flag(const char *name, uint8_t flag);
extern bool_t reiser4_param_get_flag(const char *name, uint8_t flag);

#endif
#endif
