/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format40_repair.h -- reiser4 disk-format plugin repair functions. */

#ifndef FORMAT40_REPAIR_H
#define FORMAT40_REPAIR_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern errno_t format40_pack(generic_entity_t *entity,
			      aal_stream_t *stream);

extern errno_t format40_update(generic_entity_t *entity);

extern generic_entity_t *format40_unpack(fs_desc_t *desc,
					 aal_stream_t *stream);

extern void format40_print(generic_entity_t *entity,
			   aal_stream_t *stream,
			   uint16_t options);

#endif
