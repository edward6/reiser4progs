/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   repair/repair.h -- the common structures and methods for recovery. */

#ifndef REPAIR_H
#define REPAIR_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aux/bitmap.h>
#include <reiser4/reiser4.h>
#include <repair/plugin.h>

typedef enum repair_progress_type {
	PROGRESS_SILENT	= 0x1,
	PROGRESS_RATE	= 0x2,
	PROGRESS_TREE	= 0x3
} repair_progress_type_t;

typedef enum repair_progress_state {
	PROGRESS_START	= 0x1,
	PROGRESS_UPDATE	= 0x2,
	PROGRESS_END	= 0x3,
	PROGRESS_STAT	= 0x4
} repair_progress_state_t;

typedef struct repair_progress_rate {
	uint64_t done;		/* current element is been handled		*/
	uint64_t total;		/* total elements to be handled			*/
} repair_progress_rate_t;

typedef struct repair_progress_tree {
	uint32_t item;		/* current element is been handled		*/
	uint32_t unit;		/* current subelement is been handled		*/
	uint32_t i_total;	/* total of elements				*/
	uint32_t u_total;	/* total of subelements				*/
} repair_progress_tree_t;

typedef struct repair_progress {
	uint8_t type;		/* type of the progress - progress_type_t	*/
	uint8_t state;		/* state of the progress - progress_state_t	*/
	char *title;		/* The title of the progress.			*/
	char *text;		/* Some uptodate text for the progress.     
				   Becomes the name of the gauge for now.	*/
    
	union {
		repair_progress_rate_t rate;
		repair_progress_tree_t tree;
	} u;
    
	void *data;		/* opaque application data			*/
} repair_progress_t;

/* Callback for repair passes to print the progress. */
typedef errno_t (repair_progress_handler_t) (repair_progress_t *);

typedef struct repair_data {
	reiser4_fs_t *fs;
    
	uint64_t fatal;
	uint64_t fixable;

	uint8_t mode;
	uint8_t debug_flag;

	repair_progress_handler_t *progress_handler;
} repair_data_t;

extern errno_t repair_check(repair_data_t *repair);

#define INVAL_PTR ((void *)-1)

#endif
