/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tails.c -- tails only tail policy plugin. */

#ifndef ENABLE_STAND_ALONE
#include <reiser4/plugin.h>

static int tails_tails(uint64_t value) {
	return 1;
}

reiser4_policy_ops_t tails_ops = {
	.tails = tails_tails
};

static reiser4_plug_t tails_plug = {
	.cl    = CLASS_INIT,
	.id    = {TAIL_ALWAYS_ID, 0, POLICY_PLUG_TYPE},
	.label = "tails",
	.desc  = "Tail policy for tails only for reiser4, ver. " VERSION,
	.o = {
		.policy_ops = &tails_ops
	}
};

static reiser4_plug_t *tails_start(reiser4_core_t *c) {
	return &tails_plug;
}

plug_register(tails, tails_start, NULL);
#endif
