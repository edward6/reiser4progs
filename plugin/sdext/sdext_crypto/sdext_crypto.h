/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_crypto.h -- crypto stat data extention plugin declaration. */

#ifndef SDEXT_CRC_H
#define SDEXT_CRC_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

#define reiser4_keysign_size(digestid) (4 << ((uint32_t)digestid))

typedef struct sdext_crypto {
	/* secret key size. */ 
	d16_t keylen;
	
	/* Signature. */
	d8_t sign[0];
} __attribute__((packed)) sdext_crypto_t;

extern reiser4_core_t *sdext_crypto_core;

#define sdext_crypto_get_keylen(ext)		aal_get_le16(ext, keylen)
#define sdext_crypto_set_keylen(ext, val)	aal_set_le16(ext, keylen, val)

#define sdext_crypto_get_signlen(ext)		aal_get_le16(ext, signlen)
#define sdext_crypto_set_signlen(ext, val)	aal_set_le16(ext, signlen, val)

uint32_t sdext_crypto_length(stat_entity_t *stat, void *hint);

#endif
