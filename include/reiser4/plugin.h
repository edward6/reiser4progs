/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   plugin.h -- reiser4 plugin known types and macros. */

#ifndef REISER4_PLUGIN_H
#define REISER4_PLUGIN_H

#include <aal/libaal.h>

/* Leaf and twig levels. */
#define LEAF_LEVEL	        1
#define TWIG_LEVEL	        (LEAF_LEVEL + 1)

/* Master related stuff like magic and offset in bytes. These are used by both
   plugins and library itself. */
#define REISER4_MASTER_MAGIC	("ReIsEr4")
#define REISER4_MASTER_OFFSET	(65536)

/* The same for fs stat block. */
#define REISER4_STATUS_BLOCK    (21)
#define REISER4_STATUS_MAGIC    ("ReiSeR4StATusBl")

/* Max number of backups on the reiser4. */
#define REISER4_BACKUPS_MAX	16

/* Root key locality and objectid. This is actually defined in oid plugin,
   but hardcoded here to exclude oid plugin from the stand alone mode at all,
   nothing but these oids is needed there. */
#define REISER4_ROOT_LOCALITY   (0x29)
#define REISER4_ROOT_OBJECTID   (0x2a)

/* Macros for hole and unallocated extents. Used by both plugins (extent40) and
   library itself. */
#define EXTENT_HOLE_UNIT        (0)
#define EXTENT_UNALLOC_UNIT     (1)

/* Defining the types for disk structures. All types like f32_t are fake ones
   and needed to avoid gcc-2.95.x bug with size of typedefined aligned types. */
typedef uint8_t  f8_t;  typedef f8_t  d8_t  __attribute__((aligned(1)));
typedef uint16_t f16_t; typedef f16_t d16_t __attribute__((aligned(2)));
typedef uint32_t f32_t; typedef f32_t d32_t __attribute__((aligned(4)));
typedef uint64_t f64_t; typedef f64_t d64_t __attribute__((aligned(8)));

/* Basic reiser4 types used by both library and plugins. */
typedef uint32_t rid_t;
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

/* Known by library plugin types. */
enum reiser4_plug_type {
	OBJECT_PLUG_TYPE        = 0x0,
	ITEM_PLUG_TYPE          = 0x1,
	NODE_PLUG_TYPE          = 0x2,
	HASH_PLUG_TYPE          = 0x3,
	FIBRE_PLUG_TYPE		= 0x4,
	POLICY_PLUG_TYPE        = 0x5,
	PERM_PLUG_TYPE          = 0x6,
	SDEXT_PLUG_TYPE         = 0x7,
	FORMAT_PLUG_TYPE        = 0x8,
	OID_PLUG_TYPE           = 0x9,
	JNODE_PLUG_TYPE         = 0xa,
	CRYPTO_PLUG_TYPE	= 0xb,
	DIGEST_PLUG_TYPE	= 0xc,
	COMPRESS_PLUG_TYPE	= 0xd,
	
	/* These are not plugins in the kernel. */
	ALLOC_PLUG_TYPE         = 0xe,
	JOURNAL_PLUG_TYPE       = 0xf,
	KEY_PLUG_TYPE           = 0x10,
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
	ITEM_STAT40_ID		= 0x0,
	ITEM_SDE40_ID	        = 0x1,
	ITEM_CDE40_ID	        = 0x2,
	ITEM_NODEPTR40_ID	= 0x3,
	ITEM_ACL40_ID		= 0x4,
	ITEM_EXTENT40_ID	= 0x5,
	ITEM_TAIL40_ID		= 0x6,
	ITEM_CTAIL40_ID		= 0x7,
	ITEM_BLACKBOX40_ID	= 0x8,
	ITEM_LAST_ID
};

/* Known item groups. */
enum reiser4_item_group {
	STAT_ITEM		= 0x0,
	PTR_ITEM		= 0x1,
	DIR_ITEM		= 0x2,
	TAIL_ITEM		= 0x3,
	EXTENT_ITEM		= 0x4,
	PERMISSION_ITEM		= 0x5, /* not used yet */
	SAFE_LINK_ITEM		= 0x6,
	LAST_ITEM
};

typedef enum reiser4_item_group reiser4_item_group_t;

extern const char *reiser4_igname[];
extern const char *reiser4_slink_name[];

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
	TAIL_ALWAYS_ID		= 0x1,
	TAIL_SMART_ID		= 0x2,
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
	SDEXT_FLAGS_ID          = 0x5,
	SDEXT_CAPS_ID		= 0x6,
	SDEXT_CLUSTER_ID	= 0x7,
	SDEXT_CRYPTO_ID		= 0x8,
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

enum reiser4_fibre_plug_id {
	FIBRE_LEXIC_ID		= 0x0,
	FIBRE_DOT_O_ID		= 0x1,
	FIBRE_EXT_1_ID		= 0x2,
	FIBRE_EXT_3_ID		= 0x3,
	FIBRE_LAST_ID
};

typedef enum reiser4_fibre_plug_id reiser4_fibre_plug_id_t;

#define INVAL_PTR	        ((void *)-1)
#define INVAL_PID	        ((rid_t)~0)

/* Type for key which is used both by library and plugins. */
struct reiser4_key {
	reiser4_plug_t *plug;
	d64_t body[4];
#ifndef ENABLE_STAND_ALONE
	uint32_t adjust;
#endif
};

typedef struct reiser4_key reiser4_key_t;

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

/* Tree Plugin SET index. */
enum reiser4_tpset_id {
	TPSET_KEY		= 0x0,
	TPSET_NODE		= 0x1,
	TPSET_NODEPTR		= 0x2,

	TPSET_LAST
};

/* Object Plugin SET index. */
enum reiser4_opset_id {
	OPSET_OBJ		= 0x0,
	OPSET_DIR		= 0x1,
	OPSET_PERM		= 0x2,
	OPSET_POLICY		= 0x3,
	OPSET_HASH		= 0x4,
	OPSET_FIBRE		= 0x5,
	OPSET_STAT		= 0x6,
	OPSET_DIRITEM		= 0x7,
	OPSET_CRYPTO		= 0x8,
	OPSET_DIGEST		= 0x9,
	OPSET_COMPRES		= 0xa,
	
	OPSET_STORE_LAST        = (OPSET_COMPRES + 1),
	
	/* These are not stored on disk in the current implementation. */
	OPSET_CREATE		= 0xb,
	OPSET_MKDIR		= 0xc,
	OPSET_SYMLINK		= 0xd,
	OPSET_MKNODE		= 0xe,
	
#ifndef ENABLE_STAND_ALONE
	OPSET_TAIL		= 0xf,
	OPSET_EXTENT		= 0x10,
	OPSET_ACL		= 0x11,
#endif
	OPSET_LAST
};

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

struct tree_entity {
	/* Plugin SET. Set of plugins needed for the reiser4progs work.
	   Consists of tree-specific plugins and object-specific plugins. */
	reiser4_plug_t *tpset[TPSET_LAST];
	reiser4_plug_t *opset[OPSET_LAST];
};

typedef struct tree_entity tree_entity_t;

typedef struct reiser4_node reiser4_node_t;
typedef struct reiser4_place reiser4_place_t;

#define place_blknr(place) ((place)->node->block->nr)

/* Tree coord struct. */
struct reiser4_place {
	/* Item/unit pos and node it lies in. These fields should be always
	   initialized in a @place instance. */
	pos_t pos;
	reiser4_node_t *node;

	/* Item header stuff. There are item body pointer, length, flags, key
	   and plugin used in item at @pos. These fields are initialized in node
	   method fetch() and stored here to make work with them simpler. */
	void *body;
	uint32_t len;
	uint16_t flags;
	reiser4_key_t key;
	reiser4_plug_t *plug;
};

enum node_flags {
	NF_HEARD_BANSHEE  = 1 << 0,
};

typedef enum node_flags node_flags_t;

/* Reiser4 in-memory node structure. */
struct reiser4_node {
	/* Node plugin. */
	reiser4_plug_t *plug;

	/* Block node lies in. */
	aal_block_t *block;

	/* Reference to tree if node is attached to tree. Sometimes node needs
	   access tree and tree functions. */
	tree_entity_t *tree;
	
	/* Place in parent node. */
	reiser4_place_t p;

	/* Reference to left neighbour. It is used for establishing silbing
	   links among nodes in memory tree cache. */
	reiser4_node_t *left;

	/* Reference to right neighbour. It is used for establishing silbing
	   links among nodes in memory tree cache. */
	reiser4_node_t *right;
	
	/* Usage counter to prevent releasing used nodes. */
	signed int counter;

	/* Key plugin. */
	reiser4_plug_t *kplug;

	/* Node state flags. */
	uint32_t state;

#ifndef ENABLE_STAND_ALONE
	/* Different node flags. */
	uint32_t flags;
	
	/* Applications using this library sometimes need to embed information
	   into the objects of our library for their own use. */
	void *data;
#endif
};

/* Stat data extension entity. */
struct stat_entity {
	reiser4_place_t *place;
	reiser4_plug_t *ext_plug;
	uint32_t offset;
};

typedef struct stat_entity stat_entity_t;

#define stat_body(stat) ((char *)(stat)->place->body + (stat)->offset)

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

	/* Controls if shift allowed to merge border items or only whole items
	   may be shifted. Needed for repair code in order to disable merge of
	   checked item and not checked one. */
	SF_ALLOW_MERGE   = 1 << 4,

	/* Controls if shift allowed to allocate new nodes during making
	   space. This is needed sometimes if there is not enough of free space
	   in existent nodes (one insert point points to and its neighbours)*/
	SF_ALLOW_ALLOC   = 1 << 5,

	SF_ALLOW_PACK    = 1 << 6
};

typedef enum shift_flags shift_flags_t;

#define SF_DEFAULT					   \
	(SF_ALLOW_LEFT | SF_ALLOW_RIGHT | SF_ALLOW_ALLOC | \
	 SF_ALLOW_MERGE | SF_MOVE_POINT | SF_ALLOW_PACK)

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

/* Different hints used for getting data to/from corresponding objects. */
struct ptr_hint {    
	uint64_t start;
	uint64_t width;
};

typedef struct ptr_hint ptr_hint_t;

struct sdhint_unix {
	uint32_t uid;
	uint32_t gid;
	uint32_t atime;
	uint32_t mtime;
	uint32_t ctime;
	uint32_t rdev;
	uint64_t bytes;
};

typedef struct sdhint_unix sdhint_unix_t;

struct sdhint_lw {
	uint16_t mode;
	uint32_t nlink;
	uint64_t size;
};

typedef struct sdhint_lw sdhint_lw_t;

struct sdhint_lt {
	uint32_t atime;
	uint32_t mtime;
	uint32_t ctime;
};

typedef struct sdhint_lt sdhint_lt_t;

struct sdhint_flags {
	uint32_t flags;
};

typedef struct sdhint_flags sdhint_flags_t;

struct sdhint_plug {
	reiser4_plug_t *plug[OPSET_LAST];
	uint64_t mask;
};

typedef struct sdhint_plug sdhint_plug_t;

typedef sdhint_plug_t reiser4_opset_t;

/* These fields should be changed to what proper description of needed
   extensions. */
struct stat_hint {
	/* Extensions mask */
	uint64_t extmask;
    
	/* Stat data extensions */
	void *ext[SDEXT_LAST_ID];
};

typedef struct stat_hint stat_hint_t;

enum entry_type {
	ET_NAME	= 0,
	ET_SPCL	= 1
};

typedef enum entry_type entry_type_t;

/* Object info struct contains the main information about a reiser4
   object. These are: its key, parent key and coord of first item. */
struct object_info {
	reiser4_opset_t opset;
	
	tree_entity_t *tree;
	reiser4_place_t start;
	reiser4_key_t object;
	reiser4_key_t parent;
};

typedef struct object_info object_info_t;

/* Object plugin entity. */
/*
struct object_entity {
	object_info_t info;
	reiser4_plug_t *plug;
};

typedef struct object_entity object_entity_t;
*/

typedef object_info_t object_entity_t;



/* Object hint. It is used to bring all about object information to object
   plugin to create appropriate object by it. */
struct object_hint {
	/* Object info. */
	object_info_t info;
	
	/* Additional mode to be masked to stat data mode. This is
	   needed for special files, but mat be used somewhere else. */
	uint32_t mode;
	
	/* rdev field for special file. Nevertheless, it is stored
	   inside special file stat data field, we put it to @body
	   definition, because it is the special file essence. */
	uint64_t rdev;
	
	/* SymLink name. */
	char *name;
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

/* Type for object place enumeration functions. */
typedef errno_t (*place_func_t) (reiser4_place_t *, void *);

/* Function definitions for enumeration item metadata and data. */
typedef errno_t (*layout_func_t) (void *, region_func_t, void *);
typedef errno_t (*metadata_func_t) (void *, place_func_t, void *);

#define REISER4_MAX_BLKSIZE (8192)

struct entry_hint {
	/* Entry metadata size. Filled by rem_entry and add_entry. */
	uint16_t len;
	
	/* Tree coord entry lies at. Filled by dir plugin's lookup. */
	reiser4_place_t place;

	/* Entry key within the current directory */
	reiser4_key_t offset;

	/* The stat data key of the object entry points to */
	reiser4_key_t object;

	/* Entry type (name or special), filled by readdir */
	uint8_t type;

	/* Name of entry */
	char name[REISER4_MAX_BLKSIZE];

#ifndef ENABLE_STAND_ALONE
	/* Hook called onto each create item during write flow. */
	place_func_t place_func;

	/* Related opaque data. May be used for passing something to
	   region_func() and place_func(). */
	void *data;
#endif
};

typedef struct entry_hint entry_hint_t;

#ifndef ENABLE_STAND_ALONE
enum slink_type {
	SL_UNLINK,   /* safe-link for unlink */
	SL_TRUNCATE, /* safe-link for truncate */
	SL_E2T,      /* safe-link for extent->tail conversion */
	SL_T2E,      /* safe-link for tail->extent conversion */
	SL_LAST
};

typedef enum slink_type slink_type_t;

struct slink_hint {
	/* Key of StatData the link points to. */
	reiser4_key_t key;
	
	/* The size to be truncated. */
	uint64_t size;

	slink_type_t type;
};

typedef struct slink_hint slink_hint_t;
#endif

/* This structure contains fields which describe an item or unit to be inserted
   into the tree. This is used for all tree modification purposes like
   insertitem, or write some file data. */ 
struct trans_hint {
	/* Overhead of data to be inserted. This is needed for the case when we
	   insert directory item and tree should know how much space should be
	   prepared in the tree (ohd + len), but we don't need overhead for
	   updating stat data bytes field. Set by estimate. */
	uint32_t overhead;
	
	/* Length of the data to be inserted/removed. Set by prep methods. */
	int32_t len;

	/* Value needed for updating bytes field in stat data. Set by
	   estimate. */
	uint64_t bytes;

	/* This is opaque pointer to item type specific information. */
	void *specific;

	/* Count of items/units to be inserted into the tree. */
	uint64_t count;

	/* The key of item/unit to be inserted. */
	reiser4_key_t offset;

	/* Max real key. Set by estimate and needed for file body items. */
	reiser4_key_t maxkey;

	/* Flags specific for the insert (raw for now), set at prepare stage. */
	uint16_t insert_flags;

	/* Shift flags for shift operation. */
	uint32_t shift_flags;
	
	/* Count of handled blocks in the first and the last extent unit. */
	uint64_t head, tail;

	/* Hash table unformatted blocks lie in. Needed for extent code. */
	aal_hash_table_t *blocks;
	
	/* Plugin to be used for working with item. */
	reiser4_plug_t *plug;

	/* Hook, which lets know, that passed block region is removed. Used for
	   releasing unformatted blocks during tail converion, or for merging
	   extents, etc. */
	region_func_t region_func;

	/* Hook called onto each create item during write flow. */
	place_func_t place_func;

	/* Related opaque data. May be used for passing something to
	   region_func() and place_func(). */
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
	reiser4_key_t offset;
	
	/* Plugin item will be converted to. */
	reiser4_plug_t *plug;

	/* Callback function caled onto each new created item during tail
	   conversion. */
	place_func_t place_func;
};

typedef struct conv_hint conv_hint_t;

struct coll_hint {
	uint8_t type;
	void *specific;
};

typedef struct coll_hint coll_hint_t;

/* Lookup bias. */
enum lookup_bias {
	/* Find for read, the match should be exact. */
	FIND_EXACT              = 1,
	/* Find for insert, the match should not be exaact. */
	FIND_CONV               = 2
};

typedef enum lookup_bias lookup_bias_t;

typedef struct lookup_hint lookup_hint_t;

typedef lookup_t (*coll_func_t) (tree_entity_t *, 
				 reiser4_place_t *, 
				 coll_hint_t *);

/* Hint to be used when looking for data in tree. */
struct lookup_hint {
	/* Key to be found. */
	reiser4_key_t *key;

	/* Tree level lookup should stop on. */
	uint8_t level;

#ifndef ENABLE_STAND_ALONE
	/* Function for modifying position during lookup in some way needed by
	   caller. Key collisions may be handler though this. */
	coll_func_t collision;

	/* Data needed by @lookup_func. */
	coll_hint_t *hint;
#endif
};

struct repair_hint {
	int64_t len;
	uint8_t mode;
};

typedef struct repair_hint repair_hint_t;

/* Filesystem description. */
struct fs_desc {
	reiser4_plug_t *policy;
	uint32_t blksize;
	aal_device_t *device;
};

typedef struct fs_desc fs_desc_t;

struct reiser4_key_ops {
	/* Function for dermining is key contains direntry name hashed or
	   not? */
	int (*hashed) (reiser4_key_t *);

	/* Returns minimal key for this key-format */
	reiser4_key_t *(*minimal) (void);
    
	/* Returns maximal key for this key-format */
	reiser4_key_t *(*maximal) (void);

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
	int (*compfull) (reiser4_key_t *, reiser4_key_t *);

	/* Compares two keys by comparing locality and objectid. */
	int (*compshort) (reiser4_key_t *, reiser4_key_t *);
	
	/* Copyies src key to dst one */
	errno_t (*assign) (reiser4_key_t *, reiser4_key_t *);
	
	/* Builds generic key (statdata, file body, etc). That is build key by
	   all its components. */
	errno_t (*build_generic) (reiser4_key_t *, key_type_t,
				  uint64_t, uint64_t, uint64_t, uint64_t);

	/* Builds key used for directory entries access. It uses name and hash
	   plugin to build hash and put it to key offset component. */
	errno_t (*build_hashed) (reiser4_key_t *, reiser4_plug_t *,
				 reiser4_plug_t *, uint64_t, uint64_t, char *);
	
#ifndef ENABLE_STAND_ALONE
	
	/* Gets/sets key type (minor in reiser4 notation). */	
	void (*set_type) (reiser4_key_t *, key_type_t);
	key_type_t (*get_type) (reiser4_key_t *);

	/* Gets/sets key full objectid */
	void (*set_fobjectid) (reiser4_key_t *, uint64_t);
	uint64_t (*get_fobjectid) (reiser4_key_t *);
	
	/* Gets/sets key locality. */
	void (*set_locality) (reiser4_key_t *, uint64_t);
#endif
	uint64_t (*get_locality) (reiser4_key_t *);
	
	/* Gets/sets key locality. */
	void (*set_ordering) (reiser4_key_t *, uint64_t);
	uint64_t (*get_ordering) (reiser4_key_t *);
    
	/* Gets/sets key objectid. */
	void (*set_objectid) (reiser4_key_t *, uint64_t);
	uint64_t (*get_objectid) (reiser4_key_t *);

	/* Gets/sets key offset */
	void (*set_offset) (reiser4_key_t *, uint64_t);
	uint64_t (*get_offset) (reiser4_key_t *);

	/* Extracts name from passed key. */
	char *(*get_name) (reiser4_key_t *, char *);

#ifndef ENABLE_STAND_ALONE
	/* Gets/sets directory key hash */
	void (*set_hash) (reiser4_key_t *, uint64_t);
	uint64_t (*get_hash) (reiser4_key_t *);
	
	/* Prints key into specified buffer */
	void (*print) (reiser4_key_t *, aal_stream_t *, uint16_t);

	/* Check key body for validness. */
	errno_t (*check_struct) (reiser4_key_t *);
#endif
};

typedef struct reiser4_key_ops reiser4_key_ops_t;


struct reiser4_object_ops {
	/* Loads object stat data to passed hint. */
	errno_t (*stat) (object_entity_t *, stat_hint_t *);

#ifndef ENABLE_STAND_ALONE
	/* These methods change @nlink value of passed @entity. */
	errno_t (*link) (object_entity_t *);
	errno_t (*unlink) (object_entity_t *);
	uint32_t (*links) (object_entity_t *);

	/* Establish parent child relationship. */
	errno_t (*attach) (object_entity_t *, object_entity_t *);
	errno_t (*detach) (object_entity_t *, object_entity_t *);

	/* Updates object stat data from passed hint. */
	errno_t (*update) (object_entity_t *, stat_hint_t *);
	
	/* Creates new file with passed parent and object keys. */
	object_entity_t *(*create) (object_hint_t *);

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
				 place_func_t, void *, uint8_t);
	
	/* Realizes if the object can be of this plugin and can be recovered as
	   a such. */
	object_entity_t *(*recognize) (object_info_t *);
	
	/* Creates the fake object by the gived @info. Needed to recover "/" and
	   "lost+found" direcories if their SD are broken. */
	object_entity_t *(*fake) (object_info_t *);
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
	lookup_t (*lookup) (object_entity_t *, char *, entry_hint_t *);

	/* Finds actual file stat data (used in symlinks). */
	errno_t (*follow) (object_entity_t *, reiser4_key_t *,
			   reiser4_key_t *);

	/* Reads the data from file to passed buffer. */
	int64_t (*read) (object_entity_t *, void *, uint64_t);

	/* Directory read method. */
	int32_t (*readdir) (object_entity_t *, entry_hint_t *);

	/* Return current position in directory. */
	errno_t (*telldir) (object_entity_t *, reiser4_key_t *);

	/* Change current position in directory. */
	errno_t (*seekdir) (object_entity_t *, reiser4_key_t *);
};

typedef struct reiser4_object_ops reiser4_object_ops_t;

struct item_balance_ops {
	/* Returns unit count in item passed place point to. */
	uint32_t (*units) (reiser4_place_t *);
	
	/* Makes lookup for passed key. */
	lookup_t (*lookup) (reiser4_place_t *, lookup_hint_t *,
			    lookup_bias_t);
	
#ifndef ENABLE_STAND_ALONE
	/* Fuses two neighbour items in the same node. Returns space released
	   Needed for fsck. */
	int32_t (*fuse) (reiser4_place_t *, reiser4_place_t *);
	
	/* Checks if items mergeable, that is if unit of one item can belong to
	   another one. Returns 1 if so, 0 otherwise. */
	int (*mergeable) (reiser4_place_t *, reiser4_place_t *);

	/* Estimates shift operation. */
	errno_t (*prep_shift) (reiser4_place_t *, reiser4_place_t *, shift_hint_t *);
	
	/* Performs shift of units from passed @src item to @dst item. */
	errno_t (*shift_units) (reiser4_place_t *, reiser4_place_t *, shift_hint_t *);

	/* Set the key of a particular unit of the item. */
	errno_t (*update_key) (reiser4_place_t *, reiser4_key_t *);
	
	/* Get the max real key which is stored in the item. */
	errno_t (*maxreal_key) (reiser4_place_t *, reiser4_key_t *);

	/* Collision handler item method. */
	lookup_t (*collision) (reiser4_place_t *, coll_hint_t *);
#endif

	/* Get the key of a particular unit of the item. */
	errno_t (*fetch_key) (reiser4_place_t *, reiser4_key_t *);

	/* Get the max key which could be stored in the item of this type. */
	errno_t (*maxposs_key) (reiser4_place_t *, reiser4_key_t *);
};

typedef struct item_balance_ops item_balance_ops_t;

struct item_object_ops {
	/* Reads passed amount of bytes from the item. */
	int64_t (*read_units) (reiser4_place_t *, trans_hint_t *);
		
	/* Fetches one or more units at passed @place to passed hint. */
	int64_t (*fetch_units) (reiser4_place_t *, trans_hint_t *);

#ifndef ENABLE_STAND_ALONE
	/* Estimates write operation. */
	errno_t (*prep_write) (reiser4_place_t *, trans_hint_t *);

	/* Writes data to item. */
	int64_t (*write_units) (reiser4_place_t *, trans_hint_t *);

	/* Estimates insert operation. */
	errno_t (*prep_insert) (reiser4_place_t *, trans_hint_t *);

	/* Inserts some amount of units described by passed hint into passed
	   item denoted by place. */
	int64_t (*insert_units) (reiser4_place_t *, trans_hint_t *);

	/* Removes specified unit from the item. */
	errno_t (*remove_units) (reiser4_place_t *, trans_hint_t *);

	/* Updates unit at passed place by data from passed hint. */
	int64_t (*update_units) (reiser4_place_t *, trans_hint_t *);

	/* Cuts out some amount of data */
	int64_t (*trunc_units) (reiser4_place_t *, trans_hint_t *);

	/* Goes through all blocks item points to. */
	errno_t (*layout) (reiser4_place_t *, region_func_t, void *);
		
	/* Gets the size of the data item keeps. */
	uint64_t (*size) (reiser4_place_t *);
	
	/* Gets the amount of bytes data item keeps takes on the disk. */
	uint64_t (*bytes) (reiser4_place_t *);

	/* Gets the overhead for the item creation. */
	uint16_t (*overhead) ();
#endif
};

typedef struct item_object_ops item_object_ops_t;

#ifndef ENABLE_STAND_ALONE
struct item_repair_ops {
	/* Estimate merge operation. */
	errno_t (*prep_insert_raw) (reiser4_place_t *, trans_hint_t *);

	/* Copies some amount of units from @src to @dst with partial
	   overwritting. */
	errno_t (*insert_raw) (reiser4_place_t *, trans_hint_t *);

	/* Checks the item structure. */
	errno_t (*check_struct) (reiser4_place_t *, repair_hint_t *);
	
	/* Does some specific actions if a block the item points to is wrong. */
	errno_t (*check_layout) (reiser4_place_t *, repair_hint_t *,
				 region_func_t, void *);

	errno_t (*pack) (reiser4_place_t *, aal_stream_t *);
	errno_t (*unpack) (reiser4_place_t *, aal_stream_t *);
};
#endif

typedef struct item_repair_ops item_repair_ops_t;

#ifndef ENABLE_STAND_ALONE
struct item_debug_ops {
	/* Prints item into specified buffer. */
	void (*print) (reiser4_place_t *, aal_stream_t *, uint16_t);
};
#endif

typedef struct item_debug_ops item_debug_ops_t;

struct item_tree_ops {
	/* Return block number from passed place. Place is nodeptr item. */
	blk_t (*down_link) (reiser4_place_t *);

#ifndef ENABLE_STAND_ALONE
	/* Update link block number. */
	errno_t (*update_link) (reiser4_place_t *, blk_t);
#endif
};

typedef struct item_tree_ops item_tree_ops_t;

struct reiser4_item_ops {
	item_tree_ops_t *tree;
	item_object_ops_t *object;
	item_balance_ops_t *balance;
#ifndef ENABLE_STAND_ALONE
	item_debug_ops_t *debug;
	item_repair_ops_t *repair;
#endif
};

typedef struct reiser4_item_ops reiser4_item_ops_t;

/* Stat data extension plugin */
struct reiser4_sdext_ops {
#ifndef ENABLE_STAND_ALONE
	/* Initialize stat data extension data at passed pointer. */
	errno_t (*init) (stat_entity_t *, void *);

	/* Prints stat data extension data into passed buffer. */
	void (*print) (stat_entity_t *, aal_stream_t *, uint16_t);

	/* Checks sd extension content. */
	errno_t (*check_struct) (stat_entity_t *, repair_hint_t *);
#endif
	/* Reads stat data extension data. */
	errno_t (*open) (stat_entity_t *, void *);

	/* Returns length of the extension. */
	uint32_t (*length) (stat_entity_t *, void *);
};

typedef struct reiser4_sdext_ops reiser4_sdext_ops_t;

/* Node plugin operates on passed block. It doesn't any initialization, so it
   hasn't close method and all its methods accepts first argument aal_block_t,
   not initialized previously hypothetic instance of node. */
struct reiser4_node_ops {
#ifndef ENABLE_STAND_ALONE
	/* Get node state flags and set them back. */
	uint32_t (*get_state) (reiser4_node_t *);
	void (*set_state) (reiser4_node_t *, uint32_t);

	/* Performs shift of items and units. */
	errno_t (*shift) (reiser4_node_t *, reiser4_node_t *, 
			  shift_hint_t *);

	/* Fuses two neighbour items in passed node at passed positions. */
	errno_t (*fuse) (reiser4_node_t *, pos_t *, pos_t *);
    
	/* Checks thoroughly the node structure and fixes what needed. */
	errno_t (*check_struct) (reiser4_node_t *, uint8_t);

	/* Packing/unpacking metadata. */
	reiser4_node_t *(*unpack) (aal_block_t *, reiser4_plug_t *,
				  aal_stream_t *, int);
	
	errno_t (*pack) (reiser4_node_t *, aal_stream_t *, int);

	/* Prints node into given buffer. */
	void (*print) (reiser4_node_t *, aal_stream_t *,
		       uint32_t, uint32_t, uint16_t);
    
	/* Returns node overhead. */
	uint16_t (*overhead) (reiser4_node_t *);

	/* Returns node max possible space. */
	uint16_t (*maxspace) (reiser4_node_t *);
    
	/* Returns free space in the node. */
	uint16_t (*space) (reiser4_node_t *);

	/* Inserts item at specified pos. */
	errno_t (*insert) (reiser4_node_t *, pos_t *,
			   trans_hint_t *);
    
	/* Writes data to the node. */
	int64_t (*write) (reiser4_node_t *, pos_t *,
			  trans_hint_t *);

	/* Truncate item at passed pos. */
	int64_t (*trunc) (reiser4_node_t *, pos_t *,
			  trans_hint_t *);

	/* Removes item/unit at specified pos. */
	errno_t (*remove) (reiser4_node_t *, pos_t *,
			   trans_hint_t *);

	/* Shrinks node without calling any item methods. */
	errno_t (*shrink) (reiser4_node_t *, pos_t *,
			   uint32_t, uint32_t);

	/* Merge 2 items -- insert/overwrite @src_entity parts to
	   @dst_entity. */
	errno_t (*insert_raw) (reiser4_node_t *, pos_t *, 
			       trans_hint_t *);

	/* Copies items from @src_entity to @dst_entity. */
	errno_t (*copy) (reiser4_node_t *, pos_t *,
			 reiser4_node_t *, pos_t *,
			 uint32_t);
	
	/* Expands node (makes space) at passed pos. */
	errno_t (*expand) (reiser4_node_t *, pos_t *,
			   uint32_t, uint32_t);

	/* Updates key at passed pos by passed key. */
	errno_t (*set_key) (reiser4_node_t *, pos_t *,
			    reiser4_key_t *);

	void (*set_level) (reiser4_node_t *, uint8_t);
	void (*set_mstamp) (reiser4_node_t *, uint32_t);
	void (*set_fstamp) (reiser4_node_t *, uint64_t);

	/* Get mkfs and flush stamps */
	uint32_t (*get_mstamp) (reiser4_node_t *);
    	uint64_t (*get_fstamp) (reiser4_node_t *);
	
	/* Get/set item flags. */
	void (*set_flags) (reiser4_node_t *, uint32_t, uint16_t);
	uint16_t (*get_flags) (reiser4_node_t *, uint32_t);

	/* Saves node to device */
	errno_t (*sync) (reiser4_node_t *);

	/* Initializes node with passed block and key plugin. */
	reiser4_node_t *(*init) (aal_block_t *, uint8_t , 
				reiser4_plug_t *);
#endif
	/* Open the node on the given block with the given key plugin. */
	reiser4_node_t *(*open) (aal_block_t *, reiser4_plug_t *);
	
	/* Destroys the node entity. */
	errno_t (*fini) (reiser4_node_t *);

	/* Fetches item data to passed @place */
	errno_t (*fetch) (reiser4_node_t *, pos_t *,
			  reiser4_place_t *);
	
	/* Returns item count */
	uint32_t (*items) (reiser4_node_t *);
    
	/* Makes lookup inside node by specified key */
	lookup_t (*lookup) (reiser4_node_t *, lookup_hint_t *, 
			    lookup_bias_t, pos_t *);
    
	/* Gets/sets key at pos */
	errno_t (*get_key) (reiser4_node_t *, pos_t *,
			    reiser4_key_t *);

	/* Return node level. */
	uint8_t (*get_level) (reiser4_node_t *);
};

typedef struct reiser4_node_ops reiser4_node_ops_t;

/* Hash plugin operations. */
struct reiser4_hash_ops {
	uint64_t (*build) (char *, uint32_t);
};

typedef struct reiser4_hash_ops reiser4_hash_ops_t;

struct reiser4_fibre_ops {
	uint8_t (*build) (char *, uint32_t);
};

typedef struct reiser4_fibre_ops reiser4_fibre_ops_t;

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
	void (*print) (generic_entity_t *, aal_stream_t *, uint16_t);
    
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

	/* Sets/gets next object id */
	oid_t (*get_next) (generic_entity_t *);
	void (*set_next) (generic_entity_t *, oid_t);

	/* Gets next object id */
	oid_t (*allocate) (generic_entity_t *);

	/* Releases passed object id */
	void (*release) (generic_entity_t *, oid_t);
    
	/* Gets/sets the number of used object ids */
	uint64_t (*get_used) (generic_entity_t *);
	void (*set_used) (generic_entity_t *, uint64_t);
    
	/* Returns the number of free object ids */
	uint64_t (*free) (generic_entity_t *);

	/* Prints oid allocator data */
	void (*print) (generic_entity_t *, aal_stream_t *, uint16_t);

	/* Makes check for validness */
	errno_t (*valid) (generic_entity_t *);
	
	/* Root locality and objectid and lost+found objectid. */
	oid_t (*root_locality) ();
	oid_t (*root_objectid) ();
	oid_t (*lost_objectid) ();
	oid_t (*slink_locality) ();
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
	
	/* Checks blocks allocator on validness */
	errno_t (*check_struct) (generic_entity_t *, uint8_t mode);

	/* Prints block allocator data */
	void (*print) (generic_entity_t *, aal_stream_t *, uint16_t);

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
				   generic_entity_t *, uint64_t, uint64_t);

	/* Creates journal on specified device. */
	generic_entity_t *(*create) (fs_desc_t *, generic_entity_t *,
				     generic_entity_t *, uint64_t, uint64_t);

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
	void (*print) (generic_entity_t *, aal_stream_t *, uint16_t);
	
	/* Checks thoroughly the journal structure. */
	errno_t (*check_struct) (generic_entity_t *,
				 layout_func_t, void *);

	/* Invalidates the journal. */
	void (*invalidate) (generic_entity_t *);
	
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

#define PLUG_MAX_DESC	64
#define PLUG_MAX_LABEL	22

typedef struct reiser4_core reiser4_core_t;

/* Plugin init() and fini() function types. They are used for calling these
   functions during plugin initialization. */
typedef errno_t (*plug_fini_t) (reiser4_core_t *);
typedef errno_t (*plug_func_t) (reiser4_plug_t *, void *);
typedef reiser4_plug_t *(*plug_init_t) (reiser4_core_t *);

/* Plugin class descriptor. Used for loading plugins. */
struct plug_class {
	/* Plugin initialization routine. */
	plug_init_t init;
	
	/* Plugin finalization routine. */
	plug_fini_t fini;
};

typedef struct plug_class plug_class_t;

struct plug_ident {
	/* Plugin id, type and group. */
	rid_t id;
	uint8_t group;
	uint8_t type;
};

typedef struct plug_ident plug_ident_t;

#define class_init {NULL, NULL}

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
		reiser4_fibre_ops_t *fibre_ops;
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

/* Macros for dirtying nodes place lie at. */
#define place_mkdirty(place) \
        ((place)->node->block->dirty = 1)

#define place_mkclean(place) \
        ((place)->node->block->dirty = 0)

#define place_isdirty(place) \
        ((place)->node->block->dirty)

struct flow_ops {
	/* Reads data from the tree. */
	int64_t (*read) (tree_entity_t *, trans_hint_t *);

#ifndef ENABLE_STAND_ALONE
	/* Writes data to tree. */
	int64_t (*write) (tree_entity_t *, trans_hint_t *);

	/* Truncates data from tree. */
	int64_t (*truncate) (tree_entity_t *, trans_hint_t *);
	
	/* Convert some particular place to another plugin. */
	errno_t (*convert) (tree_entity_t *, conv_hint_t *);
#endif
};

typedef struct flow_ops flow_ops_t;

struct tree_ops {
	/* Makes lookup in the tree in order to know where say stat data item of
	   a file realy lies. It is used in all object plugins. */
	lookup_t (*lookup) (tree_entity_t *, lookup_hint_t *, 
			    lookup_bias_t, reiser4_place_t *);

#ifndef ENABLE_STAND_ALONE
	/* Collisions handler. It takes start place and looks for actual data in
	   collided array. */
	lookup_t (*collision) (tree_entity_t *, reiser4_place_t *, 
			       coll_hint_t *);
	
	/* Inserts item/unit in the tree by calling tree_insert() function, used
	   by all object plugins (dir, file, etc). */
	int64_t (*insert) (tree_entity_t *, reiser4_place_t *,
			   trans_hint_t *, uint8_t);

	/* Removes item/unit from the tree. It is used in all object plugins for
	   modification purposes. */
	errno_t (*remove) (tree_entity_t *, reiser4_place_t *, trans_hint_t *);
	
	/* Update the key in the place and the node itsef. */
	errno_t (*update_key) (tree_entity_t *, reiser4_place_t *, reiser4_key_t *);

	/* Get the safe link locality. */
	uint64_t (*slink_locality) (tree_entity_t *);
#endif
	/* Returns the next item. */
	errno_t (*next_item) (tree_entity_t *, reiser4_place_t *, 
			      reiser4_place_t *);
};

typedef struct tree_ops tree_ops_t;

struct factory_ops {
	/* Finds plugin by its attributes (type and id). */
	reiser4_plug_t *(*ifind) (rid_t, rid_t);
};

typedef struct factory_ops factory_ops_t;

#ifdef ENABLE_SYMLINKS
struct object_ops {
	errno_t (*resolve) (tree_entity_t *, char *, 
			    reiser4_key_t *, reiser4_key_t *);
};

typedef struct object_ops object_ops_t;
#endif

struct pset_ops {
	/* Obtains the plugin from the profile by its profile index. */
	reiser4_plug_t *(*find) (rid_t, rid_t);
#ifndef ENABLE_STAND_ALONE
	void (*diff) (tree_entity_t *, reiser4_opset_t *);
#endif
};

typedef struct pset_ops pset_ops_t;

#ifndef ENABLE_STAND_ALONE
struct key_ops {
	char *(*print) (reiser4_key_t *, uint16_t);
};

typedef struct key_ops key_ops_t;

struct item_ops {
	/* Checks if items mergeable. */
	int (*mergeable) (reiser4_place_t *, reiser4_place_t *);
};

typedef struct item_ops item_ops_t;
#endif

/* This structure is passed to all plugins in initialization time and used for
   access libreiser4 factories. */
struct reiser4_core {
	flow_ops_t flow_ops;
	tree_ops_t tree_ops;
	factory_ops_t factory_ops;
	pset_ops_t pset_ops;

#ifdef ENABLE_SYMLINKS
	object_ops_t object_ops;
#endif

#ifndef ENABLE_STAND_ALONE
	key_ops_t key_ops;
	item_ops_t item_ops;
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
