/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   journal40_repair.h -- reiser4 default journal plugin repair. */

#ifndef JOURNAL40_REPAIR_H
#define JOURNAL40_REPAIR_H

#ifndef ENABLE_STAND_ALONE

#include <aal/aal.h>
#include <reiser4/plugin.h>

extern errno_t journal40_check_struct(generic_entity_t *entity,
				      layout_func_t layout_func,
				      void *data);

extern void journal40_print(generic_entity_t *entity,
			    aal_stream_t *stream, 
			    uint16_t options);

#endif
#endif
