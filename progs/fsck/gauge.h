/*
    gauge.h -- fsck gauge declarations.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#ifndef FSCK_GAUGE_H
#define FSCK_GAUGE_H

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include <time.h>
#include <stdio.h>
#include <unistd.h>

#include <misc/misc.h>
#include <misc/gauge.h>
#include <reiser4/reiser4.h>
#include <repair/librepair.h>

extern aal_gauge_t *current_gauge;

extern void gauge_embedded(aal_gauge_t *gauge);
extern void gauge_percentage(aal_gauge_t *gauge);
extern errno_t gauge_handler(repair_progress_t *progress);

typedef struct gauge_percentage_hint {
    uint64_t displayed, done, total;
    uint64_t speed;
    time_t start_time, displayed_time;
} gauge_percentage_hint_t;

typedef struct gauge_embedded_hint {
    uint64_t done, total;
} gauge_embedded_hint_t;

#define GAUGE_EMBEDDED	GAUGE_LAST

#endif
