/* Copyright (C) 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   fsck.h -- fsck structure declarations. */

#ifndef FSCK_H
#define FSCK_H

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

#include <repair/librepair.h>
#include <reiser4/reiser4.h>
#include "gauge.h"

/* Extended error codes for described in misc.h */
#define FIXABLE_ERROR	1
#define FATAL_SB_ERROR	2
#define FATAL_ERROR	3

/* fsck options. */
typedef enum fsck_options {
    FSCK_OPT_AUTO	    = 0x1,
    FSCK_OPT_FORCE	    = 0x2,
    FSCK_OPT_VERBOSE	    = 0x3,
    FSCK_OPT_READ_ONLY	    = 0x4,
    FSCK_OPT_DEBUG	    = 0x5
} fsck_options_t;

typedef struct fsck_parse {
    reiser4_param_t *param;
    uint8_t sb_mode, fs_mode;

    FILE *logfile;
    aal_device_t *host_device;
    uint16_t options;
} fsck_parse_t;

#endif
