/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/plugin.h - reiser4 plugins repair code known types and macros. */

#ifndef REPAIR_PLUGIN_H
#define REPAIR_PLUGIN_H

#include <aal/types.h>

typedef enum repair_flag {
	REPAIR_CHECK    = 1,
	REPAIR_FIX	    = 2,
	REPAIR_REBUILD  = 3,
	REPAIR_ROLLBACK = 4
} repair_flag_t;

typedef enum repair_error {
	/* To make repair_error_t signed. */
	REPAIR_BUG		= (-1),
	/* No error were detected. */
	REPAIR_OK		= (0),
	/* When item gets removed from the node, to correct position in the 
	   loop correctly. */
	REPAIR_REMOVED	= (1 << 0),
	/* All errors were fixed. */
	REPAIR_FIXED	= (1 << 1),
	/* Fixable errors were detected. */
	REPAIR_FIXABLE	= (1 << 2),
	/* Fatal errors were detected. */
	REPAIR_FATAL	= (1 << 3),
	/* For expansibility. */
	REPAIR_ERROR_LAST	= (1 << 4)
} repair_error_t;

#define repair_error_exists(result)  ((result > REPAIR_FIXED) || (result < 0))
#define repair_error_fatal(result)   ((result >= REPAIR_FATAL) || (result < 0))

#define repair_error_check(result, mode)			\
({								\
	aal_assert("vpf-785", (mode != REPAIR_CHECK) ||		\
			      !(res & REPAIR_FIXED));		\
	aal_assert("vpf-786", (mode != REPAIR_FIX) ||		\
			      !(res & REPAIR_FIXABLE));		\
	aal_assert("vpf-787", (mode != REPAIR_REBUILD) ||	\
			      !(res & REPAIR_FIXABLE));		\
})

#define LOST_PREFIX "lost_name_"

#endif
