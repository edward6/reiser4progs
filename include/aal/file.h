/*
  file.h -- standard file device.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef AAL_FILE_H
#define AAL_FILE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_ALONE

#include <aal/types.h>
extern struct aal_device_ops file_ops;

#endif

#endif

