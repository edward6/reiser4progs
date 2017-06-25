/*
  volume.c -- reiser4 volume plugins.
  Plugins are needed for the fsck and for all  utilities when
  specifying them by the name with --override option.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_MINIMAL
#include "reiser4/plugin.h"

reiser4_plug_t simple_vol_plug = {
	.id    = {VOL_SIMPLE_ID, 0, VOL_PLUG_TYPE},
	.label = "simple",
	.desc  = "Simple Logical Volume.",
};

reiser4_plug_t asym_plug = {
	.id    = {VOL_ASYM_ID, 0, VOL_PLUG_TYPE},
	.label = "asym",
	.desc  = "Asymmetric Heterogeneous Logical Volume.",
};

#endif
