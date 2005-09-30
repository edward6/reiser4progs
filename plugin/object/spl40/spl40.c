/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   spl40.c -- reiser4 special files plugin. */

#ifdef ENABLE_SPECIAL
#include "spl40.h"
#include "spl40_repair.h"

static reiser4_object_plug_t spl40 = {
#ifndef ENABLE_MINIMAL
	.inherit	= obj40_inherit,
	.create	        = obj40_create,
	.metadata       = obj40_metadata,
	.link           = obj40_link,
	.unlink         = obj40_unlink,
	.linked         = obj40_linked,
	.clobber        = obj40_clobber,
	.update         = obj40_save_stat,
	.check_struct	= spl40_check_struct,
	.recognize	= obj40_recognize,

	.layout         = NULL,
	.seek	        = NULL,
	.write	        = NULL,
	.convert        = NULL,
	.truncate       = NULL,
	.rem_entry      = NULL,
	.add_entry      = NULL,
	.build_entry    = NULL,
	.attach         = NULL,
	.detach         = NULL,
	
	.fake		= NULL,
	.check_attach 	= NULL,
#endif
	.lookup	        = NULL,
	.reset	        = NULL,
	.offset	        = NULL,
	.readdir        = NULL,
	.telldir        = NULL,
	.seekdir        = NULL,
	.read	        = NULL,
	.follow         = NULL,
	
	.stat           = obj40_load_stat,
	.open	        = obj40_open,
	.close	        = NULL,

#ifndef ENABLE_MINIMAL
	.sdext_mandatory = (1 << SDEXT_LW_ID),
	.sdext_unknown   = (1 << SDEXT_SYMLINK_ID)
#endif
};

reiser4_plug_t spl40_plug = {
	.cl    = class_init,
	.id    = {OBJECT_SPL40_ID, SPL_OBJECT, OBJECT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
	.label = "spl40",
	.desc  = "Special file plugin.",
#endif
	.pl = {
		.object = &spl40
	}
};

static reiser4_plug_t *spl40_start(reiser4_core_t *c) {
	obj40_core = c;
	return &spl40_plug;
}

plug_register(spl40, spl40_start, NULL);
#endif
