/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   status.h -- filesystem status block functions. */

#ifndef REISER4_STATUS_H
#define REISER4_STATUS_H

#ifndef ENABLE_STAND_ALONE
#include <reiser4/types.h>

#define STATUS(status) (&status->ent)

typedef enum reiser4_state {
	FS_OK		= 0,
	FS_CORRUPTED	= 1 << 0,
	FS_DAMAGED	= 1 << 1,
	FS_DESTROYED	= 1 << 2,
	FS_IO		= 1 << 3
} reiser4_state_t;

extern void reiser4_status_close(reiser4_status_t *status);
extern errno_t reiser4_status_sync(reiser4_status_t *status);

extern errno_t reiser4_status_print(reiser4_status_t *status,
				    aal_stream_t *stream);

extern errno_t reiser4_status_layout(reiser4_status_t *status, 
				     region_func_t region_func,
				     void *data);

extern reiser4_status_t *reiser4_status_open(aal_device_t *device,
					     uint32_t blksize);

extern reiser4_status_t *reiser4_status_create(aal_device_t *device,
					       uint32_t blksize);

#endif

#endif
