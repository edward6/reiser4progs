/*
  gauge.h -- common for all progs gauge fucntions.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef PROGS_GAUGE_H
#define PROGS_GAUGE_H

#include <aal/gauge.h>

extern void progs_gauge_percentage_handler(aal_gauge_t *gauge);
extern void progs_gauge_indicator_handler(aal_gauge_t *gauge);
extern void progs_gauge_silent_handler(aal_gauge_t *gauge);

#endif
