/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   print.h -- handlers for expanded printf keys like %k for printing
   reiser4_key_t and so on. */

#ifndef REISER4_PRINT_H
#define REISER4_PRINT_H

#ifndef ENABLE_STAND_ALONE
#include <reiser4/types.h>

extern errno_t reiser4_print_init(void);
extern errno_t reiser4_print_fini(void);

extern char *reiser4_print_key(reiser4_key_t *key,
			       uint16_t options);

extern char *repair_node_print(reiser4_node_t *node, uint32_t start, 
			       uint32_t count, uint16_t options);

#endif

#endif
