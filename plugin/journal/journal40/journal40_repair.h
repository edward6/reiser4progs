/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   journal40_repair.h -- reiser4 default journal plugin repair. */

#ifndef JOURNAL40_REPAIR_H
#define JOURNAL40_REPAIR_H

#ifndef ENABLE_MINIMAL

#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern errno_t journal40_check_struct(reiser4_journal_ent_t *entity,
				      layout_func_t layout_func,
				      void *data);

extern void journal40_print(reiser4_journal_ent_t *entity,
			    aal_stream_t *stream, 
			    uint16_t options);

extern void journal40_invalidate(reiser4_journal_ent_t *entity);

extern errno_t journal40_pack(reiser4_journal_ent_t *entity, 
			      aal_stream_t *stream);

extern reiser4_journal_ent_t *journal40_unpack(aal_device_t *device, 
					       uint32_t blksize, 
					       reiser4_format_ent_t *format, 
					       reiser4_oid_ent_t *oid,
					       uint64_t start, 
					       uint64_t blocks, 
					       aal_stream_t *stream);

#endif
#endif
