/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   extents.c -- extents only tail policy plugin. */

#ifndef ENABLE_MINIMAL
#include <reiser4/plugin.h>

static int extents_tails(uint64_t value) {
	return 0;
}

reiser4_policy_plug_t extents = {
	.tails = extents_tails
};

reiser4_plug_t extents_plug = {
	.id    = {TAIL_NEVER_ID, 0, POLICY_PLUG_TYPE},
	.label = "extents",
	.desc  = "'Extents only' tail policy plugin.",
	.pl = {
		.policy = &extents
	}
};
#endif
