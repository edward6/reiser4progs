/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   smart.c -- smart tail policy plugin. */

#ifndef ENABLE_STAND_ALONE
#include <reiser4/plugin.h>

static int smart_tails(uint64_t value) {
	if (value > 16384)
		return 0;
	
	return 1;
}

reiser4_policy_ops_t smart_ops = {
	.tails = smart_tails
};

static reiser4_plug_t smart_plug = {
	.cl    = CLASS_INIT,
	.id    = {TAIL_SMART_ID, 0, POLICY_PLUG_TYPE},
	.label = "smart",
	.desc  = "Smart tail policy for reiser4, ver. " VERSION,
	.o = {
		.policy_ops = &smart_ops
	}
};

static reiser4_plug_t *smart_start(reiser4_core_t *c) {
	return &smart_plug;
}

plug_register(smart, smart_start, NULL);
#endif
