/*
    repair/repair.h -- the common structures and methods for recovery.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_H
#define REPAIR_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aux/bitmap.h>
#include <reiser4/reiser4.h>
#include <repair/repair_plugin.h>

/*
typedef struct repair_check_info {
    // amounts of different kinds of nodes.
    uint64_t leaves;
    uint64_t twigs;
    uint64_t branches;
    uint64_t unformatted;
    uint64_t zero_unformatted;
    uint64_t broken;
} repair_check_info_t;

typedef union repair_info {
    repair_check_info_t check;    
} repair_info_t;
*/


typedef enum repair_progress_type {
    PROGRESS_SILENT	= 0x1,
    PROGRESS_INDICATOR	= 0x2,
    PROGRESS_EMBEDDED	= 0x3
} repair_progress_type_t;

typedef enum repair_progress_state {
    PROGRESS_START  = 0x1,
    PROGRESS_UPDATE = 0x2,
    PROGRESS_END    = 0x3
} repair_progress_state_t;

typedef struct repair_progress {
    uint8_t type;   /* type of the progress - progress_type_t */
    uint8_t state;  /* state of the progress - progress_state_t */
    uint64_t total; /* count of elements to be handled */
    uint64_t done;   /* current element is been handled */
    char *title;    /* The gauge title. */
    char *text;	    /* text to be printed */
    void *data;	    /* opaque application data */
} repair_progress_t;

/* Callback for repair passes to print the progress. */
typedef errno_t (repair_progress_handler_t) (repair_progress_t *);

typedef struct repair_data {
    reiser4_fs_t *fs;
    
    uint64_t fatal;
    uint64_t fixable;

    uint8_t mode;

    repair_progress_handler_t *progress_handler;
} repair_data_t;

extern errno_t repair_check(repair_data_t *repair);

#endif

