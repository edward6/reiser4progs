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

#define LEAF_LEVEL	        1
#define TWIG_LEVEL	        (LEAF_LEVEL + 1)

#define REISER4_SECSIZE         (512)
#define REISER4_BLKSIZE         (4096)

#define REISER4_MASTER_MAGIC	("R4Sb")
#define REISER4_MASTER_OFFSET	(65536)

#define REISER4_ROOT_LOCALITY   (0x29)
#define REISER4_ROOT_OBJECTID   (0x2a)

/* 
  Defining the types for disk structures. All types like f32_t are fake ones
  needed to avoid gcc-2.95.x bug with typedef of aligned types.
*/
typedef uint8_t  f8_t;  typedef f8_t  d8_t  __attribute__((aligned(1)));
typedef uint16_t f16_t; typedef f16_t d16_t __attribute__((aligned(2)));
typedef uint32_t f32_t; typedef f32_t d32_t __attribute__((aligned(4)));
typedef uint64_t f64_t; typedef f64_t d64_t __attribute__((aligned(8)));

/* Basic reiser4 types used in library and plugins */
typedef void body_t;
typedef uint16_t rid_t;
typedef uint64_t oid_t;

struct pos {
	uint32_t item;
	uint32_t unit;
};

typedef struct pos pos_t;

/* Lookup return values */
enum lookup {
	PRESENT                 = 1,
	ABSENT                  = 0,
	FAILED                  = -1
};

typedef enum lookup lookup_t;

#define POS_INIT(p, i, u) \
        (p)->item = i, (p)->unit = u

enum reiser4_plugin_type {
	OBJECT_PLUGIN_TYPE      = 0x0,
	ITEM_PLUGIN_TYPE        = 0x2,
	NODE_PLUGIN_TYPE        = 0x3,
	HASH_PLUGIN_TYPE        = 0x4,
	TAIL_PLUGIN_TYPE        = 0x5,
	PERM_PLUGIN_TYPE        = 0x6,
	SDEXT_PLUGIN_TYPE       = 0x7,
	FORMAT_PLUGIN_TYPE      = 0x8,
	OID_PLUGIN_TYPE         = 0x9,
	ALLOC_PLUGIN_TYPE       = 0xa,
	JNODE_PLUGIN_TYPE       = 0xb,
	JOURNAL_PLUGIN_TYPE     = 0xc,
	KEY_PLUGIN_TYPE         = 0xd
};

typedef enum reiser4_plugin_type reiser4_plugin_type_t;

enum reiser4_object_plugin_id {
	OBJECT_FILE40_ID        = 0x0,
	OBJECT_DIRTORY40_ID     = 0x1,
	OBJECT_SYMLINK40_ID     = 0x2,
	OBJECT_SPECIAL40_ID     = 0x3
};

enum reiser4_object_group {
	FILE_OBJECT             = 0x0,
	DIRTORY_OBJECT          = 0x1,
	SYMLINK_OBJECT          = 0x2,
	SPECIAL_OBJECT          = 0x3
};

typedef enum reiser4_object_group reiser4_object_group_t;

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

typedef struct reiser4_plugin reiser4_plugin_t;

#define INVAL_PID               0xffff

/* Maximal possible key size in 8 byte elements */
#define KEY_SIZE 3

struct key_entity {
	reiser4_plugin_t *plugin;
	d64_t body[KEY_SIZE];
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
	blk_t blk;
	uint32_t blocksize;
 	aal_device_t *device; 
};

typedef struct item_context item_context_t;

/*
  Type for describing an item. The pointer of this type will be passed to the
  all item plugins.
*/
struct item_entity {
	reiser4_plugin_t *plugin;

	pos_t pos;

	body_t *body;
	uint32_t len;
	
	key_entity_t key;
	item_context_t context;
};

typedef struct item_entity item_entity_t;

struct sdext_entity {
	reiser4_plugin_t *plugin;

	body_t *body;
	uint32_t sdlen;
	uint32_t offset;
};

typedef struct sdext_entity sdext_entity_t;

struct place {
	void *node;
	pos_t pos;
	item_entity_t item;
};

typedef struct place place_t;

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

	/* Should be new node allocated durring make space or not */
	SF_ALLOC = 1 << 5
};

typedef enum shift_flags shift_flags_t;

#define SF_DEFAULT (SF_LEFT | SF_RIGHT | SF_ALLOC)

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
	  not. Of course we might use control flags for that, but it would led
	  us to write a lot of useless stuff for saving control flags before
	  modifying it.
	*/
	uint32_t control;
	uint32_t result;

	/* Insert point. It will be modified durring shfiting */
	pos_t pos;
};

typedef struct shift_hint shift_hint_t;

struct copy_hint {	
	uint32_t dst_count;
	uint32_t src_count;
	int32_t  len_delta;
	
	key_entity_t start, end;
	
	/*
	  Fields bellow are only related to extent estimate_copy() and copy()
	  operations.
	*/
	
	/* Offset in blocks in the start and end units of dst and src */
	uint64_t dst_tail, src_tail;
	uint64_t dst_head, src_head;

	/*
	  Should be dst head and tail splitted into 2 units while performing
	  copy() operation.
	*/
	bool_t head, tail;
};

typedef struct copy_hint copy_hint_t;

typedef errno_t (*region_func_t) (void *, uint64_t, uint64_t, void *);
typedef errno_t (*block_func_t) (object_entity_t *, uint64_t, void *);
typedef errno_t (*place_func_t) (object_entity_t *, place_t *, void *);

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
    
  (2) Estimate common item method which gets place of where to insert into
      (NULL or unit == -1 for insertion, otherwise it is pasting) and data
      description from 1.
    
  (3) Insert node methods prepare needed space and call create/paste item
      methods if data description is specified.
    
  (4) Create/Paste item methods if data description has not beed specified
      on 3.
*/
struct ptr_hint {    
	uint64_t start;
	uint64_t width;
};

typedef struct ptr_hint ptr_hint_t;

struct sdext_unix_hint {
	uint32_t uid;
	uint32_t gid;
	uint32_t atime;
	uint32_t mtime;
	uint32_t ctime;
	uint32_t rdev;
	uint64_t bytes;
};

typedef struct sdext_unix_hint sdext_unix_hint_t;

struct sdext_lw_hint {
	uint16_t mode;
	uint32_t nlink;
	uint64_t size;
};

typedef struct sdext_lw_hint sdext_lw_hint_t;

struct sdext_lt_hint {
	uint32_t atime;
	uint32_t mtime;
	uint32_t ctime;
};

typedef struct sdext_lt_hint sdext_lt_hint_t;

/* These fields should be changed to what proper description of needed extentions */
struct statdata_hint {
	
	/* Extentions mask */
	uint64_t extmask;
    
	/* Stat data extentions */
	void *ext[60];
};

typedef struct statdata_hint statdata_hint_t;

struct entry_hint {

	/* Entry key within the current directory */
	key_entity_t offset;

	/* The stat data key of the object entry points to */
	key_entity_t object;

	/* Name of entry */
	char name[256];
};

typedef struct entry_hint entry_hint_t;

#define SYMLINK_MAX_LEN 1024

struct object_hint {
	
	rid_t statdata;

	/* Hint for a file body */
	union {

		/* Plugin ids for the directory body */
		struct {
			rid_t direntry;
			rid_t hash;
		} dir;
	
		/* Plugin id for the regular file body */
		struct {
			rid_t tail;
			rid_t extent;
			rid_t policy;
		} reg;

		/* Symlink data */
		char sym[SYMLINK_MAX_LEN];
	} body;
    
	/* The plugin in use */
	reiser4_plugin_t *plugin;
};

typedef struct object_hint object_hint_t;

struct object_info {
	void *tree;
	place_t start;
	
	key_entity_t object;
	key_entity_t parent;
};

typedef struct object_info object_info_t;

/*
  Flags for using in item hint for denoting is type_specific point for type
  specific hint or onto raw data.
*/
enum hint_flags {
	HF_FORMATD    = 1 << 0,
	HF_RAWDATA    = 1 << 1
};

typedef enum hint_flags hint_flags_t;

/* 
  This structure contains fields which describe an item or unit to be inserted
  into the tree.
*/ 
struct create_hint {
	hint_flags_t flags;
	
	/*
	  This is pointer to already formated item body. It is useful for item
	  copying, replacing, etc. This will be used by fsck probably.
	*/
	void *data;

	/* Length of the data field */
	uint16_t len;

	/* This is opaque pointer to item type specific information */
	void *type_specific;

	/* Count of units to be inserted into the tree */
	uint16_t count;
    
	/* The key of item/unit to be inserted */
	key_entity_t key;

	/* Plugin to be used for working with item */
	reiser4_plugin_t *plugin;
};

typedef struct create_hint create_hint_t;

struct reiser4_key_ops {
	/* 
	  Cleans key up. Actually it just memsets it by zeros, but more smart
	  behavior may be implemented.
	*/
	void (*clean) (key_entity_t *);

	/* Confirms key format */
	int (*confirm) (key_entity_t *);

	/* Returns minimal key for this key-format */
	key_entity_t *(*minimal) (void);
    
	/* Returns maximal key for this key-format */
	key_entity_t *(*maximal) (void);
    
	/* Compares two keys by comparing its all components */
	int (*compare) (key_entity_t *, key_entity_t *);

	/* Compares two keys by comparing its all components */
	int (*compare_short) (key_entity_t *, key_entity_t *);
	
	/* Functions for determining is key long */
	int (*tall) (key_entity_t *);

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

#ifndef ENABLE_STAND_ALONE
	/* Gets/sets directory key hash */
	void (*set_hash) (key_entity_t *, uint64_t);
	uint64_t (*get_hash) (key_entity_t *);
	
	/* Check of key structure */
	errno_t (*valid) (key_entity_t *);
    
	/* Prints key into specified buffer */
	errno_t (*print) (key_entity_t *, aal_stream_t *,
			  uint16_t);
#endif
};

typedef struct reiser4_key_ops reiser4_key_ops_t;

struct reiser4_object_ops {
#ifndef ENABLE_STAND_ALONE
	
	/* Creates new file with passed parent and object keys */
	object_entity_t *(*create) (object_info_t *, object_hint_t *);

	/* These methods change @nlink value of passed @entity */
	errno_t (*link) (object_entity_t *);
	errno_t (*unlink) (object_entity_t *);

	/* Establish parent child relationchip */
	errno_t (*attach) (object_entity_t *,
			   object_entity_t *);
	errno_t (*detach) (object_entity_t *,
			   object_entity_t *);

	/* Writes the data to file from passed buffer */
	int32_t (*write) (object_entity_t *, void *, uint32_t);

	/* Directory specific methods */
	errno_t (*add_entry) (object_entity_t *, entry_hint_t *);
	errno_t (*rem_entry) (object_entity_t *, entry_hint_t *);
	
	/* Truncates file at current offset onto passed units */
	errno_t (*truncate) (object_entity_t *, uint64_t);

	/*
	  Function for going through all metadata blocks specfied file
	  occupied. It is needed for accessing file's metadata.
	*/
	errno_t (*metadata) (object_entity_t *, place_func_t, void *);
	
	/*
	  Function for going through the all data blocks specfied file
	  occupies. It is needed for the purposes like data fragmentation
	  measuring, etc.
	*/
	errno_t (*layout) (object_entity_t *, block_func_t, void *);
	
	/* Checks and recover the structure of the object. */
	object_entity_t *(*check_struct) (object_info_t *, place_func_t, 
					  uint8_t, void *);
	
	/* Checks and recover the up link of the object. */
	errno_t (*check_link) (object_entity_t *, object_entity_t *, uint8_t);
	
	/* 
	  Realizes if the object can be of this plugin and can be recovered 
	  as a such. 
	*/
	errno_t (*realize) (object_info_t *);

#endif
	
	/* Change current position to passed value */
	errno_t (*seek) (object_entity_t *, uint64_t);
	
	/* Opens file with specified key */
	object_entity_t *(*open) (object_info_t *);

	/* Closes previously opened or created directory */
	void (*close) (object_entity_t *);

	/* Resets internal position */
	errno_t (*reset) (object_entity_t *);
   
	/* Returns current position in directory */
	uint64_t (*offset) (object_entity_t *);

	/* Returns file size */
	uint64_t (*size) (object_entity_t *);

	/* Makes lookup inside file */
	lookup_t (*lookup) (object_entity_t *, char *,
			    entry_hint_t *);

	/* Finds actual file stat data (used in symlinks) */
	errno_t (*follow) (object_entity_t *, key_entity_t *,
			   key_entity_t *);

	/* Reads the data from file to passed buffer */
	int32_t (*read) (object_entity_t *, void *, uint32_t);

	/* Directory read method */
	errno_t (*readdir) (object_entity_t *,
			    entry_hint_t *);

	/* Return current position in dirctory */
	errno_t (*telldir) (object_entity_t *, key_entity_t *);

	/* Change current position in directory */
	errno_t (*seekdir) (object_entity_t *, key_entity_t *);
};

typedef struct reiser4_object_ops reiser4_object_ops_t;

struct reiser4_item_ops {
#ifndef ENABLE_STAND_ALONE
	/* Prepares item body for working with it */
	errno_t (*init) (item_entity_t *);

	/* Returns overhead */
	uint16_t (*overhead) (item_entity_t *);
	
	/* Estimate copy operation */
	errno_t (*estimate_copy) (item_entity_t *, uint32_t, 
				  item_entity_t *, uint32_t, 
				  copy_hint_t *);

	/* Estimates insert operation */
	errno_t (*estimate_insert) (item_entity_t *, create_hint_t *,
				    uint32_t);

	/* Predicts the shift parameters (units, bytes, etc) */
	errno_t (*estimate_shift) (item_entity_t *, item_entity_t *,
				   shift_hint_t *);
	
	/*
	  Inserts some amount of units described by passed hint into passed
	  item.
	*/
	errno_t (*insert) (item_entity_t *, create_hint_t *, uint32_t);
	
	/* Performs shift of units from passed @src item to @dst item */
	errno_t (*shift) (item_entity_t *, item_entity_t *,
			  shift_hint_t *);

	/*
	  Copies some amount of units from @src_item to @dst_item with partial
	  overwritting.
	*/
	errno_t (*copy) (item_entity_t *, uint32_t,
			 item_entity_t *, uint32_t,
			 copy_hint_t *);

	/* Copes @count units from @src_item to @dst_item */
	errno_t (*rep) (item_entity_t *, uint32_t,
			item_entity_t *, uint32_t,
			uint32_t);
	
	uint32_t (*expand) (item_entity_t *, uint32_t,
			    uint32_t, uint32_t);
	
	uint32_t (*shrink) (item_entity_t *, uint32_t,
			    uint32_t, uint32_t);
	
	/* Removes specified unit from the item. Returns released space */
	int32_t (*remove) (item_entity_t *, uint32_t, uint32_t);
	
	/* Checks the item structure. */
	errno_t (*check_struct) (item_entity_t *, uint8_t);
	
	/* Prints item into specified buffer */
	errno_t (*print) (item_entity_t *, aal_stream_t *, uint16_t);

	/* Goes through all blocks item points to. */
	errno_t (*layout) (item_entity_t *, region_func_t, void *);

	/* Does some specific actions if a block the item points to is wrong. */
	errno_t (*check_layout) (item_entity_t *, region_func_t,
				 void *, uint8_t);

	/* Set the key of a particular unit of the item. */
	errno_t (*set_key) (item_entity_t *, uint32_t, key_entity_t *);
#endif
	
	/* Checks if items mergeable. Returns 1 if so, 0 otherwise */
	int (*mergeable) (item_entity_t *, item_entity_t *);

	/* Reads passed amount of units from the item. */
	int32_t (*read) (item_entity_t *, void *, uint32_t,
			 uint32_t);

	/* Returns unit count */
	uint32_t (*units) (item_entity_t *);

	/* Makes lookup for passed key */
	lookup_t (*lookup) (item_entity_t *, key_entity_t *, uint32_t *);

	/*
	  Returns TRUE is specified item is a nodeptr one. That is, it points to
	  formatted node in the tree. If this method if not implemented, then
	  item is assumed as not nodeptr one. All tree running operations like
	  going from the root to leaves will use this function.
	*/
	int (*branch) (void);
	
	/* 
	  Returns TRUE if instances of the plugin can contain data, not just
	  tree index data.
	*/
	int (*data) (void);
	
	/* Get the key of a particular unit of the item. */
	errno_t (*get_key) (item_entity_t *, uint32_t, key_entity_t *);

	/* Get the max key which could be stored in the item of this type */
	errno_t (*maxposs_key) (item_entity_t *, key_entity_t *);

#ifndef ENABLE_STAND_ALONE
	/* Get the max real key which is stored in the item */
	errno_t (*maxreal_key) (item_entity_t *, key_entity_t *);
	
	/* Get the plugin id of the specified type if stored in SD. */
	rid_t (*get_plugid) (item_entity_t *, uint16_t);
#endif
};

typedef struct reiser4_item_ops reiser4_item_ops_t;

/* Stat data extention plugin */
struct reiser4_sdext_ops {
#ifndef ENABLE_STAND_ALONE
	/* Initialize stat data extention data at passed pointer */
	errno_t (*init) (body_t *, void *);

	/* Prints stat data extention data into passed buffer */
	errno_t (*print) (body_t *, aal_stream_t *, uint16_t);

	/* Checks sd extention content. */
	errno_t (*check) (sdext_entity_t *, uint8_t);
#endif

	/* Reads stat data extention data */
	errno_t (*open) (body_t *, void *);

	/* Returns length of the extention */
	uint16_t (*length) (body_t *);
};

typedef struct reiser4_sdext_ops reiser4_sdext_ops_t;

/*
  Node plugin operates on passed block. It doesn't any initialization, so it
  hasn't close method and all its methods accepts first argument aal_block_t,
  not initialized previously hypothetic instance of node.
*/
struct reiser4_node_ops {
#ifndef ENABLE_STAND_ALONE
	/* Saves node onto device */
	errno_t (*sync) (object_entity_t *);
	
	/* Performs shift of items and units */
	errno_t (*shift) (object_entity_t *, object_entity_t *, 
			  shift_hint_t *);
    
	/* Checks thoroughly the node structure and fixes what needed. */
	errno_t (*check) (object_entity_t *, uint8_t);

	int (*isdirty) (object_entity_t *);
	void (*mkdirty) (object_entity_t *);
	void (*mkclean) (object_entity_t *);

	/* Prints node into given buffer */
	errno_t (*print) (object_entity_t *, aal_stream_t *,
			  uint32_t, uint32_t, uint16_t);
    
	/* Returns item's overhead */
	uint16_t (*overhead) (object_entity_t *);

	/* Returns item's max size */
	uint16_t (*maxspace) (object_entity_t *);
    
	/* Returns free space in the node */
	uint16_t (*space) (object_entity_t *);

	/* Inserts item at specified pos */
	errno_t (*insert) (object_entity_t *, pos_t *,
			   create_hint_t *);
    
	/* Removes item/unit at specified pos */
	errno_t (*remove) (object_entity_t *, pos_t *, uint32_t);

	/* Removes some amount of items/units */
	errno_t (*cut) (object_entity_t *, pos_t *, pos_t *);
	
	/* Shrinks node without calling any item methods */
	errno_t (*shrink) (object_entity_t *, pos_t *,
			   uint32_t, uint32_t);

	/*
	  Makes copy from @src_entity to @dst_entity with partial
	  overwriting.
	*/
	errno_t (*copy) (object_entity_t *, pos_t *, 
			 object_entity_t *, pos_t *, 
			 copy_hint_t *);

	/* Copies items from @src_entity to @dst_entity */
	errno_t (*rep) (object_entity_t *, pos_t *,
			object_entity_t *, pos_t *,
			uint32_t);
	
	/* Expands node */
	errno_t (*expand) (object_entity_t *, pos_t *,
			   uint32_t, uint32_t);

	errno_t (*set_key) (object_entity_t *, pos_t *,
			    key_entity_t *);

	void (*set_level) (object_entity_t *, uint8_t);

	void (*set_mstamp) (object_entity_t *, uint32_t);
	void (*set_fstamp) (object_entity_t *, uint64_t);

	/*
	  Creates node data block and initializes node header by means of
	  setting up level, plugin id,etc.
	*/
	errno_t (*form) (object_entity_t *, uint8_t);

	/* Changes node location */
	void (*move) (object_entity_t *, blk_t);

	/* Get mkfs and flush stamps */
	uint32_t (*get_mstamp) (object_entity_t *);
    	uint64_t (*get_fstamp) (object_entity_t *);
	
	/* Get/set/test item flags. */
	void (*set_flag) (object_entity_t *, uint32_t, uint16_t);
	void (*clear_flag) (object_entity_t *, uint32_t, uint16_t);
	bool_t (*test_flag) (object_entity_t *, uint32_t, uint16_t);
#endif

	/* Cerates node entity */
	object_entity_t *(*init) (aal_device_t *,
				  uint32_t, blk_t);
	
	/* Loads data block node lies in */
	errno_t (*load) (object_entity_t *);

	/* Unloads node data */
	errno_t (*unload) (object_entity_t *);
	
	/* 
	   Destroys the node entity. If node data is not unloaded, it also
	   unloads data.
	*/
	errno_t (*close) (object_entity_t *);

	/* Confirms that given block contains valid node */
	int (*confirm) (object_entity_t *);

	/* Returns item count */
	uint16_t (*items) (object_entity_t *);
    
	/* Makes lookup inside node by specified key */
	lookup_t (*lookup) (object_entity_t *, key_entity_t *, 
			    pos_t *);
    
	/* Gets/sets key at pos */
	errno_t (*get_key) (object_entity_t *, pos_t *,
			    key_entity_t *);
    
	uint8_t (*get_level) (object_entity_t *);

	errno_t (*get_item) (object_entity_t *, pos_t *,
			     item_entity_t *);
};

typedef struct reiser4_node_ops reiser4_node_ops_t;

struct reiser4_hash_ops {
	uint64_t (*build) (const unsigned char *, uint32_t);
};

typedef struct reiser4_hash_ops reiser4_hash_ops_t;

/* Disk-format plugin */
struct reiser4_format_ops {
#ifndef ENABLE_STAND_ALONE
	/* 
	   Called during filesystem creating. It forms format-specific super
	   block, initializes plugins and calls their create method.
	*/
	object_entity_t *(*create) (aal_device_t *, uint64_t,
				    uint32_t, uint16_t);
	
	errno_t (*sync) (object_entity_t *);
	
	int (*isdirty) (object_entity_t *);
	void (*mkdirty) (object_entity_t *);
	void (*mkclean) (object_entity_t *);
	
	/* 
	   Update only fields which can be changed after journal replay in 
	   memory to avoid second checking.
	*/
	errno_t (*update) (object_entity_t *);
	    
	/* Checks thoroughly the format structure and fixes what needed. */
	errno_t (*check) (object_entity_t *, uint8_t);

	/* Prints all useful information about the format */
	errno_t (*print) (object_entity_t *, aal_stream_t *, uint16_t);
    
	/*
	  Probes whether filesystem on given device has this format. Returns
	  "true" if so and "false" otherwise.
	*/
	int (*confirm) (aal_device_t *device);

	void (*set_root) (object_entity_t *, uint64_t);
	void (*set_len) (object_entity_t *, uint64_t);
	void (*set_height) (object_entity_t *, uint16_t);
	void (*set_free) (object_entity_t *, uint64_t);
	void (*set_stamp) (object_entity_t *, uint32_t);
	void (*set_policy) (object_entity_t *, uint16_t);
	    
	rid_t (*journal_pid) (object_entity_t *);
	rid_t (*alloc_pid) (object_entity_t *);

	errno_t (*layout) (object_entity_t *, block_func_t, void *);
	errno_t (*skipped) (object_entity_t *, block_func_t, void *);

	/*
	  Checks format-specific super block for validness. Also checks whether
	  filesystem objects lie in valid places. For example, format-specific
	  super block for format40 must lie in 17-th block for 4096 byte long
	  blocks.
	*/
	errno_t (*valid) (object_entity_t *);

	/* Returns the device disk-format lies on */
	aal_device_t *(*device) (object_entity_t *);

	/*
	  Returns format string for this format. For example "reiserfs 4.0" ot
	  something like this.
	*/
	const char *(*name) (object_entity_t *);
#endif
	/* 
	   Called during filesystem opening (mounting). It reads format-specific
	   super block and initializes plugins suitable for this format.
	*/
	object_entity_t *(*open) (aal_device_t *, uint32_t);
    
	/*
	  Closes opened or created previously filesystem. Frees all assosiated
	  memory.
	*/
	void (*close) (object_entity_t *);
    
	uint64_t (*get_root) (object_entity_t *);
	uint16_t (*get_height) (object_entity_t *);

#ifndef ENABLE_STAND_ALONE
	/* Gets the start of the filesystem. */
	uint64_t (*start) (object_entity_t *);
	
	uint64_t (*get_len) (object_entity_t *);
	uint64_t (*get_free) (object_entity_t *);
    
	uint32_t (*get_stamp) (object_entity_t *);
	uint16_t (*get_policy) (object_entity_t *);
#endif
	    
	rid_t (*oid_pid) (object_entity_t *);

	/* Returns area where oid data lies in */
	void (*oid) (object_entity_t *, void **, uint32_t *);
};

typedef struct reiser4_format_ops reiser4_format_ops_t;

struct reiser4_oid_ops {
	/* Opens oid allocator on passed area */
	object_entity_t *(*open) (void *,
				  uint32_t);

	/* Closes passed instance of oid allocator */
	void (*close) (object_entity_t *);
    
#ifndef ENABLE_STAND_ALONE
	/* Creates oid allocator on passed area */
	object_entity_t *(*create) (void *,
				    uint32_t);

	/* Synchronizes oid allocator */
	errno_t (*sync) (object_entity_t *);

	errno_t (*layout) (object_entity_t *,
			   block_func_t, void *);

	int (*isdirty) (object_entity_t *);
	void (*mkdirty) (object_entity_t *);
	void (*mkclean) (object_entity_t *);

	/* Gets next object id */
	oid_t (*next) (object_entity_t *);

	/* Gets next object id */
	oid_t (*allocate) (object_entity_t *);

	/* Releases passed object id */
	void (*release) (object_entity_t *, oid_t);
    
	/* Returns the number of used object ids */
	uint64_t (*used) (object_entity_t *);
    
	/* Returns the number of free object ids */
	uint64_t (*free) (object_entity_t *);

	/* Prints oid allocator data */
	errno_t (*print) (object_entity_t *, aal_stream_t *,
			  uint16_t);

	/* Makes check for validness */
	errno_t (*valid) (object_entity_t *);
#endif
	
	/* Root locality and objectid */
	oid_t (*root_locality) (void);
	oid_t (*root_objectid) (void);
};

typedef struct reiser4_oid_ops reiser4_oid_ops_t;

#ifndef ENABLE_STAND_ALONE
struct reiser4_alloc_ops {
	/* Creates block allocator */
	object_entity_t *(*create) (aal_device_t *,
				    uint64_t, uint32_t);

	/* Opens block allocator */
	object_entity_t *(*open) (aal_device_t *,
				  uint64_t, uint32_t);

	/* Closes blcok allocator */
	void (*close) (object_entity_t *);

	/* Synchronizes block allocator */
	errno_t (*sync) (object_entity_t *);

	int (*isdirty) (object_entity_t *);
	void (*mkdirty) (object_entity_t *);
	void (*mkclean) (object_entity_t *);
	
	/* Assign the bitmap to the block allocator */
	errno_t (*assign) (object_entity_t *, void *);

	/* Extract block allocator data into passed bitmap */
	errno_t (*extract) (object_entity_t *, void *);
	
	/* Returns number of used blocks */
	uint64_t (*used) (object_entity_t *);

	/* Returns number of unused blocks */
	uint64_t (*free) (object_entity_t *);

	/* Checks blocks allocator on validness */
	errno_t (*valid) (object_entity_t *);

	errno_t (*check) (object_entity_t *, uint8_t);
	    
	/* Prints block allocator data */
	errno_t (*print) (object_entity_t *, aal_stream_t *,
			  uint16_t);

	/* Calls func for each block in block allocator */
	errno_t (*layout) (object_entity_t *, block_func_t, void *);
	
	/* Checks if passed range of blocks used */
	int (*occupied) (object_entity_t *, uint64_t,
			 uint64_t);
    	
	/* Checks if passed range of blocks unused */
	int (*available) (object_entity_t *, uint64_t,
			  uint64_t);

	/* Marks passed block as used */
	errno_t (*occupy) (object_entity_t *, uint64_t,
			   uint64_t);

	/* Tries to allocate passed amount of blocks */
	uint64_t (*allocate) (object_entity_t *, uint64_t *,
			      uint64_t);
	
	/* Deallocates passed blocks */
	errno_t (*release) (object_entity_t *, uint64_t,
			    uint64_t);

	/* Calls func for all block of the same area as blk is. */
	errno_t (*related) (object_entity_t *, blk_t,
			    region_func_t, void *);
};

typedef struct reiser4_alloc_ops reiser4_alloc_ops_t;

struct reiser4_journal_ops {
	/* Opens journal on specified device */
	object_entity_t *(*open) (object_entity_t *, aal_device_t *,
				  uint64_t, uint64_t, uint32_t);

	/* Creates journal on specified device */
	object_entity_t *(*create) (object_entity_t *, aal_device_t *,
				    uint64_t, uint64_t, uint32_t, void *);

	/* Returns the device journal lies on */
	aal_device_t *(*device) (object_entity_t *);
    
	/* Frees journal instance */
	void (*close) (object_entity_t *);

	/* Checks journal metadata on validness */
	errno_t (*valid) (object_entity_t *);
    
	/* Synchronizes journal */
	errno_t (*sync) (object_entity_t *);

	int (*isdirty) (object_entity_t *);
	void (*mkdirty) (object_entity_t *);
	void (*mkclean) (object_entity_t *);
	
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
#endif

#define PLUGIN_MAX_LABEL	22
#define PLUGIN_MAX_NAME		22
#define PLUGIN_MAX_DESC		64

typedef struct reiser4_core reiser4_core_t;

typedef errno_t (*plugin_fini_t) (reiser4_core_t *);
typedef reiser4_plugin_t *(*plugin_init_t) (reiser4_core_t *);
typedef errno_t (*plugin_func_t) (reiser4_plugin_t *, void *);

struct plugin_class {
	void *data;

	plugin_init_t init;
	
#ifndef ENABLE_STAND_ALONE
	plugin_fini_t fini;
	char name[PLUGIN_MAX_NAME];
#endif
};

typedef struct plugin_class plugin_class_t;

#ifndef ENABLE_STAND_ALONE
#define CLASS_INIT \
        {NULL, NULL, NULL, ""}
#else
#define CLASS_INIT \
        {NULL, NULL}
#endif

/* Common plugin header */
struct plugin_header {

	/* Plugin handle. It is used by plugin factory */
	plugin_class_t class;

	/* Plugin will be looked by its id, type, etc */
	rid_t id;
	rid_t type;
	rid_t group;

#ifndef ENABLE_STAND_ALONE
	/* Plugin label (name) */
	const char label[PLUGIN_MAX_LABEL];
	
	/* Plugin description */
	const char desc[PLUGIN_MAX_DESC];
#endif
};

typedef struct plugin_header plugin_header_t;

struct reiser4_plugin {
	plugin_header_t h;

	union {
		reiser4_item_ops_t *item_ops;
		reiser4_node_ops_t *node_ops;
		reiser4_hash_ops_t *hash_ops;
		reiser4_sdext_ops_t *sdext_ops;
		reiser4_object_ops_t *object_ops;
		reiser4_format_ops_t *format_ops;

#ifndef ENABLE_STAND_ALONE
		reiser4_alloc_ops_t *alloc_ops;
		reiser4_journal_ops_t *journal_ops;
#endif
		reiser4_oid_ops_t *oid_ops;
		reiser4_key_ops_t *key_ops;
	} o;
};

/* The common node header */
struct node_header {
	d16_t pid;
};

typedef struct node_header node_header_t;

struct tree_ops {
		
#ifndef ENABLE_STAND_ALONE
	/* Returns blocksize in passed tree */
	uint32_t (*blocksize) (void *);
	
	/* Returns maximal available space in a node */
	uint32_t (*maxspace) (void *);
#endif
	
	/*
	  Makes lookup in the tree in order to know where say stat data item of
	  a file really lies. It is used in all object plugins.
	*/
	lookup_t (*lookup) (void *, key_entity_t *, uint8_t,
			    place_t *);

	/* Initializes all item fields in passed place */
	errno_t (*realize) (void *, place_t *);

	/* Checks if passed @place points to some real item inside a node */
	int (*valid) (void *, place_t *);
	
#ifndef ENABLE_STAND_ALONE
	/* 
	  Inserts item/unit in the tree by calling reiser4_tree_insert function,
	  used by all object plugins (dir, file, etc)
	*/
	errno_t (*insert) (void *, place_t *, uint8_t, create_hint_t *);
    
	/*
	  Removes item/unit from the tree. It is used in all object plugins for
	  modification purposes.
	*/
	errno_t (*remove) (void *, place_t *, uint32_t);

#endif
	/* Lock control functions */
	errno_t (*lock) (void *, place_t *);
	errno_t (*unlock) (void *, place_t *);
		
	/* Returns next and prev items respectively */
	errno_t (*next) (void *, place_t *, place_t *);
	errno_t (*prev) (void *, place_t *, place_t *);
};

typedef struct tree_ops tree_ops_t;

struct factory_ops {

	/* Finds plugin by its attributes (type and id) */
	reiser4_plugin_t *(*ifind) (rid_t, rid_t);
	
#ifndef ENABLE_STAND_ALONE	
	/* Finds plugin by its type and name */
	reiser4_plugin_t *(*nfind) (const char *);
#endif
};

typedef struct factory_ops factory_ops_t;

#ifdef ENABLE_SYMLINKS_SUPPORT
struct object_ops {
	errno_t (*resolve) (void *, place_t *, char *,
			    key_entity_t *, key_entity_t *);
};

typedef struct object_ops object_ops_t;
#endif

/* 
  This structure is passed to all plugins in initialization time and used for
  access libreiser4 factories.
*/
struct reiser4_core {
	tree_ops_t tree_ops;
	
	factory_ops_t factory_ops;
#ifdef ENABLE_SYMLINKS_SUPPORT
	object_ops_t object_ops;
#endif
};

#define plugin_equal(plugin1, plugin2)                           \
        (plugin1->h.group == plugin2->h.group &&                 \
	 plugin1->h.id == plugin2->h.id)


/* Makes check is needed method implemengted */
#define plugin_call(ops, method, ...) ({                         \
        aal_assert("Method \""#method"\" isn't implemented in"   \
                   ""#ops".", ops->method != NULL);              \
        ops->method(__VA_ARGS__);			         \
})

#if defined(ENABLE_MONOLITHIC) || defined(ENABLE_STAND_ALONE)
typedef void (*register_builtin_t) (plugin_init_t,
				    plugin_fini_t);
#endif


/*
  Macro for registering a plugin in plugin factory. It accepts two pointers to
  functions. The first one is pointer to plugin init function and second - to
  plugin finalization function. The idea the same as in the linux kernel module
  support.
*/
#if defined(ENABLE_MONOLITHIC)

#define plugin_register(n, i, f)			       \
    extern register_builtin_t __register_builtin;              \
                                                               \
    static void __plugin_init(void)                            \
            __attribute__((constructor));                      \
                                                               \
    static void __plugin_init(void) {                          \
	    __register_builtin(i, f);                          \
    }

#elif defined (ENABLE_STAND_ALONE)

#define plugin_register(n, i, f)                               \
    plugin_init_t __##n##_plugin_init = i
#else

#define plugin_register(n, i, f)			       \
    plugin_init_t __plugin_init = i;                           \
    plugin_fini_t __plugin_fini = f

#endif

#endif
