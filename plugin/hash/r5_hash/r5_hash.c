/*
  r5_hash.c -- r5 hash implementation.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef ENABLE_R5_HASH

#include <reiser4/plugin.h>

uint64_t r5_hash_build(const unsigned char *name, uint32_t len) {
	uint32_t i;
	uint64_t a = 0;
	
	for (i = 0; i < len; i++) {
		a += name[i] << 4;
		a += name[i] >> 4;
		a *= 11;
	}
    
	return a;
}

static reiser4_plugin_t r5_hash_plugin = {
	.hash_ops = {
		.h = {
			.handle = EMPTY_HANDLE,
			.id = HASH_R5_ID,
			.group = 0,
			.type = HASH_PLUGIN_TYPE,
			.label = "r5_hash",
#ifndef ENABLE_STAND_ALONE
			.desc = "Implementation r5 hash for reiser4, ver. " VERSION
#else
			.desc = ""
#endif
		},
		.build = r5_hash_build
	}
};

static reiser4_plugin_t *r5_hash_start(reiser4_core_t *c) {
	return &r5_hash_plugin;
}

plugin_register(r5_hash_start, NULL);

#endif
