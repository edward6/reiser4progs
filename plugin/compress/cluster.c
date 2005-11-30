/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   cluster.c -- reiser4 cluster plugins. The real compression/encryption 
   is not implemented yet, but plugins are needed for the fsck and for all 
   utilities when specifying them by the name with --override option. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_MINIMAL
#include "reiser4/plugin.h"

reiser4_cluster_plug_t clust64_plug = {
	.p = {
		.id    = {CLUSTER_64K_ID, 0, CLUSTER_PLUG_TYPE},
		.label = "64K",
		.desc  = "64K size cluster plugin.",
	},
	.clsize = 64 * 1024,
};

reiser4_cluster_plug_t clust32_plug = {
	.p = {
		.id    = {CLUSTER_32K_ID, 0, CLUSTER_PLUG_TYPE},
		.label = "32K",
		.desc  = "32K size cluster plugin.",
	},
	.clsize = 32 * 1024,
};

reiser4_cluster_plug_t clust16_plug = {
	.p = {
		.id    = {CLUSTER_16K_ID, 0, CLUSTER_PLUG_TYPE},
		.label = "16K",
		.desc  = "16K size cluster plugin.",
	},
	.clsize = 16 * 1024,
};

reiser4_cluster_plug_t clust8_plug = {
	.p = {
		.id    = {CLUSTER_8K_ID, 0, CLUSTER_PLUG_TYPE},
		.label = "8K",
		.desc  = "8K size cluster plugin.",
	},
	.clsize = 8 * 1024,
};

reiser4_cluster_plug_t clust4_plug = {
	.p = {
		.id    = {CLUSTER_4K_ID, 0, CLUSTER_PLUG_TYPE},
		.label = "4K",
		.desc  = "4K size cluster plugin.",
	},
	.clsize = 4 * 1024,
};
#endif
