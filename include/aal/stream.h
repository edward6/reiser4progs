/*
  stream.h -- simple stream implementation. 
    
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/  

#ifndef STREAM_H
#define STREAM_H

struct aal_stream {
	int size;
	int offset;
	void *data;
};

typedef struct aal_stream aal_stream_t;

#define EMPTY_STREAM {0, 0, NULL}

extern aal_stream_t *aal_stream_create(void);
extern void aal_stream_fini(aal_stream_t *stream);
extern void aal_stream_close(aal_stream_t *stream);
extern errno_t aal_stream_init(aal_stream_t *stream);
extern void aal_stream_reset(aal_stream_t *stream);

extern int aal_stream_write(aal_stream_t *stream,
			    void *buff, int size);

extern int aal_stream_read(aal_stream_t *stream,
			   void *buff, int size);

extern int aal_stream_format(aal_stream_t *stream,
			     const char *format, ...)
                             __aal_check_format__(printf, 2, 3);

#endif
