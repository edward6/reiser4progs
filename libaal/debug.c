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

#ifndef ENABLE_STAND_ALONE
#  include <unistd.h>
#  include <stdlib.h>
#endif

static void default_assert_handler(char *hint, int cond, char *text,
				   char *file, int line, char *func)
{
	/* 
	  Actual exception throwing. Messages will contain hint for owner, file,
	  line and function assertion was failed in.
	*/ 
	aal_exception_bug("%s: Assertion (%s) at %s:%d in function %s() failed.",
			  hint, text, file, line, func);
	
#ifndef ENABLE_STAND_ALONE
	exit(-1);
#endif
}

static assert_handler_t assert_handler = default_assert_handler;

assert_handler_t aal_assert_get_handler(void) {
	return assert_handler;
}

void aal_assert_set_handler(assert_handler_t handler) {
	if (!handler)
		handler = default_assert_handler;
	
	assert_handler = handler;
}

/* 
   This function is used to provide asserts via exceptions. It is used by macro
   aal_assert().
*/
void __assert(
	char *hint,	     /* person owner of assert */
	int cond,	     /* condition of assertion */
	char *text,	     /* text of the assertion */
	char *file,	     /* source file assertion was failed in */
	int line,	     /* line of code assertion was failed in */
	char *func)          /* function in code assertion was failed in */
{
	/* Checking the condition and assert handler validness */
	if (!cond && assert_handler)
		assert_handler(hint, cond, text, file, line, func);
}

#endif

