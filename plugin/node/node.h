/* We need a definition of the default node layout here. */

/* 

Generally speaking, it is best to have free space in the middle of the
node so that two sets of things can grow towards it, and to have the
item bodies on the left so that the last one of them grows into free
space.  We optimize for the case where we append new items to the end
of the node, or grow the last item, because it hurts nothing to so
optimize and it is a common special case to do massive insertions in
increasing key order (and one of cases more likely to have a real user
notice the delay time for).

formatted leaf default layout: (leaf1)

|node header:item bodies:free space:key + pluginid + item offset|

We grow towards the middle, optimizing layout for the case where we
append new items to the end of the node.  The node header is fixed
length.  Keys, and item offsets plus pluginids for the items
corresponding to them are in increasing key order, and are fixed
length.  Item offsets are relative to start of node (16 bits creating
a node size limit of 64k, 12 bits might be a better choice....).  Item
bodies are in decreasing key order.  Item bodies have a variable size.
There is a one to one to one mapping of keys to item offsets to item
bodies.  Item offsets consist of pointers to the zeroth byte of the
item body.  Item length equals the start of the next item minus the
start of this item, except the zeroth item whose length equals the end
of the node minus the start of that item (plus a byte).  In other
words, the item length is not recorded anywhere, and it does not need
to be since it is computable.

Leaf variable length items and keys layout : (lvar)

|node header:key offset + item offset + pluginid triplets:free space:key bodies:item bodies|

We grow towards the middle, optimizing layout for the case where we
append new items to the end of the node.  The node header is fixed
length.  Keys and item offsets for the items corresponding to them are
in increasing key order, and keys are variable length.  Item offsets
are relative to start of node (16 bits).  Item bodies are in
decreasing key order.  Item bodies have a variable size.  There is a
one to one to one mapping of keys to item offsets to item bodies.
Item offsets consist of pointers to the zeroth byte of the item body.
Item length equals the start of the next item's key minus the start of
this item, except the zeroth item whose length equals the end of the
node minus the start of that item (plus a byte).  

leaf compressed keys layout: (lcomp)

|node header:key offset + key inherit + item offset pairs:free space:key bodies:item bodies|

We grow towards the middle, optimizing layout for the case where we
append new items to the end of the node.  The node header is fixed
length.  Keys and item offsets for the items corresponding to them are
in increasing key order, and keys are variable length.  The "key
inherit" field indicates how much of the key prefix is identical to
the previous key (stem compression as described in "Managing
Gigabytes" is used).  key_inherit is a one byte integer.  The
intra-node searches performed through this layout are linear searches,
and this is theorized to not hurt performance much due to the high
cost of processor stalls on modern CPUs, and the small number of keys
in a single node.  Item offsets are relative to start of node (16
bits).  Item bodies are in decreasing key order.  Item bodies have a
variable size.  There is a one to one to one mapping of keys to item
offsets to item bodies.  Item offsets consist of pointers to the
zeroth byte of the item body.  Item length equals the start of the
next item minus the start of this item, except the zeroth item whose
length equals the end of the node minus the start of that item (plus a
byte).  In other words, item length and key length is not recorded
anywhere, and it does not need to be since it is computable.

internal node default layout: (idef1)

just like ldef1 except that item bodies are either blocknrs of
children or extents, and moving them may require updating parent
pointers in the nodes that they point to.

 */
/* There is an inherent 3-way tradeoff between optimizing and
   exchanging disks between different architectures and code
   complexity.  This is optimal and simple and inexchangeable.
   Someone else can do the code for the exchanging disks and make it
   complex. It would not be that hard.  Using other than the PAGE_SIZE
   might be suboptimal.  y*/
#define LEAF40_NODE_SIZE PAGE_CACHE_SIZE

typedef enum { 
	NS_FOUND = 0, 
	NS_NOT_FOUND = -ENOENT, 
	NS_IOERROR = -EIO	/* can this really result, or is the
				   node read into cache (and any IO
				   errors over with) before any node
				   methods are called? */
} node_search_result;

/**
 * Maximal possible space overhead for creation of new item in a node
 */
#define REISER4_NODE_MAX_OVERHEAD ( sizeof( reiser4_key ) + 32 )

typedef enum {
	REISER4_NODE_PRINT_HEADER  = ( 1u << 0 ),
	REISER4_NODE_PRINT_KEYS    = ( 1u << 1 ),
	REISER4_NODE_PRINT_PLUGINS = ( 1u << 2 ),
	REISER4_NODE_PRINT_ITEMS   = ( 1u << 3 ),
	REISER4_NODE_PRINT_DATA    = ( 1u << 4 ),
	REISER4_NODE_CHECK         = ( 1u << 5 ),
	REISER4_NODE_PANIC         = ( 1u << 6 ),
	REISER4_NODE_PRINT_ZNODE   = ( 1u << 7 ),
	REISER4_NODE_DKEYS         = ( 1u << 8 ),
	REISER4_NODE_TREE_STABLE   = ( 1u << 9 ),
	REISER4_NODE_DONT_DOT      = ( 1u << 10 ),
	REISER4_NODE_PRINT_BRIEF   = ( 1u << 11 ),
	REISER4_NODE_ONLY_INCORE   = ( 1u << 12 ),
	REISER4_NODE_SILENT        = ( 1u << 13 ),
	REISER4_COLLECT_STAT       = ( 1u << 14 ),
	REISER4_NODE_PRINT_ALL     = ~0u
} reiser4_node_print_flag;

#define REISER4_TREE_CHECK ( REISER4_NODE_CHECK | REISER4_NODE_ONLY_INCORE | REISER4_NODE_SILENT | REISER4_NODE_TREE_STABLE )
#define REISER4_TREE_VERBOSE ( REISER4_NODE_PRINT_ALL & ~REISER4_NODE_SILENT )
#define REISER4_TREE_BRIEF ( REISER4_NODE_PRINT_BRIEF )
#define REISER4_TREE_CHECK_ALL ( REISER4_TREE_CHECK & ~REISER4_NODE_ONLY_INCORE )

/**
   The responsibility of the node layout is to store and give access
   to the sequence of items within the node.  */
typedef struct node_plugin {
	/* generic fields */
	plugin_header           h;

	/* calculates the amount of space that will be required to store an
	   item which is in addition to the space consumed by the item body.
	   (the space consumed by the item body can be gotten by calling
	   item->estimate) */
	size_t        ( *item_overhead )( const znode *node, flow_t *f );

	/**
	 * returns free space by looking into node (i.e., without using
	 * znode->free_space).
	 */
	size_t           ( *free_space )( znode *node );
	/** search within the node for the one item which might
	    contain the key, invoking item->search_within to search within
	    that item to see if it is in there */
	node_search_result ( *lookup )( znode *node, const reiser4_key *key, 
					lookup_bias bias, 
					coord_t *coord );
	/** number of items in node */
	int ( *num_of_items )( const znode *node );

	/** store information about item in @coord in @data */
	/* break into several node ops, don't add any more uses of this before doing so */
	/*int ( *item_at )( const coord_t *coord, reiser4_item_data *data );*/
	char *( *item_by_coord )( const coord_t *coord );	
	int ( *length_by_coord )( const coord_t *coord );
	item_plugin *( *plugin_by_coord )( const coord_t *coord );

	/** store item key in @key */
	reiser4_key * ( *key_at )( const coord_t *coord, reiser4_key *key );
	/** conservatively estimate whether unit of what size can fit
	    into node. This estimation have to be performed without
	    actually looking into node's content (free space is saved in
	    znode). */
	size_t  ( *estimate )( znode *node );

	/* performs every consistency check the node layout author could
	 * imagine. Optional. */
	int   ( *check )( const znode *node, __u32 flags, const char **error );

	/*
	 * Called when node is read into memory and node layout plugin is
	 * already detected. This should read some data into znode (like free
	 * space counter) and, optionally, check data consistency.
	 */
	int ( *parse )( znode *node );
	/**
	 * This method is called on a new node to initialise plugin specific
	 * data (header, etc.)
	 */
	int ( *init )( znode *node );
	/**
	 * Check whether @node content conforms to this plugin format.
	 * Probably only useful after support for old V3.x formats is added.
	 * Uncomment after 4.0 only.
	 */
	/* 	int ( *guess )( const znode *node ); */
	void ( *print )( const char *prefix, const znode *node, __u32 flags );

	/* change size of @item by @by bytes. @item->node has enough free
	   space. When @by > 0 - free space is appended to end of item. When
	   @by < 0 - item is truncated - it is assumed that last @by bytes if
	   the item are freed already */
	void ( *change_item_size )( coord_t *item, int by );

	/* create new item @length bytes long in coord @target */
	int ( *create_item )( coord_t *target, const reiser4_key *key,
			      reiser4_item_data *data, carry_plugin_info *info );

	/**
	 * update key of item.
	 */
	void ( *update_item_key )( coord_t *target, const reiser4_key *key,
				   carry_plugin_info *info );

	/* remove data between @from and @to from the tree */
	int ( *cut_and_kill )( coord_t *from, coord_t *to,
			       const reiser4_key *from_key,
			       const reiser4_key *to_key,
			       reiser4_key *smallest_removed,
			       carry_plugin_info *info, void *kill_params,
			       __u32 flags );

	/* remove data between @from and @to from a node (when shifting from
	   one node to another, one cuts from a node but does not cut_and_kill
	   from the tree) */
	int ( *cut )( coord_t *from, coord_t *to,
		      const reiser4_key *from_key, const reiser4_key *to_key,
		      reiser4_key *smallest_removed,
		      carry_plugin_info *info, __u32 flags );
	
	/* copy as much as possible but not more than up to @stop from
	   @stop->node to @target. If (pend == append) then data from beginning of
	   @stop->node are copied to the end of @target. If (pend == prepend) then
	   data from the end of @stop->node are copied to the beginning of
	   @target. Copied data are removed from @stop->node. Information
	   about what to do on upper level is stored in @todo */
	int ( *shift ) ( coord_t *stop, znode *target, shift_direction pend, 
			 int delete_node, int including_insert_coord,
			 carry_plugin_info *info );
	/*
	 * return true if this node allows skip carry() in some situations
	 * (see fs/reiser4/tree.c:insert_by_coord()). Reiser3.x format
	 * emulation doesn't.
	 *
	 * This will speedup insertions that doesn't require updates to the
	 * parent, by bypassing initialisation of carry() structures. It's
	 * believed that majority of insertions will fit there.
	 *
	 */
	int ( *fast_insert )( const coord_t *coord );
	int ( *fast_paste )( const coord_t *coord );
	int ( *fast_cut )( const coord_t *coord );
	/*
	 * this limits max size of item which can be inserted into a node and
	 * number of bytes item in a node may be appended with
	 */
	int ( *max_item_size )( void );	
	int ( *prepare_removal )( znode * empty, carry_plugin_info *info );
} node_plugin;


typedef enum { 
	/** standard unified node layout used for both leaf and internal
	    nodes */
	NODE40_ID,
	LAST_NODE_ID
} reiser4_node_id;

extern reiser4_key *leftmost_key_in_node( const znode *node, reiser4_key *key );
extern void print_node_content( const char *prefix, 
				const znode *node, __u32 flags );
extern void indent( unsigned indentation );
extern void indent_znode( const znode *node );

#if REISER4_DEBUG_NODE
extern void node_check( znode *node, __u32 flags );
#define DISABLE_NODE_CHECK				\
({							\
	++ get_current_context() -> disable_node_check;	\
})

#define ENABLE_NODE_CHECK				\
({							\
	-- get_current_context() -> disable_node_check;	\
})

#else
#define node_check( n, f ) noop
#define DISABLE_NODE_CHECK noop
#define ENABLE_NODE_CHECK noop
#endif

extern void indent_znode( const znode *node );

#if YOU_CAN_COMPILE_PSEUDO_CODE

/** returns length of condensation corresponding to the item, in other
    words, the length of the item's data when uncompressed. */
extern size_t get_condensation_length( item *item );

#endif


typedef struct common_node_header {
	/** 
	 * identifier of node plugin. Must be located at the very beginning
	 * of a node.
	 */
	d16            plugin_id;
} common_node_header;

/* 
 * Local variables:
 * c-indentation-style: "K&R"
 * mode-name: "LC"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 120
 * scroll-step: 1
 * End:
 */
