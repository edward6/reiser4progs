/*
    fsck.h -- fsck structure declarations.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <aux/aux.h>
#include <misc/misc.h>

#include <repair/librepair.h>
#include <reiser4/reiser4.h>

/* Extended error codes for described in misc.h */
#define FIXABLE_ERROR	1
#define FATAL_ERROR	2
