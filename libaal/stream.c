/*
  stream.c -- simple stream implementation. 
    
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/  

#include <aal/aal.h>

aal_stream_t *aal_stream_create(void) {
	aal_stream_t *stream;

	if (!(stream = aal_calloc(sizeof(*stream), 0)))
		return NULL;

	aal_stream_init(stream);
	return stream;
	
 error_free_stream:
	aal_free(stream);
	return NULL;
}

#define CHUNK_SIZE (4096)

void aal_stream_init(aal_stream_t *stream) {
	aal_assert("umka-1543", stream != NULL, return);

	stream->offset = 0;
	stream->size = 4096;
	stream->data = NULL;
	
	aal_realloc((void **)&stream->data, stream->size);
}

void aal_stream_fini(aal_stream_t *stream) {
	aal_assert("umka-1549", stream != NULL, return);
	aal_free(stream->data);
}

void aal_stream_close(aal_stream_t *stream) {
	aal_assert("umka-1542", stream != NULL, return);
	
	aal_stream_fini(stream);
	aal_free(stream);
}

int aal_stream_write(aal_stream_t *stream, void *buff, int size) {
	aal_assert("umka-1544", stream != NULL, return 0);
	aal_assert("umka-1545", buff != NULL, return 0);

	if (stream->offset + size > stream->size) {
		
		stream->size = stream->offset + CHUNK_SIZE;

		aal_assert("umka-1551", size > 0, return 0);
		
		if (!aal_realloc((void **)&stream->data, stream->size))
			return -1;
	}

	aal_memcpy(stream->data + stream->offset, buff, size);
	stream->offset += size;
	
	return size;
}

int aal_stream_read(aal_stream_t *stream, void *buff, int size) {
	aal_assert("umka-1546", stream != NULL, return 0);
	aal_assert("umka-1547", buff != NULL, return 0);

	if (stream->offset + size > stream->size)
		size = stream->size - stream->offset;
	
	aal_memcpy(buff, stream->data + stream->offset, size);
	stream->offset += size;
	
	return size;
}

int aal_stream_format(aal_stream_t *stream, const char *format, ...) {
	int res;
	char buff[4096];
	va_list arg_list;

	aal_memset(buff, 0, sizeof(buff));

	va_start(arg_list, format);
	
	res = aal_vsnprintf(buff, sizeof(buff),
			    format, arg_list);
	
	va_end(arg_list);
	
	if (!(res = aal_stream_write(stream, buff, res)))
		return res;

	*(char *)(stream->data + stream->offset) = '\0';
	
	return res;
}
