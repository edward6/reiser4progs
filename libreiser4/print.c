/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   print.c -- printing different reiser4 objects stuff. */

#ifndef ENABLE_MINIMAL

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/libreiser4.h>

static aal_list_t *current = NULL;
static aal_list_t *streams = NULL;

/* Adds passed stream to stream pool. */
static void reiser4_print_add_stream(aal_stream_t *stream) {
	streams = aal_list_append(streams, stream);
}

/* Removes passed stream from stream pool. */
static void reiser4_print_rem_stream(aal_stream_t *stream) {
	aal_list_t *next;
	
	next = aal_list_remove(streams, stream);

	if (!next || !next->prev)
		streams = next;
}

/* Initializes stream pool. It creates @pool number of streams, which will be
   used later for printing something to them. */
errno_t reiser4_print_init(uint32_t pool) {
	streams = NULL;
	
	for (; pool > 0; pool--) {
		aal_stream_t *stream;
		
		if (!(stream = aal_stream_create(NULL, &memory_stream)))
			return -ENOMEM;

		reiser4_print_add_stream(stream);
	}

	current = aal_list_first(streams);
	return 0;
}

/* Finalizes stream pool. */
void reiser4_print_fini(void) {
	aal_list_t *walk, *next;
	
	for (walk = streams; walk; walk = next)	{
		void *stream = walk->data;
		
		next = walk->next;
		
		reiser4_print_rem_stream(stream);
		aal_stream_fini(stream);
	}

	current = NULL;
	streams = NULL;
}

/* Prints passed @key with @options to some of stream from stream pool and
   retrun pointer to result. */
char *reiser4_print_key(reiser4_key_t *key) {
	aal_stream_t *stream;
	
	aal_assert("umka-2379", key != NULL);
	aal_assert("umka-3086", current != NULL);
	aal_assert("umka-3087", streams != NULL);

	stream = (aal_stream_t *)current->data;

	if (!(current = current->next))
		current = aal_list_first(streams);

	aal_stream_reset(stream);
	reiser4_key_print(key, stream, PO_DEFAULT);

	return (char *)stream->entity;
}

/* Prints passed @key with @options to some of stream from stream pool and
   retrun pointer to result. */
char *reiser4_print_inode(reiser4_key_t *key) {
	aal_stream_t *stream;
	
	aal_assert("umka-2379", key != NULL);
	aal_assert("umka-3086", current != NULL);
	aal_assert("umka-3087", streams != NULL);

	stream = (aal_stream_t *)current->data;

	if (!(current = current->next))
		current = aal_list_first(streams);

	aal_stream_reset(stream);
	reiser4_key_print(key, stream, PO_INODE);

	return (char *)stream->entity;
}

#ifdef ENABLE_DEBUG
void reiser4_print_node(reiser4_node_t *node, uint32_t start, 
			uint32_t count, uint16_t options) 
{
	aal_stream_t stream;
	
	aal_stream_init(&stream, NULL, &file_stream);
	
	if (!node || !node->block || !node->plug || 
	    node->plug->id.type != NODE_PLUG_TYPE)
	{
		aal_stream_format(&stream, "Does not look "
				  "like a proper node pointer.\n");
	} else {
		plug_call(node->plug->pl.node, print,
			  node, &stream, start, count, options);
	}
	
	aal_stream_fini(&stream);
}

void reiser4_print_format(reiser4_format_t *format,uint16_t options) {
	aal_stream_t stream;

	aal_assert("vpf-175", format != NULL);

	aal_stream_init(&stream, NULL, &file_stream);

	plug_call(format->ent->plug->pl.format,
		  print, format->ent, &stream, options);
	
	aal_stream_fini(&stream);
}
#endif

#endif
