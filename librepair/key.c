/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key.c -- repair key code. */ 

#include <reiser4/libreiser4.h>

errno_t repair_key_check_struct(reiser4_key_t *key) {
	aal_assert("vpf-1279", key != NULL);

	return objcall(key, check_struct);
}
