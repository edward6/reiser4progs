/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   compress.c -- reiser4 compression plugins. The real compression is 
   not implemented yet, but plugins are needed for the fsck and for all 
   utilities when specifying them by the name with --override option. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_MINIMAL
#include "reiser4/plugin.h"

reiser4_plug_t lzo1_plug = {
	.id    = {COMPRESS_LZO1_ID, 0, COMPRESS_PLUG_TYPE},
	.label = "lzo1",
	.desc  = "Enabled Lzo1 compression.",
};

reiser4_plug_t nolzo1_plug = {
	.id    = {COMPRESS_NOLZO1_ID, 0, COMPRESS_PLUG_TYPE},
	.label = "nolzo1",
	.desc  = "Disabled Lzo1 compression.",
};

reiser4_plug_t gzip1_plug = {
	.id    = {COMPRESS_GZIP1_ID, 0, COMPRESS_PLUG_TYPE},
	.label = "gzip1",
	.desc  = "Enabled gzip1 compression.",
};

reiser4_plug_t nogzip1_plug = {
	.id    = {COMPRESS_NOGZIP1_ID, 0, COMPRESS_PLUG_TYPE},
	.label = "nogzip1",
	.desc  = "Disabled gzip1 compression.",
};
#endif
