/*
  types.h -- debugs types.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef DEBUGFS_TYPE_H
#define DEBUGFS_TYPE_H

enum print_flags {
	PF_SUPER    = 1 << 0,
	PF_JOURNAL  = 1 << 1,
	PF_ALLOC    = 1 << 2,
	PF_OID	    = 1 << 3,
	PF_TREE	    = 1 << 4,
	PF_BLOCK    = 1 << 5,
	PF_FILE     = 1 << 6,
	PF_ITEMS    = 1 << 7
};

typedef enum print_flags print_flags_t;

enum behav_flags {
	BF_FORCE    = 1 << 0,
	BF_QUIET    = 1 << 1,
	BF_TFRAG    = 1 << 2,
	BF_FFRAG    = 1 << 3,
	BF_TSTAT    = 1 << 4,
	BF_DFRAG    = 1 << 5,
	BF_SEACH    = 1 << 6,
	BF_LS       = 1 << 7,
	BF_CAT      = 1 << 8,
	BF_PLUGS    = 1 << 9
};

typedef enum behav_flags behav_flags_t;

#endif
