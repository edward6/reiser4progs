/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   gauge.c -- common for all progs gauge fucntions. */

#include <stdio.h>
#include <unistd.h>

#include <aal/aal.h>
#include <misc/misc.h>

#define GAUGE_BITS_SIZE 4

aal_gauge_t *current_gauge = NULL;

static inline void misc_gauge_blit(void) {
	static short bitc = 0;
	static const char bits[] = "|/-\\";

	putc(bits[bitc], stderr);
	putc('\b', stderr);
	fflush(stderr);
	bitc++;
	bitc %= GAUGE_BITS_SIZE;
}

/* This functions "draws" gauge header */
static inline void misc_gauge_header(
	const char *name,       /* gauge name */
	int silent)             /* gauge type */
{
	setlinebuf(stderr);
    
	if (name) {
		fprintf(stderr, "\r%s%s", name,
			silent ? "..." : ": ");
	}
}

/* This function "draws" gauge footer */
static inline void misc_gauge_footer(
	const char *name,       /* footer name */
	int silent)             /* gauge type */
{
	if (name)
		fputs(name, stderr);
}

void misc_gauge_percentage_handler(aal_gauge_t *gauge) {
	unsigned int i;
	char display[10] = {0};
	
	if (!isatty(2))
		return;
	
	if (gauge->state == GAUGE_PAUSED) {
		misc_wipe_line(stderr);
		fflush(stderr);
		return;
	}
	
	if (gauge->state == GAUGE_STARTED) {
		current_gauge = gauge;
		misc_gauge_header(gauge->name, 0);
	}

	sprintf(display, "%d%%", gauge->value);
	fputs(display, stderr);

	for (i = 0; i < aal_strlen(display); i++)
		fputc('\b', stderr);

	if (gauge->state == GAUGE_DONE) {
		current_gauge = NULL;
		misc_gauge_footer("done\n", 0);
	}
    
	fflush(stderr);
}

void misc_gauge_indicator_handler(aal_gauge_t *gauge) {
	if (!isatty(2))
		return;
	
	if (gauge->state == GAUGE_PAUSED) {
		misc_wipe_line(stderr);
		fflush(stderr);
		return;
	}
	
	if (gauge->state == GAUGE_STARTED) {
		current_gauge = gauge;
		misc_gauge_header(gauge->name, 0);
	}

	misc_gauge_blit();
	
	if (gauge->state == GAUGE_DONE) {
		current_gauge = NULL;
		misc_gauge_footer("done\n", 0);
	}
    
	fflush(stderr);
}

void misc_gauge_silent_handler(aal_gauge_t *gauge) {
	if (!isatty(2))
		return;
	
	if (gauge->state == GAUGE_PAUSED) {
		misc_wipe_line(stderr);
		fflush(stderr);
		return;
	}
	
	if (gauge->state == GAUGE_STARTED) {
		current_gauge = gauge;
		misc_gauge_header(gauge->name, 1);
	}

	if (gauge->state == GAUGE_DONE) {
		current_gauge = NULL;
		misc_gauge_footer("done\n", 1);
	}
    
	fflush(stderr);
}
