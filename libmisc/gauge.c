/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   gauge.c -- common for all progs gauge fucntions. */

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#include <aal/libaal.h>
#include <misc/misc.h>

#define GAUGE_BITS_SIZE 4

aal_gauge_t *current_gauge = NULL;

static int misc_gauge_time(aal_gauge_time_t *time) {
	struct timeval t;
	uint64_t usec;
	
	if (!time->gap) 
		return 1;
	
	gettimeofday(&t, NULL);

	usec = ((uint64_t)t.tv_sec * 1000000 + t.tv_usec) / 1000;
	
	if (usec < time->shown || 
	    usec > time->shown + time->gap) 
	{
		time->shown = usec;
		return 1;
	}
	
	return 0;
}

static inline void misc_gauge_blit(void) {
	static short bitc = 0;
	static const char bits[] = "|/-\\";

	fprintf(stderr, "%c", bits[bitc]);
	bitc++;
	bitc %= GAUGE_BITS_SIZE;
}

void misc_progress_handler(aal_gauge_t *gauge) {
	if (!isatty(2))
		return;

	setlinebuf(stderr);

	if (gauge->state == GS_ACTIVE) {
		/* Show gauge once per rate->time.interval. */
		if (!misc_gauge_time(&gauge->time))
			return;
	}

	misc_wipe_line(stderr);

	if (gauge->state == GS_PAUSE)
		goto done;

	if (gauge->label[0] != '\0')
		fprintf(stderr, "\r%s", gauge->label);
	
	if (gauge->state == GS_DONE) {
		current_gauge = NULL;
		
		if (gauge->label[0] != '\0')
			fprintf(stderr, "done\n");

		goto done;
	}

	if (gauge->state == GS_START) {
		current_gauge = gauge;
		misc_gauge_time(&gauge->time);
		goto done;
	}

	if (gauge->value_func)
		gauge->value_func(gauge);
	
	if (gauge->value != -1) {
		uint32_t width, count;
		
		width = misc_screen_width();
		if (width < 10)
			goto done;
		
		width -= 10;
		
		if (width > 50)
			width = 50;
		
		fprintf(stderr, "[");
		count = width * gauge->value / 100;
		width -=  count;
		while (count--) {
			fprintf(stderr, "=");
		}
		
		misc_gauge_blit();
		
		while(width--) {
			fprintf(stderr, " ");
		}
		
		fprintf(stderr, "] %lld%%", gauge->value);
	} else {
		misc_gauge_blit();
	}

 done:
	fflush(stderr);
}

