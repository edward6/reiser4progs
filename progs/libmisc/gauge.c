/*
  gauge.c -- common for all progs gauge fucntions.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <stdio.h>
#include <unistd.h>

#include <aal/aal.h>
#include <misc/misc.h>

#define GAUGE_BITS_SIZE 4

aal_gauge_t *current_gauge = NULL;

static inline void progs_gauge_blit(void) {
	static short bitc = 0;
	static const char bits[] = "|/-\\";

	putc(bits[bitc], stderr);
	putc('\b', stderr);
	fflush(stderr);
	bitc++;
	bitc %= GAUGE_BITS_SIZE;
}

/* This functions "draws" gauge header */
static inline void progs_gauge_header(
	const char *name,	/* gauge name */
	aal_gauge_type_t type)	/* gauge type */
{
	if (name) {
		fprintf(stderr, "\r%s%s", name,
			type == GAUGE_SILENT ? "..." : ": ");
	}
}

/* This function "draws" gauge footer */
static inline void progs_gauge_footer(
	const char *name,	/* footer name */
	aal_gauge_type_t type)  /* gauge type */
{
	if (name)
		fputs(name, stderr);
}

void progs_gauge_handler(aal_gauge_t *gauge) {
	unsigned int i;
	char display[10] = {0};

	if (!isatty(2))
		return;
	
	if (gauge->state == GAUGE_PAUSED) {
		progs_wipe_line(stderr);
		fflush(stderr);
		return;
	}
	
	if (gauge->state == GAUGE_STARTED) {
		current_gauge = gauge;
		progs_gauge_header(gauge->name, gauge->type);
	}
	
	switch (gauge->type) {
	case GAUGE_PERCENTAGE:
		
		sprintf(display, "%d%%", gauge->value);
		fputs(display, stderr);
		
		for (i = 0; i < strlen(display); i++)
			fputc('\b', stderr);
		break;
	case GAUGE_INDICATOR:
		progs_gauge_blit();
		break;
	case GAUGE_SILENT: break;
	}

	if (gauge->state == GAUGE_DONE) {
		current_gauge = NULL;
		progs_gauge_footer("done\n", gauge->type);
	}
    
	fflush(stderr);
}

