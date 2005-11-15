/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tails.c -- tails only tail policy plugin. */

#ifndef ENABLE_MINIMAL
#include <reiser4/plugin.h>

static int tails_tails(uint64_t value) {
	return 1;
}

reiser4_policy_plug_t tails_plug = {
	.p = {
		.id    = {TAIL_ALWAYS_ID, 0, POLICY_PLUG_TYPE},
		.label = "tails",
		.desc  = "'Tails only' tail policy plugin.",
	},

	.tails = tails_tails
};
#endif
