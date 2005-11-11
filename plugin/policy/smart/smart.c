/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   smart.c -- smart tail policy plugin. */

#ifndef ENABLE_MINIMAL
#include <reiser4/plugin.h>

static int smart_tails(uint64_t value) {
	if (value > 16384)
		return 0;
	
	return 1;
}

reiser4_policy_plug_t smart = {
	.tails = smart_tails
};

reiser4_plug_t smart_plug = {
	.id    = {TAIL_SMART_ID, 0, POLICY_PLUG_TYPE},
	.label = "smart",
	.desc  = "Smart tail policy plugin.",
	.pl = {
		.policy = &smart
	}
};
#endif
