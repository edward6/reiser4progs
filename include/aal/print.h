/*
  print.h -- printing and formating strings functions.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef AAL_PRINT_H
#define AAL_PRINT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef ENABLE_ALONE

extern int aal_vsnprintf(char *buff, unsigned int n, const char *format, 
			 va_list arg_list);

extern int aal_snprintf(char *buff, unsigned int n, const char *format, 
			...) __aal_check_format__(printf, 3, 4);

#else

#include <stdio.h>

#define aal_vsnprintf vsnprintf
#define aal_snprintf snprintf

#endif

#endif

