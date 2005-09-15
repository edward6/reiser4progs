/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   plugin.h -- reiser4 plugin known types and macros. */

#ifndef REISER4_PLUGIN_H
#define REISER4_PLUGIN_H

#include <aal/libaal.h>

/* Leaf and twig levels. */
#define LEAF_LEVEL			1
#define TWIG_LEVEL			(LEAF_LEVEL + 1)

/* Master related stuff like magic and offset in bytes. These are used by both
   plugins and library itself. */
#define REISER4_MASTER_MAGIC		("ReIsEr4")
#define REISER4_MASTER_OFFSET		(65536)
#define REISER4_MASTER_BLOCKNR(blksize)	(REISER4_MASTER_OFFSET/blksize)

/* The same for fs stat block. */
#define REISER4_STATUS_BLOCKNR(blksize)	(REISER4_MASTER_BLOCKNR(blksize) + 5)
#define REISER4_STATUS_MAGIC		("ReiSeR4StATusBl")

/* Where the backup starts. */
#define REISER4_BACKUP_START(blksize)	(REISER4_MASTER_BLOCKNR(blksize) + 6)

/* Root key locality and objectid. This is actually defined in oid plugin,
   but hardcoded here to exclude oid plugin from the minimal mode at all,
   nothing but these oids is needed there. */
#define REISER4_ROOT_LOCALITY		(0x29)
#define REISER4_ROOT_OBJECTID		(0x2a)

/* Macros for hole and unallocated extents. Used by both plugins (extent40) and
   library itself. */
#define EXTENT_HOLE_UNIT		(0)
#define EXTENT_UNALLOC_UNIT		(1)

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
typedef struct pos {
	uint32_t item;
	uint32_t unit;
} pos_t;

#define POS_INIT(p, i, u) \
        (p)->item = i, (p)->unit = u

/* Lookup return values. */
enum lookup {
	PRESENT                 = 1,
	ABSENT                  = 0,
};

typedef int64_t lookup_t;

/* Known by library plugin types. */
typedef enum reiser4_plug_type {
	OBJECT_PLUG_TYPE        = 0x0,
	ITEM_PLUG_TYPE          = 0x1,
	NODE_PLUG_TYPE          = 0x2,
	HASH_PLUG_TYPE          = 0x3,
	FIBRE_PLUG_TYPE		= 0x4,
	POLICY_PLUG_TYPE        = 0x5,
	SDEXT_PLUG_TYPE         = 0x7,
	FORMAT_PLUG_TYPE        = 0x8,

	/* These are not plugins in the kernel. */
	OID_PLUG_TYPE           = 0xe,
	ALLOC_PLUG_TYPE         = 0xf,
	JOURNAL_PLUG_TYPE       = 0x10,
	KEY_PLUG_TYPE           = 0x11,
	
	/* Not really a plugin, at least in progs, but a value that 
	   needs to be checked only. */
	PARAM_PLUG_TYPE		= 0x12,
	LAST_PLUG_TYPE
} reiser4_plug_type_t;

/* Known object plugin ids. */
enum reiser4_object_plug_id {
	OBJECT_REG40_ID         = 0x0,
	OBJECT_DIR40_ID		= 0x1,
	OBJECT_SYM40_ID		= 0x2,
	OBJECT_SPL40_ID	        = 0x3,
	OBJECT_CRC40_ID	        = 0x4,
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

/* Known item plugin ids. */
enum reiser4_item_plug_id {
	ITEM_STAT40_ID		= 0x0,
	ITEM_SDE40_ID	        = 0x1,
	ITEM_CDE40_ID	        = 0x2,
	ITEM_NODEPTR40_ID	= 0x3,
	ITEM_ACL40_ID		= 0x4,
	ITEM_EXTENT40_ID	= 0x5,
	ITEM_PLAIN40_ID		= 0x6,
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
	SAFE_LINK_ITEM		= 0x5,
	CTAIL_ITEM		= 0x6,
	BLACK_BOX_ITEM		= 0x7,
	LAST_ITEM
};

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

/* Know tail policy plugin ids. */
enum reiser4_tail_plug_id {
	TAIL_NEVER_ID		= 0x0,
	TAIL_ALWAYS_ID		= 0x1,
	TAIL_SMART_ID		= 0x2,
	TAIL_LAST_ID
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

enum reiser4_param_group {
	PERM_PARAM	= 0x0,
	CRYPTO_PARAM	= 0x1,
	DIGEST_PARAM	= 0x2,
	COMPRESS_PARAM	= 0x3,
	CMODE_PARAM	= 0x4,
	CLUSTER_PARAM	= 0x5,
	LAST_PARAM
};

/* Known permission plugin ids. */
enum reiser4_perm_plug_id {
	PERM_RWX_ID		= 0x0,
	PERM_LAST_ID
};


enum reiser4_compress_plug_id {
	COMPRESS_LZO1_ID	= 0x0,
	COMPRESS_NOLZO1_ID	= 0x1,
	COMPRESS_GZIP1_ID	= 0x2,
	COMPRESS_NOGZIP1_ID	= 0x3,
	COMPRESS_NONE_ID	= 0x4,
	COMPRESS_LAST_ID
};

enum reiser4_crypto_id {
	CRYPTO_NONE_ID = 0x0,
	CRYPTO_LAST_ID
};

enum reiser4_compress_mode_id {
        CMODE_SMART_ID	= 0x0,
        CMODE_LAZY_ID	= 0x1,
        CMODE_FORCE_ID	= 0x2,
        CMODE_TEST_ID	= 0x3,
        CMODE_LAST_ID
};

enum reiser4_cluster_id {
	CLUSTER_4K_ID  = 0x0,
	CLUSTER_8K_ID  = 0x1,
	CLUSTER_16K_ID = 0x2,
	CLUSTER_32K_ID = 0x3,
	CLUSTER_64K_ID = 0x4,
	CLUSTER_LAST_ID
};

enum reiser4_digest_id {
	DIGEST_NONE_ID = 0x0,
	DIGEST_LAST_ID
};

#define INVAL_PTR	        ((void *)-1)
#define INVAL_PID	        ((rid_t)~0)

/* Type for key which is used both by library and plugins. */
struct reiser4_key {
	reiser4_plug_t *plug;
	d64_t body[4];
#ifndef ENABLE_MINIMAL
	uint32_t adjust;
#endif
};

typedef struct reiser4_key reiser4_key_t;

/* Known key types. */
typedef enum key_type {
	KEY_FILENAME_TYPE       = 0x0,
	KEY_STATDATA_TYPE       = 0x1,
	KEY_ATTRNAME_TYPE       = 0x2,
	KEY_ATTRBODY_TYPE       = 0x3,
	KEY_FILEBODY_TYPE       = 0x4,
	KEY_LAST_TYPE
} key_type_t;

/* Tree Plugin SET index. */
enum reiser4_tpset_id {
	TPSET_KEY		= 0x0,
#ifndef ENABLE_MINIMAL
	TPSET_NODE		= 0x1,
	TPSET_NODEPTR		= 0x2,
#endif
	TPSET_LAST
};

/* Object Plugin SET index. */
enum reiser4_opset_id {
	OPSET_OBJ	= 0x0,
	OPSET_DIR	= 0x1,
	OPSET_PERM	= 0x2,
	OPSET_POLICY	= 0x3,
	OPSET_HASH	= 0x4,
	OPSET_FIBRE	= 0x5,
	OPSET_STAT	= 0x6,
	OPSET_DIRITEM	= 0x7,
	OPSET_CRYPTO	= 0x8,
	OPSET_DIGEST	= 0x9,
	OPSET_COMPRESS	= 0xa,
	OPSET_CMODE	= 0xb,
	OPSET_CLUSTER	= 0xc,
	OPSET_CREATE	= 0xd,
	
	OPSET_STORE_LAST,
	
	/* These are not stored on disk in the current implementation. */
	OPSET_REGFILE	= OPSET_STORE_LAST,
	OPSET_DIRFILE	= OPSET_STORE_LAST + 1,
	OPSET_SYMFILE	= OPSET_STORE_LAST + 2,
	OPSET_SPLFILE	= OPSET_STORE_LAST + 3,
	
#ifndef ENABLE_MINIMAL
	OPSET_TAIL	= OPSET_STORE_LAST + 4,
	OPSET_EXTENT	= OPSET_STORE_LAST + 5,
	OPSET_CTAIL	= OPSET_STORE_LAST + 6,
#endif
	OPSET_LAST
};

/* Known print options for key. */
enum print_options {
	PO_DEFAULT              = 0x0,
	PO_INODE                = 0x1,
	PO_UNIT_OFFSETS         = 0x2
};

/* Type for describing reiser4 objects (like format, block allocator, etc)
   inside the library, created by plugins themselves. */
typedef struct generic_entity {
	reiser4_plug_t *plug;
} generic_entity_t;

typedef struct tree_entity {
	/* Plugin SET. Set of plugins needed for the reiser4progs work.
	   Consists of tree-specific plugins and object-specific plugins. */
	reiser4_plug_t *tpset[TPSET_LAST];
	reiser4_plug_t *opset[OPSET_LAST];
	uint64_t param_mask;
} tree_entity_t;

typedef struct reiser4_node reiser4_node_t;
typedef struct reiser4_place reiser4_place_t;

#define place_blknr(place) ((place)->node->block->nr)
#define place_blksize(place) ((place)->node->block->size)

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

	/* First unit offset within the item. Used by item internal code only. 
	   Useful to minimize the code (e.g. tail, ctail). */
	uint32_t off;
	
	reiser4_key_t key;
	reiser4_plug_t *plug;
};

enum node_flags {
	NF_HEARD_BANSHEE  = 1 << 0,
	NF_LAST
};

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

	/* Key policy. Optimisation to not call key->bodysize all the time. */
	uint8_t keypol;
	
	/* Node state flags. */
	uint32_t state;

#ifndef ENABLE_MINIMAL
	/* Different node flags. */
	uint32_t flags;
	
	/* Applications using this library sometimes need to embed information
	   into the objects of our library for their own use. */
	void *data;
#endif
};

/* Stat data extension entity. */
typedef struct stat_entity {
	reiser4_place_t *place;
	reiser4_plug_t *ext_plug;
	uint32_t offset;
} stat_entity_t;

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

	/* Care about the insert point, shift only till this point. If not 
	   given, as much data as fit the destination node will be shifted. */
	SF_UPDATE_POINT  = 1 << 3,

	/* Controls if shift allowed to merge border items or only whole items
	   may be shifted. Needed for repair code in order to disable merge of
	   checked item and not checked one. */
	SF_ALLOW_MERGE   = 1 << 4,

	/* Controls if shift allowed to allocate new nodes during making
	   space. This is needed sometimes if there is not enough of free space
	   in existent nodes (one insert point points to and its neighbours)*/
	SF_ALLOW_ALLOC   = 1 << 5,

	SF_ALLOW_PACK    = 1 << 6,

	/* Hold (do not leave) the current position (pointed by place->pos). */
	SF_HOLD_POS      = 1 << 7
};

#define SF_DEFAULT					   \
	(SF_ALLOW_LEFT | SF_ALLOW_RIGHT | SF_ALLOW_ALLOC | \
	 SF_ALLOW_MERGE | SF_MOVE_POINT | SF_ALLOW_PACK)

typedef struct shift_hint {
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
} shift_hint_t;

/* Different hints used for getting data to/from corresponding objects. */
typedef struct ptr_hint {    
	uint64_t start;
	uint64_t width;
} ptr_hint_t;

typedef struct sdhint_unix {
	uint32_t uid;
	uint32_t gid;
	uint32_t atime;
	uint32_t mtime;
	uint32_t ctime;
	uint32_t rdev;
	uint64_t bytes;
} sdhint_unix_t;

typedef struct sdhint_lw {
	uint16_t mode;
	uint32_t nlink;
	uint64_t size;
} sdhint_lw_t;

typedef struct sdhint_lt {
	uint32_t atime;
	uint32_t mtime;
	uint32_t ctime;
} sdhint_lt_t;

typedef struct sdhint_flags {
	uint32_t flags;
} sdhint_flags_t;

typedef struct sdhint_plug {
	/* Set of initialized fields. */
	uint64_t plug_mask;
	reiser4_plug_t *plug[OPSET_LAST];
} sdhint_plug_t;

typedef sdhint_plug_t reiser4_opset_t;

#ifndef ENABLE_MINIMAL
typedef struct sdhint_crc {
	uint16_t keysize;
	uint8_t key[128];
} sdhint_crc_t;
#endif

/* These fields should be changed to what proper description of needed
   extensions. */
typedef struct stat_hint {
	/* Extensions mask */
	uint64_t extmask;
    
	/* Stat data extensions */
	void *ext[SDEXT_LAST_ID];
} stat_hint_t;

typedef enum entry_type {
	ET_NAME	= 0,
	ET_SPCL	= 1,
	ET_LAST
} entry_type_t;

/* Object info struct contains the main information about a reiser4
   object. These are: its key, parent key and coord of first item. */
typedef struct object_info {
	reiser4_opset_t opset;
	
	tree_entity_t *tree;
	reiser4_place_t start;
	reiser4_key_t object;
	reiser4_key_t parent;
} object_info_t;

/* Object hint. It is used to bring all about object information to object
   plugin to create appropriate object by it. */
typedef struct object_hint {
	/* Additional mode to be masked to stat data mode. This is
	   needed for special files, but mat be used somewhere else. */
	uint32_t mode;
	
	/* rdev field for special file. Nevertheless, it is stored
	   inside special file stat data field, we put it to @body
	   definition, because it is the special file essence. */
	uint64_t rdev;
	
	/* SymLink name. */
	char *name;
} object_hint_t;

/* Reiser4 file structure (regular file, directory, symlinks, etc) */
typedef struct reiser4_object {
	/* Info about the object, stat data place, object and parent keys and
	   pointer to the instance of internal libreiser4 tree for modiying
	   purposes. It is passed by reiser4 library during initialization of
	   the file instance. */
	object_info_t info;

	/* Current body item coord stored here */
	reiser4_place_t body;

	/* Current position in the reg file */
	reiser4_key_t position;

#ifndef ENABLE_MINIMAL
	/* File body plugin is use. */
	reiser4_plug_t *body_plug;
#endif
} reiser4_object_t;

#define reiser4_oplug(object) (object->info.opset.plug[OPSET_OBJ])

/* Bits for entity state field. For now here is only "dirty" bit, but possible
   and other ones. */
enum entity_state {
	ENTITY_DIRTY = 0
};

/* Type for region enumerating callback functions. */
typedef errno_t (*region_func_t) (uint64_t, uint64_t, void *);

/* Type for on-place functions. */
typedef errno_t (*place_func_t) (reiser4_place_t *, void *);

/* Type for on-node functions. */
typedef errno_t (*node_func_t) (reiser4_node_t *, void *);

/* Function definitions for enumeration item metadata and data. */
typedef errno_t (*layout_func_t) (void *, region_func_t, void *);

#define REISER4_MIN_BLKSIZE (512)
#define REISER4_MAX_BLKSIZE (8192)

typedef struct entry_hint {
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

#ifndef ENABLE_MINIMAL
	/* Hook called onto each create item during write flow. */
	place_func_t place_func;

	/* Related opaque data. May be used for passing something to
	   region_func() and place_func(). */
	void *data;
#endif
} entry_hint_t;

#ifndef ENABLE_MINIMAL
typedef enum slink_type {
	SL_UNLINK,   /* safe-link for unlink */
	SL_TRUNCATE, /* safe-link for truncate */
	SL_E2T,      /* safe-link for extent->tail conversion */
	SL_T2E,      /* safe-link for tail->extent conversion */
	SL_LAST
} slink_type_t;

typedef struct slink_hint {
	/* Key of StatData the link points to. */
	reiser4_key_t key;
	
	/* The size to be truncated. */
	uint64_t size;

	slink_type_t type;
} slink_hint_t;
#endif

/* This structure contains fields which describe an item or unit to be inserted
   into the tree. This is used for all tree modification purposes like
   insertitem, or write some file data. */ 
typedef struct trans_hint {
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
} trans_hint_t;

/* This structure contains related to tail conversion. */
typedef struct conv_hint {
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

	/* The flag if the hole should be inserted if there is nothing 
	   to read. Could be useful for recovery when there could be a 
	   gap between 2 items. */
	bool_t ins_hole;
} conv_hint_t;

typedef struct coll_hint {
	uint8_t type;
	void *specific;
} coll_hint_t;

/* Lookup bias. */
typedef enum lookup_bias {
	/* Find for read, the match should be exact. */
	FIND_EXACT              = 1,
	/* Find for insert, the match should not be exaact. */
	FIND_CONV               = 2
} lookup_bias_t;

typedef lookup_t (*coll_func_t) (tree_entity_t *, 
				 reiser4_place_t *, 
				 coll_hint_t *);

/* Hint to be used when looking for data in tree. */
typedef struct lookup_hint {
	/* Key to be found. */
	reiser4_key_t *key;

	/* Tree level lookup should stop on. */
	uint8_t level;

#ifndef ENABLE_MINIMAL
	/* Function for modifying position during lookup in some way needed by
	   caller. Key collisions may be handler though this. */
	coll_func_t collision;

	/* Data needed by @lookup_func. */
	coll_hint_t *hint;
#endif
} lookup_hint_t;

#ifndef ENABLE_MINIMAL
typedef struct repair_hint {
	int64_t len;
	uint8_t mode;
} repair_hint_t;

enum reiser4_backuper {
	BK_MASTER	= 0x0,
	BK_FORMAT	= 0x1,
	BK_LAST		= 0x2
};

typedef struct backup_hint {
	aal_block_t block;
	uint16_t off[BK_LAST + 1];

	/* Fields below are used by check_backup. */
	
	/* Block count. */
	uint64_t blocks;

	/* Matched block count. */
	uint64_t count;
	uint64_t total;
} backup_hint_t;

#endif

enum format_hint_mask {
	PM_POLICY	= 0x0,
	PM_KEY		= 0x1
};

typedef struct format_hint {
	uint64_t blocks;
	uint32_t blksize;
	rid_t policy;
	rid_t key;
	
	/* For repair purposes. Set plugin types that are overridden 
	   in the profile here, they must be set in the format plugin
	   check_struct. If bit is not set, plugins given with the above 
	   hints are merely hints. */
	uint64_t mask;
} format_hint_t;

typedef struct reiser4_key_plug {
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
	
	/* Builds generic key (statdata, file body, etc). That is build key by
	   all its components. */
	errno_t (*build_generic) (reiser4_key_t *, key_type_t,
				  uint64_t, uint64_t, uint64_t, uint64_t);

	/* Builds key used for directory entries access. It uses name and hash
	   plugin to build hash and put it to key offset component. */
	void (*build_hashed) (reiser4_key_t *, reiser4_plug_t *,
			      reiser4_plug_t *, uint64_t, uint64_t, char *);
	
#ifndef ENABLE_MINIMAL
	
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

#ifndef ENABLE_MINIMAL
	/* Gets/sets directory key hash */
	void (*set_hash) (reiser4_key_t *, uint64_t);
	uint64_t (*get_hash) (reiser4_key_t *);
	
	/* Prints key into specified buffer */
	void (*print) (reiser4_key_t *, aal_stream_t *, uint16_t);

	/* Check key body for validness. */
	errno_t (*check_struct) (reiser4_key_t *);
#endif
} reiser4_key_plug_t;


typedef struct reiser4_object_plug {
	/* Loads object stat data to passed hint. */
	errno_t (*stat) (reiser4_object_t *, stat_hint_t *);

#ifndef ENABLE_MINIMAL
	/* These methods change @nlink value of passed @entity. */
	errno_t (*link) (reiser4_object_t *);
	errno_t (*unlink) (reiser4_object_t *);
	bool_t (*linked) (reiser4_object_t *);

	/* Establish parent child relationship. */
	errno_t (*attach) (reiser4_object_t *, reiser4_object_t *);
	errno_t (*detach) (reiser4_object_t *, reiser4_object_t *);

	/* Updates object stat data from passed hint. */
	errno_t (*update) (reiser4_object_t *, stat_hint_t *);
	
	/* Creates new file with passed parent and object keys. */
	errno_t (*create) (reiser4_object_t *, object_hint_t *);

	/* Delete file body and stat data if any. */
	errno_t (*clobber) (reiser4_object_t *);

	/* Writes the data to file from passed buffer. */
	int64_t (*write) (reiser4_object_t *, void *, uint64_t);

	/* Directory specific methods */
	errno_t (*add_entry) (reiser4_object_t *, entry_hint_t *);
	errno_t (*rem_entry) (reiser4_object_t *, entry_hint_t *);
	errno_t (*build_entry) (reiser4_object_t *, entry_hint_t *);
	
	/* Truncates file at current offset onto passed units. */
	errno_t (*truncate) (reiser4_object_t *, uint64_t);

	/* Function for going through all metadata blocks specfied file
	   occupied. It is needed for accessing file's metadata. */
	errno_t (*metadata) (reiser4_object_t *, place_func_t, void *);
	
	/* Function for going through the all data blocks specfied file
	   occupies. It is needed for the purposes like data fragmentation
	   measuring, etc. */
	errno_t (*layout) (reiser4_object_t *, region_func_t, void *);

	/* Converts file body to item denoted by @plug. */
	errno_t (*convert) (reiser4_object_t *, reiser4_plug_t *plug);
	
	/* Checks and recover the structure of the object. */
	errno_t (*check_struct) (reiser4_object_t *, place_func_t, 
				 void *, uint8_t);
	
	/* Checks attach of the @object to the @parent. */
	errno_t (*check_attach) (reiser4_object_t *, reiser4_object_t *,
				 place_func_t, void *, uint8_t);
	
	/* Realizes if the object can be of this plugin and can be recovered as
	   a such. */
	errno_t (*recognize) (reiser4_object_t *);
	
	/* Creates the fake object by the gived @info. Needed to recover "/" and
	   "lost+found" direcories if their SD are broken. */
	errno_t (*fake) (reiser4_object_t *);
#endif
	
	/* Change current position to passed value. */
	errno_t (*seek) (reiser4_object_t *, uint64_t);
	
	/* Opens file with specified key */
	errno_t (*open) (reiser4_object_t *);

	/* Closes previously opened or created directory. */
	void (*close) (reiser4_object_t *);

	/* Resets internal position. */
	errno_t (*reset) (reiser4_object_t *);
   
	/* Returns current position in directory. */
	uint64_t (*offset) (reiser4_object_t *);

	/* Makes lookup inside file */
	lookup_t (*lookup) (reiser4_object_t *, char *, entry_hint_t *);

	/* Finds actual file stat data (used in symlinks). */
	errno_t (*follow) (reiser4_object_t *, reiser4_key_t *,
			   reiser4_key_t *);

	/* Reads the data from file to passed buffer. */
	int64_t (*read) (reiser4_object_t *, void *, uint64_t);

	/* Directory read method. */
	int32_t (*readdir) (reiser4_object_t *, entry_hint_t *);

	/* Return current position in directory. */
	errno_t (*telldir) (reiser4_object_t *, reiser4_key_t *);

	/* Change current position in directory. */
	errno_t (*seekdir) (reiser4_object_t *, reiser4_key_t *);

#ifndef ENABLE_MINIMAL
	uint64_t sdext_mandatory;
	uint64_t sdext_unknown;
#endif
} reiser4_object_plug_t;

typedef struct item_balance_ops {
	/* Returns unit count in item passed place point to. */
	uint32_t (*units) (reiser4_place_t *);
	
	/* Makes lookup for passed key. */
	lookup_t (*lookup) (reiser4_place_t *, lookup_hint_t *,
			    lookup_bias_t);
	
#ifndef ENABLE_MINIMAL
	/* Merges two neighbour items in the same node. Returns space released
	   Needed for fsck. */
	int32_t (*merge) (reiser4_place_t *, reiser4_place_t *);
	
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
} item_balance_ops_t;

typedef struct item_object_ops {
	/* Reads passed amount of bytes from the item. */
	int64_t (*read_units) (reiser4_place_t *, trans_hint_t *);
		
	/* Fetches one or more units at passed @place to passed hint. */
	int64_t (*fetch_units) (reiser4_place_t *, trans_hint_t *);

#ifndef ENABLE_MINIMAL
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
} item_object_ops_t;

#ifndef ENABLE_MINIMAL
typedef struct item_repair_ops {
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
} item_repair_ops_t;

typedef struct item_debug_ops {
	/* Prints item into specified buffer. */
	void (*print) (reiser4_place_t *, aal_stream_t *, uint16_t);
} item_debug_ops_t;
#endif

typedef struct item_tree_ops {
	/* Initialize the item-specific place info. */
	void (*init) (reiser4_place_t *);
	
	/* Return block number from passed place. Place is nodeptr item. */
	blk_t (*down_link) (reiser4_place_t *);

#ifndef ENABLE_MINIMAL
	/* Update link block number. */
	errno_t (*update_link) (reiser4_place_t *, blk_t);
#endif
} item_tree_ops_t;

typedef struct reiser4_item_plug {
	item_tree_ops_t *tree;
	item_object_ops_t *object;
	item_balance_ops_t *balance;
#ifndef ENABLE_MINIMAL
	item_debug_ops_t *debug;
	item_repair_ops_t *repair;
#endif
} reiser4_item_plug_t;

/* Stat data extension plugin */
typedef struct reiser4_sdext_plug {
#ifndef ENABLE_MINIMAL
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
} reiser4_sdext_plug_t;

/* Node plugin operates on passed block. It doesn't any initialization, so it
   hasn't close method and all its methods accepts first argument aal_block_t,
   not initialized previously hypothetic instance of node. */
typedef struct reiser4_node_plug {
#ifndef ENABLE_MINIMAL
	/* Get node state flags and set them back. */
	uint32_t (*get_state) (reiser4_node_t *);
	void (*set_state) (reiser4_node_t *, uint32_t);

	/* Performs shift of items and units. */
	errno_t (*shift) (reiser4_node_t *, reiser4_node_t *, 
			  shift_hint_t *);

	/* Fuses two neighbour items in passed node at passed positions. */
	errno_t (*merge) (reiser4_node_t *, pos_t *, pos_t *);
    
	/* Checks thoroughly the node structure and fixes what needed. */
	errno_t (*check_struct) (reiser4_node_t *, uint8_t);

	/* Packing/unpacking metadata. */
	reiser4_node_t *(*unpack) (aal_block_t *, 
				   reiser4_plug_t *, 
				   aal_stream_t *);
	
	errno_t (*pack) (reiser4_node_t *, aal_stream_t *);

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
} reiser4_node_plug_t;

/* Hash plugin operations. */
typedef struct reiser4_hash_plug {
	uint64_t (*build) (unsigned char *, uint32_t);
} reiser4_hash_plug_t;

typedef struct reiser4_fibre_plug {
	uint8_t (*build) (char *, uint32_t);
} reiser4_fibre_plug_t;

/* Disk-format plugin */
typedef struct reiser4_format_plug {
#ifndef ENABLE_MINIMAL
	/* Called during filesystem creating. It forms format-specific super
	   block, initializes plugins and calls their create method. */
	generic_entity_t *(*create) (aal_device_t *, format_hint_t *);

	/* Save the important permanent info about the format into the stream 
	   to be backuped on the fs & check this info. */
	errno_t (*backup) (generic_entity_t *, backup_hint_t *);
	errno_t (*check_backup) (backup_hint_t *);

	/* Regenerate the format instance by the backup. */
	generic_entity_t *(*regenerate) (aal_device_t *, backup_hint_t *);
	
	/* Save format data to device. */
	errno_t (*sync) (generic_entity_t *);

	/* Change entity state (dirty, etc) */
	uint32_t (*get_state) (generic_entity_t *);
	void (*set_state) (generic_entity_t *, uint32_t);

	/* Format pack/unpack methods. */
	generic_entity_t *(*unpack) (aal_device_t *, uint32_t, aal_stream_t *);
	errno_t (*pack) (generic_entity_t *, aal_stream_t *);
	
	/* Update only fields which can be changed after journal replay in
	   memory to avoid second checking. */
	errno_t (*update) (generic_entity_t *);
	    
	/* Prints all useful information about the format */
	void (*print) (generic_entity_t *, aal_stream_t *, uint16_t);
    
	void (*set_len) (generic_entity_t *, uint64_t);
	void (*set_root) (generic_entity_t *, uint64_t);
	void (*set_free) (generic_entity_t *, uint64_t);
	void (*set_stamp) (generic_entity_t *, uint32_t);
	void (*set_policy) (generic_entity_t *, rid_t);
	void (*set_height) (generic_entity_t *, uint16_t);

	/* Return plugin ids for journal, block allocator, and oid allocator
	   components. */
	rid_t (*journal_pid) (generic_entity_t *);
	rid_t (*alloc_pid) (generic_entity_t *);
	rid_t (*oid_pid) (generic_entity_t *);

	/* Format enumerator function. */
	errno_t (*layout) (generic_entity_t *, region_func_t, void *);

	/* Basic consistency checks */
	errno_t (*valid) (generic_entity_t *);

	/* Check format-specific super block for validness. */
	errno_t (*check_struct) (generic_entity_t *, backup_hint_t *, 
				 format_hint_t *, uint8_t);
#endif
	/* Returns the key plugin id. */
	rid_t (*key_pid) (generic_entity_t *);
	
	/* Called during filesystem opening (mounting). It reads format-specific
	   super block and initializes plugins suitable for this format. */
	generic_entity_t *(*open) (aal_device_t *, uint32_t);
    
	/* Closes opened or created previously filesystem. Frees all assosiated
	   memory. */
	void (*close) (generic_entity_t *);

	/* Get tree root block number from format. */
	uint64_t (*get_root) (generic_entity_t *);

	/* Get tree height from format. */
	uint16_t (*get_height) (generic_entity_t *);

#ifndef ENABLE_MINIMAL
	/* Gets start of the filesystem. */
	uint64_t (*start) (generic_entity_t *);

	/* Format length in blocks. */
	uint64_t (*get_len) (generic_entity_t *);

	/* Number of free blocks. */
	uint64_t (*get_free) (generic_entity_t *);

	/* Return mkfs stamp. */
	uint32_t (*get_stamp) (generic_entity_t *);

	/* Return policy (tail, extents, etc). */
	rid_t (*get_policy) (generic_entity_t *);

	/* Returns area where oid data lies in */
	void (*oid_area) (generic_entity_t *, void **, uint32_t *);
#endif
} reiser4_format_plug_t;

#ifndef ENABLE_MINIMAL
typedef struct reiser4_oid_plug {
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
} reiser4_oid_plug_t;

typedef struct reiser4_alloc_plug {
	/* Functions for create and open block allocator. */
	generic_entity_t *(*open) (aal_device_t *, uint32_t, uint64_t);
	generic_entity_t *(*create) (aal_device_t *, uint32_t, uint64_t);

	/* Closes block allocator. */
	void (*close) (generic_entity_t *);

	/* Saves block allocator data to desired device. */
	errno_t (*sync) (generic_entity_t *);

	/* Make dirty and clean functions. */
	uint32_t (*get_state) (generic_entity_t *);
	void (*set_state) (generic_entity_t *, uint32_t);
	
	/* Format pack/unpack methods. */
	errno_t (*pack) (generic_entity_t *, aal_stream_t *);
	generic_entity_t *(*unpack) (aal_device_t *, uint32_t, aal_stream_t *);
	
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
} reiser4_alloc_plug_t;

typedef struct reiser4_journal_plug {
	/* Opens journal on specified device. */
	generic_entity_t *(*open) (aal_device_t *, uint32_t, 
				   generic_entity_t *, generic_entity_t *,
				   uint64_t, uint64_t);

	/* Creates journal on specified device. */
	generic_entity_t *(*create) (aal_device_t *, uint32_t, 
				     generic_entity_t *, generic_entity_t *, 
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
	void (*print) (generic_entity_t *, aal_stream_t *, uint16_t);
	
	/* Checks thoroughly the journal structure. */
	errno_t (*check_struct) (generic_entity_t *, layout_func_t, void *);

	/* Invalidates the journal. */
	void (*invalidate) (generic_entity_t *);
	
	/* Calls func for each block in block allocator. */
	errno_t (*layout) (generic_entity_t *, region_func_t, void *);

	/* Pack/unpack the journal blocks. */
	errno_t (*pack) (generic_entity_t *, aal_stream_t *);
	generic_entity_t *(*unpack) (aal_device_t *, uint32_t, 
				     generic_entity_t *, 
				     generic_entity_t *, 
				     uint64_t, uint64_t, 
				     aal_stream_t *);
} reiser4_journal_plug_t;

/* Tail policy plugin operations. */
typedef struct reiser4_policy_plug {
	int (*tails) (uint64_t);
} reiser4_policy_plug_t;

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
typedef struct plug_class {
	/* Plugin initialization routine. */
	plug_init_t init;
	
	/* Plugin finalization routine. */
	plug_fini_t fini;
} plug_class_t;

/* Plugin id, type and group. */
typedef struct plug_ident {
	rid_t id;
	uint8_t group;
	uint8_t type;
} plug_ident_t;

#define class_init {NULL, NULL}

struct reiser4_plug {
	/* Plugin class. This will be used by plugin factory for initializing
	   plugin. */
	plug_class_t cl;

	/* Plugin id. This will be used for looking for a plugin. */
	plug_ident_t id;
	
#ifndef ENABLE_MINIMAL
	/* Plugin label (name). */
	const char label[PLUG_MAX_LABEL];
	
	/* Short plugin description. */
	const char desc[PLUG_MAX_DESC];
#endif

        /* All possible plugin operations. */
	union {
		reiser4_key_plug_t *key;
		reiser4_item_plug_t *item;
		reiser4_node_plug_t *node;
		reiser4_hash_plug_t *hash;
		reiser4_fibre_plug_t *fibre;
		reiser4_sdext_plug_t *sdext;
		reiser4_object_plug_t *object;
		reiser4_format_plug_t *format;

#ifndef ENABLE_MINIMAL
		reiser4_oid_plug_t *oid;
		reiser4_alloc_plug_t *alloc;
		reiser4_policy_plug_t *policy;
		reiser4_journal_plug_t *journal;
#endif
	} pl;
};

/* Macros for dirtying nodes place lie at. */
#define place_mkdirty(place) \
        ((place)->node->block->dirty = 1)

#define place_mkclean(place) \
        ((place)->node->block->dirty = 0)

#define place_isdirty(place) \
        ((place)->node->block->dirty)

typedef struct flow_ops {
	/* Reads data from the tree. */
	int64_t (*read) (tree_entity_t *, trans_hint_t *);

#ifndef ENABLE_MINIMAL
	/* Writes data to tree. */
	int64_t (*write) (tree_entity_t *, trans_hint_t *);

	/* Truncates data from tree. */
	int64_t (*truncate) (tree_entity_t *, trans_hint_t *);
	
	/* Convert some particular place to another plugin. */
	errno_t (*convert) (tree_entity_t *, conv_hint_t *);
#endif
} flow_ops_t;

typedef struct tree_ops {
	/* Makes lookup in the tree in order to know where say stat data item of
	   a file realy lies. It is used in all object plugins. */
	lookup_t (*lookup) (tree_entity_t *, lookup_hint_t *, 
			    lookup_bias_t, reiser4_place_t *);

#ifndef ENABLE_MINIMAL
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

	/* increment/decriment the free block count in the format. */
	errno_t (*inc_free) (tree_entity_t *, count_t);
	errno_t (*dec_free) (tree_entity_t *, count_t);
#endif
	/* Returns the next item. */
	errno_t (*next_item) (tree_entity_t *, reiser4_place_t *, 
			      reiser4_place_t *);
} tree_ops_t;

typedef struct factory_ops {
	/* Finds plugin by its attributes (type and id). */
	reiser4_plug_t *(*ifind) (rid_t, rid_t);
} factory_ops_t;

#ifdef ENABLE_SYMLINKS
typedef struct object_ops {
	errno_t (*resolve) (tree_entity_t *, char *, 
			    reiser4_key_t *, reiser4_key_t *);
} object_ops_t;
#endif

typedef struct pset_ops {
	/* Obtains the plugin from the profile by its profile index. */
	reiser4_plug_t *(*find) (rid_t, rid_t);
#ifndef ENABLE_MINIMAL
	/* Diffs 2 psets & returns what needs to be stored on disk. */
	uint64_t (*build_mask) (tree_entity_t *, reiser4_opset_t *);
#endif
} pset_ops_t;

#ifndef ENABLE_MINIMAL
typedef struct key_ops {
	char *(*print) (reiser4_key_t *, uint16_t);
} key_ops_t;

typedef struct item_ops {
	/* Checks if items mergeable. */
	int (*mergeable) (reiser4_place_t *, reiser4_place_t *);
} item_ops_t;
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

#ifndef ENABLE_MINIMAL
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
