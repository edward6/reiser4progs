/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
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

static reiser4_hash_plug_t fnv1_hash = {
	.build = fnv1_hash_build
};

reiser4_plug_t fnv1_hash_plug = {
	.id    = {HASH_FNV1_ID, 0, HASH_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
	.label = "fnv1_hash",
	.desc  = "Fnv1 hash plugin.",
#endif
	.pl = {
		.hash = &fnv1_hash
	}
};
#endif
