/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   print.h -- different print functions. */

#ifndef REISER4_PRINT_H
#define REISER4_PRINT_H

#ifndef ENABLE_MINIMAL
#include <reiser4/types.h>

extern void reiser4_print_fini(void);

extern errno_t reiser4_print_init(uint32_t pool);

extern char *reiser4_print_key(reiser4_key_t *key);

extern char *reiser4_print_inode(reiser4_key_t *key);

#ifdef ENABLE_DEBUG

extern void reiser4_print_format(reiser4_format_t *format, uint16_t options);

extern void reiser4_print_node(reiser4_node_t *node, uint32_t start, 
			       uint32_t count, uint16_t options);

#endif

#endif
#endif
