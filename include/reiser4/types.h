/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   types.h -- reiser4 filesystem structures and macros. */

#ifndef REISER4_TYPES_H
#define REISER4_TYPES_H

#include <aal/libaal.h>
#include <reiser4/bitmap.h>
#include <reiser4/plugin.h>

/* Minimal block number needed for a reiser4 filesystem: Master, Format40,
   Bitmap, JHeader, JFooter, Status, Backup, Twig, Leaf + skipped ones. */
#define REISER4_FS_MIN_SIZE(blksize) \
	(9 + REISER4_MASTER_BLOCKNR(blksize))

/* Master super block structure. It is the same for all reiser4 filesystems,
   so, we can declare it here. It contains common for all format fields like
   block size etc. */
typedef struct reiser4_master_sb {
	/* Master super block magic. */
	char ms_magic[16];

	/* Disk format in use. */
	d16_t ms_format;

	/* Filesyetem block size in use. */
	d16_t ms_blksize;

	/* Filesyetm uuid in use. */
	char ms_uuid[16];

	/* Filesystem label in use. */
	char ms_label[16];
} reiser4_master_sb_t;

#define get_ms_format(ms)        aal_get_le16(ms, ms_format)
#define set_ms_format(ms, val)   aal_set_le16(ms, ms_format, val)

#define get_ms_blksize(ms)       aal_get_le16(ms, ms_blksize)
#define set_ms_blksize(ms, val)  aal_set_le16(ms, ms_blksize, val)

#define SS_MAGIC_SIZE	16
#define SS_STACK_SIZE	10
#define SS_MESSAGE_SIZE 256

typedef struct reiser4_status_sb {
	/* Status block magic string. */
	char ss_magic[16];

	/* Flags that contains current fs status like, corrupted, etc. */
	d64_t ss_status;

	/* Extended status flags. May be used as addition to main ones. */
	d64_t ss_extended;

	/* If there was some errors in last time filesystem was used, here may
	   be stored stack trace where it was. */
	d64_t ss_stack[SS_STACK_SIZE];

	/* Error message related to saved status and stack trace. */
	char ss_message[SS_MESSAGE_SIZE];
} reiser4_status_sb_t;

#define get_ss_status(ss)		aal_get_le64(ss, ss_status)
#define set_ss_status(ss, val)		aal_set_le64(ss, ss_status, val)

#define get_ss_extended(ss)		aal_get_le64(ss, ss_extended)
#define set_ss_extended(ss, val)	aal_set_le64(ss, ss_extended, val)

#define ss_stack(ss, n)			LE64_TO_CPU(ss->ss_stack[n])

typedef struct reiser4_fs reiser4_fs_t;

#ifndef ENABLE_MINIMAL
typedef struct reiser4_backup {
	reiser4_fs_t *fs;

	/* The backup data are stored here. */
	backup_hint_t hint;

	bool_t dirty;
	void *data;
} reiser4_backup_t;
#endif

enum reiser4_state {
	FS_OK		= 0,
	FS_CORRUPTED	= 1 << 0,
	FS_DAMAGED	= 1 << 1,
	FS_DESTROYED	= 1 << 2,
	FS_IO		= 1 << 3
};

typedef struct reiser4_master {
	/* Flag for marking master dirty */
	bool_t dirty;

	/* Device master is opened on */
	aal_device_t *device;

	/* Loaded master data */
	reiser4_master_sb_t ent;
} reiser4_master_t;

typedef struct reiser4_status {
	/* Flag for marking status block dirty */
	bool_t dirty;

	/* Block size */
	uint32_t blksize;

	/* Device status is opened on */
	aal_device_t *device;

	/* Loaded status data */
	reiser4_status_sb_t ent;
} reiser4_status_t;

typedef struct reiser4_tree reiser4_tree_t;

/* Calback types used in object code. */
typedef reiser4_object_t *(*object_open_func_t) (reiser4_object_t *, 
						 entry_hint_t *, void *);

#ifndef ENABLE_MINIMAL
typedef enum reiser4_owner {
	O_MASTER   = 1 << 0,
	O_FORMAT   = 1 << 1,
	O_JOURNAL  = 1 << 2,
	O_ALLOC    = 1 << 3,
	O_OID      = 1 << 4,
	O_STATUS   = 1 << 5,
	O_BACKUP   = 1 << 6,
	O_UNKNOWN  = 1 << 7
} reiser4_owner_t;
#endif

/* Reiser4 wrappers for all filesystem objects (journal, block allocator,
   etc.). They are used for make its plugins access simple. */
typedef struct reiser4_format {
	reiser4_fs_t *fs;
	
	/* Disk-format entity. It is initialized by disk-format plugin during
	   initialization. */
	reiser4_format_ent_t *ent;
} reiser4_format_t;

#ifndef ENABLE_MINIMAL

/* Journal structure */
typedef struct reiser4_journal {
	reiser4_fs_t *fs;
    
	/* Device journal will be opened on. In the case journal lie on the same
	   device as filesystem does, this field will point to the same device
	   instance as in fs struct. */
	aal_device_t *device;

	/* Journal entity. Initializied by plugin */
	reiser4_journal_ent_t *ent;
} reiser4_journal_t;

typedef struct reiser4_alloc reiser4_alloc_t;

typedef errno_t (*hook_alloc_t) (reiser4_alloc_t *,
				 uint64_t, uint64_t, void *);

/* Block allocator structure */
struct reiser4_alloc {
	reiser4_fs_t *fs;
	reiser4_alloc_ent_t *ent;

	struct {
		hook_alloc_t alloc;
		hook_alloc_t release;
		void *data;
	} hook;
};

#endif

/* Oid allocator structure */
typedef struct reiser4_oid {
	reiser4_fs_t *fs;
	reiser4_oid_ent_t *ent;
} reiser4_oid_t;

#ifndef ENABLE_MINIMAL
typedef errno_t (*estimate_func_t) (reiser4_place_t *place, 
				    trans_hint_t *hint);

typedef errno_t (*modify_func_t) (reiser4_node_t *node,
				  pos_t *pos, trans_hint_t *hint);
#endif

typedef int (*mpc_func_t) (reiser4_tree_t *);

/* Tree structure. */
struct reiser4_tree {
	tree_entity_t ent;
	
	/* Flag that shows, that tree adjusting is running now and should not be
	   called again until this flag is turned off. */
	int adjusting;
	
	/* Reference to filesystem instance tree opened on. */
	reiser4_fs_t *fs;

	/* Reference to root node. */
	reiser4_node_t *root;

	/* Tree root key. */
	reiser4_key_t key;

	/* Memory pressure detect function. */
	mpc_func_t mpc_func;

	/* Formatted nodes hash table. */
	aal_hash_table_t *nodes;

#ifndef ENABLE_MINIMAL
	/* Extents data stored here. */
	aal_hash_table_t *blocks;
#endif
};


#ifndef ENABLE_MINIMAL

/* Callback function type for opening node. */
typedef reiser4_node_t *(*tree_open_func_t) (reiser4_tree_t *, 
					     reiser4_place_t *, 
					     void *);
#endif

/* Filesystem compound structure */
struct reiser4_fs {
    
	/* Device filesystem is opened/created on */
	aal_device_t *device;
    
	/* Pointer to the master super block wrapper object */
	reiser4_master_t *master;

	/* Pointer to the disk-format instance */
	reiser4_format_t *format;

#ifndef ENABLE_MINIMAL
	/* Pointer to the journal in use */
	reiser4_journal_t *journal;

	/* Pointer to the block allocator in use */
	reiser4_alloc_t *alloc;

	/* Filesystem status block. */
	reiser4_status_t *status;
	
	/* Filesystem backup. */
	reiser4_backup_t *backup;

	/* Pointer to the oid allocator in use */
	reiser4_oid_t *oid;
#endif

	/* Pointer to the storage tree wrapper object */
	reiser4_tree_t *tree;

#ifndef ENABLE_MINIMAL
	/* Pointer to the semantic tree wrapper object */
	reiser4_object_t *root;

	/* Applications using this library sometimes need to embed information
	   into the objects of our library for their own use. */
	void *data;
#endif
};

typedef struct fs_hint {
	count_t blocks;
	uint32_t blksize;
	char uuid[17];
	char label[17];
} fs_hint_t;

typedef void (*uuid_unparse_t) (char *uuid, char *string);
typedef errno_t (*walk_func_t) (reiser4_tree_t *, reiser4_node_t *);
typedef errno_t (*walk_on_func_t) (reiser4_tree_t *, reiser4_place_t *);

#endif
