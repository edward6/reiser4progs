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

#define get_ms_format(ms)               aal_get_le16(ms, ms_format)
#define set_ms_format(ms, val)          aal_set_le16(ms, ms_format, val)

#define get_ms_blksize(ms)              aal_get_le16(ms, ms_blksize)
#define set_ms_blksize(ms, val)         aal_set_le16(ms, ms_blksize, val)

struct reiser4_master {
	/* The flag, which shows that master super block is realy exist on disk
	   and that filesystem was opened by reading it rather than probbing all
	   known format plugins. We need to distinguish these two cases, because
	   we should not save master super block in reiser4_master_sync() if it
	   is not realy exist on disk without special actions like converting. */
	bool_t native;

	/* Flag for marking master as dirty */
	bool_t dirty;

	/* Device master is opened on */
	aal_device_t *device;

	/* Loaded master super block */
	reiser4_master_sb_t super;
};

typedef struct reiser4_fs reiser4_fs_t;
typedef struct reiser4_master reiser4_master_t;

struct reiser4_pid {
	char name[255];
	uint32_t type;
	uint64_t value;
};

typedef struct reiser4_pid reiser4_pid_t;

/* Profile structure. It describes what plugins will be used for every part of
   the filesystem. */
struct reiser4_profile {
	char name[10];
	reiser4_pid_t plugin[20];
};

typedef struct reiser4_profile reiser4_profile_t;

typedef struct reiser4_tree reiser4_tree_t;
typedef struct reiser4_node reiser4_node_t;
typedef struct reiser4_place reiser4_place_t;

struct reiser4_place {
	reiser4_tree_t *tree;
	reiser4_node_t *node;
	pos_t pos;
	
	item_entity_t item;
};

enum node_flags {
	NF_FOREIGN = 1 << 0
};

typedef enum node_flags node_flags_t;

/* Reiser4 in-memory node structure */
struct reiser4_node {
	
	/* Place in parent node */
	reiser4_place_t p;

	/* Reference to the tree. Sometimes node needs access tree and tree
	   functions. */
	reiser4_tree_t *tree;
	
	/* Reference to left neighbour. It is used for establishing silbing
	   links among nodes in memory tree cache. */
	reiser4_node_t *left;

	/* Refernce to right neighbour. It is used for establishing silbing
	   links among nodes in memory tree cache. */
	reiser4_node_t *right;
	
	/* List of children nodes. It is used for constructing part of on-disk
	   tree in the memory. */
	aal_list_t *children;
	
	/* Node entity. Node plugin initializes this value and return it back in
	   node initializing time. This node entity is used for performing all
	   on-node actions. */
	object_entity_t *entity;

	/* Device node lies on */
	aal_device_t *device;

	/* Node size */
	uint32_t size;

	/* Block number node lies in */
	blk_t number;

	/* Usage counter to prevent releasing used nodes */
	signed counter;

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
	/* Object entity. It is initialized by object plugin */
	object_entity_t *entity;

	/* Object info (keys, etc) */
	object_info_t info;
	bool_t follow;

#ifndef ENABLE_STAND_ALONE
	
	/* Full file name or printed key */
	char name[OBJECT_NAME_SIZE];

	/* Applications using this library sometimes need to embed information
	   into the objects of our library for their own use. */
	void *data;
#endif
};

typedef struct reiser4_object reiser4_object_t;

#ifndef ENABLE_STAND_ALONE
enum reiser4_owner {
	O_SKIPPED  = 1 << 0,
	O_FORMAT   = 1 << 1,
	O_JOURNAL  = 1 << 2,
	O_ALLOC    = 1 << 3,
	O_OID      = 1 << 4,
	O_UNKNOWN  = 1 << 5
};

typedef enum reiser4_owner reiser4_owner_t;
#endif

/* Reiser4 wrappers for all filesystem objects (journal, block allocator,
   etc.). They are used for make its plugins access simple. */
struct reiser4_format {
	reiser4_fs_t *fs;
	
	/* Disk-format entity. It is initialized by disk-format plugin durring
	   initialization. */
	object_entity_t *entity;
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
	object_entity_t *entity;
};

typedef struct reiser4_journal reiser4_journal_t;

/* Block allocator structure */
struct reiser4_alloc {
	reiser4_fs_t *fs;
	
	aux_bitmap_t *forbid;
	object_entity_t *entity;
};

typedef struct reiser4_alloc reiser4_alloc_t;
#endif

/* Oid allocator structure */
struct reiser4_oid {
	reiser4_fs_t *fs;
	object_entity_t *entity;
};

typedef struct reiser4_oid reiser4_oid_t;

typedef errno_t (*connect_func_t) (reiser4_tree_t *,
				   reiser4_place_t *,
				   reiser4_node_t *, void *);

#ifndef ENABLE_STAND_ALONE
typedef bool_t (*enough_func_t) (reiser4_tree_t *,
				 reiser4_place_t *,
				 uint32_t);

/* Tree modification trap typedefs */
typedef bool_t (*insert_func_t) (reiser4_tree_t *,
				 reiser4_place_t *,
				 create_hint_t *, 
				 void *);

typedef bool_t (*remove_func_t) (reiser4_tree_t *,
				 reiser4_place_t *,
				 void *);

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
		/* These traps will be called durring insert an item/unit */
		insert_func_t pre_insert;
		insert_func_t post_insert;

		/* These traps will be called durring remove an item/unit */
		remove_func_t pre_remove;
		remove_func_t post_remove;

		/* These traps will be called for connect/disconnect nodes in
		   tree. They may be used for keeping track nodes in tree. */
		connect_func_t connect;
		connect_func_t disconnect;

		/* This trap is called by any remove from the tree. It may be
		   used for implementing an alternative tree packing in remove
		   point. By default it uses so called "local packing", that is,
		   shift target node into left neighbour and shift right node
		   into target one. */
		pack_func_t pack;
#endif

		/* Traps used opaque data  */
		void *data;
	} traps;

#ifndef ENABLE_STAND_ALONE
	/* Extents data is stored here */
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
#endif

	/* Pointer to the oid allocator in use */
	reiser4_oid_t *oid;

	/* Pointer to the storage tree wrapper object */
	reiser4_tree_t *tree;

#ifndef ENABLE_STAND_ALONE
	/* Pointer to the semantic tree wrapper object */
	reiser4_object_t *root;

	/* Default profile. All plugin ids which cannot be obtained anywhere
	   (for instance, in parent node or directory) will be taken from this
	   profile.*/
	reiser4_profile_t *profile;

	/* Applications using this library sometimes need to embed information
	   into the objects of our library for their own use. */
	void *data;
#endif

	/* Key policy (short, large) */
	key_policy_t key;
};

struct fs_hint {
	count_t blocks;
	uint32_t blksize;
	key_policy_t key;
	char uuid[17], label[17];
	reiser4_profile_t *profile;
};

typedef struct fs_hint fs_hint_t;

typedef errno_t (*walk_func_t) (reiser4_tree_t *,
				reiser4_node_t *);

#endif
