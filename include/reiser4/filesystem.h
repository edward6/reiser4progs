/*
  filesystem.h -- reiser4 filesystem structures and macros.    

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>
#include <reiser4/plugin.h>
#include <reiser4/key.h>

/* Master super block structure and macros */
struct reiser4_master_super {

	/* Reiser4 magic R4Sb */
	char mr_magic[4];

	/* Disk format plugin in use */
	d16_t mr_format_id;

	/* Block size in use */
	d16_t mr_blocksize;

	/* Universaly unique identifier */
	char mr_uuid[16];

	/* File system label in use */
	char mr_label[16];
};

typedef struct reiser4_master_super reiser4_master_super_t;

#define get_mr_format_id(mr)		aal_get_le16(mr, mr_format_id)
#define set_mr_format_id(mr, val)	aal_set_le16(mr, mr_format_id, val)

#define get_mr_blocksize(mr)		aal_get_le16(mr, mr_blocksize)
#define set_mr_blocksize(mr, val)	aal_set_le16(mr, mr_blocksize, val)

struct reiser4_master {
	int native;

	aal_block_t *block;
	reiser4_master_super_t *super;
};

typedef struct reiser4_master reiser4_master_t;

typedef struct reiser4_fs reiser4_fs_t;

/* 
   Profile structure. It describes what plugins will be used for every part of
   the filesystem.
*/
struct reiser4_profile {
	char label[255];
	char desc[255];
    
	rpid_t node;

	struct {
		rpid_t regular;
		rpid_t dirtory;
		rpid_t symlink;
		rpid_t special;      
	} file;
    
	struct {	    
		rpid_t statdata;
		rpid_t nodeptr;
	
		struct {
			rpid_t tail;
			rpid_t extent;
			rpid_t direntry;
		} file_body;
	
		rpid_t acl;
	} item;
    
	rpid_t hash;
	rpid_t tail;
	rpid_t perm;
	rpid_t format;
	rpid_t oid;
	rpid_t alloc;
	rpid_t journal;
	rpid_t key;

	uint64_t sdext;
};

typedef struct reiser4_profile reiser4_profile_t;

typedef struct reiser4_tree reiser4_tree_t;
typedef struct reiser4_node reiser4_node_t;
typedef struct reiser4_coord reiser4_coord_t;
typedef struct reiser4_joint reiser4_joint_t;

enum coord_context {
	CT_RAW    = 0x0,
	CT_ENTITY = 0x1,
	CT_NODE   = 0x2,
	CT_JOINT  = 0x3
};

typedef enum coord_context coord_context_t;

struct reiser4_coord {

	/* Coord may used in any context (with node, joint, etc) */
	union {
		void *data;
		
		reiser4_node_t *node;
		reiser4_joint_t *joint;
		object_entity_t *entity;
	} u;

	/* Coord context flag */
	coord_context_t context;

	/* Pos inside used entity */
	reiser4_pos_t pos;

	/* Item entity needed for working with item plugin */
	item_entity_t entity;
};

enum joint_flags {
	JF_DIRTY = 1 << 0
};

typedef enum joint_flags joint_flags_t;

/* The personalization of on-disk node in libreiser4 internal tree */
struct reiser4_joint {
	
	/* Reference to parent node */
	reiser4_joint_t *parent;

	/* Position in parent node */
	reiser4_pos_t pos;

	/* List of children */
	aal_list_t *children;
	
	/* Reference to the tree */
	reiser4_tree_t *tree;
    
	/* Pointer to the node */
	reiser4_node_t *node;

	/* Reference to left neighbour */
	reiser4_joint_t *left;

	/* Refernce to right neighbour */
	reiser4_joint_t *right;

	/* Some flags (dirty, etc) */
	joint_flags_t flags;

	/* Pointers to next and prev items in lru list */
	aal_list_t *prev;
	aal_list_t *next;

	/* Usage counter */
	int counter;
	
	/* User specified data */
	void *data;
};

/* Reiser4 in-memory node structure */
struct reiser4_node {

	/* Node entity. This field is uinitializied by node plugin */
	object_entity_t *entity;

	/* Device node lies on */
	aal_device_t *device;

	/* Block number node lies in */
	blk_t blk;

	/* Some per-node user-specified data */
	void *data;
};

/* Reiserfs object structure (file, dir) */
struct reiser4_file {

	/* Object entity. It is initialized by object plugin */
	object_entity_t *entity;
    
	/* Current coord */
	reiser4_coord_t coord;

	/* Object key of first item (most probably stat data item) */
	reiser4_key_t key;
    
	/* Referrence to the filesystem object opened on */
	reiser4_fs_t *fs;

	/* Full file name */
	char name[256];

	/* Some per-file user-specified data */
	void *data;
};

typedef struct reiser4_file reiser4_file_t;

enum reiser4_belong {
	RB_SKIPPED  = 1 << 0,
	RB_FORMAT   = 1 << 1,
	RB_JOURNAL  = 1 << 2,
	RB_ALLOC    = 1 << 3,
	RB_UNKNOWN  = 1 << 5
};

typedef enum reiser4_belong reiser4_belong_t;

/* Reiser4 disk-format in-memory structure */
struct reiser4_format {

	/* Device filesystem opended on */
	aal_device_t *device;
    
	/* 
	   Disk-format entity. It is initialized by disk-format plugin durring
	   initialization.
	*/
	object_entity_t *entity;
};

typedef struct reiser4_format reiser4_format_t;

/* Journal structure */
struct reiser4_journal {
    
	/* 
	   Device journal opened on. In the case of standard journal this field
	   will be pointing to the same device as in disk-format struct. If the
	   journal is t relocated one then device will be contain pointer to
	   opened device journal is opened on.
	*/
	aal_device_t *device;

	/* Journal entity. Initializied by plugin */
	object_entity_t *entity;
};

typedef struct reiser4_journal reiser4_journal_t;

/* Block allocator structure */
struct reiser4_alloc {
	object_entity_t *entity;
};

typedef struct reiser4_alloc reiser4_alloc_t;

/* Oid allocator structure */
struct reiser4_oid {
	object_entity_t *entity;
};

typedef struct reiser4_oid reiser4_oid_t;

struct reiser4_lru {
	
	/* The body of lru list */
	aal_list_t *list;

	/* The number of blocks to be freed on the next cache shrink */
	uint32_t adjust;

	/* Is lru djustable */
	int adjustable;

	/* Memory pressure handler handle */
	void *mpressure;

	/* Some usefull data */
	void *data;
};

typedef struct reiser4_lru reiser4_lru_t;

/* Tree structure */
struct reiser4_tree {

	/* Reference to filesystem instance tree opened on */
	reiser4_fs_t *fs;

	/* 
	   Reference to root node. It is created by tree initialization routines
	   and always exists. All other nodes are loaded on demand and flushed
	   at memory presure event.
	*/
	reiser4_joint_t *root;

	/* Tree root key */
	reiser4_key_t key;

	/*
	  The list of joints present in tree cache sorted in recently used
	  order. Thanks a lot to Nikita for this good idea.
	*/
	reiser4_lru_t lru;
};

struct traverse_hint {

	/* Flag which shows, should traverse remove nodes from tree cache or
	 * not */
	int cleanup;
	
	/* Current level traverse operates on */
	uint8_t level;
	
	/* Bitmask of item types which should be handled. */
	rpid_t objects;
	
	/* user-spacified data */
	void *data;
};

typedef struct traverse_hint traverse_hint_t;

/* Callback function type for opening node. */
typedef errno_t (*reiser4_open_func_t) (reiser4_joint_t **, blk_t, traverse_hint_t *);

/* Callback function type for preparing per-node traverse data. */
typedef errno_t (*reiser4_edge_func_t) (reiser4_joint_t *, traverse_hint_t *);

/* Callback function type for preparing per-item traverse data. */
typedef errno_t (*reiser4_setup_func_t) (reiser4_coord_t *, traverse_hint_t *);

/* Filesystem compound structure */
struct reiser4_fs {
    
	/* Pointer to the master super block wrapp object */
	reiser4_master_t *master;

	/* Pointer to the disk-format instance */
	reiser4_format_t *format;

	/* Pointer to the journal in use */
	reiser4_journal_t *journal;

	/* Pointer to the block allocator in use */
	reiser4_alloc_t *alloc;

	/* Pointer to the oid allocator */
	reiser4_oid_t *oid;

	/* The part of tree */
	reiser4_tree_t *tree;

	/* Root file (by default directory) */
	reiser4_file_t *root;

	/* Some usefull user-specified data */
	void *data;
};

/* Public functions */
extern reiser4_fs_t *reiser4_fs_open(aal_device_t *host_device, 
				     aal_device_t *journal_device, int replay);

extern void reiser4_fs_close(reiser4_fs_t *fs);

#ifndef ENABLE_COMPACT

extern reiser4_fs_t *reiser4_fs_create(reiser4_profile_t *profile,
				       aal_device_t *host_device,
				       size_t blocksize, const char *uuid, 
				       const char *label, count_t len,
				       aal_device_t *journal_device, 
				       void *journal_params);

extern errno_t reiser4_fs_clobber(aal_device_t *device);
extern errno_t reiser4_fs_sync(reiser4_fs_t *fs);

#endif

extern const char *reiser4_fs_name(reiser4_fs_t *fs);
extern uint16_t reiser4_fs_blocksize(reiser4_fs_t *fs);

extern rpid_t reiser4_fs_format_pid(reiser4_fs_t *fs);
extern aal_device_t *reiser4_fs_host_device(reiser4_fs_t *fs);
extern aal_device_t *reiser4_fs_journal_device(reiser4_fs_t *fs);

#endif

