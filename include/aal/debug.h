/*
  debug.h -- assert through exception implementation.
    
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef DEBUG_H
#define DEBUG_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef ENABLE_DEBUG

/*
  Something like standard assert, but working through exception 
  factory.
*/
#ifdef __GNUC__

#define aal_assert(hint, cond, action)	\
    do {				\
    	if (!__assert(hint, cond,	\
	   #cond,			\
	    __FILE__,			\
	    __LINE__,			\
	    __PRETTY_FUNCTION__))	\
	{				\
	    action;			\
	}				\
    } while (0);

#else

#define aal_assert(hint, cond, action)	\
    do {				\
	if (!__assert(hint, cond,	\
	    #cond,			\
	    "unknown",			\
	    0,				\
	    "unknown"))			\
	{				\
	    action;			\
	}				\
    } while (0);

#endif

#else

#define aal_assert(hint, cond, action) while (0) {}

#endif

extern int __assert(char *hint, int cond, char *text, char *file, 
		    int line, char *function);

#endif

