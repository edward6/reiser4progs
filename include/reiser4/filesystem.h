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
#include <aux/bitmap.h>
#include <reiser4/plugin.h>

typedef struct key_entity reiser4_key_t;

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

struct reiser4_coord {
	reiser4_node_t *node;
	reiser4_pos_t pos;
	item_entity_t item;
};

enum node_flags {
	NF_DIRTY = 1 << 0
};

typedef enum node_flags node_flags_t;

struct lru_link {

	/* Pointers to next and prev items in lru list */
	aal_list_t *prev;
	aal_list_t *next;
};

typedef struct lru_link lru_link_t;

/* Reiser4 in-memory node structure */
struct reiser4_node {
	
	/* Lru related fields */
	lru_link_t lru;

	/* Position in parent node */
	reiser4_pos_t pos;
	
	/* List of children */
	aal_list_t *children;
	
	/* Reference to the tree */
	reiser4_tree_t *tree;
	
	/* Reference to parent node */
	reiser4_node_t *parent;
	
	/* Reference to left neighbour */
	reiser4_node_t *left;

	/* Refernce to right neighbour */
	reiser4_node_t *right;
	
	/* Node entity. */
	object_entity_t *entity;

	/* Some flags (dirty, etc) */
	node_flags_t flags;
	
	/* Device node lies on */
	aal_device_t *device;

	/* Block number node lies in */
	blk_t blk;

	/* Usage counter to prevent releasing used node */
	int counter;
	
	/* Some per-node user-specified data */
	void *data;
};

/* Reiser4 object structure (file, dir) */
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

enum reiser4_owner {
	O_SKIPPED  = 1 << 0,
	O_FORMAT   = 1 << 1,
	O_JOURNAL  = 1 << 2,
	O_ALLOC    = 1 << 3,
	O_UNKNOWN  = 1 << 5
};

typedef enum reiser4_owner reiser4_owner_t;

/* Reiser4 disk-format in-memory structure */
struct reiser4_format {
	reiser4_fs_t *fs;
	
	/* 
	   Disk-format entity. It is initialized by disk-format plugin durring
	   initialization.
	*/
	object_entity_t *entity;
};

typedef struct reiser4_format reiser4_format_t;

/* Journal structure */
struct reiser4_journal {
	reiser4_fs_t *fs;
    
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
	reiser4_fs_t *fs;
	
	aux_bitmap_t *forbid;
	object_entity_t *entity;
};

typedef struct reiser4_alloc reiser4_alloc_t;

/* Oid allocator structure */
struct reiser4_oid {
	reiser4_fs_t *fs;
	object_entity_t *entity;
};

typedef struct reiser4_oid reiser4_oid_t;

/* Tree modification trap typedefs */
typedef int (*preinsert_func_t) (reiser4_coord_t *, reiser4_item_hint_t *, 
				 void *);
typedef int (*pstinsert_func_t) (reiser4_coord_t *, reiser4_item_hint_t *, 
				 void *);

typedef int (*preremove_func_t) (reiser4_coord_t *, void *);
typedef int (*pstremove_func_t) (reiser4_coord_t *, void *);

/* Tree structure */
struct reiser4_tree {

	/* Reference to filesystem instance tree opened on */
	reiser4_fs_t *fs;

	/* 
	   Reference to root node. It is created by tree initialization routines
	   and always exists. All other nodes are loaded on demand and flushed
	   at memory presure event.
	*/
	reiser4_node_t *root;

	/* Tree root key */
	reiser4_key_t key;

	/*
	  The list of nodes present in tree cache sorted in recently used
	  order. Thanks a lot to Nikita for this good idea.
	*/
	aal_lru_t *lru;

	/* Memory pressure handler */
	void *mpressure;

	/* Tree modification traps */
	struct {
		preinsert_func_t preinsert;
		pstinsert_func_t pstinsert;

		preremove_func_t preremove;
		pstremove_func_t pstremove;
		void *data;
	} traps;

	/* Tree related plugin ids */
	struct {
		rpid_t key;
		rpid_t nodeptr;
	} profile;
};

struct traverse_hint {

	/* Flag which shows, should traverse remove nodes from tree cache or
	 * not */
	int cleanup;
	
	/* user-spacified data */
	void *data;
};

typedef struct traverse_hint traverse_hint_t;

/* Callback function type for opening node. */
typedef errno_t (*traverse_open_func_t) (reiser4_node_t **, blk_t, void *);

/* Callback function type for preparing per-node traverse data. */
typedef errno_t (*traverse_edge_func_t) (reiser4_node_t *, void *);

/* Callback function type for preparing per-item traverse data. */
typedef errno_t (*traverse_setup_func_t) (reiser4_coord_t *, void *);

/* Filesystem compound structure */
struct reiser4_fs {
    
	/* Device filesystem is opended/created on */
	aal_device_t *device;
    
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
				     aal_device_t *journal_device);

extern void reiser4_fs_close(reiser4_fs_t *fs);

#ifndef ENABLE_COMPACT

extern errno_t reiser4_fs_mark(reiser4_fs_t *fs);

extern reiser4_fs_t *reiser4_fs_create(aal_device_t *host_device,
				       char *uuid, char *label,
				       count_t len,
				       reiser4_profile_t *profile,
				       aal_device_t *journal_device, 
				       void *journal_hint);

extern errno_t reiser4_fs_clobber(aal_device_t *device);
extern errno_t reiser4_fs_sync(reiser4_fs_t *fs);

#endif

extern const char *reiser4_fs_name(reiser4_fs_t *fs);
extern uint16_t reiser4_fs_blocksize(reiser4_fs_t *fs);

extern rpid_t reiser4_fs_format_pid(reiser4_fs_t *fs);

extern aal_device_t *reiser4_fs_host_device(reiser4_fs_t *fs);
extern aal_device_t *reiser4_fs_journal_device(reiser4_fs_t *fs);

extern errno_t reiser4_fs_layout(reiser4_fs_t *fs,
				 block_func_t func, 
				 void *data);

extern reiser4_owner_t reiser4_fs_belongs(reiser4_fs_t *fs,
					  blk_t blk);
	
#endif

