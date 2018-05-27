/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   reg42.c -- reiser4 regular striped file plugin. */

#ifndef ENABLE_MINIMAL
#  include <unistd.h>
#endif

#include <aal/libaal.h>
#include "reiser4/plugin.h"
#include "plugin/object/obj40/obj40.h"
#include "../reg40/reg40_repair.h"

/*
 * Striped regular file plugin
 */
reiser4_object_plug_t reg42_plug = {
	.p = {
		.id    = {OBJECT_REG42_ID, REG_OBJECT, OBJECT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "reg42",
		.desc  = "Striped regular file plugin.",
#endif
	},

#ifndef ENABLE_MINIMAL
	.inherit	= NULL,//obj40_inherit,
	.create	        = NULL,//reg40_create,
	.write	        = NULL,//reg40_write,
	.truncate       = NULL,//reg40_truncate,
	.layout         = NULL,//reg40_layout,
	.metadata       = NULL,//reg40_metadata,
	.convert        = NULL,//reg40_convert,
	.link           = NULL,//obj40_link,
	.unlink         = NULL,//obj40_unlink,
	.linked         = NULL,//obj40_linked,
	.clobber        = NULL,//reg40_clobber,
	.recognize	= NULL,//obj40_recognize,
	.check_struct   = NULL,//reg40_check_struct,

	.add_entry      = NULL,
	.rem_entry      = NULL,
	.build_entry    = NULL,
	.attach         = NULL,
	.detach         = NULL,

	.fake		= NULL,
	.check_attach 	= NULL,
#endif
	.lookup	        = NULL,
	.follow         = NULL,
	.readdir        = NULL,
	.telldir        = NULL,
	.seekdir        = NULL,

	.stat           = NULL,//obj40_load_stat,
	.open	        = NULL,//reg40_open,
	.close	        = NULL,
	.reset	        = NULL,//obj40_reset,
	.seek	        = NULL,//obj40_seek,
	.offset	        = NULL,//obj40_offset,
	.read	        = NULL,//reg40_read,

#ifndef ENABLE_MINIMAL
	.sdext_mandatory = (1 << SDEXT_LW_ID),
	.sdext_unknown   = (1 << SDEXT_SYMLINK_ID),
#endif
};
