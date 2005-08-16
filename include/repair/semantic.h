/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/semantic.h -- the structures and methods needed for semantic
   pass of fsck. */

#ifndef REPAIR_SEM_H
#define REPAIR_SEM_H

#include <time.h>
#include <repair/librepair.h>

#define LOST_PREFIX "lost_name_"

/* Statistics gathered during the pass. */
typedef struct repair_semantic_stat {
	uint64_t reached_files;
	uint64_t lost_files;
	uint64_t shared, rm_entries, broken;
	uint64_t oid;

	/* Files counted on previous passes. */
	uint64_t statdatas;
	uint64_t files;
	time_t time;
} repair_semantic_stat_t;

typedef struct repair_ancestor {
	reiser4_object_t *object;
	entry_type_t link;
} repair_ancestor_t;

/* Data semantic pass works on. */
typedef struct repair_semantic {
	repair_data_t *repair;

	reiser4_object_t *root;
	reiser4_object_t *lost;
	
	repair_semantic_stat_t stat;

	aal_gauge_t *gauge;
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
