/*
  gauge.c -- progress-bar functions. Gauge is supporting three gauge kinds:
  (1) percentage gauge - for operations, whose completion time may be foreseen;
  looks like, "initializing: 14%"
    
  (2) indicator gauge - for operations, whose completion time may not be foreseen; 
  for example, "traversing: /"
    
  (3) silent gauge - for operations, without any indication of progress; 
  for example, "synchronizing..."
    
  The all kinds of gauges will report about operation result (done/failed) in maner
  like this:

  "initializing: done" or "initializing: failed"

  In the case some exception occurs durring gauge running, it will be stoped and
  failing report will be made.
    
  Copyright 1996-2002 (C) Hans Reiser.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_COMPACT
#  include <stdio.h>
#endif

#include <aal/aal.h>

/* Gauge creating function */
aal_gauge_t *aal_gauge_create(
	aal_gauge_type_t type,	     /* gauge type */
	const char *name,	     /* gauge name */
	aal_gauge_handler_t handler, /* gauge handler */
	void *data)		     /* user-specific data */
{
	aal_gauge_t *gauge;
	
	aal_assert("umka-889", name != NULL, return NULL);
	aal_assert("umka-889", handler != NULL, return NULL);
	aal_assert("umka-889", type <= GAUGE_SILENT, return NULL);
    
	if (!(gauge = aal_calloc(sizeof(*gauge), 0)))
		return NULL;
    
	aal_strncpy(gauge->name, name, sizeof(gauge->name));
    
	gauge->value = 0;
	gauge->type = type;
	gauge->data = data;
	gauge->handler = handler;
	gauge->state = GAUGE_STARTED;

#ifndef ENABLE_COMPACT
	if (type == GAUGE_INDICATOR)
		setlinebuf(stderr);
#endif
    
	return gauge;
}

/* Resets gauge */
void aal_gauge_reset(aal_gauge_t *gauge) {
	aal_assert("umka-894", gauge != NULL, return);

	gauge->value = 0;
	gauge->state = GAUGE_STARTED;
}

/* Resets gauge and forces it to redraw itself */
void aal_gauge_start(aal_gauge_t *gauge) {
	aal_assert("umka-892", gauge != NULL, return);

	aal_gauge_reset(gauge);
	aal_gauge_touch(gauge);
    
	gauge->state = GAUGE_RUNNING;
}

/* Private function for changing gauge state */
static void aal_gauge_change(aal_gauge_t *gauge,
			     aal_gauge_state_t state) {
	if (!gauge) return;
	
	if (gauge->state == state)
		return;
    
	gauge->state = state;
	aal_gauge_touch(gauge);
}

static void aal_gauge_resume(aal_gauge_t *gauge) {
	if (!gauge) return;
    
	if (gauge->state == GAUGE_PAUSED)
		aal_gauge_change(gauge, GAUGE_STARTED);
}

void aal_gauge_done(aal_gauge_t *gauge) {
	if (!gauge) return;
    
	aal_gauge_resume(gauge);
    
	if (gauge->state == GAUGE_RUNNING || gauge->state == GAUGE_STARTED)
		aal_gauge_change(gauge, GAUGE_DONE);
}

void aal_gauge_pause(aal_gauge_t *gauge) {
	if (!gauge) return;
    
	if (gauge->state == GAUGE_RUNNING)
		aal_gauge_change(gauge, GAUGE_PAUSED);
}

/* Updates gauge value */
void aal_gauge_update(aal_gauge_t *gauge, uint32_t value) {
	aal_assert("umka-895", gauge != NULL, return);

	aal_gauge_resume(gauge);
    
	gauge->value = value;
	aal_gauge_touch(gauge);
}

/* Renames gauge */
void aal_gauge_rename(aal_gauge_t *gauge, const char *name, ...) {
	int len;
	va_list arg_list;
	
	aal_assert("umka-896", gauge != NULL, return);
    
	if (!name) return;
    
	va_start(arg_list, name);
    
	len = aal_vsnprintf(gauge->name, sizeof(gauge->name), 
			    name, arg_list);
    
	va_end(arg_list);
    
	gauge->name[len] = '\0';
   
	gauge->state = GAUGE_STARTED;
	aal_gauge_touch(gauge);
}

/* Calls gauge handler */
void aal_gauge_touch(aal_gauge_t *gauge) {
	aal_assert("umka-891", gauge != NULL, return);
    
	if (!gauge->handler)
		return;
    
	gauge->handler(gauge);
}

/* Frees gauge */
void aal_gauge_free(aal_gauge_t *gauge) {
	aal_assert("umka-890", gauge != NULL, return);
	aal_free(gauge);
}
