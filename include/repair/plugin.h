/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/plugin.h - reiser4 plugins repair code known types and macros. */

#ifndef REPAIR_PLUGIN_H
#define REPAIR_PLUGIN_H

#include <aal/types.h>

typedef enum repair_mode {
	RM_CHECK	= 1,
	RM_FIX		= 2,
	RM_BUILD	= 3,
	RM_BACK		= 4,
	RM_LAST		= 5
} repair_mode_t;

typedef enum repair_error {
	/* To make repair_error_t signed. */
	RE_BUG		= (-1),
	/* No error were detected. */
	RE_OK		= (0),
	/* When item gets removed from the node, to correct position in the 
	   loop correctly. */
	RE_REMOVED	= (1 << 0),
	/* All errors were fixed. */
	RE_FIXED	= (1 << 1),
	/* Fixable errors were detected. */
	RE_FIXABLE	= (1 << 2),
	/* Fatal errors were detected. */
	RE_FATAL	= (1 << 3),
	/* For expansibility. */
	RE_LAST		= (1 << 4)
} repair_error_t;

#define repair_error_exists(result)  ((result > RE_FIXED) || (result < 0))
#define repair_error_fatal(result)   ((result >= RE_FATAL) || (result < 0))

#define repair_error_check(result, mode)		\
({							\
	aal_assert("vpf-785", (mode != RM_CHECK) ||	\
			      !(res & RE_FIXED));	\
	aal_assert("vpf-786", (mode != RM_FIX) ||	\
			      !(res & RE_FIXABLE));	\
	aal_assert("vpf-787", (mode != RM_BUILD) ||	\
			      !(res & RE_FIXABLE));	\
})

#define LOST_PREFIX "lost_name_"

#endif
