/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   ext_3.c -- 3-symbol extention fibration code. */

#include <reiser4/plugin.h>

static uint8_t fibre_ext_3_build(char *name, uint32_t len) {
	if (len > 4 && name[len - 4] == '.')
		return (uint8_t)(name[len - 3] +
				 name[len - 2] +
				 name[len - 1]);

	return 0;
}

static reiser4_fibre_ops_t fibre_ext_3_ops = {
	.build = fibre_ext_3_build
};

static reiser4_plug_t fibre_ext_3_plug = {
	.cl    = class_init,
	.id    = {FIBRE_EXT_3_ID, 0, FIBRE_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "ext_3_fibre",
	.desc  = "Lexicographic fibration plugin for reiser4, ver. " VERSION,
#endif
	.o = {
		.fibre_ops = &fibre_ext_3_ops
	}
};

static reiser4_plug_t *fibre_ext_3_start(reiser4_core_t *c) {
	return &fibre_ext_3_plug;
}

plug_register(fibre_ext_3, fibre_ext_3_start, NULL);
