/*
  fnv1_hash.c -- fnv1 hash implementation.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef ENABLE_FNV1_HASH

#include <reiser4/plugin.h>

static uint64_t fnv1_hash_build(const unsigned char *name, uint32_t len) {
	uint32_t i;
	uint64_t a = 0xcbf29ce484222325ull;
	const uint64_t fnv_64_prime = 0x100000001b3ull;

	for(i = 0; i < len; i++) {
		a *= fnv_64_prime;
		a ^= (uint64_t)name[i];
	}
	return a;
}

static reiser4_plugin_t fnv1_hash_plugin = {
	.hash_ops = {
		.h = {
			.handle = EMPTY_HANDLE,
			.id = HASH_FNV1_ID,
			.group = 0,
			.type = HASH_PLUGIN_TYPE,
			.label = "fnv1_hash",
#ifndef ENABLE_STAND_ALONE
			.desc = "Implementation fnv1 hash for reiser4, ver. " VERSION
#else
			.desc = ""
#endif
		},
		.build = fnv1_hash_build
	}
};

static reiser4_plugin_t *fnv1_hash_start(reiser4_core_t *c) {
	return &fnv1_hash_plugin;
}

plugin_register(fnv1_hash, fnv1_hash_start, NULL);

#endif
