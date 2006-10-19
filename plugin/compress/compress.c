/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   compress.c -- reiser4 compression transform plugins.
   Plugins are needed for the fsck and for all  utilities when
   specifying them by the name with --override option. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_MINIMAL
#include "reiser4/plugin.h"

reiser4_plug_t lzo1_plug = {
	.id    = {COMPRESS_LZO1_ID, 0, COMPRESS_PLUG_TYPE},
	.label = "lzo1",
	.desc  = "lzo1 compression transform plugin.",
};

reiser4_plug_t gzip1_plug = {
	.id    = {COMPRESS_GZIP1_ID, 0, COMPRESS_PLUG_TYPE},
	.label = "gzip1",
	.desc  = "gzip1 compression transform plugin.",
};

#endif
