/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   debug.h -- debug related stuff. */

#ifndef REISER4_DEBUG_H
#define REISER4_DEBUG_H

#if !defined(ENABLE_MINIMAL) && defined(ENABLE_DEBUG)

#include <reiser4/types.h>

extern void reiser4_print_format(reiser4_format_t *format, uint16_t options)
				__attribute__((used));

extern void reiser4_print_node(reiser4_node_t *node, uint32_t start, 
			       uint32_t count, uint16_t options)
			       __attribute__((used));
#endif

#endif
