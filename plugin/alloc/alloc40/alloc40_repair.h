/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   alloc40_repair.h -- reiser4 block allocator plugin repair functions. */

#ifndef ALLOC40_REPAIR_H
#define ALLOC40_REPAIR_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

extern errno_t alloc40_pack(generic_entity_t *entity,
			    aal_stream_t *stream);

extern generic_entity_t *alloc40_unpack(fs_desc_t *desc3,
					aal_stream_t *stream);

extern errno_t alloc40_check_struct(generic_entity_t *entity,
				    uint8_t mode);

extern errno_t alloc40_region(generic_entity_t *entity, blk_t blk, 
			      region_func_t func, void *data);

extern errno_t alloc40_layout_bad(generic_entity_t *entity,
				  region_func_t region_func,
				  void *data);
#endif
