/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   print.h -- different print functions. */

#ifndef REISER4_PRINT_H
#define REISER4_PRINT_H

#ifndef ENABLE_STAND_ALONE
#include <reiser4/types.h>

extern void reiser4_print_fini(void);

extern errno_t reiser4_print_init(uint32_t pool);

extern char *reiser4_print_key(reiser4_key_t *key,
			       uint16_t options);
#endif

#endif
