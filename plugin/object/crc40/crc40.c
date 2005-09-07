/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   crc40.c -- reiser4 crypto compression regular file plugin. */

#ifndef ENABLE_MINIMAL
#include "crc40.h"

#include <crc40.h>

static int64_t crc40_read(reiser4_object_t *crc, 
			  void *buff, uint64_t n)
{
	aal_error("Plugin \"%s\": The method ->read is not "
		  "implemented yet.", reiser4_oplug(crc)->label);

	return -EIO;
}

static int64_t crc40_write(reiser4_object_t *crc, 
			   void *buff, uint64_t n)
{
	aal_error("Plugin \"%s\": The method ->write is not "
		  "implemented yet.", reiser4_oplug(crc)->label);

	return -EIO;
}

/* CRC regular file operations. */
static reiser4_object_plug_t crc40 = {
	.create	        = obj40_create,
	.write	        = crc40_write,
	.truncate       = NULL,
	.layout         = NULL,
	.metadata       = NULL,
	.convert        = NULL,
	.update         = obj40_save_stat,
	.link           = obj40_link,
	.unlink         = obj40_unlink,
	.linked         = obj40_linked,
	.clobber        = NULL,
	.recognize	= obj40_recognize,
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
	.open	        = obj40_open,
	.close	        = NULL,
	.reset	        = obj40_reset,
	.seek	        = obj40_seek,
	.offset	        = obj40_offset,
	.read	        = crc40_read,

	.sdext_mandatory = (1 << SDEXT_LW_ID),
	.sdext_unknown   = (1 << SDEXT_SYMLINK_ID)
};

/* CRC regular file plugin. */
reiser4_plug_t crc40_plug = {
	.cl    = class_init,
	.id    = {OBJECT_CRC40_ID, REG_OBJECT, OBJECT_PLUG_TYPE},
	.label = "crc40",
	.desc  = "Crypto-Compression regular file plugin.",
	.pl = {
		.object = &crc40
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
