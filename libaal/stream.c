/*
  stream.c -- simple stream implementation. 
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/  

#ifndef ENABLE_STAND_ALONE

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

errno_t aal_stream_init(aal_stream_t *stream) {
	aal_assert("umka-1543", stream != NULL);

	stream->size = 0;
	stream->offset = 0;
	stream->data = NULL;
	
	return 0;
}

void aal_stream_fini(aal_stream_t *stream) {
	aal_assert("umka-1549", stream != NULL);
	aal_free(stream->data);
}

void aal_stream_close(aal_stream_t *stream) {
	aal_assert("umka-1542", stream != NULL);
	
	aal_stream_fini(stream);
	aal_free(stream);
}

static errno_t aal_stream_grow(aal_stream_t *stream, int size) {
	
	if (stream->offset + size + 1 > stream->size) {
		stream->size = stream->offset + size + 1;

		if (aal_realloc((void **)&stream->data, stream->size))
			return -ENOMEM;
	}

	return 0;
}

int aal_stream_write(aal_stream_t *stream, void *buff, int size) {
	aal_assert("umka-1544", stream != NULL);
	aal_assert("umka-1545", buff != NULL);

	if (aal_stream_grow(stream, size))
		return 0;
	
	aal_memcpy(stream->data + stream->offset, buff, size);
	stream->offset += size;
	
	return size;
}

int aal_stream_read(aal_stream_t *stream, void *buff, int size) {
	aal_assert("umka-1546", stream != NULL);
	aal_assert("umka-1547", buff != NULL);

	if (stream->offset + size > stream->size)
		size = stream->size - stream->offset;

	if (size > 0) {
		aal_memcpy(buff, stream->data +
			   stream->offset, size);
		
		stream->offset += size;
	}
	
	return size;
}

void aal_stream_reset(aal_stream_t *stream) {
	aal_assert("umka-1711", stream != NULL);
	stream->offset = 0;
}

int aal_stream_format(aal_stream_t *stream, const char *format, ...) {
	int len;
	char buff[4096];
	va_list arg_list;

	aal_memset(buff, 0, sizeof(buff));

	va_start(arg_list, format);
	
	len = aal_vsnprintf(buff, sizeof(buff), format,
			    arg_list);
	
	va_end(arg_list);

	if (!(len = aal_stream_write(stream, buff, len)))
		return len;

	*((char *)stream->data + stream->offset) = '\0';
	
	return len;
}

#endif
