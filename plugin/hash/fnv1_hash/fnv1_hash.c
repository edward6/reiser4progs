/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   fnv1_hash.c -- fnv1 hash implementation. */

#ifdef ENABLE_FNV1_HASH
#include <reiser4/plugin.h>

static uint64_t fnv1_hash_build(unsigned char *name, uint32_t len) {
	uint32_t i;
	uint64_t a = 0xcbf29ce484222325ull;
	const uint64_t fnv_64_prime = 0x100000001b3ull;

	for(i = 0; i < len; i++) {
		a *= fnv_64_prime;
		a ^= (uint64_t)name[i];
	}
	return a;
}

static reiser4_hash_ops_t fnv1_hash_ops = {
	.build = fnv1_hash_build
};

static reiser4_plug_t fnv1_hash_plug = {
	.cl    = class_init,
	.id    = {HASH_FNV1_ID, 0, HASH_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
	.label = "fnv1_hash",
	.desc  = "Fnv1 hash for reiser4, ver. " VERSION,
#endif
	.o = {
		.hash_ops = &fnv1_hash_ops
	}
};

static reiser4_plug_t *fnv1_hash_start(reiser4_core_t *c) {
	return &fnv1_hash_plug;
}

plug_register(fnv1_hash, fnv1_hash_start, NULL);
#endif
