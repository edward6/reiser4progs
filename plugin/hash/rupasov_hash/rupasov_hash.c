/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   rupasov_hash.c -- rupasov hash. */

#ifdef ENABLE_RUPASOV_HASH
#include <reiser4/plugin.h>

uint64_t rupasov_hash_build(char *name, uint32_t len) {
	uint32_t i;
	uint64_t a, c;
	uint32_t j, pow;
	
	for (pow = 1, i = 1; i < len; i++) pow = pow * 10; 
	
	if (len == 1) 
		a = name[0] - 48;
	else
		a = (name[0] - 48) * pow;
	
	for (i = 1; i < len; i++) {
		c = name[i] - 48; 
	
		for (pow = 1, j = i; j < len - 1; j++) 
			pow = pow * 10;
	
		a = a + c * pow;
	}
	
	for (; i < 40; i++) {
		c = '0' - 48;
	
		for (pow = 1,j = i; j < len - 1; j++) 
			pow = pow * 10;
	
		a = a + c * pow;
	}
	
	for (; i < 256; i++) {
		c = i; 
	
		for (pow = 1, j = i; j < len - 1; j++) 
			pow = pow * 10;
	
		a = a + c * pow;
	}
	
	a = a << 7;
	return a;
}

static reiser4_hash_ops_t rupasov_hash_ops = {
	.build = rupasov_hash_build
};

static reiser4_plugin_t rupasov_hash_plugin = {
	.cl    = CLASS_INIT,
	.id    = {HASH_RUPASOV_ID, 0, HASH_PLUGIN_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "rupasov_hash",
	.desc  = "Rupasov hash for reiser4, ver. " VERSION,
#endif
	.o = {
		.hash_ops = &rupasov_hash_ops
	}
};

static reiser4_plugin_t *rupasov_hash_start(reiser4_core_t *c) {
	return &rupasov_hash_plugin;
}

plugin_register(rupasov_hash, rupasov_hash_start, NULL);
#endif
