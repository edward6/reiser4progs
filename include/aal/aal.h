/*
  aal.h -- the central aal header. aal - application abstraction library.  It
  contains functions which will help libreiser4 work in any environment, out of
  the box. For now libaal supports two envinments: standard (usespace, libc) and
  so called allone mode - the mode, for instance, bootloaders work in (real mode
  of processor, no libc, etc).
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef AAL_H
#define AAL_H

/*
  As libaal may be used without any standard headers, we need to declare NULL
  macro here in order to avoid compilation errors.
*/
#undef NULL

#if defined(__cplusplus)
#  define NULL 0
#else
#  define NULL ((void *)0)
#endif

/*
  Here we define FALSE and TRUE macros in order to make sources more clean for
  understanding. I mean, that there where we need some boolean value, we will
  use these two macro.
*/
#if !defined(FALSE)
#  define FALSE 0
#endif

#if !defined(TRUE)
#  define TRUE 1
#endif

typedef int bool_t;

/* 
  Macro for checking the format string in situations like this:

  aal_exception_throw(EXCEPTION_ERROR, EXCEPTION_OK, "Operation %d failed.",
                      "open");

  As aal_exception_throw is declared with this macro, compiller in the comile
  time will make warning about incorrect format string.
*/
#ifdef __GNUC__
#  define __aal_check_format__(style, format, start) \
       __attribute__((__format__(style, format, start)))
#else
#  define __aal_check_format__(style, format, start)
#endif

#if !defined(__GNUC__) && (defined(__sparc__) || defined(__sparcv9))
#  include <sys/int_types.h>
#else
#  include <stdint.h>
#endif

#include <stdarg.h>

/* Simple type for direction denoting */
enum aal_direction {
	D_TOP    = 1 << 0,
	D_BOTTOM = 1 << 1,
	D_LEFT   = 1 << 2,
	D_RIGHT  = 1 << 3
};

typedef enum aal_direction aal_direction_t;

/* 
  This type is used for return of result of execution some function.
    
  Success - 0 (not errors),
  Failure - negative error code
*/
typedef int errno_t;

/*
  Type for callback compare function. It is used in list functions and in 
  other places.
*/
typedef int (*comp_func_t) (const void *, const void *, void *);

/* 
  Type for callback function that is called for each element of list. Usage is 
  the same as previous one.
*/
typedef int (*foreach_func_t) (const void *, const void *);

#include "device.h"
#include "file.h"
#include "exception.h"
#include "list.h"
#include "malloc.h"
#include "print.h"
#include "string.h"
#include "math.h"
#include "bitops.h"
#include "endian.h"
#include "debug.h"
#include "gauge.h"
#include "block.h"
#include "stream.h"
#include "ui.h"
#include "lru.h"

#endif

