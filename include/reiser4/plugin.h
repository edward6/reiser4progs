/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   plugin.h -- reiser4 plugin known types and macros. */

#ifndef REISER4_PLUGIN_H
#define REISER4_PLUGIN_H

#include <aal/aal.h>

/* Leaf and twig levels. */
#define LEAF_LEVEL	        1
#define TWIG_LEVEL	        (LEAF_LEVEL + 1)

/* Master related stuff like magic and offset in bytes. These are sued by both
   plugins and library itself. */
#define REISER4_MASTER_MAGIC	("R4Sb")
#define REISER4_MASTER_OFFSET	(65536)

/* The same for fs stat block. */
#define REISER4_STATUS_BLOCK    (21)
#define REISER4_STATUS_MAGIC    ("ReiSeR4StATusBl")

/* Root key locality and objectid. This actually is defined by oid plugin, but
   we hardcoded them her to avoid use oid plugin in stand alone mode. Because in
   stand alone mode we do not need from it nothing but root key component. */
#define REISER4_ROOT_LOCALITY   (0x29)
#define REISER4_ROOT_OBJECTID   (0x2a)

/* Macros for hole and unallocated extents. Used by both plugins (extent40) and
   library itself. */
#define EXTENT_SPARSE_UNIT      (0)
#define EXTENT_UNALLOC_UNIT     (1)

/* Defining the types for disk structures. All types like f32_t are fake ones
   and needed to avoid gcc-2.95.x bug with size of typedefined aligned types. */
typedef uint8_t  f8_t;  typedef f8_t  d8_t  __attribute__((aligned(1)));
typedef uint16_t f16_t; typedef f16_t d16_t __attribute__((aligned(2)));
typedef uint32_t f32_t; typedef f32_t d32_t __attribute__((aligned(4)));
typedef uint64_t f64_t; typedef f64_t d64_t __attribute__((aligned(8)));

/* Basic reiser4 types used by both library and plugins. */
typedef uint8_t rid_t;
typedef uint64_t oid_t;

/* Type for position in node (item and unit component). */
struct pos {
	uint32_t item;
	uint32_t unit;
};

typedef struct pos pos_t;

#define POS_INIT(p, i, u) \
        (p)->item = i, (p)->unit = u

/* Lookup return values. */
enum lookup {
	PRESENT                 = 1,
	ABSENT                  = 0,
};

typedef int32_t lookup_t;

/* Lookup bias. */
enum bias {
	FIND_EXACT              = 1,
	FIND_CONV               = 2
};

typedef enum bias bias_t;

/* Known by library plugin types. */
enum reiser4_plug_type {
	OBJECT_PLUG_TYPE        = 0x0,
	ITEM_PLUG_TYPE          = 0x2,
	NODE_PLUG_TYPE          = 0x3,
	HASH_PLUG_TYPE          = 0x4,
	POLICY_PLUG_TYPE        = 0x5,
	PERM_PLUG_TYPE          = 0x6,
	SDEXT_PLUG_TYPE         = 0x7,
	FORMAT_PLUG_TYPE        = 0x8,
	OID_PLUG_TYPE           = 0x9,
	ALLOC_PLUG_TYPE         = 0xa,
	JNODE_PLUG_TYPE         = 0xb,
	JOURNAL_PLUG_TYPE       = 0xc,
	KEY_PLUG_TYPE           = 0xd,
	LAST_PLUG_TYPE
};

typedef enum reiser4_plug_type reiser4_plug_type_t;

/* Known object plugin ids. */
enum reiser4_object_plug_id {
	OBJECT_REG40_ID         = 0x0,
	OBJECT_DIR40_ID		= 0x1,
	OBJECT_SYM40_ID		= 0x2,
	OBJECT_SPL40_ID	        = 0x3,
	OBJECT_LAST_ID
};

/* Known object groups. */
enum reiser4_object_group {
	REG_OBJECT		= 0x0,
	DIR_OBJECT		= 0x1,
	SYM_OBJECT		= 0x2,
	SPL_OBJECT		= 0x3,
	LAST_OBJECT
};

typedef enum reiser4_object_group reiser4_object_group_t;

/* Known item plugin ids. */
enum reiser4_item_plug_id {
	ITEM_STATDATA40_ID	= 0x0,
	ITEM_SDE40_ID	        = 0x1,
	ITEM_CDE40_ID	        = 0x2,
	ITEM_NODEPTR40_ID	= 0x3,
	ITEM_ACL40_ID		= 0x4,
	ITEM_EXTENT40_ID	= 0x5,
	ITEM_TAIL40_ID		= 0x6,
	ITEM_LAST_ID
};

/* Known item groups. */
enum reiser4_item_group {
	STATDATA_ITEM		= 0x0,
	NODEPTR_ITEM		= 0x1,
	DIRENTRY_ITEM		= 0x2,
	TAIL_ITEM		= 0x3,
	EXTENT_ITEM		= 0x4,
	PERMISSION_ITEM		= 0x5, /* not used yet */
	LAST_ITEM
};

typedef enum reiser4_item_group reiser4_item_group_t;

/* Known node plugin ids. */
enum reiser4_node_plug_id {
	NODE_REISER40_ID        = 0x0,
	NODE_LAST_ID
};

/* Known hash plugin ids. */
enum reiser4_hash_plug_id {
	HASH_RUPASOV_ID		= 0x0,
	HASH_R5_ID		= 0x1,
	HASH_TEA_ID		= 0x2,
	HASH_FNV1_ID		= 0x3,
	HASH_DEG_ID             = 0x4,
	HASH_LAST_ID
};

typedef enum reiser4_hash_plug_id reiser4_hash_plug_id_t;

/* Know tail policy plugin ids. */
enum reiser4_tail_plug_id {
	TAIL_NEVER_ID		= 0x0,
	TAIL_SUPPRESS_OLD_ID	= 0x1,
	TAIL_ALWAYS_ID		= 0x3,
	TAIL_SMART_ID		= 0x4,
	TAIL_LAST_ID
};

/* Known permission plugin ids. */
enum reiser4_perm_plug_id {
	PERM_RWX_ID		= 0x0,
	PERM_LAST_ID
};

/* Known stat data extension plugin ids. */
enum reiser4_sdext_plug_id {
	SDEXT_LW_ID	        = 0x0,
	SDEXT_UNIX_ID		= 0x1,
	SDEXT_LT_ID             = 0x2,
	SDEXT_SYMLINK_ID	= 0x3,
	SDEXT_PLUG_ID		= 0x4,
	SDEXT_GEN_FLAGS_ID      = 0x5,
	SDEXT_CAPS_ID           = 0x6,
	SDEXT_LARGE_TIMES_ID    = 0x7,
	SDEXT_LAST_ID
};

typedef enum reiser4_sdext_plug_id reiser4_sdext_plug_id_t;

/* Known format plugin ids. */
enum reiser4_format_plug_id {
	FORMAT_REISER40_ID	= 0x0,
	FORMAT_LAST_ID
};

/* Known oid allocator plugin ids. */
enum reiser4_oid_plug_id {
	OID_REISER40_ID		= 0x0,
	OID_LAST_ID
};

/* Known block allocator plugin ids. */
enum reiser4_alloc_plug_id {
	ALLOC_REISER40_ID	= 0x0,
	ALLOC_LAST_ID
};

/* Known journal plugin ids. */
enum reiser4_journal_plug_id {
	JOURNAL_REISER40_ID	= 0x0,
	JOURNAL_LAST_ID
};

/* Known key plugin ids. All these plugins are virtual in the sense they not
   exist in kernel and needed in reiser4progs because we need them working with
   both keys policy (large and short) without recompiling. */
enum reiser4_key_plug_id {
	KEY_SHORT_ID		= 0x0,
	KEY_LARGE_ID		= 0x1,
	KEY_LAST_ID
};

typedef struct reiser4_plug reiser4_plug_t;

#define INVAL_PTR	        ((void *)-1)
#define INVAL_PID	        ((rid_t)~0)

/* Type for key which is used both by library and plugins. */
struct key_entity {
	reiser4_plug_t *plug;
	d64_t body[4];
	uint32_t adjust;
};

typedef struct key_entity key_entity_t;

/* Known key types. */
enum key_type {
	KEY_FILENAME_TYPE       = 0x0,
	KEY_STATDATA_TYPE       = 0x1,
	KEY_ATTRNAME_TYPE       = 0x2,
	KEY_ATTRBODY_TYPE       = 0x3,
	KEY_FILEBODY_TYPE       = 0x4,
	KEY_LAST_TYPE
};

typedef enum key_type key_type_t;

/* Known print options for key. */
enum print_options {
	PO_DEFAULT              = 0x0,
	PO_INODE                = 0x1
};

typedef enum print_options print_options_t;

/* Type for describing reiser4 objects (like format, block allocator, etc)
   inside the library, created by plugins themselves. */
struct generic_entity {
	reiser4_plug_t *plug;
};

typedef struct generic_entity generic_entity_t;

/* Node plugins entity. */
struct node_entity {
	reiser4_plug_t *plug;
	aal_block_t *block;
};

typedef struct node_entity node_entity_t;

typedef struct node node_t;
typedef struct place place_t;

/* Tree coord struct. */
struct place {
	node_t *node;
	pos_t pos;

	void *body;
	uint32_t len;
	key_entity_t key;
	aal_block_t *block;
	reiser4_plug_t *plug;
};

/* Reiser4 in-memory node structure. */
struct node {
	/* Node entity. Node plugin initializes this value and return it back in
	   node initializing time. This node entity is used for performing all
	   on-node actions. */
	node_entity_t *entity;

	/* Place in parent node */
	place_t p;

	/* Reference to tree if node is attached to tree. Sometimes node needs
	   access tree and tree functions. */
	void *tree;
	
	/* Reference to left neighbour. It is used for establishing silbing
	   links among nodes in memory tree cache. */
	node_t *left;

	/* Reference to right neighbour. It is used for establishing silbing
	   links among nodes in memory tree cache. */
	node_t *right;
	
	/* Usage counter to prevent releasing used nodes */
	signed int counter;

#ifndef ENABLE_STAND_ALONE
	/* Applications using this library sometimes need to embed information
	   into the objects of our library for their own use. */
	void *data;
#endif
};

/* Object info struct contains the main information about a reiser4
   object. These are: its key, parent key and coord of first item. */
struct object_info {
	void *tree;
	place_t start;
	
	key_entity_t object;
	key_entity_t parent;
};

typedef struct object_info object_info_t;

/* Object plugin entity. */
struct object_entity {
	reiser4_plug_t *plug;
	object_info_t info;
};

typedef struct object_entity object_entity_t;

/* Stat data extension entity. */
struct sdext_entity {
	reiser4_plug_t *plug;

	void *body;
	uint32_t sdlen;
	uint32_t offset;
};

typedef struct sdext_entity sdext_entity_t;

/* Shift flags control shift process */
enum shift_flags {
	/* Allows to try to make shift from the passed node to left neighbour
	   node. */
	SF_ALLOW_LEFT    = 1 << 0,

	/* Allows to try to make shift from the passed node to right neighbour
	   node. */
	SF_ALLOW_RIGHT   = 1 << 1,

	/* Allows to move insert point to one of neighbour nodes during
	   shift. */
	SF_MOVE_POINT    = 1 << 2,

	/* Allows to update insert point during shift. */
	SF_UPDATE_POINT  = 1 << 3,

	/* Controls is shift allowed to merge border items or only whole items
	   may be shifted. Needed for repair code in order to disable merge of
	   checked item and not checked one. */
	SF_ALLOW_MERGE   = 1 << 4,

	/* Controls is shift allowed to allocate new nodes during making
	   space. This is needed sometimes if there is not enough of free space
	   in existent nodes (one insert point points to and its neighbours)*/
	SF_ALLOW_ALLOC   = 1 << 5
};

typedef enum shift_flags shift_flags_t;

#define SF_DEFAULT \
(SF_ALLOW_LEFT | SF_ALLOW_RIGHT | SF_ALLOW_ALLOC | SF_ALLOW_MERGE)

struct shift_hint {
	/* Flag which shows that we need create an item before we will move
	   units into it. That is because node does not contain any items at all
	   or border items are not mergeable. Set and used by shift code. */
	int create;

	/* Shows, that one of neighbour nodes has changed its leftmost key and
	   internal tree should be updated. */
	int update;

	/* Item count and unit count which will be moved. */
	uint32_t items_number;
	uint32_t units_number;

	/* Used for internal shift purposes. */
	uint32_t items_bytes;
	uint32_t units_bytes;

	/* Shift control flags (left shift, move insert point, merge, etc) and
	   shift result flags. The result flags are needed for determining for
	   example was insert point moved to the corresponding neighbour or
	   not. Of course we might use control flags for that, but it would led
	   us to write a lot of useless stuff for saving control flags before
	   modifying it. */
	uint32_t control;
	uint32_t result;

	/* Insert point. It will be modified during shift. */
	pos_t pos;
};

typedef struct shift_hint shift_hint_t;

#if 0
/* Merge hint. It is used in merging two items in repair code. Merge is process
   of fusing two items, which are overlapped by keys. */
struct merge_hint {	
	uint32_t dst_count;
	uint32_t src_count;
	int32_t  len_delta;
	
	key_entity_t start, end;
	
	/* Fields bellow are only related to extent prep_merge() and
	   merge_units() operations. */
	
	/* Offset in blocks in the start and end units of dst and src */
	uint64_t dst_tail, src_tail;
	uint64_t dst_head, src_head;

	/* Should be dst head and tail splitted into 2 units while performing
	   merge_units() operation. */
	bool_t head, tail;
};

typedef struct merge_hint merge_hint_t;
#endif

/* Different hints used for getting data to/from corresponding objects. */
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
/* These fields should be changed to what proper description of needed
   extensions. */
struct statdata_hint {
	
	/* Extensions mask */
	uint64_t extmask;
    
	/* Stat data extensions */
	void *ext[60];
};

typedef struct statdata_hint statdata_hint_t;

enum entry_type {
	ET_NAME	= 0,
	ET_SPCL	= 1
};

typedef enum entry_type entry_type_t;

struct entry_hint {
	/* Entry metadata size. Filled by rem_entry and add_entry. */
	uint16_t len;
	
	/* Tree coord entry lies at. Filled by dir plugin's lookup. */
	place_t place;
	
	/* Entry key within the current directory */
	key_entity_t offset;

	/* The stat data key of the object entry points to */
	key_entity_t object;

	/* Entry type (name or special), filled by readdir */
	entry_type_t type;

	/* Name of entry */
	char name[256];
};

typedef struct entry_hint entry_hint_t;

/* Object hint. It is used to bring all about object information to object
   plugin to create appropriate object by it. */
struct object_hint {

	/* Stat data related fields. */
	struct {
		/* Additional mode to be masked to stat data mode. This is
		   needed for special files, but mat be used somewhere else. */
		uint32_t mode;
		
		/* Stata plugin id. */
		rid_t statdata;
	} label;

	/* Hint for a file body */
	union {

		/* Plugin ids for the directory body */
		struct {
			rid_t hash;
			rid_t direntry;
		} dir;
	
		/* Plugin id for the regular file body */
		struct {
			rid_t tail;
			rid_t extent;
			rid_t policy;
		} reg;

		/* rdev field for special file. Nevertheless, it is stored
		   inside special file stat data field, we put it to @body
		   definition, because it is the special file essence. */
		struct {
			uint64_t rdev;
		} spl;

		/* Symlink data */
		char *sym;
	} body;
    
	/* The plugin in use */
	reiser4_plug_t *plug;

	/* Parent object key. */
	key_entity_t *parent;
};

typedef struct object_hint object_hint_t;

/* Bits for entity state field. For now here is only "dirty" bit, but possible
   and other ones. */
enum entity_state {
	ENTITY_DIRTY = 0
};

typedef enum entity_state entity_state_t;

/* Type for region enumerating callback functions. */
typedef errno_t (*region_func_t) (void *, uint64_t,
				  uint64_t, void *);

/* Function definitions for enumeration item metadata and data. */
typedef errno_t (*place_func_t) (void *, place_t *, void *);
typedef errno_t (*layout_func_t) (void *, region_func_t, void *);
typedef errno_t (*metadata_func_t) (void *, place_func_t, void *);

/* This structure contains fields which describe an item or unit to be inserted
   into the tree. This is used for all tree modification purposes like
   insertitem, or write some file data. */ 
struct trans_hint {
	/* Overhead of data to be inserted. This is needed for the case when we
	   insert directory item and tree should know how much space should be
	   prepared in the tree (ohd + len), but we don't need overhead for
	   updating stat data bytes field. Set by estimate. */
	uint32_t overhead;
	
	/* Length of the data to be inserted/removed. Set by estimate. */
	int32_t len;

	/* Value needed for updating bytes field in stat data. Set by estimate. */
	uint64_t bytes;

	/* This is opaque pointer to item type specific information */
	void *specific;

	/* The storage tree instance */
	void *tree;

	/* Count of units to be inserted into the tree */
	uint64_t count;

	/* The key of item/unit to be inserted */
	key_entity_t offset;

	/* Max real key. Needed for extents only. Set by estimate. */
	key_entity_t maxkey;

	/* Flags specific for the operation, set at prepare stage. */
	uint16_t flags;
	
	/* Count of handled blocks in the first and the last extent unit. */
	uint64_t head, tail;
	
	/* Plugin to be used for working with item */
	reiser4_plug_t *plug;

	/* Hook, which lets know, that passed block region is removed. Used for
	   releasing unformatted blocks during tail converion, etc. */
	region_func_t region_func;

	/* Hook called onto each create item during write flow. */
	place_func_t place_func;

	/* Related opaque data. May be used for passing something to
	   remove_hook(). */
	void *data;
};

typedef struct trans_hint trans_hint_t;

/* This structure contains related to tail conversion. */
struct conv_hint {
	/* Chuck size to be used for tail convertion. */
	uint32_t chunk;
	
	/* New bytes value */
	uint64_t bytes;

	/* Bytes to be converted. */
	uint64_t count;

	/* File will be converted starting from this key. */
	key_entity_t offset;
	
	/* Plugin item will be converted to. */
	reiser4_plug_t *plug;

	/* Callback function caled onto each new created item during tail
	   conversion. */
	place_func_t place_func;
};

typedef struct conv_hint conv_hint_t;

/* Filesystem description. */
struct fs_desc {
	uint16_t policy;
	uint32_t blksize;
	aal_device_t *device;
};

typedef struct fs_desc fs_desc_t;

struct reiser4_key_ops {
	/* Cleans key up. Actually it just memsets it by zeros, but more smart
	   behavior may be implemented. */
	void (*clean) (key_entity_t *);

	/* Function for dermining is key contains direntry name hashed or
	   not? */
	int (*hashed) (key_entity_t *);

	/* Returns minimal key for this key-format */
	key_entity_t *(*minimal) (void);
    
	/* Returns maximal key for this key-format */
	key_entity_t *(*maximal) (void);

	/* Returns key size for particular key-format */
	uint32_t (*bodysize) (void);

	/* Compares two keys by comparing its all components. This function
	   accepts not key entities, but key bodies. This is needed in order to
	   avoid memory copying in some cases. For instance when we look into
	   node and try to find position by key, we prefer to pass to comraw()
	   pointers to key bodies, than to copy tjem to new created key
	   entities. */
	int (*compraw) (void *, void *);

	/* Compares two keys by comparing its all components. */
	int (*compfull) (key_entity_t *, key_entity_t *);

	/* Compares two keys by comparing locality and objectid. */
	int (*compshort) (key_entity_t *, key_entity_t *);
	
	/* Copyies src key to dst one */
	errno_t (*assign) (key_entity_t *, key_entity_t *);
	
	/* Builds generic key (statdata, file body, etc). That is build key by
	   all its components. */
	errno_t (*build_generic) (key_entity_t *, key_type_t,
				  uint64_t, uint64_t, uint64_t,
				  uint64_t);

	/* Builds key used for directory entries access. It uses name and hash
	   plugin to build hash and put it to key offset component. */
	errno_t (*build_hashed) (key_entity_t *, reiser4_plug_t *,
				 uint64_t, uint64_t, char *);
	
	/* Gets/sets key type (minor in reiser4 notation). */	
	void (*set_type) (key_entity_t *, key_type_t);
	key_type_t (*get_type) (key_entity_t *);

	/* Gets/sets key locality. */
	void (*set_locality) (key_entity_t *, uint64_t);
	uint64_t (*get_locality) (key_entity_t *);
    
	/* Gets/sets key locality. */
	void (*set_ordering) (key_entity_t *, uint64_t);
	uint64_t (*get_ordering) (key_entity_t *);
    
	/* Gets/sets key objectid. */
	void (*set_objectid) (key_entity_t *, uint64_t);
	uint64_t (*get_objectid) (key_entity_t *);

	/* Gets/sets key full objectid */
	void (*set_fobjectid) (key_entity_t *, uint64_t);
	uint64_t (*get_fobjectid) (key_entity_t *);

	/* Gets/sets key offset */
	void (*set_offset) (key_entity_t *, uint64_t);
	uint64_t (*get_offset) (key_entity_t *);

	/* Extracts name from passed key. */
	char *(*get_name) (key_entity_t *, char *);

#ifndef ENABLE_STAND_ALONE
	/* Gets/sets directory key hash */
	void (*set_hash) (key_entity_t *, uint64_t);
	uint64_t (*get_hash) (key_entity_t *);
	
	/* Prints key into specified buffer */
	errno_t (*print) (key_entity_t *, aal_stream_t *,
			  uint16_t);

	/* Check key body for validness. */
	errno_t (*check_struct) (key_entity_t *);
#endif
};

typedef struct reiser4_key_ops reiser4_key_ops_t;

struct reiser4_object_ops {
	/* Loads object stat data to passed hint. */
	errno_t (*stat) (object_entity_t *, statdata_hint_t *);

#ifndef ENABLE_STAND_ALONE
	/* These methods change @nlink value of passed @entity. */
	errno_t (*link) (object_entity_t *);
	errno_t (*unlink) (object_entity_t *);
	uint32_t (*links) (object_entity_t *);

	/* Establish parent child relationship. */
	errno_t (*attach) (object_entity_t *, object_entity_t *);
	errno_t (*detach) (object_entity_t *, object_entity_t *);

	/* Updates object stat data from passed hint. */
	errno_t (*update) (object_entity_t *, statdata_hint_t *);
	
	/* Creates new file with passed parent and object keys. */
	object_entity_t *(*create) (object_info_t *, object_hint_t *);

	/* Delete file body and stat data if any. */
	errno_t (*clobber) (object_entity_t *);

	/* Writes the data to file from passed buffer. */
	int64_t (*write) (object_entity_t *, void *, uint64_t);

	/* Directory specific methods */
	errno_t (*add_entry) (object_entity_t *, entry_hint_t *);
	errno_t (*rem_entry) (object_entity_t *, entry_hint_t *);
	errno_t (*build_entry) (object_entity_t *, entry_hint_t *);
	
	/* Truncates file at current offset onto passed units. */
	errno_t (*truncate) (object_entity_t *, uint64_t);

	/* Function for going through all metadata blocks specfied file
	   occupied. It is needed for accessing file's metadata. */
	errno_t (*metadata) (object_entity_t *, place_func_t, void *);
	
	/* Function for going through the all data blocks specfied file
	   occupies. It is needed for the purposes like data fragmentation
	   measuring, etc. */
	errno_t (*layout) (object_entity_t *, region_func_t, void *);

	/* Converts file body to item denoted by @plug. */
	errno_t (*convert) (object_entity_t *, reiser4_plug_t *plug);
	
	/* Checks and recover the structure of the object. */
	errno_t (*check_struct) (object_entity_t *, place_func_t, 
				 void *, uint8_t);
	
	/* Checks attach of the @object to the @parent. */
	errno_t (*check_attach) (object_entity_t *, object_entity_t *,
				 uint8_t);
	
	/* Realizes if the object can be of this plugin and can be recovered as
	   a such. */
	object_entity_t *(*recognize) (object_info_t *);
	
	/* Creates the fake object by the gived @info. Needed to recover "/" and
	   "lost+found" direcories if their SD are broken. */
	object_entity_t *(*fake) (object_info_t *);

	/* Forms the correct object from what was openned/checked. */
	errno_t (*form) (object_entity_t *);
#endif
	
	/* Change current position to passed value. */
	errno_t (*seek) (object_entity_t *, uint64_t);
	
	/* Opens file with specified key */
	object_entity_t *(*open) (object_info_t *);

	/* Closes previously opened or created directory. */
	void (*close) (object_entity_t *);

	/* Resets internal position. */
	errno_t (*reset) (object_entity_t *);
   
	/* Returns current position in directory. */
	uint64_t (*offset) (object_entity_t *);

	/* Makes lookup inside file */
	lookup_t (*lookup) (object_entity_t *, char *,
			    entry_hint_t *);

	/* Finds actual file stat data (used in symlinks). */
	errno_t (*follow) (object_entity_t *, key_entity_t *,
			   key_entity_t *);

	/* Reads the data from file to passed buffer. */
	int64_t (*read) (object_entity_t *, void *, uint64_t);

	/* Directory read method. */
	int32_t (*readdir) (object_entity_t *, entry_hint_t *);

	/* Return current position in directory. */
	errno_t (*telldir) (object_entity_t *, key_entity_t *);

	/* Change current position in directory. */
	errno_t (*seekdir) (object_entity_t *, key_entity_t *);
};

typedef struct reiser4_object_ops reiser4_object_ops_t;

struct item_balance_ops {
	/* Returns unit count in item passed place point to. */
	uint32_t (*units) (place_t *);
	
	/* Makes lookup for passed key. */
	lookup_t (*lookup) (place_t *, key_entity_t *, bias_t);
	
#ifndef ENABLE_STAND_ALONE
	/* Fuses two neighbour items in the same node. Returns space released
	   Needed for fsck. */
	int32_t (*fuse) (place_t *, place_t *);
	
	/* Checks if items mergeable, that is if unit of one item can belong to
	   another one. Returns 1 if so, 0 otherwise. */
	int (*mergeable) (place_t *, place_t *);

	/* Estimates shift operation. */
	errno_t (*prep_shift) (place_t *, place_t *, shift_hint_t *);
	
	/* Performs shift of units from passed @src item to @dst item. */
	errno_t (*shift_units) (place_t *, place_t *, shift_hint_t *);

	/* Set the key of a particular unit of the item. */
	errno_t (*update_key) (place_t *, key_entity_t *);
	
	/* Get the max real key which is stored in the item. */
	errno_t (*maxreal_key) (place_t *, key_entity_t *);
#endif

	/* Get the key of a particular unit of the item. */
	errno_t (*fetch_key) (place_t *, key_entity_t *);

	/* Get the max key which could be stored in the item of this type. */
	errno_t (*maxposs_key) (place_t *, key_entity_t *);
};

typedef struct item_balance_ops item_balance_ops_t;

struct item_object_ops {
	/* Get plugin id of specified type if stored in stat data item. */
	rid_t (*object_plug) (place_t *, rid_t);
		
	/* Reads passed amount of bytes from the item. */
	int64_t (*read_units) (place_t *, trans_hint_t *);
		
	/* Fetches one or more units at passed @place to passed hint. */
	int64_t (*fetch_units) (place_t *, trans_hint_t *);

#ifndef ENABLE_STAND_ALONE
	/* Estimates write operation. */
	errno_t (*prep_write) (place_t *, trans_hint_t *);

	/* Writes data to item. */
	int64_t (*write_units) (place_t *, trans_hint_t *);

	/* Estimates insert operation. */
	errno_t (*prep_insert) (place_t *, trans_hint_t *);

	/* Inserts some amount of units described by passed hint into passed
	   item denoted by place. */
	int64_t (*insert_units) (place_t *, trans_hint_t *);

	/* Removes specified unit from the item. */
	errno_t (*remove_units) (place_t *, trans_hint_t *);

	/* Updates unit at passed place by data from passed hint. */
	int64_t (*update_units) (place_t *, trans_hint_t *);

	/* Cuts out some amount of data */
	int64_t (*trunc_units) (place_t *, trans_hint_t *);

	/* Goes through all blocks item points to. */
	errno_t (*layout) (place_t *, region_func_t, void *);
		
	/* Gets the size of the data item keeps. */
	uint64_t (*size) (place_t *);
	
	/* Gets the amount of bytes data item keeps takes on the disk. */
	uint64_t (*bytes) (place_t *);
#endif
};

typedef struct item_object_ops item_object_ops_t;

struct item_repair_ops {
#ifndef ENABLE_STAND_ALONE
	/* Estimate merge operation. */
	errno_t (*prep_merge) (place_t *, trans_hint_t *);

	/* Copies some amount of units from @src to @dst with partial
	   overwritting. */
	errno_t (*merge) (place_t *, trans_hint_t *);

	/* Checks the item structure. */
	errno_t (*check_struct) (place_t *, uint8_t);
	
	/* Does some specific actions if a block the item points to is wrong. */
	errno_t (*check_layout) (place_t *, region_func_t,
				 void *, uint8_t);

	errno_t (*pack) (place_t *, aal_stream_t *);
	errno_t (*unpack) (place_t *, aal_stream_t *);
#endif
};

typedef struct item_repair_ops item_repair_ops_t;

struct item_debug_ops {
#ifndef ENABLE_STAND_ALONE
	/* Prints item into specified buffer. */
	errno_t (*print) (place_t *, aal_stream_t *, uint16_t);
#endif
};

typedef struct item_debug_ops item_debug_ops_t;

struct item_tree_ops {
	/* Return block number from passed place. Place is nodeptr item. */
	blk_t (*down_link) (place_t *);

#ifndef ENABLE_STAND_ALONE
	/* Update link block number. */
	errno_t (*update_link) (place_t *, blk_t);
#endif
};

typedef struct item_tree_ops item_tree_ops_t;

struct reiser4_item_ops {
	item_tree_ops_t *tree;
	item_debug_ops_t *debug;
	item_object_ops_t *object;
	item_repair_ops_t *repair;
	item_balance_ops_t *balance;
};

typedef struct reiser4_item_ops reiser4_item_ops_t;

/* Stat data extension plugin */
struct reiser4_sdext_ops {
#ifndef ENABLE_STAND_ALONE
	/* Initialize stat data extension data at passed pointer. */
	errno_t (*init) (void *, void *);

	/* Prints stat data extension data into passed buffer. */
	errno_t (*print) (void *, aal_stream_t *, uint16_t);

	/* Checks sd extension content. */
	errno_t (*check_struct) (sdext_entity_t *, uint8_t);
#endif
	/* Reads stat data extension data. */
	errno_t (*open) (void *, void *);

	/* Returns length of the extension. */
	uint16_t (*length) (void *);
};

typedef struct reiser4_sdext_ops reiser4_sdext_ops_t;

/* Node plugin operates on passed block. It doesn't any initialization, so it
   hasn't close method and all its methods accepts first argument aal_block_t,
   not initialized previously hypothetic instance of node. */
struct reiser4_node_ops {
#ifndef ENABLE_STAND_ALONE
	/* Get node state flags and set them back. */
	uint32_t (*get_state) (node_entity_t *);
	void (*set_state) (node_entity_t *, uint32_t);

	/* Makes clone of passed node. */
	errno_t (*clone) (node_entity_t *, node_entity_t *);

	/* Performs shift of items and units. */
	errno_t (*shift) (node_entity_t *, node_entity_t *, 
			  shift_hint_t *);

	/* Fuses two neighbour items in passed node at passed positions. */
	errno_t (*fuse) (node_entity_t *, pos_t *, pos_t *);
    
	/* Checks thoroughly the node structure and fixes what needed. */
	errno_t (*check_struct) (node_entity_t *, uint8_t);

	/* Packing/unpacking metadata. */
	node_entity_t *(*unpack) (aal_block_t *, reiser4_plug_t *,
				  aal_stream_t *, int);
	
	errno_t (*pack) (node_entity_t *, aal_stream_t *, int);

	/* Prints node into given buffer. */
	errno_t (*print) (node_entity_t *, aal_stream_t *,
			  uint32_t, uint32_t, uint16_t);
    
	/* Returns item's overhead. */
	uint16_t (*overhead) (node_entity_t *);

	/* Returns item's max size. */
	uint16_t (*maxspace) (node_entity_t *);
    
	/* Returns free space in the node. */
	uint16_t (*space) (node_entity_t *);

	/* Inserts item at specified pos. */
	errno_t (*insert) (node_entity_t *, pos_t *,
			   trans_hint_t *);
    
	/* Writes data to the node. */
	int64_t (*write) (node_entity_t *, pos_t *,
			  trans_hint_t *);

	/* Truncate item at passed pos. */
	int64_t (*trunc) (node_entity_t *, pos_t *,
			  trans_hint_t *);

	/* Removes item/unit at specified pos. */
	errno_t (*remove) (node_entity_t *, pos_t *,
			   trans_hint_t *);

	/* Shrinks node without calling any item methods. */
	errno_t (*shrink) (node_entity_t *, pos_t *,
			   uint32_t, uint32_t);

	/* Merge 2 items -- insert/overwrite @src_entity parts to
	   @dst_entity. */
	errno_t (*merge) (node_entity_t *, pos_t *, 
			  trans_hint_t *);

	/* Copies items from @src_entity to @dst_entity. */
	errno_t (*copy) (node_entity_t *, pos_t *,
			 node_entity_t *, pos_t *,
			 uint32_t);
	
	/* Expands node. */
	errno_t (*expand) (node_entity_t *, pos_t *,
			   uint32_t, uint32_t);

	errno_t (*set_key) (node_entity_t *, pos_t *,
			    key_entity_t *);

	void (*set_level) (node_entity_t *, uint8_t);
	void (*set_mstamp) (node_entity_t *, uint32_t);
	void (*set_fstamp) (node_entity_t *, uint64_t);

	/* Changes node location */
	void (*move) (node_entity_t *, blk_t);

	/* Get mkfs and flush stamps */
	uint32_t (*get_mstamp) (node_entity_t *);
    	uint64_t (*get_fstamp) (node_entity_t *);
	
	/* Get/set/test item flags. */
	void (*set_flag) (node_entity_t *, uint32_t, uint16_t);	
	void (*clear_flag) (node_entity_t *, uint32_t, uint16_t);	
	bool_t (*test_flag) (node_entity_t *, uint32_t, uint16_t);

	/* Saves node to device */
	errno_t (*sync) (node_entity_t *);

	/* Makes fresh node (zero items, etc) */
	errno_t (*fresh) (node_entity_t *, uint8_t);
#endif

	/* Initializes node with passed block and key plugin. */
	node_entity_t *(*init) (aal_block_t *,
				reiser4_plug_t *);
	
	/* Destroys the node entity. */
	errno_t (*fini) (node_entity_t *);

	/* Fetches item data to passed @place */
	errno_t (*fetch) (node_entity_t *, pos_t *,
			  place_t *);
	
	/* Returns item count */
	uint32_t (*items) (node_entity_t *);
    
	/* Makes lookup inside node by specified key */
	lookup_t (*lookup) (node_entity_t *, key_entity_t *, 
			    bias_t, pos_t *);
    
	/* Gets/sets key at pos */
	errno_t (*get_key) (node_entity_t *, pos_t *,
			    key_entity_t *);

	/* Return node level. */
	uint8_t (*get_level) (node_entity_t *);
};

typedef struct reiser4_node_ops reiser4_node_ops_t;

/* Hash plugin operations. */
struct reiser4_hash_ops {
	uint64_t (*build) (char *, uint32_t);
};

typedef struct reiser4_hash_ops reiser4_hash_ops_t;

/* Disk-format plugin */
struct reiser4_format_ops {
	uint64_t (*get_flags) (generic_entity_t *);
	
#ifndef ENABLE_STAND_ALONE
	void (*set_flags) (generic_entity_t *, uint64_t);
	
	/* Called during filesystem creating. It forms format-specific super
	   block, initializes plugins and calls their create method. */
	generic_entity_t *(*create) (fs_desc_t *, uint64_t);

	/* Save the important permanent info about the format into the stream 
	   to be backuped on the fs. */
	errno_t (*backup) (generic_entity_t *, aal_stream_t *);
	
	/* Save format data to device. */
	errno_t (*sync) (generic_entity_t *);

	/* Change entity state (dirty, etc) */
	uint32_t (*get_state) (generic_entity_t *);
	void (*set_state) (generic_entity_t *, uint32_t);

	/* Format pack/unpack methods. */
	generic_entity_t *(*unpack) (fs_desc_t *, aal_stream_t *);
	errno_t (*pack) (generic_entity_t *, aal_stream_t *);
	
	/* Update only fields which can be changed after journal replay in
	   memory to avoid second checking. */
	errno_t (*update) (generic_entity_t *);
	    
	/* Checks thoroughly the format structure and fixes what needed. */
	errno_t (*check_struct) (generic_entity_t *, uint8_t);

	/* Prints all useful information about the format */
	errno_t (*print) (generic_entity_t *, aal_stream_t *, uint16_t);
    
	void (*set_len) (generic_entity_t *, uint64_t);
	void (*set_root) (generic_entity_t *, uint64_t);
	void (*set_free) (generic_entity_t *, uint64_t);
	void (*set_stamp) (generic_entity_t *, uint32_t);
	void (*set_policy) (generic_entity_t *, uint16_t);
	void (*set_height) (generic_entity_t *, uint16_t);

	/* Return plugin ids for journal, block allocator, and oid allocator
	   components. */
	rid_t (*journal_pid) (generic_entity_t *);
	rid_t (*alloc_pid) (generic_entity_t *);
	rid_t (*oid_pid) (generic_entity_t *);

	/* Format enumerator function. */
	errno_t (*layout) (generic_entity_t *, region_func_t, void *);

	/* Checks format-specific super block for validness. Also checks whether
	   filesystem objects lie in valid places. For example, format-specific
	   super block for format40 must lie in 17-th block for 4096 byte long
	   blocks. */
	errno_t (*valid) (generic_entity_t *);
#endif
	/* Called during filesystem opening (mounting). It reads format-specific
	   super block and initializes plugins suitable for this format. */
	generic_entity_t *(*open) (fs_desc_t *);
    
	/* Closes opened or created previously filesystem. Frees all assosiated
	   memory. */
	void (*close) (generic_entity_t *);

	/* Get tree root block number from format. */
	uint64_t (*get_root) (generic_entity_t *);

	/* Get tree height from format. */
	uint16_t (*get_height) (generic_entity_t *);

#ifndef ENABLE_STAND_ALONE
	/* Gets start of the filesystem. */
	uint64_t (*start) (generic_entity_t *);

	/* Format length in blocks. */
	uint64_t (*get_len) (generic_entity_t *);

	/* Number of free blocks. */
	uint64_t (*get_free) (generic_entity_t *);

	/* Return mkfs stamp. */
	uint32_t (*get_stamp) (generic_entity_t *);

	/* Return policy flags (tail, extents, etc). */
	uint16_t (*get_policy) (generic_entity_t *);

	/* Returns area where oid data lies in */
	void (*oid_area) (generic_entity_t *, void **, uint32_t *);
#endif
};

typedef struct reiser4_format_ops reiser4_format_ops_t;

#ifndef ENABLE_STAND_ALONE
struct reiser4_oid_ops {
	/* Opens oid allocator on passed format entity. */
	generic_entity_t *(*open) (generic_entity_t *);

	/* Closes passed instance of oid allocator */
	void (*close) (generic_entity_t *);
    
	/* Creates oid allocator on passed format entity. */
	generic_entity_t *(*create) (generic_entity_t *);

	/* Synchronizes oid allocator */
	errno_t (*sync) (generic_entity_t *);

	errno_t (*layout) (generic_entity_t *,
			   region_func_t, void *);

	/* Entity state functions. */
	uint32_t (*get_state) (generic_entity_t *);
	void (*set_state) (generic_entity_t *, uint32_t);

	/* Gets next object id */
	oid_t (*next) (generic_entity_t *);

	/* Gets next object id */
	oid_t (*allocate) (generic_entity_t *);

	/* Releases passed object id */
	void (*release) (generic_entity_t *, oid_t);
    
	/* Returns the number of used object ids */
	uint64_t (*used) (generic_entity_t *);
    
	/* Returns the number of free object ids */
	uint64_t (*free) (generic_entity_t *);

	/* Prints oid allocator data */
	errno_t (*print) (generic_entity_t *, aal_stream_t *,
			  uint16_t);

	/* Makes check for validness */
	errno_t (*valid) (generic_entity_t *);
	
	/* Root locality and objectid */
	oid_t (*root_locality) (generic_entity_t *);
	oid_t (*root_objectid) (generic_entity_t *);
};

typedef struct reiser4_oid_ops reiser4_oid_ops_t;

struct reiser4_alloc_ops {
	/* Functions for create and open block allocator. */
	generic_entity_t *(*open) (fs_desc_t *, uint64_t);
	generic_entity_t *(*create) (fs_desc_t *, uint64_t);

	/* Closes block allocator. */
	void (*close) (generic_entity_t *);

	/* Saves block allocator data to desired device. */
	errno_t (*sync) (generic_entity_t *);

	/* Make dirty and clean functions. */
	uint32_t (*get_state) (generic_entity_t *);
	void (*set_state) (generic_entity_t *, uint32_t);
	
	/* Format pack/unpack methods. */
	errno_t (*pack) (generic_entity_t *, aal_stream_t *);
	generic_entity_t *(*unpack) (fs_desc_t *, aal_stream_t *);
	
	/* Assign the bitmap to the block allocator */
	errno_t (*assign) (generic_entity_t *, void *);

	/* Extract block allocator data into passed bitmap */
	errno_t (*extract) (generic_entity_t *, void *);
	
	/* Returns number of used blocks */
	uint64_t (*used) (generic_entity_t *);

	/* Returns number of unused blocks */
	uint64_t (*free) (generic_entity_t *);

	/* Checks blocks allocator on validness */
	errno_t (*valid) (generic_entity_t *);

	/* Prints block allocator data */
	errno_t (*print) (generic_entity_t *, aal_stream_t *,
			  uint16_t);

	/* Calls func for each block in block allocator */
	errno_t (*layout) (generic_entity_t *,
			   region_func_t, void *);
	
	/* Checks if passed range of blocks used */
	int (*occupied) (generic_entity_t *, uint64_t,
			 uint64_t);
    	
	/* Checks if passed range of blocks unused */
	int (*available) (generic_entity_t *, uint64_t,
			  uint64_t);

	/* Marks passed block as used */
	errno_t (*occupy) (generic_entity_t *, uint64_t,
			   uint64_t);

	/* Tries to allocate passed amount of blocks */
	uint64_t (*allocate) (generic_entity_t *, uint64_t *,
			      uint64_t);
	
	/* Deallocates passed blocks */
	errno_t (*release) (generic_entity_t *, uint64_t,
			    uint64_t);

	/* Calls func for all not reliable regions. */
	errno_t (*layout_bad) (generic_entity_t *,
			       region_func_t, void *);
	
	/* Calls func for the region the blk lies in. */
	errno_t (*region) (generic_entity_t *, blk_t,
			   region_func_t, void *);
};

typedef struct reiser4_alloc_ops reiser4_alloc_ops_t;

struct reiser4_journal_ops {
	/* Opens journal on specified device. */
	generic_entity_t *(*open) (fs_desc_t *, generic_entity_t *,
				   uint64_t, uint64_t);

	/* Creates journal on specified device. */
	generic_entity_t *(*create) (fs_desc_t *, generic_entity_t *,
				     uint64_t, uint64_t);

	/* Returns the device journal lies on */
	aal_device_t *(*device) (generic_entity_t *);
    
	/* Frees journal instance */
	void (*close) (generic_entity_t *);

	/* Checks journal metadata on validness */
	errno_t (*valid) (generic_entity_t *);
    
	/* Synchronizes journal */
	errno_t (*sync) (generic_entity_t *);

	/* Functions for set/get object state (dirty, clean, etc). */
	uint32_t (*get_state) (generic_entity_t *);
	void (*set_state) (generic_entity_t *, uint32_t);
	
	/* Replays the journal */
	errno_t (*replay) (generic_entity_t *);

	/* Prints journal content */
	errno_t (*print) (generic_entity_t *,
			  aal_stream_t *, uint16_t);
	
	/* Checks thoroughly the journal structure. */
	errno_t (*check_struct) (generic_entity_t *,
				 layout_func_t, void *);

	/* Calls func for each block in block allocator. */
	errno_t (*layout) (generic_entity_t *, region_func_t,
			   void *);
};

typedef struct reiser4_journal_ops reiser4_journal_ops_t;

/* Tail policy plugin operations. */
struct reiser4_policy_ops {
	int (*tails) (uint64_t);
};

typedef struct reiser4_policy_ops reiser4_policy_ops_t;
#endif

#define PLUG_MAX_PATH   1024
#define PLUG_MAX_DESC	64
#define PLUG_MAX_LABEL	22

typedef struct reiser4_core reiser4_core_t;

/* Plugin init() and fini() function types. They are used for calling these
   functions during plugin initialization. */
typedef errno_t (*plug_fini_t) (reiser4_core_t *);
typedef reiser4_plug_t *(*plug_init_t) (reiser4_core_t *);
typedef errno_t (*plug_func_t) (reiser4_plug_t *, void *);

/* Plugin class descriptor. Used for loading plugins. */
struct plug_class {
	/* Plugin initialization routine. */
	plug_init_t init;
	
	/* Plugin finalization routine. */
	plug_fini_t fini;

#ifndef ENABLE_STAND_ALONE
	/* Plugin location, that is address of its init() function. */
	char loc[PLUG_MAX_PATH];
#endif
};

typedef struct plug_class plug_class_t;

struct plug_ident {
	/* Plugin id, type and group. */
	rid_t id;
	rid_t group;
	rid_t type;
};

typedef struct plug_ident plug_ident_t;

#ifndef ENABLE_STAND_ALONE
#define class_init {NULL, NULL, ""}
#else
#define class_init {NULL, NULL}
#endif

struct reiser4_plug {
	/* Plugin class. This will be used by plugin factory for initializing
	   plugin. */
	plug_class_t cl;

	/* Plugin id. This will be used for looking for a plugin. */
	plug_ident_t id;
	
#ifndef ENABLE_STAND_ALONE
	/* Plugin label (name). */
	const char label[PLUG_MAX_LABEL];
	
	/* Short plugin description. */
	const char desc[PLUG_MAX_DESC];
#endif

        /* All possible plugin operations. */
	union {
		reiser4_key_ops_t *key_ops;
		reiser4_item_ops_t *item_ops;
		reiser4_node_ops_t *node_ops;
		reiser4_hash_ops_t *hash_ops;
		reiser4_sdext_ops_t *sdext_ops;
		reiser4_object_ops_t *object_ops;
		reiser4_format_ops_t *format_ops;

#ifndef ENABLE_STAND_ALONE
		reiser4_oid_ops_t *oid_ops;
		reiser4_alloc_ops_t *alloc_ops;
		reiser4_policy_ops_t *policy_ops;
		reiser4_journal_ops_t *journal_ops;
#endif
	} o;
};

/* Macros for dirtying nodes place lie at */
#define place_mkdirty(place) \
        ((place)->block->dirty = 1)

#define place_mkclean(place) \
        ((place)->block->dirty = 0)

#define place_isdirty(place) \
        ((place)->block->dirty)

struct tree_ops {
	/* Checks if passed @place points to some real item inside a node. */
	int (*valid) (void *, place_t *);
	
	/* Initializes all item fields in passed place. */
	errno_t (*fetch) (void *, place_t *);

	/* Makes lookup in the tree in order to know where say stat data item of
	   a file realy lies. It is used in all object plugins. */
	lookup_t (*lookup) (void *, key_entity_t *,
			    uint8_t, bias_t, place_t *);

	/* Reads data from the tree. */
	int64_t (*read) (void *, trans_hint_t *);

#ifndef ENABLE_STAND_ALONE
	/* Truncates data from tree. */
	int64_t (*truncate) (void *, trans_hint_t *);
	
	/* Convert some particular place to another plugin. */
	errno_t (*convert) (void *, conv_hint_t *);
	
	/* Writes data to tree. */
	int64_t (*write) (void *, trans_hint_t *);

	/* Inserts item/unit in the tree by calling tree_insert() function, used
	   by all object plugins (dir, file, etc). */
	int64_t (*insert) (void *, place_t *,
			   trans_hint_t *, uint8_t);

	/* Removes item/unit from the tree. It is used in all object plugins for
	   modification purposes. */
	errno_t (*remove) (void *, place_t *, trans_hint_t *);

	/* Functions for getting/setting extent data. */
	aal_block_t *(*get_data) (void *, key_entity_t *);
	
	errno_t (*put_data) (void *, key_entity_t *,
			     aal_block_t *);

	/* Removes data from the cache */
	errno_t (*rem_data) (void *, key_entity_t *);
	
	/* Update the key in the place and the node itsef. */
	errno_t (*update_key) (void *, place_t *, key_entity_t *);
#endif
	/* Returns next items respectively. */
	errno_t (*next) (void *, place_t *, place_t *);
};

typedef struct tree_ops tree_ops_t;

struct factory_ops {
	/* Finds plugin by its attributes (type and id). */
	reiser4_plug_t *(*ifind) (rid_t, rid_t);
	
#ifndef ENABLE_STAND_ALONE
	/* Finds plugin by its type and name. */
	reiser4_plug_t *(*nfind) (char *);
#endif
};

typedef struct factory_ops factory_ops_t;

#ifdef ENABLE_SYMLINKS
struct object_ops {
	errno_t (*resolve) (void *, char *, key_entity_t *,
			    key_entity_t *);
};

typedef struct object_ops object_ops_t;
#endif

#ifndef ENABLE_STAND_ALONE
struct param_ops {
	/* Obtains the param value by its name. */
	uint64_t (*value) (char *);
};

typedef struct param_ops param_ops_t;

struct key_ops {
	char *(*print) (key_entity_t *, uint16_t);
};

typedef struct key_ops key_ops_t;
#endif

/* This structure is passed to all plugins in initialization time and used for
   access libreiser4 factories. */
struct reiser4_core {
	tree_ops_t tree_ops;
	factory_ops_t factory_ops;
	
#ifndef ENABLE_STAND_ALONE
	param_ops_t param_ops;
#endif
	
#ifdef ENABLE_SYMLINKS
	object_ops_t object_ops;
#endif

#ifndef ENABLE_STAND_ALONE
	key_ops_t key_ops;
#endif
};

#define print_key(core, key)                                 \
	((core)->key_ops.print((key), PO_DEFAULT))

#define print_inode(core, key)                               \
	((core)->key_ops.print((key), PO_INODE))

#define ident_equal(ident1, ident2)                          \
	((ident1)->type == (ident2)->type &&		     \
	 (ident1)->id == (ident2)->id)

#define plug_equal(plug1, plug2)                             \
        ident_equal(&((plug1)->id), &((plug2)->id))

/* Makes check is needed method implemengted */
#define plug_call(ops, method, ...) ({                       \
        aal_assert("Method \""#method"\" isn't implemented " \
                   "in "#ops"", ops->method != NULL);        \
        ops->method(__VA_ARGS__);			     \
})

typedef void (*register_builtin_t) (plug_init_t, plug_fini_t);

/* Macro for registering a plugin in plugin factory. It accepts two pointers to
   functions. The first one is pointer to plugin init function and second - to
   plugin finalization function. */
#define plug_register(n, i, f)                                 \
	plug_init_t __##n##_plug_init = i;		       \
	plug_init_t __##n##_plug_fini = f

#endif
