/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   mpressure.c -- memory pressure detect functions common for all
   reiser4progs. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/libreiser4.h>

/* This is somehow opaque and seem like a magic digit. But the reasons to choose
   this value are the following:
   
   (1) Make flushing not so often, as this is not productive.
   
   (2) Try to make a disk layout better. It depends on how often tree is
   adjusted and thigs are allocated (node pointers and extents). So, this value
   is such as able to help some hipotetical big extent to fit into device region
   between two bitmap blocks.
*/
static uint32_t watermark = 5120;

void misc_mpressure_setup(uint32_t value) {
	watermark = value;
}

/* This function detects if mempry pressure is here. */
int misc_mpressure_detect(reiser4_tree_t *tree) {
	return tree->nodes->real + tree->blocks->real > watermark;
}
