/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key.c -- repair key code. */ 

#include <reiser4/libreiser4.h>

errno_t repair_key_check_struct(reiser4_key_t *key) {
	aal_assert("vpf-1279", key != NULL);

	return plug_call(key->plug->o.key_ops, check_struct, key);
}
