/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_crc.h -- crypto stat data extention plugin declaration. */

#ifndef SDEXT_CRC_H
#define SDEXT_CRC_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

typedef struct sdext_crc {
	/* secret key size. */ 
	d16_t keylen;
	
	/* fingerprint length */
	d8_t signlen;
	
	/* fingerprint. */ 
	d8_t sign[0];
} __attribute__((packed)) sdext_crc_t;

extern reiser4_core_t *sdext_crc_core;

#define sdext_crc_get_keylen(ext)		aal_get_le16(ext, keylen)
#define sdext_crc_set_keylen(ext, val)		aal_set_le16(ext, keylen, val)

#define sdext_crc_get_signlen(ext)		aal_get_le16(ext, signlen)
#define sdext_crc_set_signlen(ext, val)		aal_set_le16(ext, signlen, val)

uint32_t sdext_crc_length(stat_entity_t *stat, void *hint);

#endif
