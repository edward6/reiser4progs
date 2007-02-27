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

reiser4_plug_t lattd_plug = {
	.id    = {CMODE_LATTD_ID, 0, CMODE_PLUG_TYPE},
	.label = "latt",
	.desc  = "'Check on dynamic lattice' compression mode plugin.",
};

reiser4_plug_t ultim_plug = {
	.id    = {CMODE_ULTIM_ID, 0, CMODE_PLUG_TYPE},
	.label = "ultim",
	.desc  = "'Check ultimately' compression mode plugin.",
};

reiser4_plug_t force_plug = {
	.id    = {CMODE_FORCE_ID, 0, CMODE_PLUG_TYPE},
	.label = "force",
	.desc  = "'Compress evrything' compression mode plugin.",
};

reiser4_plug_t convx_plug = {
	.id    = {CMODE_CONVX_ID, 0, CMODE_PLUG_TYPE},
	.label = "conv",
	.desc  = "'Convert to extent' compression mode plugin.",
};
#endif
