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

#if 0
#define MEMORY_PRESSURE_WATER_MARK 8192

static int32_t swapped = 0;
#endif

/* This function uses /proc filesystem to determine is memory pressure
   exists. In order to check this, it reads amount of all vitual memory that
   current process occupies and then reads amount of swapped part of process
   memory. Then it checks if current process has increased its swapped part from
   last check and how much. If so and swapped part is more than water mark, then
   memory pressure event is here. */
int misc_mpressure_detect(uint32_t nodes) {
#if 0
#if defined(HAVE_STATFS) && defined (HAVE_SYS_VFS_H)
	long rss;
	long vms;
//	long diff;
	int shared;

	int res;
	int dint;
	char dchar;
	long dlong;
	char dcmd[256];

	FILE *file;
	struct statfs fs_st;

	/* Check if /proc is available. */
	if (!(statfs("/proc", &fs_st) != -1 && fs_st.f_type == 0x9fa0))
		return 0;

	/* Getting process memory using statistics. Here we get resident size
	   and virtual address space size. */
	if (!(file = fopen("/proc/self/stat", "r"))) {
		aal_error("Can't open /proc/self/stat.");
		return 0;
	}

	res = fscanf(file, "%d %s %c %d %d %d %d %d %lu %lu %lu %lu %lu %lu %lu "
		     "%ld %ld %ld %ld %ld %ld %lu %lu %ld %lu %lu %lu %lu %lu "
		     "%lu %lu %lu %lu %lu %lu %lu %lu %d %d %lu %lu\n",
		     &dint, dcmd, &dchar, &dint, &dint, &dint, &dint, &dint, &dlong,
		     &dlong, &dlong, &dlong, &dlong, &dlong, &dlong, &dlong, &dlong,
		     &dlong, &dlong, &dlong, &dlong, &dlong, &vms, &rss, &dlong,
		     &dlong, &dlong, &dlong, &dlong, &dlong, &dlong, &dlong, &dlong,
		     &dlong, &dlong, &dlong, &dlong, &dint, &dint, &dlong, &dlong);

	fclose(file);

	if (res == 0 || res == EOF) {
		aal_error("Can't read from /proc/self/stat. Memory pressure "
			  "will not be detected.");
		return 0;
	}

	/* Getting process shared memory using statistics. This is needed for
	   substracting it from vms along with rss in order to see if some part
	   of process adress space lies in swap and thus to detecrmine, that
	   memory pressure event is here. */
	if (!(file = fopen("/proc/self/statm", "r"))) {
		aal_error("Can't open /proc/self/statm.");
		return 0;
	}

	res = fscanf(file, "%d %d %d %d %d %d %d\n",
		     &dint, &dint, &shared, &dint, &dint, &dint, &dint);

	fclose(file);
	
	if (res == 0 || res == EOF) {
		aal_error("Can't read from /proc/self/statm. Memory pressure "
			  "will not be detected.");
		return 0;
	}
	
	/* Calculating how much memory space is moved to swapped area. */
//	diff = (vms - (rss << 12) - (shared << 12)) - swapped;

	/* Calculating new swapped value. */
	swapped = vms - (rss << 12)/* - (shared << 12)*/;

	/* Memory pressure is detected if memory usage has increased by
	   MEMORY_PRESSURE_WATER_MARK value since last time and swapped greater
	   than zero. */
	return (/*diff > MEMORY_PRESSURE_WATER_MARK && */swapped > 0);
#else
	return 0;
#endif
#endif
	return 0/*nodes > 512*/;
}
