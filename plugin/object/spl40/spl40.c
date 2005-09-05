/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   spl40.c -- reiser4 special files plugin. */

#ifdef ENABLE_SPECIAL
#include "spl40.h"
#include "spl40_repair.h"

/* Opens special file and returns initialized instance to the caller */
errno_t spl40_open(reiser4_object_t *spl) {
	aal_assert("umka-2529", spl != NULL);
 	
	if (spl->info.start.plug->id.group != STAT_ITEM)
		return -EIO;
  
	/* Initalizing file handle. */
	obj40_init(spl);
	
	/* Initializing statdata place. */
	aal_memcpy(STAT_PLACE(spl), &spl->info.start,
		   sizeof(spl->info.start));
	
	return 0;
}

#ifndef ENABLE_MINIMAL
/* Creates special file and returns initialized instance to the caller. */
static errno_t spl40_create(reiser4_object_t *spl, object_hint_t *hint) {
	errno_t res;
	
	aal_assert("umka-2533", hint != NULL);
	aal_assert("vpf-1821",  spl != NULL);
	aal_assert("umka-2532", spl->info.tree != NULL);

	/* Inizializes file handle */
	obj40_init(spl);

	if ((res = obj40_create_stat(spl, 0, 0, hint->rdev, 0, 
				     hint->mode | 0644, NULL)))
	{
		return res;
	}

	return 0;
}
#endif

static reiser4_object_ops_t spl40_ops = {
#ifndef ENABLE_MINIMAL
	.create	        = spl40_create,
	.metadata       = obj40_metadata,
	.link           = obj40_link,
	.unlink         = obj40_unlink,
	.linked         = obj40_linked,
	.clobber        = obj40_clobber,
	.update         = obj40_save_stat,
	.check_struct	= spl40_check_struct,
	.recognize	= spl40_recognize,

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
	.open	        = spl40_open,
	.close	        = NULL
};

reiser4_plug_t spl40_plug = {
	.cl    = class_init,
	.id    = {OBJECT_SPL40_ID, SPL_OBJECT, OBJECT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
	.label = "spl40",
	.desc  = "Special file plugin.",
#endif
	.o = {
		.object_ops = &spl40_ops
	}
};

static reiser4_plug_t *spl40_start(reiser4_core_t *c) {
	obj40_core = c;
	return &spl40_plug;
}

plug_register(spl40, spl40_start, NULL);
#endif
