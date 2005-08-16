/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   ext_t.c -- 1 symbol extention fibration code. */

#ifdef ENABLE_EXT_1_FIBRE
#include <reiser4/plugin.h>

static uint8_t fibre_ext_1_build(char *name, uint32_t len) {
	aal_assert("vpf-1566", name != NULL);
	
	if (len > 2 && name[len - 2] == '.')
		return (uint8_t)name[len - 1];

	return 0;
}

static reiser4_fibre_ops_t fibre_ext_1_ops = {
	.build = fibre_ext_1_build
};

static reiser4_plug_t fibre_ext_1_plug = {
	.cl    = class_init,
	.id    = {FIBRE_EXT_1_ID, 0, FIBRE_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
	.label = "ext_1_fibre",
	.desc  = "1-symbol extention fibration plugin.",
#endif
	.o = {
		.fibre_ops = &fibre_ext_1_ops
	}
};

static reiser4_plug_t *fibre_ext_1_start(reiser4_core_t *c) {
	return &fibre_ext_1_plug;
}

plug_register(fibre_ext_1, fibre_ext_1_start, NULL);
#endif
