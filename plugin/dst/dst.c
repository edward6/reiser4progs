/*
  dst.c -- reiser4 distribution plugins.
  Plugins are needed for the fsck and for all utilities when
  specifying them by the name with --override option.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_MINIMAL
#include "reiser4/plugin.h"

reiser4_plug_t triv_dst_plug = {
	.id    = {DST_TRIV_ID, 0, DST_PLUG_TYPE},
	.label = "trivial",
	.desc  = "Trivial distribution plugin.",
};

reiser4_plug_t fsw32_plug = {
	.id    = {DST_FSW32_ID, 0, DST_PLUG_TYPE},
	.label = "fsw32",
	.desc  = "Fiber-striped distribution plugin.",
};

#endif
