/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   print.c -- handlers for expanded printf keys like %k for printing
   reiser4_key_t and so on. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#if defined(HAVE_PRINTF_H) && defined (HAVE_REGISTER_PRINTF_FUNCTION)

#include <printf.h>
#include <stdio.h>
#include <stdlib.h>
#include <reiser4/reiser4.h>

/* Support for the %k occurences in the formatted messages */
#define PA_KEY  (PA_LAST)

static int callback_info_key(const struct printf_info *info,
			     size_t n, int *argtypes)
{
	if (n > 0)
		argtypes[0] = PA_KEY | PA_FLAG_PTR;
    
	return 1;
}

static int callback_print_key(FILE *file, const struct printf_info *info, 
			      const void *const *args) 
{
	int len;
	reiser4_key_t *key;
	aal_stream_t stream;

	aal_stream_init(&stream);
    
	key = *((reiser4_key_t **)(args[0]));
	reiser4_key_print(key, &stream);

	fprintf(file, (char *)stream.data);
    
	len = stream.offset;
	aal_stream_fini(&stream);
	
	return len;
}

#endif

#endif

#ifndef ENABLE_STAND_ALONE

errno_t reiser4_print_init(void) {
	
#if defined(HAVE_PRINTF_H) && defined (HAVE_REGISTER_PRINTF_FUNCTION)
	register_printf_function('k', callback_print_key,
				 callback_info_key);

#endif
	
	return 0;
}

#endif
