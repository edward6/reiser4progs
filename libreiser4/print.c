/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   print.c -- printing different reiser4 objects stuff. */

#ifndef ENABLE_STAND_ALONE
#include <reiser4/libreiser4.h>

static uint32_t curr_size; 
static uint32_t heap_size;
static aal_list_t *streams;

static void reiser4_print_add_stream(aal_stream_t *stream) {
	aal_list_t *new;
	
	if (curr_size + 1 > heap_size)
		reiser4_print_recycle(heap_size);

	new = aal_list_append(streams, stream);

	if (!new->prev)
		streams = new;

	curr_size++;
}

static void reiser4_print_rem_stream(aal_stream_t *stream) {
	aal_list_t *next;
	
	next = aal_list_remove(streams, stream);

	if (!next || !next->prev)
		streams = next;
	
	curr_size--;
}

void reiser4_print_recycle(uint32_t heap) {
	aal_list_t *walk, *next;

	for (walk = streams; walk && curr_size > heap;
	     walk = next)
	{
		next = walk->next;
		aal_stream_fini(walk->data);
		reiser4_print_rem_stream(walk->data);
	}
}

void reiser4_print_init(uint32_t heap) {
	curr_size = 0;
	streams = NULL;
	heap_size = heap;
}

void reiser4_print_fini(void) {
	reiser4_print_recycle(0);
}

char *reiser4_print_key(reiser4_key_t *key,
			uint16_t options)
{
	aal_stream_t *stream;
	
	aal_assert("umka-2379", key != NULL);

	if (!(stream = aal_stream_create(NULL, &memory_stream)))
		return NULL;

	reiser4_key_print(key, stream, options);
	reiser4_print_add_stream(stream);
	
	return (char *)stream->entity;
}
#endif
