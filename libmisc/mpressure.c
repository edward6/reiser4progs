/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   mpressure.c -- memory pressure detect functions common for all reiser4progs.
   Probably here should be more reliable method to determine memory pressure. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_SYS_VFS_H
#  include <sys/vfs.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <aal/aal.h>

#define MEMORY_PRESSURE_WATER_MARK 8192

static uint32_t swapped = 0;

/* This function uses /proc filesystem to determine is memory pressure
   exists. In order to check this, it reads amount of all vitual memory that
   current process occupies and then reads amount of swapped part of process
   memory. Then it checks if current process has increased its swapped part from
   last check and how much. If so and swapped part is more than water mark, then
   memory pressure event is here. */
bool_t misc_mpressure_detect(void) {
#if defined(HAVE_STATFS) && defined (HAVE_SYS_VFS_H)
	long rss;
	long vms;
	long diff;
	
	int dint;
	char dchar;
	long dlong;
	char dcmd[256];

	FILE *file;
	struct statfs fs_st;

	/* Check if /proc is available. */
	if (!(statfs("/proc", &fs_st) != -1 && fs_st.f_type == 0x9fa0))
		return 0;

	/* Open info file of current process. */
	if (!(file = fopen("/proc/self/stat", "r"))) {
		aal_error("Can't open /proc/self/stat.");
		return 0;
	}

	/* FIXME-UMKA: Check this on 2.6.x kernels. Tested only on 2.4.x. */
	fscanf(file, "%d %s %c %d %d %d %d %d %lu %lu %lu %lu %lu %lu %lu "
	       "%ld %ld %ld %ld %ld %ld %lu %lu %ld %lu %lu %lu %lu %lu "
	       "%lu %lu %lu %lu %lu %lu %lu %lu %d %d %lu %lu\n",
	       &dint, dcmd, &dchar, &dint, &dint, &dint, &dint, &dint, &dlong,
	       &dlong, &dlong, &dlong, &dlong, &dlong, &dlong, &dlong, &dlong,
	       &dlong, &dlong, &dlong, &dlong, &dlong, &vms, &rss, &dlong,
	       &dlong, &dlong, &dlong, &dlong, &dlong, &dlong, &dlong, &dlong,
	       &dlong, &dlong, &dlong, &dlong, &dint, &dint, &dlong, &dlong);

	fclose(file);

	if (swapped == 0)
		return 0;

	/* Calculating how much memory space is moved to spapped area. */
	diff = labs((vms - (rss << 12)) - swapped);
	swapped = labs(vms - (rss << 12));
	return (diff > MEMORY_PRESSURE_WATER_MARK && swapped > 0);
#else
	return 0;
#endif
}
