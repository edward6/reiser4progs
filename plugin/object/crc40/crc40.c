/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   crc40.c -- reiser4 crypto compression regular file plugin. */

#ifndef ENABLE_MINIMAL
#include "crc40.h"

#include <plugin/object/reg40/reg40.h>

static errno_t crc40_create(reiser4_object_t *crc, object_hint_t *hint) {
	errno_t res;
	
	aal_assert("vpf-1820", crc != NULL);
	aal_assert("vpf-1814", hint != NULL);
	aal_assert("vpf-1815", crc->info.tree != NULL);

	obj40_init(crc);
	
	/* Create stat data item with size, bytes, nlinks equal to zero. */
	if ((res = obj40_create_stat(crc, 0, 0, 0, 0, 
				     hint->mode | S_IFREG, NULL)))
	{
		return res;
	}
	
	reg40_reset(crc);
	
	return 0;
}

/* CRC regular file operations. */
static reiser4_object_ops_t crc40_ops = {
	.create	        = crc40_create,
	.write	        = NULL,
	.truncate       = NULL,
	.layout         = NULL,
	.metadata       = NULL,
	.convert        = NULL,
	.update         = obj40_save_stat,
	.link           = obj40_link,
	.unlink         = obj40_unlink,
	.linked         = obj40_linked,
	.clobber        = NULL,
	.recognize	= NULL,
	.check_struct   = NULL,
	
	.add_entry      = NULL,
	.rem_entry      = NULL,
	.build_entry    = NULL,
	.attach         = NULL,
	.detach         = NULL,
	
	.fake		= NULL,
	.check_attach 	= NULL,
	.lookup	        = NULL,
	.follow         = NULL,
	.readdir        = NULL,
	.telldir        = NULL,
	.seekdir        = NULL,
		
	.stat           = obj40_load_stat,
	.open	        = NULL,
	.close	        = NULL,
	.reset	        = reg40_reset,
	.seek	        = reg40_seek,
	.offset	        = reg40_offset,
	.read	        = NULL,
};

/* CRC regular file plugin. */
reiser4_plug_t crc40_plug = {
	.cl    = class_init,
	.id    = {OBJECT_CRC40_ID, REG_OBJECT, OBJECT_PLUG_TYPE},
	.label = "crc40",
	.desc  = "Crypto-Compression regular file plugin.",
	.o = {
		.object_ops = &crc40_ops
	}
};

/* Plugin factory related stuff. This method will be called during plugin
   initializing in plugin factory. */
static reiser4_plug_t *crc40_start(reiser4_core_t *c) {
	obj40_core = c;
	return &crc40_plug;
}

plug_register(crc40, crc40_start, NULL);
#endif
