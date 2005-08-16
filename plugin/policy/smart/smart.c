/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   smart.c -- smart tail policy plugin. */

#ifndef ENABLE_MINIMAL
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
	.cl    = class_init,
	.id    = {TAIL_SMART_ID, 0, POLICY_PLUG_TYPE},
	.label = "smart",
	.desc  = "Smart tail policy plugin.",
	.o = {
		.policy_ops = &smart_ops
	}
};

static reiser4_plug_t *smart_start(reiser4_core_t *c) {
	return &smart_plug;
}

plug_register(smart, smart_start, NULL);
#endif
