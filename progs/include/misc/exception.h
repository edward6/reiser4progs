/*
  exception.h -- common for all progs exception functions.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef PROGS_EXCEPTION_H
#define PROGS_EXCEPTION_H

#include <aal/exception.h>

extern void progs_exception_set_stream(aal_exception_type_t type, void *stream);
extern void *progs_exception_get_stream(aal_exception_type_t type);

#endif

