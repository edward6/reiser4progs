/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   exception.h -- common for all progs exception functions. */

#ifndef MISC_EXCEPTION_H
#define MISC_EXCEPTION_H

#include <aal/exception.h>

extern void *misc_exception_get_stream(aal_exception_type_t type);

extern void misc_exception_set_stream(aal_exception_type_t type,
				       void *stream);
#endif

