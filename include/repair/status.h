/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   status.h -- filesystem status block functions. */

#ifndef REPAIR_STATUS_H
#define REPAIR_STATUS_H

#include <repair/repair.h>

extern errno_t repair_status_pack(reiser4_status_t *status, 
				  aal_stream_t *stream);

extern void repair_status_state(reiser4_status_t *status,
				uint64_t state);

extern errno_t repair_status_open(reiser4_fs_t *fs, uint8_t mode);

extern reiser4_status_t *repair_status_unpack(aal_device_t *device,
					      uint32_t blksize,
					      aal_stream_t *stream);

extern void repair_status_print(reiser4_status_t *status,
				aal_stream_t *stream);

#endif
