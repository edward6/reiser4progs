/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/plugin.h - reiser4 plugins repair code known types and macros. */

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

typedef enum repair_error {
	/* To make repair_error_t signed. */
	RE_BUG		= (-1),
	/* Fixable errors were detected. */
	RE_FIXABLE	= (1 << 0),
	/* Fatal errors were detected. */
	RE_FATAL	= (1 << 1),
	/* For expansibility. */
	RE_LAST		= (1 << 2)
} repair_error_t;

#define repair_error_exists(result)  (result)
#define repair_error_fatal(result)   ((result >= RE_FATAL) || (result < 0))

#define repair_error_check(result, mode)		\
({							\
	aal_assert("vpf-786", (mode != RM_FIX) ||	\
			      !(res & RE_FIXABLE));	\
	aal_assert("vpf-787", (mode != RM_BUILD) ||	\
			      !(res & RE_FIXABLE));	\
})

#define LOST_PREFIX "lost_name_"

#endif
