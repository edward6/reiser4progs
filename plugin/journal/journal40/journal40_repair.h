/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   journal40_repair.h -- reiser4 default journal plugin repair. */

#ifndef JOURNAL40_REPAIR_H
#define JOURNAL40_REPAIR_H

#ifndef ENABLE_MINIMAL

#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern errno_t journal40_check_struct(generic_entity_t *entity,
				      layout_func_t layout_func,
				      void *data);

extern void journal40_print(generic_entity_t *entity,
			    aal_stream_t *stream, 
			    uint16_t options);

extern void journal40_invalidate(generic_entity_t *entity);

#endif
#endif
