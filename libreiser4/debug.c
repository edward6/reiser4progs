/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   debug.c -- debug related stuff. */

#if !defined(ENABLE_STAND_ALONE) && defined(ENABLE_DEBUG)
#include <reiser4/reiser4.h>

void reiser4_print_node(node_t *node, uint32_t start, 
			uint32_t count, uint16_t options) 
{
	aal_stream_t stream;
	
	aal_assert("umka-2642", node != NULL);
	
	aal_stream_init(&stream, NULL, &file_stream);

	plug_call(node->entity->plug->o.node_ops, print,
		  node->entity, &stream, start,  count, options);
	
	aal_stream_fini(&stream);
}

void reiser4_print_format(reiser4_format_t *format,
			  uint16_t options)
{
	aal_stream_t stream;

	aal_assert("vpf-175", format != NULL);

	aal_stream_init(&stream, NULL, &file_stream);
	reiser4_format_print(format, &print_stream);
	aal_stream_fini(&stream);
}
#endif
