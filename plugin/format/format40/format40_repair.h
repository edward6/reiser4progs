/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format40_repair.h -- reiser4 disk-format plugin repair functions. */

#ifndef FORMAT40_REPAIR_H
#define FORMAT40_REPAIR_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

extern errno_t format40_pack(generic_entity_t *entity,
			      aal_stream_t *stream);

extern generic_entity_t *format40_unpack(aal_device_t *device,
					 uint32_t blksize,
					 aal_stream_t *stream);

extern errno_t format40_update(generic_entity_t *entity);

extern errno_t format40_check_struct(generic_entity_t *entity,
				     uint8_t mode);
#endif
