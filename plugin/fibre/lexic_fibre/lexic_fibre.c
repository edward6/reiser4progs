/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   lexic.c -- lexicographic fibration code. */

#ifdef ENABLE_LEXIC_FIBRE
#include <reiser4/plugin.h>

static uint8_t fibre_lexic_build(char *name, uint32_t len) {
	return 0;
}

static reiser4_fibre_plug_t fibre_lexic = {
	.build = fibre_lexic_build
};

reiser4_plug_t fibre_lexic_plug = {
	.id    = {FIBRE_LEXIC_ID, 0, FIBRE_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
	.label = "lexic_fibre",
	.desc  = "Lexicographic fibration plugin.",
#endif
	.pl = {
		.fibre = &fibre_lexic
	}
};
#endif
