/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
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

reiser4_fibre_plug_t fibre_dot_o_plug = {
	.p = {
		.id    = {FIBRE_DOT_O_ID, 0, FIBRE_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "dot_o_fibre",
		.desc  = "'.o' fibration plugin.",
#endif
	},

	.build = fibre_dot_o_build
};
#endif
