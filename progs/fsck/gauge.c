/*
    gauge.c -- gauges for fsck.reiser4.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <gauge.h>

/* Current progs gauge. Used for correct pausing when exception */

void gauge_embedded(aal_gauge_t *gauge) {
    aal_list_t *path, *first, *elem;
    gauge_embedded_hint_t *hint;

    aal_assert("vpf-873", gauge != NULL);
    path = gauge->data;
    aal_assert("vpf-874", path != NULL);

    if (!isatty(2))
	return;

    progs_wipe_line(stderr);
    
    if (gauge->state == GAUGE_DONE)
	current_gauge = NULL;

    if (gauge->state == GAUGE_STARTED || gauge->state == GAUGE_RUNNING) {
	current_gauge = gauge;

	fprintf(stderr, "%s ", gauge->name);
	first = aal_list_first(path);
	for (elem = first; elem; elem = aal_list_next(elem)) {
	    hint = (gauge_embedded_hint_t *)elem->data;
	    fprintf(stderr, "%s %llu of %llu", elem == first ? "" : " /",
		hint->done, hint->total);
	}
    }

    fflush(stderr);
}

void gauge_percentage(aal_gauge_t *gauge) {
    gauge_percentage_hint_t *hint;    
    uint8_t i;
    time_t t;
    
    aal_assert("vpf-871", gauge != NULL);
    hint = gauge->data;
    aal_assert("vpf-872", hint != NULL);
    
    if (!isatty(2))
	return;
    
    progs_wipe_line(stderr);
    
    if (gauge->state == GAUGE_PAUSED)
	time(&hint->displayed_time);
     
    if (gauge->state == GAUGE_DONE) {
	current_gauge = NULL;
	fprintf(stderr, "Done %llu, Speed %llu/sec\n", hint->displayed, 
	    hint->speed);
    }
    
    if (gauge->state == GAUGE_STARTED) {
	current_gauge = gauge;
	fprintf(stderr, "%s %llu of %llu, speed %llu/sec", gauge->name, 
	    hint->displayed, hint->total, hint->speed);
	
	time(&t);
	if (hint->start_time == 0)
	    hint->start_time = hint->displayed_time = t;
	else /* Subtract the pause time. */ {
	    hint->start_time -= (t - hint->displayed_time);
	    hint->displayed_time = t;
	}
    }
    
    if (gauge->state == GAUGE_RUNNING) {
	time(&t);
	
	/* Update the gauge not more ofter then once per second or if another 
	 * percent passed. */
	if (t - hint->displayed_time < 1) {
	    if (hint->done * 100 / hint->total == 
		hint->displayed * 100 / hint->total)
		return;	    
	}
	
	hint->speed = hint->done / (t - hint->start_time);
	hint->displayed = hint->done;
	hint->displayed_time = t;
	
	fprintf(stderr, "%s %llu of %llu, speed %llu/sec", gauge->name, 
	    hint->displayed, hint->total, hint->speed);
    }
    
    fflush(stderr);
}

static errno_t gauge_data_prepare(aal_gauge_t *gauge, 
    repair_progress_t *progress) 
{
    aal_assert("vpf-875", gauge != NULL);
    aal_assert("vpf-876", progress != NULL);
    
    if (gauge->type == GAUGE_PERCENTAGE) {
	gauge_percentage_hint_t *hint;
	
	hint = gauge->data = aal_calloc(sizeof(gauge_percentage_hint_t), 0);
	
	if (!hint)
	    return -ENOMEM;
	
	hint->total = progress->total;
    }
    
    if (gauge->type == GAUGE_EMBEDDED) {
	gauge_embedded_hint_t *hint;
	aal_list_t *list;
	
	hint = aal_calloc(sizeof(gauge_embedded_hint_t), 0);
	list = (aal_list_t *)gauge->data;
	
	hint->total = progress->total;
	hint->done = progress->done;
	gauge->data = aal_list_append(list, hint);
    }
    
    return 0;
}

static void gauge_data_release(aal_gauge_t *gauge) {
    aal_assert("vpf-875", gauge != NULL);
    
    if (gauge->type == GAUGE_PERCENTAGE) {
	if (gauge->data)
	    aal_free(gauge->data);
    } else {
	aal_list_t *list, *elem;

	list = (aal_list_t *)gauge->data;
	while ((elem = aal_list_first(list))) {
	    aal_free(elem->data);
	    list = aal_list_remove(list, elem);
	}
    }
}

static void gauge_data_update() {
}

errno_t gauge_handler(repair_progress_t *progress) {
    gauge_percentage_hint_t *hint;
    aal_gauge_t *gauge;
    int gauge_type;
    errno_t res;
    
    aal_assert("vpf-868", progress != NULL);

    gauge = progress->data;
    
    gauge_type = progress->type == PROGRESS_SILENT    ? GAUGE_INDICATOR :
		 progress->type == PROGRESS_INDICATOR ? GAUGE_PERCENTAGE :
		 progress->type == PROGRESS_EMBEDDED  ? GAUGE_EMBEDDED :
		 progress->type;
	
    if (progress->state == PROGRESS_START) {
	if (gauge == NULL) {
	    fprintf(stderr, "%s\n", progress->title);
	    gauge = progress->data = 
		aal_gauge_create(gauge_type, progress->text, NULL);
	    
	    if (!gauge)
		return -ENOMEM;
	    
	    if ((res = gauge_data_prepare(gauge, progress))) {
		aal_gauge_free(gauge);
		return res;
	    }
	    
	    aal_gauge_start(gauge);
	    
	    return 0;
	} else if (progress->type == PROGRESS_EMBEDDED) {
	} else /* gauge exists, but not embedded */
	    return -EINVAL;
    } else if (progress->state == PROGRESS_END) {
	aal_gauge_done(gauge);
	
	gauge_data_release(gauge);

	aal_gauge_free(gauge);
	progress->data = NULL;

	fprintf(stderr, "%s\n", progress->text);
	
	return 0;
    } else if (progress->state != PROGRESS_UPDATE)
	return -EINVAL;

    aal_assert("vpf-869", gauge != NULL);
    
    hint = (gauge_percentage_hint_t *)gauge->data;
    hint->done = progress->done;

    /* FIXME: support printing not more ofter then once per half of a second
     * and text printing on update. */
    aal_gauge_update(gauge, progress->done * 100 / progress->total);
    
    return 0;
}
 
