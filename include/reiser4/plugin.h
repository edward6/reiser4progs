/*
  plugin.h -- reiser4 plugin known types and macros.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef REISER4_PLUGIN_H
#define REISER4_PLUGIN_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>

#define TMAX_LEVEL              5
#define LEAF_LEVEL	        1
#define TWIG_LEVEL	        (LEAF_LEVEL + 1)

#define MASTER_MAGIC	        ("R4Sb")
#define MASTER_OFFSET	        (65536)
#define BLOCKSIZE               (4096)

/* 
  Defining the types for disk structures. All types like f32_t are fake ones
  needed to avoid gcc-2.95.x bug with typedef of aligned types.
*/
typedef uint8_t  f8_t;  typedef f8_t  d8_t  __attribute__((aligned(1)));
typedef uint16_t f16_t; typedef f16_t d16_t __attribute__((aligned(2)));
typedef uint32_t f32_t; typedef f32_t d32_t __attribute__((aligned(4)));
typedef uint64_t f64_t; typedef f64_t d64_t __attribute__((aligned(8)));

/* Basic reiser4 types used in library and plugins */
typedef void rbody_t;
typedef uint64_t roid_t;
typedef uint16_t rpid_t;

struct rpos {
	uint32_t item;
	uint32_t unit;
};

typedef struct rpos rpos_t;

#define POS_INIT(p, i, u) \
        (p)->item = i, (p)->unit = u

enum reiser4_plugin_type {
	FILE_PLUGIN_TYPE        = 0x0,

	/*
	  In reiser4 kernel code DIR_PLUGIN_TYPE exists, but libreiser4 works
	  with files and directories by the unified interface and do not need an
	  additional type for that. But we have to be compatible, because
	  reiser4_plugin_type may be stored in stat data extentions. So, next
	  type has 0x2 identifier, not 0x1 one.
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
	LAST_ITEM
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

typedef enum reiser4_hash_plugin_id reiser4_hash_plugin_id_t;

enum reiser4_tail_plugin_id {
	TAIL_NEVER_ID		= 0x0,
	TAIL_SUPPRESS_ID	= 0x1,
	TAIL_FOURK_ID		= 0x2,
	TAIL_ALWAYS_ID		= 0x3,
	TAIL_SMART_ID		= 0x4,
	TAIL_LAST_ID		= 0x5
};

enum reiser4_perm_plugin_id {
	PERM_RWX_ID		= 0x0
};

enum reiser4_sdext_plugin_id {
	SDEXT_LW_ID	        = 0x0,
	SDEXT_UNIX_ID		= 0x1,
	SDEXT_LT_ID             = 0x2,
	SDEXT_SYMLINK_ID	= 0x3,
	SDEXT_PLUGIN_ID		= 0x4,
	SDEXT_GEN_FLAGS_ID      = 0x5,
	SDEXT_CAPS_ID           = 0x6,
	SDEXT_LARGE_TIMES_ID    = 0x7,
	SDEXT_LAST
};

enum reiser4_format_plugin_id {
	FORMAT_REISER40_ID	= 0x0,
	FORMAT_REISER36_ID	= 0x1
};

enum reiser4_oid_plugin_id {
	OID_REISER40_ID		= 0x0,
	OID_REISER36_ID		= 0x1
};

enum reiser4_alloc_plugin_id {
	ALLOC_REISER40_ID	= 0x0,
	ALLOC_REISER36_ID	= 0x1
};

enum reiser4_journal_plugin_id {
	JOURNAL_REISER40_ID	= 0x0,
	JOURNAL_REISER36_ID	= 0x1
};

enum reiser4_key_plugin_id {
	KEY_REISER40_ID		= 0x0,
	KEY_REISER36_ID		= 0x1
};

typedef union reiser4_plugin reiser4_plugin_t;

#define PRESENT                 0x1
#define ABSENT                  0x0
#define FAILED                  -1

#define INVAL_PID               0xffff

/* Maximal possible key size in 8 byte elements */
#define KEY_SIZE 3

struct key_entity {
	reiser4_plugin_t *plugin;
	uint64_t body[KEY_SIZE];
};

typedef struct key_entity key_entity_t;

enum key_type {
	KEY_FILENAME_TYPE       = 0x0,
	KEY_STATDATA_TYPE       = 0x1,
	KEY_ATTRNAME_TYPE       = 0x2,
	KEY_ATTRBODY_TYPE       = 0x3,
	KEY_FILEBODY_TYPE       = 0x4,
	KEY_LAST_TYPE
};

typedef enum key_type key_type_t;

/*
  Type for describing reiser4 objects (like node, block allocator, etc) inside
  the library, created by plugins themselves and which also have the our plugin
  referrence.
*/
struct object_entity {
	reiser4_plugin_t *plugin;
};

typedef struct object_entity object_entity_t;

struct item_context {

	/* Block number of the node item lies in */
	blk_t blk;

	/* Device item's node lies on */
	aal_device_t *device;
};

typedef struct item_context item_context_t;

/*
  Item enviromnent which contains referrences to block allocator and oid
  allocator.
*/
struct item_envirom {
	object_entity_t *oid;
	object_entity_t *alloc;
};

typedef struct item_envirom item_envirom_t;

/*
  Type for describing an item. The pointer of this type will be passed to the
  all item plugins.
*/
struct item_entity {
	reiser4_plugin_t *plugin;

	rpos_t pos;

	uint32_t len;
	rbody_t *body;
	
	key_entity_t key;

	item_context_t con;
	item_envirom_t env;
};

typedef struct item_entity item_entity_t;
typedef struct reiser4_place reiser4_place_t;

/* Shift flags control shift process */
enum shift_flags {

	/* Perform shift from the passed node to the left neighbour node */
	SF_LEFT	 = 1 << 0,

	/* Perform shift from the passed node to the right neighbour node */
	SF_RIGHT = 1 << 1,

	/*
	  Allows to move insert point to the corresponding neighbour node while
	  performing shift.
	*/
	SF_MOVIP = 1 << 2,

	/* Allows update insert point while performing shift */
	SF_UPTIP = 1 << 3,

	/*
	  Allows do not create new items while performing the shift of
	  units. Units from the source item may be moved into an item if the
	  items are mergeable.
	*/
	SF_MERGE = 1 << 4,
};

typedef enum shift_flags shift_flags_t;

struct shift_hint {
	/*
	  Flag which shows that we need create an item before we will move units
	  into it. That is because node does not contain any items at all or
	  border items are not mergeable.
	*/
	int create;

	/* Item count and unit count which will be moved out */
	uint32_t items;
	uint32_t units;

	/*
	  Bytes to be moved for items and units. Actually we might use just item
	  field for providing needed functionality, but I guess, we will need to
	  collect some statistics like how much items and units were moved
	  durring making space for inserting particular item or unit.
	*/
	uint32_t bytes;
	uint32_t rest;

	/*
	  Shift control flags (left shift, move insert point, merge, etc) and
	  shift result flags. The result flags are needed for determining for
	  example was insert point moved to the corresponding neighbour or
	  not. Of course we might use control flags for that, but it would be
	  led to write a lot of useless stuff for saving control flags before
	  shift, etc.
	*/
	uint32_t control;
	uint32_t result;

	/* Insert point. It will be modified durring shfiting */
	rpos_t pos;
};

typedef struct shift_hint shift_hint_t;

typedef errno_t (*region_func_t) (item_entity_t *, uint64_t, uint64_t, void *);
typedef errno_t (*block_func_t) (object_entity_t *, uint64_t, void *);
typedef errno_t (*place_func_t) (object_entity_t *, reiser4_place_t *, void *);

typedef errno_t (*layout_func_t) (void *, block_func_t, void *);
typedef errno_t (*metadata_func_t) (object_entity_t *, place_func_t, void *);

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

	/* Entry key, it may be found by */
	key_entity_t offset;

	/* The stat data key of the object entry points to */
	key_entity_t object;

	/* Name of entry */
	char name[256];
};

typedef struct reiser4_entry_hint reiser4_entry_hint_t;

struct reiser4_file_hint {
	rpid_t statdata;

	/* Hint for a file body */
	union {

		/* Plugin ids for the directory body */
		struct {
			rpid_t direntry;
			rpid_t hash;
		} dir;
	
		/* Plugin id for the regular file body */
		struct {
			rpid_t tail;
			rpid_t extent;
			rpid_t policy;
		} reg;

		/* Symlink data */
		char sym[4096];
		
	} body;
    
	key_entity_t object; 
	key_entity_t parent;
		
	/* The plugin in use */
	reiser4_plugin_t *plugin;
};

typedef struct reiser4_file_hint reiser4_file_hint_t;

/* 
  Create item or paste into item on the base of this structure. Here "data" is a
  pointer to data to be copied.
*/ 
struct reiser4_item_hint {
	/*
	  This is pointer to already formated item body. It is useful for item
	  copying, replacing, etc. This will be used by fsck probably.
	*/
	void *data;

	/* Length of the data field */
	uint16_t len;
    
	/*
	  This is pointer to hint which describes item. It is widely
	  used for creating an item.
	*/
	void *hint;
	uint16_t count;
    
	/* The key of item */
	key_entity_t key;

	/*
	  Item context and item enviromnent. They are used for access some
	  filesystem wide entities like block allocator durring item
	  estimating.
	*/
	item_context_t con;
	item_envirom_t env;
	
	/* Plugin to be used for working with item */
	reiser4_plugin_t *plugin;
};

typedef struct reiser4_item_hint reiser4_item_hint_t;

#define PLUGIN_MAX_LABEL	16
#define PLUGIN_MAX_DESC		256
#define PLUGIN_MAX_NAME		256

typedef void (*reiser4_abort_t) (char *);
typedef struct reiser4_core reiser4_core_t;

typedef errno_t (*reiser4_plugin_fini_t) (reiser4_core_t *);
typedef reiser4_plugin_t *(*reiser4_plugin_init_t) (reiser4_core_t *);

typedef errno_t (*reiser4_plugin_func_t) (reiser4_plugin_t *, void *);

#define empty_handle { "", NULL, NULL, NULL, NULL }

struct plugin_handle {
	char name[PLUGIN_MAX_NAME];
	void *data;
	
	reiser4_plugin_init_t init;
	reiser4_plugin_fini_t fini;
	
	reiser4_abort_t abort;
};

typedef struct plugin_handle plugin_handle_t;

/* Common plugin header */
struct reiser4_plugin_header {

	/*
	  Plugin handle. It is used for initializing and finalizing particular
	  plugin.
	*/
	plugin_handle_t handle;

	/* Plugin will be looked by its id, type, etc */
	rpid_t id;
	rpid_t type;
	rpid_t group;

	/* Label and description */
	const char label[PLUGIN_MAX_LABEL];
	const char desc[PLUGIN_MAX_DESC];
};

typedef struct reiser4_plugin_header reiser4_plugin_header_t;

struct reiser4_key_ops {
	reiser4_plugin_header_t h;

	/* 
	   Cleans key up. Actually it just memsets it by zeros, but more smart
	   behavior may be implemented.
	*/
	void (*clean) (key_entity_t *);

	/* Check of key structure */
	errno_t (*valid) (key_entity_t *);
    
	/* Confirms key format */
	int (*confirm) (key_entity_t *);

	/* Returns minimal key for this key-format */
	key_entity_t *(*minimal) (void);
    
	/* Returns maximal key for this key-format */
	key_entity_t *(*maximal) (void);
    
	/* Compares two keys by comparing its all components */
	int (*compare) (key_entity_t *, key_entity_t *);

	/* Copyies src key to dst one */
	errno_t (*assign) (key_entity_t *, key_entity_t *);

	/* Builds generic key (statdata, file body, etc) */
	errno_t (*build_generic) (key_entity_t *, key_type_t,
				  uint64_t, uint64_t, uint64_t);
    
	errno_t (*build_entry) (key_entity_t *, reiser4_plugin_t *,
				uint64_t, uint64_t, const char *);
	
	/* Gets/sets key type (minor in reiser4 notation) */	
	void (*set_type) (key_entity_t *, key_type_t);
	key_type_t (*get_type) (key_entity_t *);

	/* Gets/sets key locality */
	void (*set_locality) (key_entity_t *, uint64_t);
	uint64_t (*get_locality) (key_entity_t *);
    
	/* Gets/sets key objectid */
	void (*set_objectid) (key_entity_t *, uint64_t);
	uint64_t (*get_objectid) (key_entity_t *);

	/* Gets/sets key offset */
	void (*set_offset) (key_entity_t *, uint64_t);
	uint64_t (*get_offset) (key_entity_t *);

	/* Gets/sets directory key hash */
	void (*set_hash) (key_entity_t *, uint64_t);
	uint64_t (*get_hash) (key_entity_t *);

	/* Prints key into specified buffer */
	errno_t (*print) (key_entity_t *, aal_stream_t *,
			  uint16_t);
};

typedef struct reiser4_key_ops reiser4_key_ops_t;

struct reiser4_file_ops {
	reiser4_plugin_header_t h;

	/* Creates new file with passed parent and object keys */
	object_entity_t *(*create) (void *, reiser4_file_hint_t *); 
    
	/* Opens a file with specified key */
	object_entity_t *(*open) (void *, reiser4_place_t *);

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
	int (*lookup) (object_entity_t *, char *, key_entity_t *);

	/* Finds actual file stat data (symlink) */
	errno_t (*follow) (object_entity_t *, key_entity_t *);

	/* Reads the data from file to passed buffer */
	int32_t (*read) (object_entity_t *, void *, uint32_t);
    
	/* Writes the data to file from passed buffer */
	int32_t (*write) (object_entity_t *, void *, uint32_t);

	/* Truncates file to passed length */
	errno_t (*truncate) (object_entity_t *, uint64_t);

	/*
	  Function for going throught all metadata blocks specfied file
	  occupied.
	*/
	errno_t (*metadata) (object_entity_t *, place_func_t, void *);
	
	/*
	  Function for going throught the all data blocks specfied file
	  occupied.
	*/
	errno_t (*layout) (object_entity_t *, block_func_t, void *);
};

typedef struct reiser4_file_ops reiser4_file_ops_t;

struct reiser4_item_ops {
	reiser4_plugin_header_t h;

	/* Prepares item body for working with it */
	errno_t (*init) (item_entity_t *);

	/* Returns plugin file object item belongs to */
	reiser4_plugin_t *(*belongs) (item_entity_t *);
	
	/* Reads passed amount of units from the item. */
	int32_t (*read) (item_entity_t *, void *, uint32_t,
			 uint32_t);

	/* Updates passed amount of units in the item */
	int32_t (*write) (item_entity_t *, void *, uint32_t,
			  uint32_t);

	/* Inserts unit described by passed hint into the item */
	errno_t (*insert) (item_entity_t *, void *, uint32_t,
			   uint32_t);
	
	/* Removes specified unit from the item. Returns released space */
	int32_t (*remove) (item_entity_t *, uint32_t,
			   uint32_t);
	
	/*
	  Estimates item in order to find out how many bytes is needed for
	  inserting one more unit.
	*/
	errno_t (*estimate) (item_entity_t *, void *, uint32_t,
			     uint32_t);
    
	/* Checks item for validness */
	errno_t (*valid) (item_entity_t *);

	/* Checks the item structure. */
	errno_t (*check) (item_entity_t *);
	
	/* Returns unit count */
	uint32_t (*units) (item_entity_t *);

	/* Makes lookup for passed key */
	int (*lookup) (item_entity_t *, key_entity_t *, uint32_t *);

	/* Performs shift of units from passed @src item to @dst item */
	errno_t (*shift) (item_entity_t *, item_entity_t *,
			  shift_hint_t *);

	/* Predicts the shift parameters (units, bytes, etc) */
	errno_t (*predict) (item_entity_t *, item_entity_t *,
			    shift_hint_t *);
	
	/* Prints item into specified buffer */
	errno_t (*print) (item_entity_t *, aal_stream_t *, uint16_t);

	/* Checks if items mergeable. Returns 1 if so, 0 otherwise */
	int (*mergeable) (item_entity_t *, item_entity_t *);

	/* Goes through all blocks item points to. */
	errno_t (*layout) (item_entity_t *, region_func_t, void *);

	/*
	  Returns TRUE is specified item is a nodeptr one. That is, it points to
	  formatted node in the tree. If this method if not implemented, then
	  item is assumed as not nodeptr one. All tree running operations like
	  going from the root to leaves will use this function.
	*/
	int (*branch) (item_entity_t *);
	
	/* Does some specific actions if a block the item points to is wrong. */
	/* FIXME: I wish it to be joint with layout, but how? */
	int32_t (*layout_check) (item_entity_t *, region_func_t, void *);

	/* Get the key of a particular unit of the item. */
	errno_t (*get_key) (item_entity_t *, uint32_t, key_entity_t *);

	/* Set the key of a particular unit of the item. */
	errno_t (*set_key) (item_entity_t *, uint32_t, key_entity_t *);
	
	/* Get the max key which could be stored in the item of this type */
	errno_t (*maxposs_key) (item_entity_t *, key_entity_t *);
 
	/* Get the max real key which is stored in the item */
	errno_t (*utmost_key) (item_entity_t *, key_entity_t *);

	/*
	  Get the max real key stored continously from the key specified in the
	  item entity.
	*/
	errno_t (*gap_key) (item_entity_t *, key_entity_t *);
};

typedef struct reiser4_item_ops reiser4_item_ops_t;

/* Stat data extention plugin */
struct reiser4_sdext_ops {
	reiser4_plugin_header_t h;

	/* Initialize stat data extention data at passed pointer */
	errno_t (*init) (rbody_t *, void *);

	/* Reads stat data extention data */
	errno_t (*open) (rbody_t *, void *);

	/* Prints stat data extention data into passed buffer */
	errno_t (*print) (rbody_t *, aal_stream_t *, uint16_t);

	/* Returns length of the extention */
	uint16_t (*length) (rbody_t *);
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
	
	/* Performs shift of items and units */
	errno_t (*shift) (object_entity_t *, object_entity_t *, 
			  shift_hint_t *);
    
	/* Confirms that given block contains valid node of requested format */
	int (*confirm) (object_entity_t *);

	/*	Checks thoroughly the node structure and fixes what needed. */
	errno_t (*check) (object_entity_t *);

	/* Check node on validness */
	errno_t (*valid) (object_entity_t *);

	/* Prints node into given buffer */
	errno_t (*print) (object_entity_t *, aal_stream_t *,
			  uint16_t);
    
	/* Returns item count */
	uint16_t (*items) (object_entity_t *);
    
	/* Returns item's overhead */
	uint16_t (*overhead) (object_entity_t *);

	/* Returns item's max size */
	uint16_t (*maxspace) (object_entity_t *);
    
	/* Returns free space in the node */
	uint16_t (*space) (object_entity_t *);

	/* 
	   Makes lookup inside node by specified key. Returns TRUE in the case
	   exact match was found and FALSE otherwise.
	*/
	int (*lookup) (object_entity_t *, key_entity_t *, 
		       rpos_t *);
    
	/* Inserts item at specified pos */
	errno_t (*insert) (object_entity_t *, rpos_t *,
			   reiser4_item_hint_t *);
    
	/* Removes item/unit at specified pos */
	errno_t (*remove) (object_entity_t *, rpos_t *, uint32_t);

	/* Removes some amount of items/units */
	errno_t (*cut) (object_entity_t *, rpos_t *, rpos_t *);
    
	/* Shrinks node without calling any item methods */
	errno_t (*shrink) (object_entity_t *, rpos_t *,
			   uint32_t, uint32_t);

	errno_t (*copy) (object_entity_t *, rpos_t *,
			 object_entity_t *, rpos_t *, uint32_t);
	
	/* Expands node */
	errno_t (*expand) (object_entity_t *, rpos_t *,
			   uint32_t, uint32_t);
	
	/* Gets/sets key at pos */
	errno_t (*get_key) (object_entity_t *, rpos_t *,
			    key_entity_t *);
    
	errno_t (*set_key) (object_entity_t *, rpos_t *,
			    key_entity_t *);

	/* Gets/sets node level */
	uint8_t (*get_level) (object_entity_t *);
	void (*set_level) (object_entity_t *, uint8_t);
    
	/* Gets/sets node make and flush stamps */
	uint32_t (*get_mstamp) (object_entity_t *);
	void (*set_mstamp) (object_entity_t *, uint32_t);
	
    	uint64_t (*get_fstamp) (object_entity_t *);
	void (*set_fstamp) (object_entity_t *, uint64_t);

	/* Gets item at passed pos */
	rbody_t *(*item_body) (object_entity_t *, rpos_t *);

	/* Returns item's length by pos */
	uint16_t (*item_len) (object_entity_t *, rpos_t *);
    
	/* Gets/sets node's plugin ID */
	uint16_t (*item_pid) (object_entity_t *, rpos_t *);

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
	void (*oid) (object_entity_t *, void **, uint32_t *);

	errno_t (*layout) (object_entity_t *, block_func_t, void *);
	errno_t (*skipped) (object_entity_t *, block_func_t, void *);
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
	errno_t (*print) (object_entity_t *, aal_stream_t *,
			  uint16_t);

	/* Object ids of root and root parenr object */
	roid_t (*hyper_locality) (void);
	roid_t (*root_locality) (void);
	roid_t (*root_objectid) (void);
};

typedef struct reiser4_oid_ops reiser4_oid_ops_t;

struct reiser4_alloc_ops {
	reiser4_plugin_header_t h;
    
	/* Opens block allocator */
	object_entity_t *(*open) (aal_device_t *, uint64_t);

	/* Creates block allocator */
	object_entity_t *(*create) (aal_device_t *, uint64_t);
    
	/* Assign the bitmap to the block allocator. */
	errno_t (*assign) (object_entity_t *, void *);

	/* Closes blcok allocator */
	void (*close) (object_entity_t *);

	/* Synchronizes block allocator */
	errno_t (*sync) (object_entity_t *);

	/* Returns number of used blocks */
	uint64_t (*used) (object_entity_t *);

	/* Returns number of unused blocks */
	uint64_t (*unused) (object_entity_t *);

	/* Checks blocks allocator on validness */
	errno_t (*valid) (object_entity_t *);

	/* Prints block allocator data */
	errno_t (*print) (object_entity_t *, aal_stream_t *,
			  uint16_t);

	/* Calls func for each block in block allocator */
	errno_t (*layout) (object_entity_t *, block_func_t, void *);
	
	/* Checks if passed range of blocks used */
	int (*used_region) (object_entity_t *, uint64_t,
			    uint64_t);
    	
	/* Checks if passed range of blocks unused */
	int (*unused_region) (object_entity_t *, uint64_t,
			      uint64_t);

	/* Marks passed block as used */
	errno_t (*occupy_region) (object_entity_t *, uint64_t,
				  uint64_t);

	/* Tries to allocate passed amount of blocks */
	uint64_t (*allocate_region) (object_entity_t *, uint64_t *,
				     uint64_t);
	
	/* Deallocates passed blocks */
	errno_t (*release_region) (object_entity_t *, uint64_t,
				   uint64_t);

	/* Calls func for all block of the same area as blk is. */
	errno_t (*related_region) (object_entity_t *, blk_t,
				   block_func_t, void *);
};

typedef struct reiser4_alloc_ops reiser4_alloc_ops_t;

struct reiser4_journal_ops {
	reiser4_plugin_header_t h;
    
	/* Opens journal on specified device */
	object_entity_t *(*open) (object_entity_t *, aal_device_t *,
				  uint64_t, uint64_t);

	/* Creates journal on specified device */
	object_entity_t *(*create) (object_entity_t *, aal_device_t *,
				    uint64_t, uint64_t, void *);

	/* Returns the device journal lies on */
	aal_device_t *(*device) (object_entity_t *);
    
	/* Frees journal instance */
	void (*close) (object_entity_t *);

	/* Checks journal metadata on validness */
	errno_t (*valid) (object_entity_t *);
    
	/* Synchronizes journal */
	errno_t (*sync) (object_entity_t *);

	/* Replays the journal */
	errno_t (*replay) (object_entity_t *);

	/* Prints journal content */
	errno_t (*print) (object_entity_t *, aal_stream_t *,
			  uint16_t);
	
	/* Checks thoroughly the journal structure. */
	errno_t (*check) (object_entity_t *, layout_func_t, void *);

	/* Calls func for each block in block allocator */
	errno_t (*layout) (object_entity_t *, block_func_t,
			   void *);
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

	reiser4_sdext_ops_t sdext_ops;
	reiser4_alloc_ops_t alloc_ops;
	reiser4_format_ops_t format_ops;
	reiser4_journal_ops_t journal_ops;

	reiser4_oid_ops_t oid_ops;
	reiser4_key_ops_t key_ops;

	/* User-specific data */
	void *data;
};

struct reiser4_place {
	void *node;
	rpos_t pos;
	item_entity_t item;
};

/* The common node header */
struct node_header {
	d16_t pid;
};

typedef struct node_header node_header_t;

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
		uint32_t (*blockspace) (void *);
	
		/* Returns maximal available space in a node */
		uint32_t (*nodespace) (void *);

		/* Gets root key */
		errno_t (*rootkey) (void *, key_entity_t *);
	
		/*
		  Makes lookup in the tree in order to know where say stat data
		  item of a file realy lies. It is used in all object plugins.
		*/
		int (*lookup) (void *, key_entity_t *, uint8_t,
			       reiser4_place_t *);

		/* Initializes all item fields in passed place */
		errno_t (*realize) (void *, reiser4_place_t *);
		
		/* 
		   Inserts item/unit in the tree by calling reiser4_tree_insert
		   function, used by all object plugins (dir, file, etc)
		*/
		errno_t (*insert)(void *, reiser4_place_t *, reiser4_item_hint_t *);
    
		/*
		  Removes item/unit from the tree. It is used in all object
		  plugins for modification purposes.
		*/
		errno_t (*remove)(void *, reiser4_place_t *, uint32_t);
	
		/* Returns right and left neighbour respectively */
		errno_t (*right) (void *, reiser4_place_t *, reiser4_place_t *);
		errno_t (*left) (void *, reiser4_place_t *, reiser4_place_t *);

		errno_t (*lock) (void *, reiser4_place_t *);
		errno_t (*unlock) (void *, reiser4_place_t *);
	} tree_ops;
};

#define plugin_equal(plugin1, plugin2)                         \
        (plugin1->h.group == plugin2->h.group &&               \
	 plugin1->h.id == plugin2->h.id)


/*
  Macro for calling a plugin function. It checks if function is implemented and
  then calls it. In the case it is not implemented, abort handler will be called
*/
#define plugin_call(ops, method, args...) ({                    \
        if (!ops.method && ops.h.handle.abort)                  \
               ops.h.handle.abort("Method \""#method"\" isn't " \
				  "implemented in "#ops".");    \
        ops.method(args);				        \
})

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
#if defined(ENABLE_ALONE) || defined(ENABLE_MONOLITHIC)

#define plugin_register(init, fini)			       \
    static reiser4_plugin_init_t __plugin_init		       \
	    __attribute__((__section__(".plugins"))) = init;   \
                                                               \
    static reiser4_plugin_fini_t __plugin_fini		       \
	    __attribute__((__section__(".plugins"))) = fini
#else

#define plugin_register(init, fini)			       \
    reiser4_plugin_init_t __plugin_init = init;                \
    reiser4_plugin_fini_t __plugin_fini = fini

#endif

#endif

