/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   types.h -- reiser4 filesystem structures and macros. */

#ifndef REISER4_TYPES_H
#define REISER4_TYPES_H

#include <aal/aal.h>
#include <aux/bitmap.h>
#include <reiser4/plugin.h>

typedef struct key_entity reiser4_key_t;

/* Master super block structure. It is the same for all reiser4 filesystems,
   so, we can declare it here. It contains common for all format fields like
   block size etc. */
struct reiser4_master_sb {
	char ms_magic[4];
	d16_t ms_format;
	d16_t ms_blksize;
	char ms_uuid[16];
	char ms_label[16];
};

typedef struct reiser4_master_sb reiser4_master_sb_t;

#define get_ms_format(ms)        aal_get_le16(ms, ms_format)
#define set_ms_format(ms, val)   aal_set_le16(ms, ms_format, val)

#define get_ms_blksize(ms)       aal_get_le16(ms, ms_blksize)
#define set_ms_blksize(ms, val)  aal_set_le16(ms, ms_blksize, val)

#define SS_STACK_SIZE	10
#define SS_MESSAGE_SIZE 256

struct reiser4_status_sb {
	char ss_magic[16];
	d64_t ss_status;		    /* current filesystem state */
	d64_t ss_extended;		    /* any additional info that might have 
					       sense in addition to "status". */
	d64_t ss_stack[SS_STACK_SIZE];      /* last ten functional calls made
					       (addresses). */
	char ss_message[SS_MESSAGE_SIZE];   /* any error message if appropriate, 
					       otherwise filled with zeroes. */
};

typedef struct reiser4_status_sb reiser4_status_sb_t;

#define get_ss_status(ss)		aal_get_le64(ss, ss_status)
#define set_ss_status(ss, val)		aal_set_le64(ss, ss_status, val)

#define get_ss_extended(ss)		aal_get_le64(ss, ss_extended)
#define set_ss_extended(ss, val)	aal_set_le64(ss, ss_extended, val)

#define ss_stack(ss, n)			LE64_TO_CPU(ss->ss_stack[n])

struct reiser4_master {
	/* Flag for marking master dirty */
	bool_t dirty;

	/* Device master is opened on */
	aal_device_t *device;

	/* Loaded master data */
	reiser4_master_sb_t ent;
};

typedef struct reiser4_fs reiser4_fs_t;
typedef struct reiser4_master reiser4_master_t;

struct reiser4_status {
	/* Flag for marking status block dirty */
	bool_t dirty;

	/* Block size */
	uint32_t blksize;

	/* Device status is opened on */
	aal_device_t *device;

	/* Loaded status data */
	reiser4_status_sb_t ent;
};

typedef struct reiser4_status reiser4_status_t;

struct reiser4_pid {
	char name[255];
	uint32_t type;
	uint64_t value;
};

typedef struct reiser4_pid reiser4_pid_t;

#define PARAM_NR 19

/* Profile structure. It describes what plugins will be used for every part of
   the filesystem. */
struct reiser4_param {
	char name[10];
	reiser4_pid_t pid[PARAM_NR];
};

typedef struct reiser4_param reiser4_param_t;

typedef struct reiser4_tree reiser4_tree_t;
typedef struct reiser4_node reiser4_node_t;
typedef struct reiser4_place reiser4_place_t;

struct reiser4_place {
	reiser4_node_t *node;
	aal_block_t *block;
	reiser4_plug_t *plug;

	pos_t pos;
	body_t *body;
	uint32_t len;
	key_entity_t key;
};

enum node_flags {
	NF_FOREIGN = 1 << 0
};

typedef enum node_flags node_flags_t;

/* Reiser4 in-memory node structure */
struct reiser4_node {
	/* Node entity. Node plugin initializes this value and return it back in
	   node initializing time. This node entity is used for performing all
	   on-node actions. */
	node_entity_t *entity;

	/* Place in parent node */
	reiser4_place_t p;

	/* Reference to the tree. Sometimes node needs access tree and tree
	   functions. */
	reiser4_tree_t *tree;
	
	/* Reference to left neighbour. It is used for establishing silbing
	   links among nodes in memory tree cache. */
	reiser4_node_t *left;

	/* Reference to right neighbour. It is used for establishing silbing
	   links among nodes in memory tree cache. */
	reiser4_node_t *right;
	
	/* List of children nodes. It is used for constructing part of on-disk
	   tree in the memory. */
	aal_list_t *children;
	
	/* Usage counter to prevent releasing used nodes */
	signed int counter;

#ifndef ENABLE_STAND_ALONE
	/* Some node flags */
	node_flags_t flags;
	
	/* Applications using this library sometimes need to embed information
	   into the objects of our library for their own use. */
	void *data;
#endif
};

#define OBJECT_NAME_SIZE 1024

/* Reiser4 file structure (regular file, directory, symlinks, etc) */
struct reiser4_object {
	/* Tree reference */
	reiser4_tree_t *tree;
	
	/* Object info pointer */
	object_info_t *info;
	
	/* Object entity. It is initialized by object plugin */
	object_entity_t *entity;

#ifndef ENABLE_STAND_ALONE
	/* Full file name or printed key */
	char name[OBJECT_NAME_SIZE];

	/* Applications using this library sometimes need to embed information
	   into the objects of our library for their own use. */
	void *data;
#endif

	/* Should be symlinks resolved or not. */
	bool_t follow;
};

typedef struct reiser4_object reiser4_object_t;

#ifndef ENABLE_STAND_ALONE
enum reiser4_owner {
	O_SKIPPED  = 1 << 0,
	O_MASTER   = 1 << 1,
	O_FORMAT   = 1 << 2,
	O_JOURNAL  = 1 << 3,
	O_ALLOC    = 1 << 4,
	O_OID      = 1 << 5,
	O_STATUS   = 1 << 6,
	O_UNKNOWN  = 1 << 7
};

typedef enum reiser4_owner reiser4_owner_t;
#endif

/* Reiser4 wrappers for all filesystem objects (journal, block allocator,
   etc.). They are used for make its plugins access simple. */
struct reiser4_format {
	reiser4_fs_t *fs;
	
	/* Disk-format entity. It is initialized by disk-format plugin durring
	   initialization. */
	generic_entity_t *entity;
};

typedef struct reiser4_format reiser4_format_t;

#ifndef ENABLE_STAND_ALONE

/* Journal structure */
struct reiser4_journal {
	reiser4_fs_t *fs;
    
	/* Device journal will be opened on. In the case journal lie on the same
	   device as filesystem does, this field will point to the same device
	   instance as in fs struct. */
	aal_device_t *device;

	/* Journal entity. Initializied by plugin */
	generic_entity_t *entity;
};

typedef struct reiser4_journal reiser4_journal_t;

typedef struct reiser4_alloc reiser4_alloc_t;
typedef errno_t (*hook_alloc_t) (reiser4_alloc_t *, uint64_t, uint64_t, void *);

/* Block allocator structure */
struct reiser4_alloc {
	reiser4_fs_t *fs;
	
	aux_bitmap_t *forbid;
	generic_entity_t *entity;

	struct {
		hook_alloc_t alloc;
		hook_alloc_t release;
		void *data;
	} hook;
};

#endif

/* Oid allocator structure */
struct reiser4_oid {
	reiser4_fs_t *fs;
	generic_entity_t *entity;
};

typedef struct reiser4_oid reiser4_oid_t;

#ifndef ENABLE_STAND_ALONE
typedef errno_t (*pack_func_t) (reiser4_tree_t *,
				reiser4_place_t *,
				void *);

enum tree_flags {
	TF_PACK = 1 << 0
};

typedef enum tree_flags tree_flags_t;
#endif

typedef bool_t (*mpc_func_t) (void);

/* Tree structure */
struct reiser4_tree {

	/* Reference to filesystem instance tree opened on */
	reiser4_fs_t *fs;

	/* Reference to root node. It is created by tree initialization routines
	   and always exists. All other nodes are loaded on demand and flushed
	   at memory presure event. */
	reiser4_node_t *root;

	/* Tree root key */
	reiser4_key_t key;

	/* Memory pressure check function */
	mpc_func_t mpc_func;

#ifndef ENABLE_STAND_ALONE
	/* Tree operation control flags */
	uint32_t flags;
#endif
	
	/* Tree modification traps */
	struct {

#ifndef ENABLE_STAND_ALONE
		/* This trap is called by tree_remove(). It may be used for
		   implementing an alternative tree packing at remove. By
		   default it uses so called "local packing", that is, shift
		   everything from target node to left neighbour and shift
		   everything from right node to target one. */
		pack_func_t pack;

		/* Pack callback related data. User may use it for setting some
		   usefull data to it, and then use it in alternative pack(). */
		void *data;
#endif
	} traps;

#ifndef ENABLE_STAND_ALONE
	/* Extents data stored here */
	aal_hash_table_t *data;
#endif
};

#ifndef ENABLE_STAND_ALONE
/* Callback function type for opening node. */
typedef reiser4_node_t *(*tree_open_func_t) (reiser4_tree_t *, 
					     reiser4_place_t *, 
					     void *);

/* Callback function type for preparing per-node traverse data. */
typedef errno_t (*tree_edge_func_t) (reiser4_tree_t *, 
				     reiser4_node_t *, 
				     void *);

/* Callback function type for preparing per-item traverse data. */
typedef errno_t (*tree_update_func_t) (reiser4_tree_t *, 
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

#ifndef ENABLE_STAND_ALONE
	/* Pointer to the journal in use */
	reiser4_journal_t *journal;

	/* Pointer to the block allocator in use */
	reiser4_alloc_t *alloc;

	/* Filesystem status block. */
	reiser4_status_t *status;
#endif

	/* Pointer to the oid allocator in use */
	reiser4_oid_t *oid;

	/* Pointer to the storage tree wrapper object */
	reiser4_tree_t *tree;

#ifndef ENABLE_STAND_ALONE
	/* Pointer to the semantic tree wrapper object */
	reiser4_object_t *root;

	/* Applications using this library sometimes need to embed information
	   into the objects of our library for their own use. */
	void *data;
#endif
};

struct fs_hint {
	count_t blocks;
	uint32_t blksize;
	char uuid[17], label[17];
};

typedef struct fs_hint fs_hint_t;

typedef errno_t (*walk_func_t) (reiser4_tree_t *,
				reiser4_node_t *);

#endif
