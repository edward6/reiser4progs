/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   mpressure.c -- memory pressure detect functions common for all reiser4progs.
   Probably here should be more reliable method to determine memory pressure. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>

/* This function detects if mempry pressure is here. */
int misc_mpressure_detect(uint32_t nodes) {
	/* Simple and hardcoded rule. Will be replaced by some more sophisticated
	   later. */
	return nodes > 512;
}
