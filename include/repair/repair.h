/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/repair.h -- the common structures and methods for recovery. */

#ifndef REPAIR_H
#define REPAIR_H

#include <reiser4/libreiser4.h>
#include <repair/plugin.h>
#include <misc/gauge.h>

enum {
	REPAIR_DEBUG	= 0x0,
	REPAIR_WHOLE	= 0x1,
	REPAIR_NO_MKID	= 0x2,
	REPAIR_YES	= 0X3,
	REPAIR_LAST
};

typedef struct repair_data {
	reiser4_fs_t *fs;
    
	uint64_t fatal;
	uint64_t fixable;
	uint64_t sb_fixable;

	uint8_t mode;
	char *bitmap_file;
	
	uint32_t flags;
} repair_data_t;

extern errno_t repair_check(repair_data_t *repair);

#define repair_error_count(repair, error)		\
({							\
	if (error > 0) {					\
		if (error & RE_FATAL)			\
			repair->fatal++;		\
		else if (error & RE_FIXABLE)		\
			repair->fixable++;		\
	}						\
})

#endif
