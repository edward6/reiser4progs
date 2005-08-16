/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   types.h -- debugfs types. */

#ifndef DEBUGFS_TYPE_H
#define DEBUGFS_TYPE_H

typedef enum print_flags {
	PF_SUPER    = 1 << 0,
	PF_JOURNAL  = 1 << 1,
	PF_ALLOC    = 1 << 2,
	PF_OID	    = 1 << 3,
	PF_TREE	    = 1 << 4,
	PF_BLOCK    = 1 << 5,
	PF_NODES    = 1 << 6,
	PF_ITEMS    = 1 << 7
} print_flags_t;

typedef enum behav_flags {
	BF_FORCE		= 1 << 0,
	BF_YES			= 1 << 1,
	BF_CAT			= 1 << 2,
	BF_SHOW_PARM		= 1 << 3,
	BF_SHOW_PLUG		= 1 << 4,
	BF_PACK_META		= 1 << 5,
	BF_UNPACK_META		= 1 << 6,
	BF_FREE_NEW_BACKUP	= 1 << 7
} behav_flags_t;

typedef enum space_flags {
	SF_WHOLE	= 1 << 0,
	SF_FREE		= 1 << 1
} space_flags_t;

#endif
