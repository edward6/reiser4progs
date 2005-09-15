/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_crc.h -- crypto stat data extention plugin declaration. */

#ifndef SDEXT_CRC_H
#define SDEXT_CRC_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

typedef struct sdext_crc {
	/* secret key size. */ 
	d16_t keysize;
	
	/* secret key id. */ 
	d8_t keyid[0];
} __attribute__((packed)) sdext_crc_t;

extern reiser4_core_t *sdext_crc_core;

#define sdext_crc_get_key_size(ext)		aal_get_le16(ext, keysize)
#define sdext_crc_set_key_size(ext, val)	aal_set_le16(ext, keysize, val)

uint32_t sdext_crc_length(stat_entity_t *stat, void *hint);

#endif
