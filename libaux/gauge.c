/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   gauge.c -- common for all progs gauge metods. */

#ifndef ENABLE_STAND_ALONE
#include <aal/libaal.h>
#include <aux/gauge.h>

aal_gauge_handler_t aux_gauge_handlers[GT_LAST];

void aux_gauge_set_handler(aal_gauge_handler_t handler,
			   aux_gauge_type_t type)
{
	aal_assert("vpf-1685", type < GT_LAST);
	aux_gauge_handlers[type] = handler;
}

aal_gauge_handler_t aux_gauge_get_handler(aux_gauge_type_t type) {
	aal_assert("vpf-1685", type < GT_LAST);
	return aux_gauge_handlers[type];
}
#endif
