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
#include <unistd.h>

#include <misc/misc.h>
#include <misc/gauge.h>
#include <reiser4/reiser4.h>
#include <repair/librepair.h>

extern aal_gauge_t *current_gauge;

extern void gauge_tree(aal_gauge_t *gauge);
extern void gauge_rate(aal_gauge_t *gauge);
extern errno_t gauge_handler(repair_progress_t *progress);

typedef struct gauge_rate_hint {
    repair_progress_rate_t *rate;
    uint64_t speed;
} gauge_rate_hint_t;

typedef struct gauge_tree_hint {
    aal_list_t *tree;
} gauge_tree_hint_t;

typedef struct gauge_sem_hint {
    aal_list_t *names;
} gauge_sem_hint_t;

typedef struct gauge_hint {
    time_t start_time, displayed_time;
    uint8_t percent;
    union {
	gauge_rate_hint_t rate_hint;
	gauge_tree_hint_t tree_hint;
	gauge_sem_hint_t  sem_hint;
    } u;
} gauge_hint_t;

#endif
