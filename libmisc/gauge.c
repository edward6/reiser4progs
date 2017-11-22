/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
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
	uint64_t delta;
	uint64_t usec;
	
	if (!time->gap) 
		return 1;
	
	gettimeofday(&t, NULL);

	usec = t.tv_usec / 1000;
	
	if ((uint64_t)t.tv_sec < time->sec)
		goto next;
	
	if (((uint64_t)t.tv_sec == time->sec) && (usec < time->misec))
		goto next;

	delta = (t.tv_sec  - time->sec) * 1000 + (usec - time->misec);
	
	if (delta > time->gap) 
		goto next;
	
	return 0;
	
 next:
	time->sec = t.tv_sec;
	time->misec = usec;
	return 1;
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

	if (gauge->state == GS_ACTIVE ||
	    gauge->state == GS_RESUME)
	{
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
		
		fprintf(stderr, "] %lld%%", (long long)gauge->value);
	} else {
		misc_gauge_blit();
	}

 done:
	fflush(stderr);
}

