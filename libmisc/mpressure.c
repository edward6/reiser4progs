/*
  mpressure.c -- memory pressure detect functions common for all reiser4progs.
  Probably here should be more reliable method to determine memory pressure.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_SYS_VFS_H
#  include <sys/vfs.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <aal/aal.h>

static uint32_t swapped = 0;

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

	if (!(statfs("/proc", &fs_st) != -1 && fs_st.f_type == 0x9fa0))
		return 0;
	
	if (!(file = fopen("/proc/self/stat", "r"))) {
		aal_exception_error("Can't open /proc/self/stat.");
		return 0;
	}

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
	
	diff = labs((vms - (rss << 12)) - swapped);
	swapped = labs(vms - (rss << 12));
	
	return diff > 8192 && swapped > 0;
#else
	return 0;
#endif
}
