/*
  debugfs.h -- debugs used functions.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef DEBUGFS_H
#define DEBUGFS_H

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include <misc/misc.h>
#include <reiser4/reiser4.h>

#include "types.h"
#include "browse.h"
#include "print.h"
#include "measure.h"

extern errno_t debugfs_print_stream(aal_stream_t *stream);
extern errno_t debugfs_print_buff(void *buff, uint32_t size);

#endif
