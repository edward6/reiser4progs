/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   alloc40_repair.h -- reiser4 block allocator plugin repair functions. */

#ifndef ALLOC40_REPAIR_H
#define ALLOC40_REPAIR_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern errno_t alloc40_pack(reiser4_alloc_ent_t *entity, 
			    aal_stream_t *stream);

extern reiser4_alloc_ent_t *alloc40_unpack(aal_device_t *device, 
					   uint32_t blksize, 
					   aal_stream_t *stream);

extern errno_t alloc40_check_struct(reiser4_alloc_ent_t *entity, 
				    uint8_t mode);

extern errno_t alloc40_layout_bad(reiser4_alloc_ent_t *entity,
				  region_func_t region_func,
				  void *data);

extern void alloc40_print(reiser4_alloc_ent_t *entity,
			  aal_stream_t *stream,
			  uint16_t options);
#endif
