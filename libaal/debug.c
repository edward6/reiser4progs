/*
  debug.c -- implements assertions through exceptions factory. That is if some
  exception occurs, user will have the ability to make choise, continue running
  or not.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef ENABLE_DEBUG

#include <aal/aal.h>

/* 
   This function is used to provide asserts via exceptions. It is used by macro
   aal_assert().
*/
int __assert(
	char *hint,	     /* person owner of assert */
	int cond,	     /* condition of assertion */
	char *text,	     /* text of the assertion */
	char *file,	     /* source file assertion was failed in */
	int line,	     /* line of code assertion was failed in */
	char *function)      /* function in code assertion was failed in */
{
	/* Checking the condition */
	if (cond) 
		return 1;

	/* 
	   Actual exception throwing. Messages will contain hint for owner,
	   file, line and function assertion was failed in.
	*/ 
	return (aal_exception_throw(EXCEPTION_BUG, EXCEPTION_IGNORE | EXCEPTION_CANCEL,
				    "%s: Assertion (%s) at %s:%d in function %s() failed.",
				    hint, text, file, line, function) == EXCEPTION_IGNORE);
}

#endif

