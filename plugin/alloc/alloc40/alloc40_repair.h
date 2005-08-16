/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   alloc40_repair.h -- reiser4 block allocator plugin repair functions. */

#ifndef ALLOC40_REPAIR_H
#define ALLOC40_REPAIR_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern errno_t alloc40_pack(generic_entity_t *entity, 
			    aal_stream_t *stream);

extern generic_entity_t *alloc40_unpack(aal_device_t *device, 
					uint32_t blksize, 
					aal_stream_t *stream);

extern errno_t alloc40_check_struct(generic_entity_t *entity, 
				    uint8_t mode);

extern errno_t alloc40_layout_bad(generic_entity_t *entity,
				  region_func_t region_func,
				  void *data);

extern void alloc40_print(generic_entity_t *entity,
			  aal_stream_t *stream,
			  uint16_t options);
#endif
