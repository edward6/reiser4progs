/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   lexic.c -- lexicographic fibration code. */

#ifdef ENABLE_LEXIC_FIBRE
#include <reiser4/plugin.h>

static uint8_t fibre_lexic_build(char *name, uint32_t len) {
	return 0;
}

reiser4_fibre_plug_t fibre_lexic_plug = {
	.p = {
		.id    = {FIBRE_LEXIC_ID, 0, FIBRE_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "lexic_fibre",
		.desc  = "Lexicographic fibration plugin.",
#endif
	},

	.build = fibre_lexic_build
};
#endif
