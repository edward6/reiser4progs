/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/plugin.h - reiser4 plugins repair code methods. */

#ifndef REPAIR_PLUGIN_H
#define REPAIR_PLUGIN_H

#include <aal/types.h>

typedef enum repair_mode {
	/* Check the consistensy of the fs  */
	RM_CHECK	= 1,
	/* Fix all fixable corruptions. */
	RM_FIX		= 2,
	/* Rebuild the fs from the found wrecks. */
	RM_BUILD	= 3,
	/* Rollback changes have been make by the last fsck run. */
	RM_BACK		= 4,
	/* No one mode anymore. */
	RM_LAST		= 5
} repair_mode_t;

/* Fixable errors were detected. */
#define RE_FIXABLE	((int64_t)1 << 32)
/* Fatal errors were detected. */
#define RE_FATAL	((int64_t)1 << 33)
/* For expansibility. */
#define RE_LAST		((int64_t)1 << 34)

#define repair_error_fatal(result)   ((result < 0) || (result & RE_FATAL))

#define EXCEPTION_TYPE_FSCK EXCEPTION_TYPE_LAST

#define fsck_mess(msg, list...)				\
	aal_exception_throw(EXCEPTION_TYPE_FSCK,	\
			    EXCEPTION_OPT_OK,		\
			    "FSCK: "msg,		\
			    ##list)

#define MASTER_PACK_SIGN	"MSTR"
#define STATUS_PACK_SIGN	"STAT"
#define BACKUP_PACK_SIGN	"BCKP"
#define FORMAT_PACK_SIGN	"FRMT"
#define ALLOC_PACK_SIGN		"ALLO"
#define NODE_PACK_SIGN		"NODE"
#define BLOCK_PACK_SIGN		"BLCK"
#define JOURNAL_PACK_SIGN	"JRNL"

#endif
