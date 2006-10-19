/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   compress_mode.c -- reiser4 compression mode plugins. The real 
   compression is not implemented yet, but plugins are needed for 
   the fsck and for all utilities when specifying them by the name 
   with --override option. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_MINIMAL
#include "reiser4/plugin.h"

reiser4_plug_t nocompress_plug = {
	.id    = {CMODE_NONE_ID, 0, CMODE_PLUG_TYPE},
	.label = "none",
	.desc  = "'Don't compress' compression mode plugin.",
};

reiser4_plug_t col8_plug = {
	.id    = {CMODE_COL8_ID, 0, CMODE_PLUG_TYPE},
	.label = "col8",
	.desc  = "'Check on lattice-8' compression mode plugin.",
};

reiser4_plug_t col16_plug = {
	.id    = {CMODE_COL16_ID, 0, CMODE_PLUG_TYPE},
	.label = "col16",
	.desc  = "'Check on lattice-16' compression mode plugin.",
};

reiser4_plug_t col32_plug = {
	.id    = {CMODE_COL32_ID, 0, CMODE_PLUG_TYPE},
	.label = "col32",
	.desc  = "'Check on lattice-32' compression mode plugin.",
};

reiser4_plug_t convx_plug = {
	.id    = {CMODE_CONVX_ID, 0, CMODE_PLUG_TYPE},
	.label = "conv",
	.desc  = "'Convert to extent' compression mode plugin.",
};

reiser4_plug_t force_plug = {
	.id    = {CMODE_FORCE_ID, 0, CMODE_PLUG_TYPE},
	.label = "force",
	.desc  = "'Compress evrything' compression mode plugin.",
};

#endif
