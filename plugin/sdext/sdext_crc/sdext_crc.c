/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_crc.c -- crypto stat data extension plugin. */

#ifndef ENABLE_MINIMAL

#include "sdext_crc.h"

reiser4_core_t *sdext_crc_core = NULL;

uint32_t sdext_crc_length(stat_entity_t *stat, void *hint) {
	sdext_crc_t e;
	uint16_t count;
	
	aal_assert("vpf-1842", stat != NULL || hint != NULL);
	
	if (hint) {
		count = ((sdhint_crc_t *)hint)->signlen;
	} else {
		count = sdext_crc_get_signlen((sdext_crc_t *)stat_body(stat));
	}
	
	return sizeof(e.keylen) + count;
}

static errno_t sdext_crc_open(stat_entity_t *stat, void *hint) {
	sdhint_crc_t *crch;
	sdext_crc_t *ext;
	
	aal_assert("vpf-1837", stat != NULL);
	aal_assert("vpf-1838", hint != NULL);
	
	crch = (sdhint_crc_t *)hint;
	ext = (sdext_crc_t *)stat_body(stat);
	crch->keylen = sdext_crc_get_keylen(ext);
	crch->signlen = sdext_crc_get_signlen(ext);
	aal_memcpy(crch->sign, ext->sign, crch->signlen);
	
	return 0;
}

static errno_t sdext_crc_init(stat_entity_t *stat, void *hint) {
	sdhint_crc_t *crch;
	sdext_crc_t *ext;
	
	aal_assert("vpf-1839", stat != NULL);
	aal_assert("vpf-1840", hint != NULL);

	ext = (sdext_crc_t *)stat_body(stat);
	crch = (sdhint_crc_t *)hint;

	sdext_crc_set_keylen(ext, crch->keylen);
	sdext_crc_set_signlen(ext, crch->signlen);
	aal_memcpy(ext->sign, crch->sign, crch->signlen);
	
	return 0;
}

extern errno_t sdext_crc_check_struct(stat_entity_t *stat, 
				      repair_hint_t *hint);

extern void sdext_crc_print(stat_entity_t *stat, 
			    aal_stream_t *stream, 
			    uint16_t options);

static reiser4_sdext_plug_t sdext_crc = {
	.open	   	= sdext_crc_open,
	.init	   	= sdext_crc_init,
	.print     	= sdext_crc_print,
	.check_struct	= NULL,
	.open	   	= NULL,
	.length	   	= sdext_crc_length
};

static reiser4_plug_t sdext_crc_plug = {
	.cl    = class_init,
	.id    = {SDEXT_CRYPTO_ID, 0, SDEXT_PLUG_TYPE},
	.label = "sdext_crc",
	.desc  = "Crypto stat data extension plugin.",
	.pl = {
		.sdext = &sdext_crc
	}
};

static reiser4_plug_t *sdext_crc_start(reiser4_core_t *c) {
	sdext_crc_core = c;
	return &sdext_crc_plug;
}

plug_register(sdext_crc, sdext_crc_start, NULL);

#endif
