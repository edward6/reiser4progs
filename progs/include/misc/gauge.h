/*
  gauge.h -- common for all progs gauge fucntions.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef PROGS_GAUGE_H
#define PROGS_GAUGE_H

#include <aal/gauge.h>

#define GAUGE_PERCENTAGE 0x0
#define GAUGE_INDICATOR  0x1
#define GAUGE_SILENT     0x2
#define GAUGE_LAST	 0x3

extern void progs_gauge_percentage_handler(aal_gauge_t *gauge);
extern void progs_gauge_indicator_handler(aal_gauge_t *gauge);
extern void progs_gauge_silent_handler(aal_gauge_t *gauge);

#endif
