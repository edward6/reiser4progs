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

char *reiser4_print_key(reiser4_key_t *key,
			uint16_t options)
{
	aal_assert("umka-2379", key != NULL);
	
	aal_stream_reset(&stream);
	reiser4_key_print(key, &stream, options);
	
	return (char *)stream.data;
}

char *reiser4_print_node(reiser4_node_t *node, uint32_t start, 
			uint32_t count, uint16_t options) 
{
	if (node == NULL)
		return NULL;
	
	aal_stream_reset(&stream);
	
	plug_call(node->entity->plug->o.node_ops, print,
		  node->entity, &stream, start, count, options);
	
	return (char *)stream.data;
}

char *reiser4_print_format(reiser4_format_t *format, uint16_t options) {
	aal_assert("vpf-175", format != NULL);

	aal_stream_reset(&stream);
	
	plug_call(format->entity->plug->o.format_ops, print, 
		  format->entity, &stream, options);

	return (char *)stream.data;
}
#endif
