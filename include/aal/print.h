/*
  print.h -- printing and formating strings functions.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef PRINT_H
#define PRINT_H

#ifdef ENABLE_COMPACT

extern int aal_vsnprintf(char *buff, size_t n, const char *format, 
			 va_list arg_list);

extern int aal_snprintf(char *buff, size_t n, const char *format, 
			...) __check_format__(printf, 3, 4);

#else

#include <stdio.h>

#define aal_vsnprintf vsnprintf
#define aal_snprintf snprintf

#endif

#endif

