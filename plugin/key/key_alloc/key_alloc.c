/*
 * key_alloc.c - reiser4 key allocation schemes
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_MINIMAL
#include "reiser4/plugin.h"
#include <misc/misc.h>

extern reiser4_keyalloc_plug_t keyalloc_plana_plug;
extern reiser4_keyalloc_plug_t keyalloc_planb_plug;

reiser4_keyalloc_plug_t keyalloc_plana_plug = {
	.p = {
		.id    = {KEYALLOC_PLANA_ID, 0, KEYALLOC_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "plana",
		.desc  = "Plan-A key allocation scheme.",
#endif
	}
};

reiser4_keyalloc_plug_t keyalloc_planb_plug = {
	.p = {
		.id    = {KEYALLOC_PLANB_ID, 0, KEYALLOC_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "planb",
		.desc  = "Plan-B key allocation scheme.",
#endif
	}
};

#endif
