/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   r5_hash.c -- r5 hash implementation. */

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

reiser4_hash_ops_t r5_hash_ops = {
	.build = r5_hash_build
};

static reiser4_plugin_t r5_hash_plugin = {
	.h = {
		.class = CLASS_INIT,
		.id = HASH_R5_ID,
		.group = 0,
		.type = HASH_PLUGIN_TYPE,
#ifndef ENABLE_STAND_ALONE
		.label = "r5_hash",
		.desc = "Implementation r5 hash for reiser4, ver. " VERSION
#endif
	},
	.o = {
		.hash_ops = &r5_hash_ops
	}
};

static reiser4_plugin_t *r5_hash_start(reiser4_core_t *c) {
	return &r5_hash_plugin;
}

plugin_register(r5_hash, r5_hash_start, NULL);

#endif
