/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   repair/semantic.h -- the structures and methods needed for semantic pass of fsck. */

#ifndef REPAIR_SEM_H
#define REPAIR_SEM_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/librepair.h>
#include <time.h>

/* Statistics gathered during the pass. */
typedef struct repair_semantic_stat {
	time_t time;
} repair_semantic_stat_t;

typedef struct repair_ancestor {
	reiser4_object_t *object;
	entry_type_t link;
} repair_ancestor_t;

/* Data semantic pass works on. */
typedef struct repair_semantic {
	repair_data_t *repair;
	
	/* Path of semantic recovery. */
	aal_list_t *path;
	
	repair_progress_handler_t *progress_handler;
	repair_progress_t *progress;
	repair_semantic_stat_t stat;
} repair_semantic_t;

extern errno_t repair_semantic(repair_semantic_t *sem);

typedef errno_t (*semantic_link_func_t) (reiser4_object_t *object,
					 reiser4_object_t *parent,
					 entry_type_t link, 
					 void *data);

extern reiser4_object_t *repair_semantic_open_child(reiser4_object_t *parent,
						    entry_hint_t *entry,
						    repair_data_t *repair,
						    semantic_link_func_t func,
						    void *data);
#endif
