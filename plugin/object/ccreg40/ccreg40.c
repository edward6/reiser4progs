/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   crc40.c -- reiser4 crypto compression regular file plugin. */

#ifndef ENABLE_MINIMAL

#include "ccreg40.h"
#include "ccreg40_repair.h"

uint32_t crc40_get_cluster_size(reiser4_place_t *place) {
	trans_hint_t hint;
	ctail_hint_t chint;
	
	aal_assert("vpf-1866", place != NULL);
	
	hint.specific = &chint;
	hint.count = 1;

	if (plug_call(place->plug->pl.item->object, 
		      fetch_units, place, &hint) != 1)
	{
		return MAX_UINT32;
	}

	return 1 << chint.shift;
}

errno_t crc40_set_cluster_size(reiser4_place_t *place, uint32_t cluster) {
	trans_hint_t hint;
	ctail_hint_t chint;
	
	aal_assert("vpf-1867", place != NULL);
	
	hint.specific = &chint;
	hint.count = 1;
	chint.shift = aal_log2(cluster);

	if (plug_call(place->plug->pl.item->object, 
		      update_units, place, &hint) != 0)
	{
		return -EIO;
	}

	return 0;
}

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
static reiser4_object_plug_t ccreg40 = {
	.inherit	= obj40_inherit,
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
	.check_struct   = crc40_check_struct,
	
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
	.sdext_unknown   = (1 << SDEXT_SYMLINK_ID | 
			    1 << SDEXT_CLUSTER_ID)
};

/* CRC regular file plugin. */
reiser4_plug_t ccreg40_plug = {
	.cl    = class_init,
	.id    = {OBJECT_CRC40_ID, REG_OBJECT, OBJECT_PLUG_TYPE},
	.label = "crc40",
	.desc  = "Crypto-Compression regular file plugin.",
	.pl = {
		.object = &ccreg40
	}
};

/* Plugin factory related stuff. This method will be called during plugin
   initializing in plugin factory. */
static reiser4_plug_t *ccreg40_start(reiser4_core_t *c) {
	obj40_core = c;
	return &ccreg40_plug;
}

plug_register(ccreg40, ccreg40_start, NULL);
#endif
