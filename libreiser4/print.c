/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   print.c -- printing different reiser4 objects stuff. */

#ifndef ENABLE_STAND_ALONE
#include <reiser4/reiser4.h>

static aal_stream_t stream;

errno_t reiser4_print_init(void) {
	return aal_stream_init(&stream);
}

errno_t reiser4_print_fini(void) {
	aal_stream_fini(&stream);
	return 0;
}

char *reiser4_print_key(reiser4_key_t *key) {
	aal_stream_reset(&stream);
	reiser4_key_print(key, &stream);
	return (char *)stream.data;
}
#endif
