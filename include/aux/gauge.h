/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   gauge.h -- common for all progs gauge declarations. */

#ifndef AUX_GAUGE_H
#define AUX_GAUGE_H

#ifndef ENABLE_MINIMAL
#include <aal/gauge.h>

typedef enum aux_gauge_type {
	GT_PROGRESS  = 0x0,
	GT_LAST
} aux_gauge_type_t;

extern aal_gauge_handler_t aux_gauge_handlers[GT_LAST];

extern aal_gauge_handler_t aux_gauge_get_handler(aux_gauge_type_t type);

extern void aux_gauge_set_handler(aal_gauge_handler_t handler, 
				  aux_gauge_type_t type);

#endif
#endif
