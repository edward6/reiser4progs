/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   r5_hash.c -- r5 hash implementation. */

#ifdef ENABLE_R5_HASH
#include <reiser4/plugin.h>

uint64_t r5_hash_build(unsigned char *name, uint32_t len) {
	uint32_t i;
	uint64_t a = 0;
	
	for (i = 0; i < len; i++) {
		a += name[i] << 4;
		a += name[i] >> 4;
		a *= 11;
	}
    
	return a;
}

reiser4_hash_plug_t r5_hash = {
	.build = r5_hash_build
};

static reiser4_plug_t r5_hash_plug = {
	.cl    = class_init,
	.id    = {HASH_R5_ID, 0, HASH_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
	.label = "r5_hash",
	.desc  = "R5 hash plugin.",
#endif
	.pl = {
		.hash = &r5_hash
	}
};

static reiser4_plug_t *r5_hash_start(reiser4_core_t *c) {
	return &r5_hash_plug;
}

plug_register(r5_hash, r5_hash_start, NULL);
#endif
