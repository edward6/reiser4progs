/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_crypto.c -- crypto stat data extension plugin. */

#ifndef ENABLE_MINIMAL

#include "sdext_crypto.h"

reiser4_core_t *sdext_crypto_core = NULL;

extern reiser4_plug_t sdext_crypto_plug;

uint32_t sdext_crypto_length(stat_entity_t *stat, void *hint) {
	sdext_crypto_t e;
	uint16_t count;
	
	aal_assert("vpf-1842", stat != NULL || hint != NULL);
	
	if (hint) {
		count = ((sdhint_crypto_t *)hint)->signlen;
	} else {
		if (stat->info.digest == INVAL_PTR) {
			aal_error("Digest must be specified for \'%s\'.",
				  sdext_crypto_plug.label);
			return 0;
		}
		
		count = reiser4_keysign_size(stat->info.digest);
	}
	
	return sizeof(e.keylen) + count;
}

static errno_t sdext_crypto_open(stat_entity_t *stat, void *hint) {
	sdhint_crypto_t *crch;
	sdext_crypto_t *ext;
	
	aal_assert("vpf-1837", stat != NULL);
	aal_assert("vpf-1838", hint != NULL);
	
	if (stat->info.digest == INVAL_PTR) {
		aal_error("Digest must be specified for \'%s\'.",
			  sdext_crypto_plug.label);
		
		return -EIO;
	}
	
	crch = (sdhint_crypto_t *)hint;
	ext = (sdext_crypto_t *)stat_body(stat);
	crch->keylen = sdext_crypto_get_keylen(ext);
	crch->signlen = reiser4_keysign_size(stat->info.digest);
	aal_memcpy(crch->sign, ext->sign, crch->signlen);
	
	return 0;
}

static errno_t sdext_crypto_init(stat_entity_t *stat, void *hint) {
	sdhint_crypto_t *crch;
	sdext_crypto_t *ext;
	
	aal_assert("vpf-1839", stat != NULL);
	aal_assert("vpf-1840", hint != NULL);

	ext = (sdext_crypto_t *)stat_body(stat);
	crch = (sdhint_crypto_t *)hint;

	sdext_crypto_set_keylen(ext, crch->keylen);
	aal_memcpy(ext->sign, crch->sign, crch->signlen);
	
	return 0;
}

extern errno_t sdext_crypto_check_struct(stat_entity_t *stat, 
				      repair_hint_t *hint);

extern void sdext_crypto_print(stat_entity_t *stat, 
			    aal_stream_t *stream, 
			    uint16_t options);

static reiser4_sdext_plug_t sdext_crypto = {
	.open	   	= sdext_crypto_open,
	.init	   	= sdext_crypto_init,
	.info		= NULL,
	.print     	= sdext_crypto_print,
	.check_struct	= NULL,
	.open	   	= NULL,
	.length	   	= sdext_crypto_length
};

static reiser4_plug_t sdext_crypto_plug = {
	.cl    = class_init,
	.id    = {SDEXT_CRYPTO_ID, 0, SDEXT_PLUG_TYPE},
	.label = "sdext_crypto",
	.desc  = "Crypto stat data extension plugin.",
	.pl = {
		.sdext = &sdext_crypto
	}
};

static reiser4_plug_t *sdext_crypto_start(reiser4_core_t *c) {
	sdext_crypto_core = c;
	return &sdext_crypto_plug;
}

plug_register(sdext_crypto, sdext_crypto_start, NULL);

#endif
