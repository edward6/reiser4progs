/*
 * Copyright 2001, 2002 by Hans Reiser, licensing governed by reiser4/README
 */

/*
 * Tree operations. See fs/reiser4/tree.c for comments
 */

#if !defined( __REISER4_TREE_H__ )
#define __REISER4_TREE_H__

/** fictive block number never actually used */
extern const reiser4_block_nr FAKE_TREE_ADDR;

/*
 * define typed list for cbk_cache lru
 */
TS_LIST_DECLARE( cbk_cache );

/**
 * &cbk_cache_slot - entry in a coord cache.
 *
 * This is entry in a coord_by_key (cbk) cache, represented by
 * &cbk_cache.
 *
 */
typedef struct cbk_cache_slot {
	/** cached node */
	znode              *node;
	/** linkage to the next cbk cache slot in a LRU order */
	cbk_cache_list_link lru;
} cbk_cache_slot;

/**
 * &cbk_cache - coord cache. This is part of reiser4_tree.
 *
 * cbk_cache is supposed to speed up tree lookups by caching results of recent
 * successful lookups (we don't cache negative results as dentry cache
 * does). Cache consists of relatively small number of entries kept in a LRU
 * order. Each entry (&cbk_cache_slot) containts a pointer to znode, from
 * which we can obtain a range of keys that covered by this znode. Before
 * embarking into real tree traversal we scan cbk_cache slot by slot and for
 * each slot check whether key we are looking for is between minimal and
 * maximal keys for node pointed to by this slot. If no match is found, real
 * tree traversal is performed and if result is successful, appropriate entry
 * is inserted into cache, possibly pulling least recently used entry out of
 * it.
 *
 * Tree spin lock is used to protect coord cache. If contention for this
 * lock proves to be too high, more finer grained locking can be added.
 *
 */
typedef struct cbk_cache {
	int                 nr_slots;
	/** head of LRU list of cache slots */
	cbk_cache_list_head lru;
	/** actual array of slots */
	cbk_cache_slot     *slot;
	/** serializator */
	spinlock_t          guard;
} cbk_cache;

TS_LIST_DEFINE( cbk_cache, cbk_cache_slot, lru );

/**
 * level_lookup_result - possible outcome of looking up key at some level.
 * This is used by coord_by_key when traversing tree downward.
 */
typedef enum {
	/** continue to the next level */
	LOOKUP_CONT,
	/** done. Either required item was found, or we can prove it
	 * doesn't exist, or some error occurred. */
	LOOKUP_DONE,
	/** restart traversal from the root. Infamous "repetition". */
	LOOKUP_REST
} level_lookup_result;

/** PUT THIS IN THE SUPER BLOCK
 *
 * This is representation of internal reiser4 tree where all file-system
 * data and meta-data are stored. This structure is passed to all tree
 * manipulation functions. It's different from the super block because:
 * we don't want to limit ourselves to strictly one to one mapping
 * between super blocks and trees, and, because they are logically
 * different: there are things in a super block that have no relation to
 * the tree (bitmaps, journalling area, mount options, etc.) and there
 * are things in a tree that bear no relation to the super block, like
 * tree of znodes.
 *
 * At this time, there is only one tree
 * per filesystem, and this struct is part of the super block.  We only
 * call the super block the super block for historical reasons (most
 * other filesystems call the per filesystem metadata the super block).
 */
struct reiser4_tree {
	/* block_nr == 0 is fake znode. Write lock it, while changing
	   tree height. */
	/** disk address of root node of a tree */
	reiser4_block_nr     root_block;

	/** level of the root node. If this is 1, tree consists of root
	    node only */
	tree_level           height;

	/** cache of recent tree lookup results */
	cbk_cache            cbk_cache;

	/** hash table to look up znodes by block number. */
	z_hash_table         zhash_table;
	/** hash table to look up jnodes by inode and offset. */
	j_hash_table         jhash_table;
	__u64                znode_epoch;

	/** lock protecting:
	 *  - parent pointers,
	 *  - sibling pointers,
	 *  - znode hash table
	 *  - coord cache
	 */
	spinlock_t         tree_lock;

	/**
	 * lock protecting delimiting keys
	 *
	 */
	spinlock_t         dk_lock;

	/** default plugin used to create new nodes in a tree. */
	node_plugin         *nplug;
	struct super_block  *super;
	struct {
		/** carry flags used for insertion of new nodes */
		__u32        new_node_flags;
		/** carry flags used for insertion of new extents */
		__u32        new_extent_flags;
		/** carry flags used for paste operations */
		__u32        paste_flags;
		/** carry flags used for insert operations */
		__u32        insert_flags;
	} carry;
};

extern void init_tree_0( reiser4_tree * );

extern int init_tree( reiser4_tree *tree,
		      const reiser4_block_nr *root_block,
		      tree_level height, node_plugin *default_plugin);
extern void done_tree( reiser4_tree *tree );

/**
 * &reiser4_item_data - description of data to be inserted or pasted
 *
 * Q: articulate the reasons for the difference between this and flow.
 *
 * A: Becides flow we insert into tree other things: stat data, directory
 * entry, etc.  To insert them into tree one has to provide this structure. If
 * one is going to insert flow - he can use insert_flow, where this structure
 * does not have to be created
 */
struct reiser4_item_data {
	/**
	 * actual data to be inserted. If NULL, ->create_item() will not
	 * do xmemcpy itself, leaving this up to the caller. This can
	 * save some amount of unnecessary memory copying, for example,
	 * during insertion of stat data.
	 *
	 */
	char           *data;
	/* 1 if 'char * data' contains pointer to user space and 0 if it is
	 * kernel space 

	could be a char not an int?
	*/
	int             user;
	/**
	 * amount of data we are going to insert or paste
	 */
	int             length;
	/**
	 *  "Arg" is opaque data that is passed down to the
	 *  ->create_item() method of node layout, which in turn
	 *  hands it to the ->create_hook() of item being created. This
	 *  arg is currently used by:
	 *
	 *  .  ->create_hook() of internal item
	 *  (fs/reiser4/plugin/item/internal.c:internal_create_hook()),
	 *  . ->paste() method of directory item.
	 *  . ->create_hook() of extent item
	 *
	 * For internal item, this is left "brother" of new node being
	 * inserted and it is used to add new node into sibling list
	 * after parent to it was just inserted into parent.
	 *
	 * While ->arg does look somewhat of unnecessary compication,
	 * it actually saves a lot of headache in many places, because
	 * all data necessary to insert or paste new data into tree are
	 * collected in one place, and this eliminates a lot of extra
	 * argument passing and storing everywhere.
	 *
	 */
/* arg is a bad name. */
	void           *arg;
	/**
	 * plugin of item we are inserting
	 */
	item_plugin   *iplug;
};

/** cbk flags: options for coord_by_key() */
typedef enum {
	/** 
	 * coord_by_key() is called for insertion. This is necessary because
	 * of extents being located at the twig level. For explanation, see
	 * comment just above is_next_item_internal(). 
	 */
	CBK_FOR_INSERT =    ( 1 << 0 ),
	/** coord_by_key() is called with key that is known to be unique */
	CBK_UNIQUE     =    ( 1 << 1 ),
	/** 
	 * coord_by_key() can trust delimiting keys. This options is not user
	 * accessible. coord_by_key() will set it automatically. It will be
	 * only cleared by special-case in extents-on-the-twig-level handling
	 * where it is necessary to insert item with a key smaller than
	 * leftmost key in a node. This is necessary because of extents being
	 * located at the twig level. For explanation, see comment just above
	 * is_next_item_internal().
	 */
	CBK_TRUST_DK   =    ( 1 << 2 )
} cbk_flags;

/** insertion outcome. IBK = insert by key */
typedef enum { IBK_INSERT_OK        = 0,
	       IBK_ALREADY_EXISTS   = -EEXIST,
	       IBK_IO_ERROR         = -EIO,
	       IBK_NO_SPACE         = -ENOSPC,
	       IBK_OOM              = -ENOMEM
} insert_result;

typedef enum { RESIZE_OK           = 0,
	       RESIZE_NO_SPACE     = -ENOSPC,
	       RESIZE_IO_ERROR     = -EIO,
	       RESIZE_OOM          = -ENOMEM
} resize_result;

typedef int ( *tree_iterate_actor_t )( reiser4_tree *tree, 
				       coord_t *coord,
				       lock_handle *lh,
				       void *arg );
extern int iterate_tree( reiser4_tree *tree, coord_t *coord, lock_handle *lh, 
			 tree_iterate_actor_t actor, void *arg,
			 znode_lock_mode mode, int through_units_p );

/** return node plugin of @node */
static inline node_plugin *
node_plugin_by_node( const znode *node /* node to query */ )
{
	assert( "vs-213", node != NULL );
	assert( "vs-214", znode_is_loaded( node ) );

	return node->nplug;
}

static inline unsigned node_num_items (const znode * node)
{
	assert ("nikita-2468", 
		node_plugin_by_node (node)->num_of_items (node) == node->nr_items);
	return node->nr_items;
}

static inline int node_is_empty (const znode * node)
{
	return node_num_items (node) == 0;
}


typedef enum { SHIFTED_SOMETHING  = 0,
	       SHIFT_NO_SPACE     = -ENOSPC,
	       SHIFT_IO_ERROR     = -EIO,
	       SHIFT_OOM          = -ENOMEM,
} shift_result;

extern node_plugin *node_plugin_by_coord ( const coord_t *coord );
extern int is_coord_in_node( const coord_t *coord );
extern int key_in_node( const reiser4_key *, const coord_t * );
extern void coord_item_move_to( coord_t *coord, int items );
extern void coord_unit_move_to( coord_t *coord, int units );

/* there are two types of repetitive accesses (ra): intra-syscall
   (local) and inter-syscall (global). Local ra is used when
   during single syscall we add/delete several items and units in the
   same place in a tree. Note that plan-A fragments local ra by
   separating stat-data and file body in key-space. Global ra is
   used when user does repetitive modifications in the same place in a
   tree.

   Our ra implementation serves following purposes:
    1 it affects balancing decisions so that next operation in a row
      can be performed faster;
    2 it affects lower-level read-ahead in page-cache;
    3 it allows to avoid unnecessary lookups by maintaining some state
      accross several operations (this is only for local ra);
    4 it leaves room for lazy-micro-balancing: when we start a sequence of
      operations they are performed without actually doing any intra-node
      shifts, until we finish sequence or scope of sequence leaves
      current node, only then we really pack node (local ra only).
*/

/* another thing that can be useful is to keep per-tree and/or
   per-process cache of recent lookups. This cache can be organised as a
   list of block numbers of formatted nodes sorted by starting key in
   this node. Balancings should invalidate appropriate parts of this
   cache.
 */

lookup_result coord_by_key( reiser4_tree *tree, const reiser4_key *key,
			    coord_t *coord, lock_handle * handle,
			    znode_lock_mode lock, lookup_bias bias, 
			    tree_level lock_level, tree_level stop_level, 
			    __u32 flags );
lookup_result coord_by_hint_and_key (reiser4_tree * tree, 
				     const reiser4_key * key,
				     coord_t * coord, lock_handle * handle,
				     lookup_bias bias, tree_level lock_level,
				     tree_level stop_level);
insert_result insert_by_key( reiser4_tree *tree, const reiser4_key *key,
			     reiser4_item_data *data, coord_t *coord,
			     lock_handle *lh,
			     tree_level stop_level,
			     inter_syscall_rap *ra,
			     intra_syscall_rap ira, __u32 flags );
insert_result insert_by_coord( coord_t  *coord,
			       reiser4_item_data *data, const reiser4_key *key,
			       lock_handle *lh,
			       inter_syscall_rap *ra UNUSED_ARG,
			       intra_syscall_rap ira UNUSED_ARG,
			       cop_insert_flag );
insert_result insert_extent_by_coord( coord_t  *coord,
				      reiser4_item_data *data,
				      const reiser4_key *key,
				      lock_handle *lh );
int cut_node (coord_t * from, coord_t * to,
	      const reiser4_key * from_key,
	      const reiser4_key * to_key,
	      reiser4_key * smallest_removed, unsigned flags,
	      znode * left);

resize_result resize_item( coord_t *coord, reiser4_item_data *data,
			   reiser4_key *key, lock_handle *lh,
			   cop_insert_flag );
int insert_into_item( coord_t *coord, lock_handle *lh, reiser4_key *key, 
		      reiser4_item_data *data,
		      cop_insert_flag );
int insert_flow( coord_t *coord, lock_handle *lh, flow_t *f);
int find_new_child_ptr( znode *parent, znode *child, znode *left, 
			coord_t *result );

int shift_right_of_but_excluding_insert_coord (coord_t * insert_coord);
int shift_left_of_and_including_insert_coord (coord_t * insert_coord);
int shift_everything_left (znode * right, znode * left, carry_level *todo);
znode *insert_new_node (coord_t * insert_coord, lock_handle * lh);
int cut_tree (reiser4_tree * tree, 
	      const reiser4_key * from_key, const reiser4_key * to_key);

extern int check_tree_pointer( const coord_t *pointer, const znode *child );
extern int find_new_child_ptr( znode *parent, znode *child UNUSED_ARG,
			       znode *left, coord_t *result );
extern int find_child_ptr( znode *parent, znode *child, coord_t *result );
extern int find_child_by_addr( znode *parent, znode *child, 
			       coord_t *result );
extern int find_child_delimiting_keys( znode *parent, 
				       const coord_t *in_parent, 
				       reiser4_key *ld, reiser4_key *rd );
extern znode *child_znode( const coord_t *in_parent, znode *parent, int incore_p,
			   int setup_dkeys_p );

extern void print_coord_content( const char *prefix, coord_t *p );
extern void print_address( const char *prefix, const reiser4_block_nr *block );
extern const char *bias_name( lookup_bias bias );
extern int  cbk_cache_init( cbk_cache *cache );
extern void cbk_cache_done( cbk_cache *cache );
extern void cbk_cache_invalidate( const znode *node, reiser4_tree *tree );
extern void cbk_cache_add( const znode *node );

extern int check_jnode_for_unallocated (jnode * node);

#if REISER4_DEBUG
extern void print_tree_rec (const char * prefix, reiser4_tree * tree, __u32 flags);
extern void print_cbk_slot( const char *prefix, const cbk_cache_slot *slot );
extern void print_cbk_cache( const char *prefix, const cbk_cache  *cache );
#else
#define print_tree_rec( p, f, t ) noop
#define print_cbk_slot( p, s ) noop
#define print_cbk_cache( p, c ) noop
#endif

extern void forget_znode (lock_handle *handle);
extern int deallocate_znode( znode *node );

extern int is_disk_addr_unallocated( const reiser4_block_nr *addr );
extern void *unallocated_disk_addr_to_ptr( const reiser4_block_nr *addr );

/** struct used internally to pack all numerous arguments of tree lookup.
    Used to avoid passing a lot of arguments to helper functions. */
typedef struct cbk_handle {
	/** tree we are in */
	reiser4_tree        *tree;
	/** key we are going after */
	const reiser4_key   *key;
	/** coord we will store result in */
	coord_t  	    *coord;
	/** type of lock to take on target node */
	znode_lock_mode      lock_mode;
	/** lookup bias. See comments at the declaration of lookup_bias */
	lookup_bias          bias;
	/** lock level */
	tree_level           lock_level;
	/** 
	 * level where search will stop. Either item will be found between
	 * lock_level and stop_level, or CBK_COORD_NOTFOUND will be
	 * returned. 
	 */
	tree_level           stop_level;
	/** level we are currently at */
	tree_level           level;
	/** 
	 * block number of @active node. Tree traversal operates on two
	 * nodes: active and parent. 
	 */
	reiser4_block_nr     block;
	/** put here error message to be printed by caller */
	const char          *error;
	/** result passed back to caller */
	lookup_result        result;
	/** lock handles for active and parent */
	lock_handle         *parent_lh;
	lock_handle         *active_lh;
	reiser4_key          ld_key;
	reiser4_key          rd_key;
	/**
	 * flags, passed to the cbk routine. Bits of this bitmask are defined
	 * in tree.h:cbk_flags enum.
	 */
	__u32                flags;
} cbk_handle;

extern znode_lock_mode cbk_lock_mode( tree_level level, cbk_handle *h );

/* eottl.c */
extern int handle_eottl( cbk_handle *h, int *outcome );

int lookup_multikey( cbk_handle *handle, int nr_keys );
int lookup_couple( reiser4_tree *tree,
		   const reiser4_key *key1, const reiser4_key *key2,
		   coord_t *coord1, coord_t *coord2,
		   lock_handle *lh1, lock_handle *lh2,
		   znode_lock_mode lock_mode, lookup_bias bias,
		   tree_level lock_level, tree_level stop_level,
		   __u32 flags, int *result1, int *result2 );

/* list of active lock stacks */
TS_LIST_DECLARE(context);

/** 
 * global context used during system call. Variable of this type is
 * allocated on the stack at the beginning of the reiser4 part of the
 * system call and pointer to it is stored in the
 * current->journal_info. This allows us to avoid passing pointer to
 * current transaction and current lockstack (both in one-to-one mapping
 * with threads) all over the call chain.

 * It's kind of like those global variables the prof used to tell you
 * not to use in CS1, except thread specific.;-) Nikita, this was a
 * good idea.
 */
struct reiser4_context {
	/** magic constant. For debugging */
	__u32                 magic;

	/** current lock stack. See lock.[ch]. This is where list of all
	 * locks taken by current thread is kept. This is also used in
	 * deadlock detection.
	 */
	lock_stack            stack;

	/** current transcrash. */
	txn_handle           *trans;
	txn_handle            trans_in_ctx;

	/** super block we are working with.  To get the current tree
	 * use &get_super_private (reiser4_get_current_sb ())->tree.
	 */
	struct super_block   *super;

	/**
	 * per-thread grabbed (for further allocation) blocks counter
	 */
	reiser4_block_nr      grabbed_blocks;

	/**
	 * per-thread tracing flags. Use reiser4_trace_flags enum to set
	 * bits in it.
	 */
	__u32                 trace_flags;

	/** thread ID */
	__u32                 tid;

	/**
	 * A link of all active contexts. */
	context_list_link     contexts_link;
	/** parent context */
	reiser4_context      *parent;
	tap_list_head         taps;
#if REISER4_DEBUG
	lock_counters_info    locks;
	int                   nr_children; /* number of child contexts */
	struct task_struct    *task; /* so we can easily find owner of the stack */
#endif
};

extern reiser4_context * get_context_by_lock_stack (lock_stack*);

/* Debugging helps. */
extern int  init_context_mgr (void);
#if REISER4_DEBUG
extern void show_context     (int show_tree);
#else
#define show_context(st) noop
#endif

/* Hans, is this too expensive? */
#define current_tree (&(get_super_private (reiser4_get_current_sb ())->tree))
#define current_blocksize current_tree->super->s_blocksize
#define current_blocksize_bits current_tree->super->s_blocksize_bits

extern int  init_context( reiser4_context *context,
				  struct super_block *super );
extern void done_context( reiser4_context *context );

/** return context associated with given thread */
static inline reiser4_context *get_context( const struct task_struct *tsk )
{
	if (tsk == NULL) {
		BUG ();
	}
	
	return tsk -> journal_info;
}

/** return context associated with current thread */
static inline reiser4_context *get_current_context(void)
{
	reiser4_context *context;

	context = get_context( current );
	if( context != NULL )
		return context -> parent;
	else
		return NULL;
}


/* comment me.  Say something clever, like I am called at every reiser4 entry point, and I create a struct that is used
   to allow functions to efficiently pass large amounts of parameters around by moving a pointer to the parameters
   called "context". */
#define __REISER4_ENTRY( super, errret )			\
	reiser4_context __context;				\
	do {							\
                int __ret;					\
                __ret = init_context( &__context, ( super ) );	\
                if (__ret != 0) {				\
			return errret;				\
		}						\
        } while (0)

#define REISER4_ENTRY_PTR( super )  __REISER4_ENTRY( super, ERR_PTR(__ret) )
#define REISER4_ENTRY( super )      __REISER4_ENTRY( super, __ret )

#define __REISER4_EXIT( context )		\
({						\
        int __ret1 = txn_end( context );	\
	done_context( context );		\
        __ret1;					\
})

#define REISER4_EXIT( ret_exp ) 		                       \
({						                       \
	typeof ( ret_exp ) __result = ( ret_exp );                     \
        int __ret = __REISER4_EXIT( &__context );                      \
	return __result ? : __ret;		                       \
})

#define REISER4_EXIT_PTR( ret_exp ) 		                       \
({						                       \
	typeof ( ret_exp ) __result = ( ret_exp );                     \
        int __ret = __REISER4_EXIT( &__context );                      \
	return IS_ERR (__result) ? __result : ERR_PTR (__ret);         \
})

/*
 * ordering constraint for tree spin lock: tree lock is "strongest"
 */
#define spin_ordering_pred_tree( tree ) ( 1 )

/* Define spin_lock_tree, spin_unlock_tree, and spin_tree_is_locked:
 * spin lock protecting znode hash, and parent and sibling pointers. */   
SPIN_LOCK_FUNCTIONS( tree, reiser4_tree, tree_lock );


/*
 * ordering constraint for delimiting key spin lock: dk lock is weaker than 
 * tree lock
 */
#define spin_ordering_pred_dk( tree ) 				\
	( lock_counters() -> spin_locked_tree == 0 )
/*
 * Define spin_lock_dk(), spin_unlock_dk(), etc: locking for delimiting
 * keys.
 */
SPIN_LOCK_FUNCTIONS( dk, reiser4_tree, dk_lock );

#if REISER4_DEBUG
#define check_tree() print_tree_rec( "", current_tree, REISER4_TREE_CHECK )
TS_LIST_DEFINE( context, reiser4_context, contexts_link );
#else
#define check_tree() noop
#endif

/* __REISER4_TREE_H__ */
#endif

/*
 * Make Linus happy.
 * Local variables:
 * c-indentation-style: "K&R"
 * mode-name: "LC"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 120
 * scroll-step: 1
 * End:
 */
