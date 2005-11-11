/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   rupasov_hash.c -- rupasov hash. */

#ifdef ENABLE_RUPASOV_HASH
#include <reiser4/plugin.h>

uint64_t rupasov_hash_build(unsigned char *name, uint32_t len) {
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

static reiser4_hash_plug_t rupasov_hash = {
	.build = rupasov_hash_build
};

reiser4_plug_t rupasov_hash_plug = {
	.id    = {HASH_RUPASOV_ID, 0, HASH_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
	.label = "rupasov_hash",
	.desc  = "Rupasov hash plugin.",
#endif
	.pl = {
		.hash = &rupasov_hash
	}
};
#endif
