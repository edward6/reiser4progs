/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   extents.c -- extents only tail policy plugin. */

#ifndef ENABLE_STAND_ALONE
#include <reiser4/plugin.h>

static int extents_tails(uint64_t value) {
	return 0;
}

reiser4_policy_ops_t extents_ops = {
	.tails = extents_tails
};

static reiser4_plug_t extents_plug = {
	.cl    = class_init,
	.id    = {TAIL_NEVER_ID, 0, POLICY_PLUG_TYPE},
	.label = "extents",
	.desc  = "Tail policy \"extents only\" for reiser4, ver. " VERSION,
	.o = {
		.policy_ops = &extents_ops
	}
};

static reiser4_plug_t *extents_start(reiser4_core_t *c) {
	return &extents_plug;
}

plug_register(extents, extents_start, NULL);
#endif
