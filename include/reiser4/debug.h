/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   debug.h -- debug related stuff. */

#ifndef REISER4_DEBUG_H
#define REISER4_DEBUG_H

#if !defined(ENABLE_STAND_ALONE) && defined(ENABLE_DEBUG)
#include <reiser4/types.h>

extern void reiser4_print_format(reiser4_format_t *format,
				 uint16_t options);

extern void reiser4_print_node(node_t *node, uint32_t start, 
			       uint32_t count, uint16_t options);
#endif

#endif
