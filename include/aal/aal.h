/*
  aal.h -- the central libaal header. aal - application abstraction library. It
  contains functions which help to work in any environment, out of the box. For
  now libaal supports two envinments: standard (userspace, libc, etc.) and so
  called "alone" mode - the mode, bootloaders work in (real mode of processor,
  no libc, etc).
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef AAL_H
#define AAL_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdarg.h>

#include "types.h"
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

