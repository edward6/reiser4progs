/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   mpressure.c -- memory pressure detect functions common for all
   reiser4progs. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/libreiser4.h>

static uint32_t watermark = 512;

void misc_mpressure_setup(uint32_t value) {
	watermark = value;
}

/* This function detects if mempry pressure is here. */
int misc_mpressure_detect(reiser4_tree_t *tree) {
	return tree->nodes->real + tree->data->real > watermark;
}
