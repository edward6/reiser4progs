/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
reiser4progs/COPYING.

gauge.h -- common for all progs gauge declarations. */

#ifndef AUX_GAUGE_H
#define AUX_GAUGE_H

#include <aal/gauge.h>

#define GAUGE_PERCENTAGE (0x0)
#define GAUGE_INDICATOR  (0x1)
#define GAUGE_SILENT     (0x2)
#define GAUGE_LAST	 (0x3)

struct aux_gauge_time {
     struct timeval time;
     struct timeval interval;
};

typedef struct aux_gauge_time aux_gauge_time_t;

struct aux_percentage {
     /* Show it once per @time.interval seconds. */
     aux_gauge_time_t time;
     
     /* Percent value. */
     uint8_t value;
};

typedef struct aux_percentage aux_percentage_t;
typedef aux_gauge_time_t aux_indicator_t;

#endif
