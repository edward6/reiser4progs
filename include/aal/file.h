/*
  file.h -- standard file device that works via device interface.
    
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef FILE_H
#define FILE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_COMPACT

#include <aal/device.h>

extern struct aal_device_ops file_ops;

#endif

#endif

