/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dot_o.c -- ".o" fibration code. */

#ifdef ENABLE_DOT_O_FIBRE
#include <reiser4/plugin.h>

static uint8_t fibre_dot_o_build(char *name, uint32_t len) {
	aal_assert("vpf-1565", name != NULL);
	
	if (len > 2 && name[len - 1] == 'o' && name[len - 2] == '.')
		return 1;
	
	return 0;
}

static reiser4_fibre_ops_t fibre_dot_o_ops = {
	.build = fibre_dot_o_build
};

static reiser4_plug_t fibre_dot_o_plug = {
	.cl    = class_init,
	.id    = {FIBRE_DOT_O_ID, 0, FIBRE_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "dot_o_fibre",
	.desc  = "'.o' fibration plugin for reiser4, ver. " VERSION,
#endif
	.o = {
		.fibre_ops = &fibre_dot_o_ops
	}
};

static reiser4_plug_t *fibre_dot_o_start(reiser4_core_t *c) {
	return &fibre_dot_o_plug;
}

plug_register(fibre_dot_o, fibre_dot_o_start, NULL);
#endif
