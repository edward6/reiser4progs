/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/plugin.h - reiser4 plugins repair code methods. */

#ifndef REPAIR_PLUGIN_H
#define REPAIR_PLUGIN_H

#include <aal/types.h>

typedef enum repair_mode {
	/* Check the consistensy of the fs  */
	RM_CHECK	= 1,
	/* Fix all fixable corruptions. */
	RM_FIX		= 2,
	/* Rebuild the fs from the found wrecks. */
	RM_BUILD	= 3,
	/* Rollback changes have been make by the last fsck run. */
	RM_BACK		= 4,
	/* No one mode anymore. */
	RM_LAST		= 5
} repair_mode_t;

/* Fixable errors were detected. */
#define RE_FIXABLE	((uint64_t)1 << 32)
/* Fatal errors were detected. */
#define RE_FATAL	((uint64_t)1 << 33)
/* For expansibility. */
#define RE_LAST		((uint64_t)1 << 34)

#define repair_error_fatal(result)   ((result & RE_FATAL) || (result < 0))

#endif
