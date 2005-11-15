/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   deg_hash.c -- degenerate hash implementation. It is needed for testing
   reiser4progs with not unique keys. */

#ifdef ENABLE_DEG_HASH
#include <reiser4/plugin.h>

uint64_t deg_hash_build(unsigned char *name, uint32_t len) {
	return 0xc0c0c0c010101010ull;
}

reiser4_hash_plug_t deg_hash_plug = {
	.p = {
		.id = {HASH_DEG_ID, 0, HASH_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "deg_hash",
		.desc  = "Degenerate hash plugin.",
#endif
	},
	
	.build = deg_hash_build,
};
#endif
