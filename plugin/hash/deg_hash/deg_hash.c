/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   deg_hash.c -- degenerate hash implementation. It is needed for testing
   reiser4progs with not unique keys. */

#ifdef ENABLE_DEG_HASH
#include <reiser4/plugin.h>

uint64_t deg_hash_build(char *name, uint32_t len) {
	return 0xc0c0c0c010101010ull;
}

static reiser4_hash_ops_t deg_hash_ops = {
	.build = deg_hash_build
};

static reiser4_plug_t deg_hash_plug = {
	.cl = class_init,
	.id = {HASH_DEG_ID, 0, HASH_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "deg_hash",
	.desc  = "Degenerate hash for reiser4, ver. " VERSION,
#endif
	.o = {
		.hash_ops = &deg_hash_ops
	}
};

static reiser4_plug_t *deg_hash_start(reiser4_core_t *c) {
	return &deg_hash_plug;
}

plug_register(deg_hash, deg_hash_start, NULL);
#endif
