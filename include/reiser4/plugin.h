/*
  plugin.h -- reiser4 plugin factory implementation.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef PLUGIN_H
#define PLUGIN_H

#include <aal/aal.h>

#define LEAF_LEVEL	    (1)
#define TWIG_LEVEL	    (LEAF_LEVEL + 1)

#define MASTER_OFFSET	    (65536)
#define MASTER_MAGIC	    ("R4Sb")

#define DEFAULT_BLOCKSIZE   (4096)

enum direction {
	D_LEFT  = 0,
	D_RIGHT = 1
};

typedef enum direction direction_t;

enum shift_flags {
	SF_LEFT	 = 1 << 0,
	SF_RIGHT = 1 << 1,
	SF_MOVIP = 1 << 2
};

typedef enum shift_flags shift_flags_t;

/* 
   Defining types for disk structures. All types like f32_t are fake types
   needed to avoid gcc-2.95.x bug with typedef of aligned types.
*/
typedef uint8_t  f8_t;  typedef f8_t  d8_t  __attribute__((aligned(1)));
typedef uint16_t f16_t; typedef f16_t d16_t __attribute__((aligned(2)));
typedef uint32_t f32_t; typedef f32_t d32_t __attribute__((aligned(4)));
typedef uint64_t f64_t; typedef f64_t d64_t __attribute__((aligned(8)));

typedef uint64_t roid_t;
typedef uint16_t rpid_t;

typedef void reiser4_body_t;

enum reiser4_plugin_type {
	FILE_PLUGIN_TYPE    = 0x0,

	/*
	  In reiser4 kernel code DIR_PLUGIN_TYPE exists, but libreiser4 works
	  with files and directories by the unified interface and we do not need
	  that additional type. But we have to be compatible, because
	  reiser4_plugin_type may be stored in stat data extentions.
	*/

	ITEM_PLUGIN_TYPE	= 0x2,
	NODE_PLUGIN_TYPE	= 0x3,
	HASH_PLUGIN_TYPE	= 0x4,
	TAIL_PLUGIN_TYPE	= 0x5,
	PERM_PLUGIN_TYPE	= 0x6,
	SDEXT_PLUGIN_TYPE	= 0x7,
	FORMAT_PLUGIN_TYPE	= 0x8,
	OID_PLUGIN_TYPE		= 0x9,
	ALLOC_PLUGIN_TYPE	= 0xa,
	JNODE_PLUGIN_TYPE	= 0xb,
	JOURNAL_PLUGIN_TYPE	= 0xc,
	KEY_PLUGIN_TYPE		= 0xd
};

typedef enum reiser4_plugin_type reiser4_plugin_type_t;

enum reiser4_file_plugin_id {
	FILE_REGULAR40_ID	= 0x0,
	FILE_DIRTORY40_ID	= 0x1,
	FILE_SYMLINK40_ID	= 0x2,
	FILE_SPECIAL40_ID	= 0x3
};

enum reiser4_file_group {
	REGULAR_FILE		= 0x0,
	DIRTORY_FILE		= 0x1,
	SYMLINK_FILE		= 0x2,
	SPECIAL_FILE		= 0x3
};

typedef enum reiser4_file_group reiser4_file_group_t;

enum reiser4_item_plugin_id {
	ITEM_STATDATA40_ID	= 0x0,
	ITEM_SDE40_ID		= 0x1,
	ITEM_CDE40_ID		= 0x2,
	ITEM_NODEPTR40_ID	= 0x3,
	ITEM_ACL40_ID		= 0x4,
	ITEM_EXTENT40_ID	= 0x5,
	ITEM_TAIL40_ID		= 0x6
};

enum reiser4_item_group {
	STATDATA_ITEM		= 0x0,
	NODEPTR_ITEM		= 0x1,
	DIRENTRY_ITEM		= 0x2,
	TAIL_ITEM		= 0x3,
	EXTENT_ITEM		= 0x4,
	PERMISSN_ITEM		= 0x5,
	UNKNOWN_ITEM
};

typedef enum reiser4_item_group reiser4_item_group_t;

enum reiser4_node_plugin_id {
	NODE_REISER40_ID	= 0x0,
};

enum reiser4_hash_plugin_id {
	HASH_RUPASOV_ID		= 0x0,
	HASH_R5_ID		= 0x1,
	HASH_TEA_ID		= 0x2,
	HASH_FNV1_ID		= 0x3,
	HASH_DEGENERATE_ID	= 0x4
};

typedef enum reiser4_hash reiser4_hash_t;

enum reiser4_tail_plugin_id {
	TAIL_NEVER_ID		 = 0x0,
	TAIL_SUPPRESS_ID	 = 0x1,
	TAIL_FOURK_ID		 = 0x2,
	TAIL_ALWAYS_ID		 = 0x3,
	TAIL_SMART_ID		 = 0x4,
	TAIL_LAST_ID		 = 0x5
};

enum reiser4_perm_plugin_id {
	PERM_RWX_ID		 = 0x0
};

enum reiser4_sdext_plugin_id {
	SDEXT_LW_ID	         = 0x0,
	SDEXT_UNIX_ID		 = 0x1,
	SDEXT_LT_ID              = 0x2,
	SDEXT_SYMLINK_ID	 = 0x3,
	SDEXT_PLUGIN_ID		 = 0x4,
	SDEXT_GEN_FLAGS_ID       = 0x5,
	SDEXT_CAPS_ID            = 0x6,
	SDEXT_LARGE_TIMES_ID     = 0x7,
	SDEXT_LAST
};

enum reiser4_format_plugin_id {
	FORMAT_REISER40_ID	 = 0x0,
	FORMAT_REISER36_ID	 = 0x1
};

enum reiser4_oid_plugin_id {
	OID_REISER40_ID		 = 0x0,
	OID_REISER36_ID		 = 0x1
};

enum reiser4_alloc_plugin_id {
	ALLOC_REISER40_ID	 = 0x0,
	ALLOC_REISER36_ID	 = 0x1
};

enum reiser4_journal_plugin_id {
	JOURNAL_REISER40_ID	 = 0x0,
	JOURNAL_REISER36_ID	 = 0x1
};

enum reiser4_key_plugin_id {
	KEY_REISER40_ID		 = 0x0,
	KEY_REISER36_ID		 = 0x1
};

typedef union reiser4_plugin reiser4_plugin_t;

#define FAKE_PLUGIN (0xffff)

/* 
   Maximal possible key size. It is used for creating temporary keys by
   declaring array of uint8_t elements KEY_SIZE long.
*/
#define KEY_SIZE 24

struct reiser4_key {
	reiser4_plugin_t *plugin;
	uint8_t body[KEY_SIZE];
};

typedef struct reiser4_key reiser4_key_t;

#define KEY_FILENAME_TYPE   0x0
#define KEY_STATDATA_TYPE   0x1
#define KEY_ATTRNAME_TYPE   0x2
#define KEY_ATTRBODY_TYPE   0x3
#define KEY_FILEBODY_TYPE   0x4
#define KEY_LAST_TYPE	    0x5

typedef uint32_t reiser4_key_type_t;

struct reiser4_pos {
	uint32_t item;
	uint32_t unit;
};

typedef struct reiser4_pos reiser4_pos_t;

/* Type for describing inside the library the objects created by plugins
 * themselves and which also have plugin. For example, node, format, alloc,
 * etc. The pointer of this type will be passed to list plugins for working with
 * them. */
struct object_entity {
	reiser4_plugin_t *plugin;
};

typedef struct object_entity object_entity_t;

struct item_context {
	blk_t blk;
	aal_device_t *device;
	object_entity_t *node;
};

typedef struct item_context item_context_t;

/* Type for describing an item. The pointer of this type will be passed to the
 * all item plugins. */
struct item_entity {
	reiser4_plugin_t *plugin;
	reiser4_key_t key;

	uint32_t len, pos;
	reiser4_body_t *body;

	item_context_t context;
};

typedef struct item_entity item_entity_t;
typedef struct reiser4_place reiser4_place_t;

/* Types for layout defining */
typedef errno_t (*format_action_func_t) (object_entity_t *, uint64_t, void *);

typedef errno_t (*format_layout_func_t) (object_entity_t *, format_action_func_t,
					 void *);

/* Type for file layout callback function */
typedef errno_t (*file_action_func_t) (object_entity_t *, uint64_t, void *);
typedef errno_t (*file_layout_func_t) (object_entity_t *, file_action_func_t, void *);

/* 
   To create a new item or to insert into the item we need to perform the
   following operations:
    
   (1) Create the description of the data being inserted.
   (2) Ask item plugin how much space is needed for the data, described in 1.
    
   (3) Free needed space for data being inserted.
   (4) Ask item plugin to create an item (to paste into the item) on the base
   of description from 1.

   For such purposes we have:
    
   (1) Fixed description structures for all item types (statdata, direntry, 
   nodeptr, etc).
    
   (2) Estimate common item method which gets coord of where to insert into
   (NULL or unit == -1 for insertion, otherwise it is pasting) and data
   description from 1.
    
   (3) Insert node methods prepare needed space and call create/paste item
   methods if data description is specified.
    
   (4) Create/Paste item methods if data description has not beed specified
   on 3.
*/

struct reiser4_ptr_hint {    
	uint64_t ptr;
	uint64_t width;
};

typedef struct reiser4_ptr_hint reiser4_ptr_hint_t;

struct reiser4_sdext_unix_hint {
	uint32_t uid;
	uint32_t gid;
	uint32_t atime;
	uint32_t mtime;
	uint32_t ctime;
	uint32_t rdev;
	uint64_t bytes;
};

typedef struct reiser4_sdext_unix_hint reiser4_sdext_unix_hint_t;

struct reiser4_sdext_lw_hint {
	uint16_t mode;
	uint32_t nlink;
	uint64_t size;
};

typedef struct reiser4_sdext_lw_hint reiser4_sdext_lw_hint_t;

struct reiser4_sdext_lt_hint {
	uint32_t atime;
	uint32_t mtime;
	uint32_t ctime;
};

typedef struct reiser4_sdext_lt_hint reiser4_sdext_lt_hint_t;

/* These fields should be changed to what proper description of needed extentions */
struct reiser4_statdata_hint {
	
	/* Extentions mask */
	uint64_t extmask;
    
	/* Stat data extentions */
	void *ext[60];
};

typedef struct reiser4_statdata_hint reiser4_statdata_hint_t;

struct reiser4_entry_hint {

	/* Locality and objectid of object pointed by entry */
	struct {
		roid_t locality;
		roid_t objectid;
	} objid;

	/* Offset of entry */
	struct {
		roid_t objectid;
		uint64_t offset;
	} entryid;

	/* Name of entry */
	char *name;
};

typedef struct reiser4_entry_hint reiser4_entry_hint_t;

struct reiser4_direntry_hint {
	uint16_t count;
	reiser4_entry_hint_t *entry;
};

typedef struct reiser4_direntry_hint reiser4_direntry_hint_t;

struct reiser4_file_hint {
	rpid_t statdata_pid;

	/* Hint for a file body */
	union {
	
		/* Plugin ids for the directory body */
		struct {
			rpid_t direntry_pid;
			rpid_t hash_pid;
		} dir;
	
		/* Plugin id for the regular file body */
		struct {
			rpid_t tail_pid;
			rpid_t extent_pid;
		} file;
	
	} body;
    
	/* The plugin in use */
	reiser4_plugin_t *plugin;
};

typedef struct reiser4_file_hint reiser4_file_hint_t;

/* 
   Create item or paste into item on the base of this structure. Here "data" is
   a pointer to data to be copied.
*/ 
struct reiser4_item_hint {
	/*
	  This is pointer to already formated item body. It is useful for item
	  copying, replacing, etc. This will be used by fsck probably.
	*/
	void *data;

	/*
	  This is pointer to hint which describes item. It is widely used for
	  creating an item.
	*/
	void *hint;
    
	/* Length of the item to inserted */
	uint16_t len;
    
	/* The key of item */
	reiser4_key_t key;

	/* Plugin to be used for creating item */
	reiser4_plugin_t *plugin;
};

typedef struct reiser4_item_hint reiser4_item_hint_t;

#define PLUGIN_MAX_LABEL	16
#define PLUGIN_MAX_DESC		256
#define PLUGIN_MAX_NAME		256

typedef struct reiser4_core reiser4_core_t;

/* Types for plugin init and fini functions */
typedef reiser4_plugin_t *(*reiser4_plugin_init_t) (reiser4_core_t *);
typedef errno_t (*reiser4_plugin_fini_t) (reiser4_core_t *);

typedef errno_t (*reiser4_plugin_func_t) (reiser4_plugin_t *, void *);

struct plugin_handle {
	char name[PLUGIN_MAX_NAME];
	reiser4_plugin_init_t init;
	reiser4_plugin_fini_t fini;
	void *data;
};

typedef struct plugin_handle plugin_handle_t;

struct plugin_sign {
	rpid_t id;
	rpid_t type;
	rpid_t group;
};

typedef struct plugin_sign plugin_sign_t;

/* Common plugin header */
struct reiser4_plugin_header {

	/*
	  Plugin handle. It is used for initializing and finalizing particular
	  plugin.
	*/
	plugin_handle_t handle;

	/* Plugin will be found by its sign */
	plugin_sign_t sign;

	/* Label and description */
	const char label[PLUGIN_MAX_LABEL];
	const char desc[PLUGIN_MAX_DESC];
};

typedef struct reiser4_plugin_header reiser4_plugin_header_t;

struct reiser4_key_ops {
	reiser4_plugin_header_t h;

	/* Smart check of key structure */
	errno_t (*valid) (reiser4_body_t *);
    
	/* Confirms key format */
	int (*confirm) (reiser4_body_t *);

	/* Returns minimal key for this key-format */
	reiser4_body_t *(*minimal) (void);
    
	/* Returns maximal key for this key-format */
	reiser4_body_t *(*maximal) (void);
    
	/* Compares two keys by comparing its all components */
	int (*compare) (reiser4_body_t *, reiser4_body_t *);

	/* Copyies src key to dst one */
	errno_t (*assign) (reiser4_body_t *, reiser4_body_t *);
    
	/* 
	   Cleans key up. Actually it just memsets it by zeros, but more smart behavior 
	   may be implemented.
	*/
	void (*clean) (reiser4_body_t *);

	errno_t (*build_generic) (reiser4_body_t *, reiser4_key_type_t,
				  uint64_t, uint64_t, uint64_t);
    
	errno_t (*build_direntry) (reiser4_body_t *, reiser4_plugin_t *,
				   uint64_t, uint64_t, const char *);
    
	errno_t (*build_objid) (reiser4_body_t *, 
				reiser4_key_type_t, uint64_t, uint64_t);
    
	errno_t (*build_entryid) (reiser4_body_t *, 
				  reiser4_plugin_t *, const char *);

	/* Gets/sets key type (minor in reiser4 notation) */	
	void (*set_type) (reiser4_body_t *, reiser4_key_type_t);
	reiser4_key_type_t (*get_type) (reiser4_body_t *);

	/* Gets/sets key locality */
	void (*set_locality) (reiser4_body_t *, uint64_t);
	uint64_t (*get_locality) (reiser4_body_t *);
    
	/* Gets/sets key objectid */
	void (*set_objectid) (reiser4_body_t *, uint64_t);
	uint64_t (*get_objectid) (reiser4_body_t *);

	/* Gets/sets key offset */
	void (*set_offset) (reiser4_body_t *, uint64_t);
	uint64_t (*get_offset) (reiser4_body_t *);

	/* Gets/sets directory key hash */
	void (*set_hash) (reiser4_body_t *, uint64_t);
	uint64_t (*get_hash) (reiser4_body_t *);

	/* Prints key into specified buffer */
	errno_t (*print) (reiser4_body_t *, aal_stream_t *, uint16_t);
};

typedef struct reiser4_key_ops reiser4_key_ops_t;

struct reiser4_file_ops {
	reiser4_plugin_header_t h;

	/* Creates new file with passed parent and object keys */
	object_entity_t *(*create) (const void *, reiser4_key_t *, 
				     reiser4_key_t *, reiser4_file_hint_t *); 
    
	/* Opens a file with specified key */
	object_entity_t *(*open) (const void *, reiser4_key_t *);

	/* Conforms file plugin in use */
	int (*confirm) (item_entity_t *);
    
	/* Closes previously opened or created directory */
	void (*close) (object_entity_t *);

	/* Resets internal position */
	errno_t (*reset) (object_entity_t *);
   
	/* Returns current position in directory */
	uint64_t (*offset) (object_entity_t *);

	/* Makes simple check of directory */
	errno_t (*valid) (object_entity_t *);

	/* Returns current position in directory */
	errno_t (*seek) (object_entity_t *, uint64_t);
    
	/* Makes lookup inside dir */
	int (*lookup) (object_entity_t *, char *, reiser4_key_t *);
    
	/* Reads the data from file to passed buffer */
	int32_t (*read) (object_entity_t *, void *, uint32_t);
    
	/* Writes the data to file from passed buffer */
	int32_t (*write) (object_entity_t *, void *, uint32_t);

	/* Truncates file to passed length */
	errno_t (*truncate) (object_entity_t *, uint64_t);

	/* Function for going throught all blocks specfied file occupied */
	file_layout_func_t layout;
};

typedef struct reiser4_file_ops reiser4_file_ops_t;

struct reiser4_item_ops {
	reiser4_plugin_header_t h;

	/* Forms item structures based on passed hint in passed memory area */
	errno_t (*init) (item_entity_t *, reiser4_item_hint_t *);

	/* Reads item data to passed hint */
	errno_t (*open) (item_entity_t *, reiser4_item_hint_t *);
	
	/* Inserts unit described by passed hint into the item */
	errno_t (*insert) (item_entity_t *, uint32_t, 
			   reiser4_item_hint_t *);
    
	/* Removes specified unit from the item. Returns released space */
	uint16_t (*remove) (item_entity_t *, uint32_t);

	/* Reads passed amount of units from the item. */
	errno_t (*fetch) (item_entity_t *, uint32_t,
			   void *, uint32_t);

	/* Updates passed amount of units in the item */
	errno_t (*update) (item_entity_t *, uint32_t,
			   void *, uint32_t);

	/* Estimates item */
	errno_t (*estimate) (item_entity_t *, uint32_t, 
			     reiser4_item_hint_t *);
    
	/* Checks item for validness */
	errno_t (*valid) (item_entity_t *);

	/* Makes lookup for passed key */
	int (*lookup) (item_entity_t *, reiser4_key_t *, 
		       uint32_t *);

	/* Performs shift of units from passed @src item to @dst item */
	int (*shift) (item_entity_t *, item_entity_t *,
		      uint32_t *, shift_flags_t);
	
	/* Prints item into specified buffer */
	errno_t (*print) (item_entity_t *, aal_stream_t *, uint16_t);

	/* Get the max key which could be stored in the item of this type */
	errno_t (*max_poss_key) (item_entity_t *, reiser4_key_t *);
 
	/* Get the max real key which is stored in the item */
	errno_t (*max_real_key) (item_entity_t *, reiser4_key_t *);
    
	/* Returns unit count */
	uint32_t (*count) (item_entity_t *);

	/* Checks the item structure. */
	errno_t (*check) (item_entity_t *);
};

typedef struct reiser4_item_ops reiser4_item_ops_t;

/* Stat data extention plugin */
struct reiser4_sdext_ops {
	reiser4_plugin_header_t h;

	/* Initialize stat data extention data at passed pointer */
	errno_t (*init) (reiser4_body_t *, void *);

	/* Reads stat data extention data */
	errno_t (*open) (reiser4_body_t *, void *);

	/* Prints stat data extention data into passed buffer */
	errno_t (*print) (reiser4_body_t *, aal_stream_t *, uint16_t);

	/* Returns length of the extention */
	uint16_t (*length) (reiser4_body_t *);
};

typedef struct reiser4_sdext_ops reiser4_sdext_ops_t;

/*
  Node plugin operates on passed block. It doesn't any initialization, so it
  hasn't close method and all its methods accepts first argument aal_block_t,
  not initialized previously hypothetic instance of node.
*/
struct reiser4_node_ops {
	reiser4_plugin_header_t h;

	/* 
	   Forms empty node incorresponding to given level in specified block.
	   Initializes instance of node and returns it to caller.
	*/
	object_entity_t *(*create) (aal_device_t *, blk_t, uint8_t);

	/* 
	   Opens node (parses data in orser to check whether it is valid for this
	   node type), initializes instance and returns it to caller.
	*/
	object_entity_t *(*open) (aal_device_t *, blk_t);

	/* 
	   Finalizes work with node (compresses data back) and frees all memory.
	   Returns the error code to caller.
	*/
	errno_t (*close) (object_entity_t *);

	/* Saves node onto device */
	errno_t (*sync) (object_entity_t *);
	
	/* 
	   Performs shift of items and units. Returns 1 if move point shifted to
	   passed node, 0 if not shifted and -1 in the case of error.
	*/
	int (*shift) (object_entity_t *, object_entity_t *, 
		      reiser4_pos_t *pos, shift_flags_t);
    
	/* Confirms that given block contains valid node of requested format */
	int (*confirm) (object_entity_t *);

	/*	Checks thoroughly the node structure and fixes what needed. */
	errno_t (*check) (object_entity_t *);

	/* Check node on validness */
	errno_t (*valid) (object_entity_t *);

	/* Prints node into given buffer */
	errno_t (*print) (object_entity_t *, aal_stream_t *, uint16_t);
    
	/* Returns item count */
	uint16_t (*count) (object_entity_t *);
    
	/* Returns item's overhead */
	uint16_t (*overhead) (object_entity_t *);

	/* Returns item's max size */
	uint16_t (*maxspace) (object_entity_t *);
    
	/* Returns free space in the node */
	uint16_t (*space) (object_entity_t *);

	/* Gets node's plugin id */
	uint16_t (*pid) (object_entity_t *);
    
	/* 
	   Makes lookup inside node by specified key. Returns TRUE in the case
	   exact match was found and FALSE otherwise.
	*/
	int (*lookup) (object_entity_t *, reiser4_key_t *, 
		       reiser4_pos_t *);
    
	/* Inserts item at specified pos */
	errno_t (*insert) (object_entity_t *, reiser4_pos_t *, 
			   reiser4_item_hint_t *);
    
	/* Removes item at specified pos */
	errno_t (*remove) (object_entity_t *, reiser4_pos_t *);
    
	/* Pastes units at specified pos */
	errno_t (*paste) (object_entity_t *, reiser4_pos_t *, 
			  reiser4_item_hint_t *);
    
	/* Removes unit at specified pos */
	errno_t (*cut) (object_entity_t *, reiser4_pos_t *);
    
	/* Gets/sets key at pos */
	errno_t (*get_key) (object_entity_t *, reiser4_pos_t *, 
			    reiser4_key_t *);
    
	errno_t (*set_key) (object_entity_t *, reiser4_pos_t *, 
			    reiser4_key_t *);

	/* Gets/sets node level */
	uint8_t (*get_level) (object_entity_t *);
	errno_t (*set_level) (object_entity_t *, uint8_t);
    
	/* Gets/sets node mkfs stamp */
	uint32_t (*get_stamp) (object_entity_t *);
	errno_t (*set_stamp) (object_entity_t *, uint32_t);
    
	/* Gets item at passed pos */
	reiser4_body_t *(*item_body) (object_entity_t *, reiser4_pos_t *);

	/* Returns item's length by pos */
	uint16_t (*item_len) (object_entity_t *, reiser4_pos_t *);
    
	/* Gets/sets node's plugin ID */
	uint16_t (*item_pid) (object_entity_t *, reiser4_pos_t *);

	/* Constrain on the item type. */
	errno_t (*item_legal) (object_entity_t *, reiser4_plugin_t *);
};

typedef struct reiser4_node_ops reiser4_node_ops_t;

struct reiser4_hash_ops {
	reiser4_plugin_header_t h;
	uint64_t (*build) (const unsigned char *, uint32_t);
};

typedef struct reiser4_hash_ops reiser4_hash_ops_t;

struct reiser4_tail_ops {
	reiser4_plugin_header_t h;
};

typedef struct reiser4_tail_ops reiser4_tail_ops_t;

struct reiser4_hook_ops {
	reiser4_plugin_header_t h;
};

typedef struct reiser4_hook_ops reiser4_hook_ops_t;

struct reiser4_perm_ops {
	reiser4_plugin_header_t h;
};

typedef struct reiser4_perm_ops reiser4_perm_ops_t;

/* Disk-format plugin */
struct reiser4_format_ops {
	reiser4_plugin_header_t h;
    
	/* 
	   Called during filesystem opening (mounting). It reads format-specific
	   super block and initializes plugins suitable for this format.
	*/
	object_entity_t *(*open) (aal_device_t *);
    
	/* 
	   Called during filesystem creating. It forms format-specific super
	   block, initializes plugins and calls their create method.
	*/
	object_entity_t *(*create) (aal_device_t *, uint64_t, uint16_t);
    
	/* Returns the device disk-format lies on */
	aal_device_t *(*device) (object_entity_t *);
    
	/*
	  Called during filesystem syncing. It calls method sync for every
	  "child" plugin (block allocator, journal, etc).
	*/
	errno_t (*sync) (object_entity_t *);

	/*
	  Checks format-specific super block for validness. Also checks whether
	  filesystem objects lie in valid places. For example, format-specific
	  super block for format40 must lie in 17-th block for 4096 byte long
	  blocks.
	*/
	errno_t (*valid) (object_entity_t *);
    
	/* Checks thoroughly the format structure and fixes what needed. */
	errno_t (*check) (object_entity_t *);

	/* Prints all useful information about the format */
	errno_t (*print) (object_entity_t *, aal_stream_t *, uint16_t);
    
	/*
	  Probes whether filesystem on given device has this format. Returns
	  "true" if so and "false" otherwise.
	*/
	int (*confirm) (aal_device_t *device);

	/*
	  Closes opened or created previously filesystem. Frees all assosiated
	  memory.
	*/
	void (*close) (object_entity_t *);
    
	/*
	  Returns format string for this format. For example "reiserfs 4.0".
	*/
	const char *(*name) (object_entity_t *);

	/* Gets the start of the filesystem. */
	uint64_t (*start) (object_entity_t *);
	
	/* Gets/sets root block */
	uint64_t (*get_root) (object_entity_t *);
	void (*set_root) (object_entity_t *, uint64_t);
    
	/* Gets/sets block count */
	uint64_t (*get_len) (object_entity_t *);
	void (*set_len) (object_entity_t *, uint64_t);
    
	/* Gets/sets height field */
	uint16_t (*get_height) (object_entity_t *);
	void (*set_height) (object_entity_t *, uint16_t);
    
	/* Gets/sets free blocks number for this format */
	uint64_t (*get_free) (object_entity_t *);
	void (*set_free) (object_entity_t *, uint64_t);
    
	/* Gets/sets free blocks number for this format */
	uint32_t (*get_stamp) (object_entity_t *);
	void (*set_stamp) (object_entity_t *, uint32_t);
    
	/* Returns children objects plugins */
	rpid_t (*journal_pid) (object_entity_t *);
	rpid_t (*alloc_pid) (object_entity_t *);
	rpid_t (*oid_pid) (object_entity_t *);

	/* Returns area where oid data lies */
	void (*oid_area)(object_entity_t *, void **, uint32_t *);

	/* The set of methods for going through format blocks */
	format_layout_func_t layout;
	format_layout_func_t skipped_layout;
	format_layout_func_t format_layout;
	format_layout_func_t alloc_layout;
	format_layout_func_t journal_layout;
};

typedef struct reiser4_format_ops reiser4_format_ops_t;

struct reiser4_oid_ops {
	reiser4_plugin_header_t h;

	/* Opens oid allocator on passed area */
	object_entity_t *(*open) (const void *, uint32_t);

	/* Creates oid allocator on passed area */
	object_entity_t *(*create) (const void *, uint32_t);

	/* Closes passed instance of oid allocator */
	void (*close) (object_entity_t *);
    
	/* Synchronizes oid allocator */
	errno_t (*sync) (object_entity_t *);

	/* Makes check for validness */
	errno_t (*valid) (object_entity_t *);
    
	/* Gets next object id */
	roid_t (*next) (object_entity_t *);

	/* Gets next object id */
	roid_t (*allocate) (object_entity_t *);

	/* Releases passed object id */
	void (*release) (object_entity_t *, roid_t);
    
	/* Returns the number of used object ids */
	uint64_t (*used) (object_entity_t *);
    
	/* Returns the number of free object ids */
	uint64_t (*free) (object_entity_t *);

	/* Prints oid allocator data */
	errno_t (*print) (object_entity_t *, aal_stream_t *, uint16_t);

	/* Object ids of root and root parenr object */
	roid_t (*root_parent_locality) (void);
	roid_t (*root_locality) (void);
	roid_t (*root_objectid) (void);
};

typedef struct reiser4_oid_ops reiser4_oid_ops_t;

struct reiser4_alloc_ops {
	reiser4_plugin_header_t h;
    
	/* Opens block allocator */
	object_entity_t *(*open) (object_entity_t *, uint64_t);

	/* Creates block allocator */
	object_entity_t *(*create) (object_entity_t *, uint64_t);
    
	/* Closes blcok allocator */
	void (*close) (object_entity_t *);

	/* Synchronizes block allocator */
	errno_t (*sync) (object_entity_t *);

	/* Marks passed block as used */
	void (*mark) (object_entity_t *, uint64_t);

	/* Checks if passed block used */
	int (*test) (object_entity_t *, uint64_t);
    
	/* Allocates one block */
	uint64_t (*allocate) (object_entity_t *);

	/* Deallocates passed block */
	void (*release) (object_entity_t *, uint64_t);

	/* Returns number of used blocks */
	uint64_t (*used) (object_entity_t *);

	/* Returns number of unused blocks */
	uint64_t (*free) (object_entity_t *);

	/* Checks blocks allocator on validness */
	errno_t (*valid) (object_entity_t *);

	/* Prints block allocator data */
	errno_t (*print) (object_entity_t *, aal_stream_t *, uint16_t);
};

typedef struct reiser4_alloc_ops reiser4_alloc_ops_t;

struct reiser4_journal_ops {
	reiser4_plugin_header_t h;
    
	/* Opens journal on specified device */
	object_entity_t *(*open) (object_entity_t *);

	/* Creates journal on specified device */
	object_entity_t *(*create) (object_entity_t *, void *);

	/* Returns the device journal lies on */
	aal_device_t *(*device) (object_entity_t *);
    
	/* Frees journal instance */
	void (*close) (object_entity_t *);

	/* Checks journal metadata on validness */
	errno_t (*valid) (object_entity_t *);
    
	/* Synchronizes journal */
	errno_t (*sync) (object_entity_t *);

	/* Replays journal. Returns the number of replayed transactions. */
	errno_t (*replay) (object_entity_t *);

	/* Prints journal content */
	errno_t (*print) (object_entity_t *, aal_stream_t *, uint16_t);
	
	/* Checks thoroughly the journal structure. */
	errno_t (*check) (object_entity_t *);
};

typedef struct reiser4_journal_ops reiser4_journal_ops_t;

union reiser4_plugin {
	reiser4_plugin_header_t h;
	
	reiser4_file_ops_t file_ops;
	reiser4_item_ops_t item_ops;
	reiser4_node_ops_t node_ops;
	reiser4_hash_ops_t hash_ops;
	reiser4_tail_ops_t tail_ops;
	reiser4_hook_ops_t hook_ops;
	reiser4_perm_ops_t perm_ops;
	reiser4_format_ops_t format_ops;
	reiser4_oid_ops_t oid_ops;
	reiser4_alloc_ops_t alloc_ops;
	reiser4_journal_ops_t journal_ops;
	reiser4_key_ops_t key_ops;
	reiser4_sdext_ops_t sdext_ops;

	/* User-specific data */
	void *data;
};

struct reiser4_place {
	void *data;
	int context;

	reiser4_pos_t pos;
	item_entity_t entity;
};

/* The common node header */
struct reiser4_node_header {
	d16_t pid;
};

typedef struct reiser4_node_header reiser4_node_header_t;

struct reiser4_level {
	uint8_t top, bottom;
};

typedef struct reiser4_level reiser4_level_t;

/* 
   This structure is passed to all plugins in initialization time and used for
   access libreiser4 factories.
*/
struct reiser4_core {
    
	struct {
	
		/* Finds plugin by its attribues (type and id) */
		reiser4_plugin_t *(*ifind)(rpid_t, rpid_t);
	
		/* Finds plugin by its type and name */
		reiser4_plugin_t *(*nfind)(rpid_t, const char *);
	
	} factory_ops;
    
	struct {
		/* Returns blocksize in passed tree */
		uint32_t (*blockspace) (const void *);
	
		/* Returns maximal available space in a node */
		uint32_t (*nodespace) (const void *);
	
		/*
		  Makes lookup in the tree in order to know where say stat data
		  item of a file realy lies. It is used in all object plugins.
		*/
		int (*lookup) (const void *, reiser4_key_t *, reiser4_level_t *,
			       reiser4_place_t *);

		/* 
		   Inserts item/unit in the tree by calling reiser4_tree_insert
		   function, used by all object plugins (dir, file, etc)
		*/
		errno_t (*insert)(const void *, reiser4_item_hint_t *, uint8_t, 
				  reiser4_place_t *);
    
		/*
		  Removes item/unit from the tree. It is used in all object
		  plugins for modification purposes.
		*/
		errno_t (*remove)(const void *, reiser4_key_t *, uint8_t);
	
		/* Returns right and left neighbour respectively */
		errno_t (*right) (const void *, reiser4_place_t *, reiser4_place_t *);
		errno_t (*left) (const void *, reiser4_place_t *, reiser4_place_t *);

		errno_t (*lock) (const void *, reiser4_place_t *);
		errno_t (*unlock) (const void *, reiser4_place_t *);
	} tree_ops;

	struct {
		errno_t (*open) (item_entity_t *, object_entity_t *, reiser4_pos_t *);
	} item_ops;
};

/* Plugin functions and macros */
#ifndef ENABLE_COMPACT

/*
  Macro for calling a plugin function. It checks if function is implemented and
  then calls it. In the case it is not implemented, the exception will be thown
  out and @action will be performed.
*/
#define plugin_call(action, ops, method, args...)	                    \
    ({								            \
	    if (!ops.method) {					            \
	        aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_OK,	    \
		        "Method \"" #method "\" isn't implemented in %s.",  \
		        #ops);						    \
	        action;						            \
	    }							            \
	    ops.method(args);					            \
    })

#else

#define plugin_call(action, ops, method, args...)                           \
    ops.method(args)
    
#endif

/*
  Macro for registering a plugin in plugin factory. It accepts two pointers to
  functions. The first one is pointer to plugin init function and second - to
  plugin finalization function. The idea the same as in the linux kernel module
  support.

  This macro installs passed @init and @fini routines to special purposes ELF
  section in called by us ".plugins" in the case of monolithic bulding. In the
  case plugin is builing as dynamic library, macro install just wto symbols:
  __plugin_init and __plugin_fini, which may be accepted durring plugin init by
  means of using dl* functions.
*/
#if defined(ENABLE_COMPACT) || defined(ENABLE_MONOLITHIC)
#define plugin_register(init, fini)				            \
    static reiser4_plugin_init_t __plugin_init		                    \
	    __attribute__((__section__(".plugins"))) = init;                \
                                                                            \
    static reiser4_plugin_fini_t __plugin_fini		                    \
	    __attribute__((__section__(".plugins"))) = fini
#else
#define plugin_register(init, fini)					    \
    reiser4_plugin_init_t __plugin_init = init;                             \
    reiser4_plugin_fini_t __plugin_fini = fini
#endif

#endif

