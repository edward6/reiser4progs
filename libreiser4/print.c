/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   print.c -- printing different reiser4 objects stuff. */

#ifndef ENABLE_STAND_ALONE
#include <stdlib.h>
#include <reiser4/reiser4.h>

aal_stream_t print_stream;

char *reiser4_print_key(reiser4_key_t *key,
			uint16_t options)
{
	aal_assert("umka-2379", key != NULL);
	
	aal_stream_reset(&print_stream);
	reiser4_key_print(key, &print_stream, options);
	
	return (char *)print_stream.entity;
}

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

void reiser4_print_format(reiser4_format_t *format, uint16_t options) {
	aal_stream_t stream;

	aal_assert("vpf-175", format != NULL);

	aal_stream_init(&stream, NULL, &file_stream);
	reiser4_format_print(format, &print_stream);
	aal_stream_fini(&stream);
}
#endif
