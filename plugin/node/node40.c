/*
 * Copyright 2001, 2002 by Hans Reiser, licensing governed by reiser4/README
 */

#include "../../reiser4.h"

/*
  leaf 40 format:

  [node header | item 0, item 1, .., item N-1 |  free space | item_head N-1, .. item_head 1, item head 0 ]
   plugin_id (16)                                                key
   free_space (16)                                               pluginid (16)
   free_space_start (16)                                         offset (16)
   level (8)
   num_items (16)
   magic (32)
   flush_time (32)
*/

/** magic number that is stored in ->magic field of node header */
const __u32 REISER4_NODE_MAGIC = 0x52344653; /* (*(__u32 *)"R4FS"); */

static int prepare_for_update (znode * left, znode * right, carry_plugin_info *info);


/* header of node of reiser40 format is at the beginning of node */
/* Audited by: green(2002.06.12) */
static inline node40_header *node40_node_header( const znode *node /* node to
								     * query */ )
{
	assert( "nikita-567", node != NULL );
	assert( "nikita-568", znode_page( node ) != NULL );
	assert( "nikita-569", zdata( node ) != NULL );
	return ( node40_header * ) zdata( node );
}


/* functions to get/set fields of node40_header */

static __u32 nh40_get_magic (node40_header * nh)
{
	return d32tocpu (&nh->magic);
}

static void nh40_set_magic (node40_header * nh, __u32 magic)
{
	cputod32 (magic, &nh->magic);
}


/* Audited by: green(2002.06.12) */
static void nh40_set_free_space (node40_header * nh, unsigned value)
{
	cputod16 (value, &nh->free_space);
	/*node->free_space = value;*/
}

/* Audited by: green(2002.06.12) */
static inline unsigned nh40_get_free_space (node40_header * nh)
{
	return d16tocpu (&nh->free_space);
}



/* Audited by: green(2002.06.12) */
static void nh40_set_free_space_start (node40_header * nh, unsigned value)
{
	cputod16 (value, &nh->free_space_start);
}


/* Audited by: green(2002.06.12) */
static inline unsigned nh40_get_free_space_start (node40_header * nh)
{
	return d16tocpu (&nh->free_space_start);
}


/* Audited by: green(2002.06.12) */
static inline void nh40_set_level (node40_header * nh, unsigned value)
{
	cputod8 (value, &nh->level);
}


/* Audited by: green(2002.06.12) */
static unsigned nh40_get_level (node40_header * nh)
{
	return d8tocpu( &nh->level);
}


/* Audited by: green(2002.06.12) */
static void nh40_set_num_items (node40_header * nh, unsigned value)
{
	cputod16 (value, &nh->num_items);
}


/* Audited by: green(2002.06.12) */
static inline unsigned nh40_get_num_items (node40_header * nh)
{
	return d16tocpu( &nh->num_items);
}

/* plugin field of node header should be read/set by
   plugin_by_disk_id/save_disk_plugin */



/* array of item headers is at the end of node */
/* Audited by: green(2002.06.12) */
static item_header40 *node40_ih_at( const znode *node, unsigned pos )
{
	return (item_header40 *)( zdata( node ) + znode_size( node ) ) - pos - 1;
}
/* 

( page_address( node -> pg ) + PAGE_CACHE_SIZE ) - pos - 1

 */
/* Audited by: green(2002.06.12) */
static inline item_header40 *node40_ih_at_coord( const coord_t *coord )
{
	return (item_header40 *)( zdata( coord -> node ) + 
				   znode_size( coord -> node ) ) - 
		( coord -> item_pos ) - 1;
}


/* functions to get/set fields of item_header40 */
/* Audited by: green(2002.06.12) */
static void ih40_set_offset (item_header40 * ih, unsigned offset)
{
	cputod16 (offset, &ih->offset);
}


/* Audited by: green(2002.06.12) */
static inline unsigned ih40_get_offset (item_header40 * ih)
{
	return d16tocpu (&ih->offset);
}


/* plugin field of item header should be read/set by
   plugin_by_disk_id/save_disk_plugin */





/* plugin methods */

/* plugin->u.node.item_overhead
   look for description of this method in plugin/node/node.h
*/
/* Audited by: green(2002.06.12) */
size_t node40_item_overhead (const znode * node UNUSED_ARG, flow_t * f UNUSED_ARG)
{
	return sizeof (item_header40);
}

/*
  int           ( *move_items )( znode *source, pos_in_node source_pos_in_node, znode *target, pos_in_node target_pos_in_node, int item_count );
  size_t           ( *create_body_space)( znode *node, int byte_count, pos_in_node pos );
  size_t           ( *create )( znode *node, pos_in_node pos, flow *a_flow );
*/

/* plugin->u.node.free_space
   look for description of this method in plugin/node/node.h
*/
/* Audited by: green(2002.06.12) */
size_t node40_free_space ( znode *node )
{
	assert( "nikita-577", node != NULL );
	assert( "nikita-578", znode_is_loaded( node ) );
	assert( "nikita-579", zdata( node ) != NULL );
	trace_stamp( TRACE_NODES );

	return nh40_get_free_space (node40_node_header (node));
}

/* plugin->u.node.num_of_items
   look for description of this method in plugin/node/node.h
*/
/* Audited by: green(2002.06.12) */
int node40_num_of_items( const znode *node )
{
	trace_stamp( TRACE_NODES );
	return nh40_get_num_items (node40_node_header (node));
}


/* plugin->u.node.item_by_coord
   look for description of this method in plugin/node/node.h
*/
/* Audited by: green(2002.06.12) */
char * node40_item_by_coord( const coord_t *coord )
{
	item_header40 *ih;


	/* @coord is set to existing item */
	assert( "nikita-596", coord != NULL );
	assert( "vs-255", coord_is_existing_item( coord ) );

	ih = node40_ih_at_coord( coord );
	return zdata( coord->node ) + ih40_get_offset( ih );
}


/* plugin->u.node.length_by_coord
   look for description of this method in plugin/node/node.h
*/
/* Audited by: green(2002.06.12) */
int node40_length_by_coord (const coord_t * coord)
{
	item_header40 * ih;


	/* @coord is set to existing item */
	assert( "vs-256", coord != NULL );
	assert( "vs-257", coord_is_existing_item( coord ) );

	ih = node40_ih_at_coord( coord );
	if( (int) coord -> item_pos == node40_num_of_items( coord -> node ) - 1 )
		return nh40_get_free_space_start( node40_node_header (coord -> node) ) -
			ih40_get_offset( ih );
	else
		return ih40_get_offset( ih - 1 ) - ih40_get_offset( ih );
}


/* plugin->u.node.plugin_by_coord
   look for description of this method in plugin/node/node.h
*/
/* Audited by: green(2002.06.12) */
item_plugin *node40_plugin_by_coord( const coord_t *coord )
{
	item_header40 *ih;


	/* @coord is set to existing item */
	assert( "vs-258", coord != NULL );
	assert( "vs-259", coord_is_existing_item( coord ) );

	ih = node40_ih_at_coord( coord );
	/*
	 * pass NULL in stead of current tree. This is time critical call.
	 */
	return item_plugin_by_disk_id( NULL, &ih->plugin_id);
}


/* plugin->u.node.key_at
   look for description of this method in plugin/node/node.h
*/
/* Audited by: green(2002.06.12) */
reiser4_key *node40_key_at( const coord_t *coord, reiser4_key *key )
{
	item_header40 *ih;

	assert( "nikita-1765", coord_is_existing_item( coord ) );

	/* @coord is set to existing item */
	ih = node40_ih_at_coord( coord );
	xmemcpy( key, &ih -> key, sizeof (reiser4_key) );
	return key;
}


/* plugin->u.node.lookup
   look for description of this method in plugin/node/node.h
*/
/* Audited by: green(2002.06.12) */
node_search_result node40_lookup( znode *node /* node to query */, 
				  const reiser4_key *key /* key to look for */,
				  lookup_bias bias /* search bias */, 
				  coord_t *coord /* resulting coord */ )
{
	int left;
	int right;
	int found;
	item_plugin *iplug;
	item_header40 *bstop;
	cmp_t order;

	assert( "nikita-583", node != NULL );
	assert( "nikita-584", key != NULL );
	assert( "nikita-585", coord != NULL );
	trace_stamp( TRACE_NODES );

	/* binary search for item that can contain given key */
	left = 0;
	right = node40_num_of_items( node ) - 1;
	coord -> node = node;
	found = 0;

	/*
	 * It is known that for small arrays sequential search is on average
	 * more efficient than binary. This is because sequential search is
	 * coded as tight loop that can be better optimized by compilers and
	 * for small array size gain from this optimization makes sequential
	 * search the winner. Another, maybe more important, reason for this,
	 * is that sequential array is more CPU cache friendly, whereas binary
	 * search effectively destroys CPU caching.
	 *
	 * Critical here is the notion of "smallness". Reasonable value of
	 * REISER4_SEQ_SEARCH_BREAK can be found by playing with code in
	 * fs/reiser4/ulevel/ulevel.c:test_search().
	 *
	 * Don't try to further optimize sequential search by scanning from
	 * right to left in attempt to use more efficient loop termination
	 * condition (comparison with 0). This doesn't work.
	 *
	 */

	if( right < REISER4_SEQ_SEARCH_BREAK ) {
#define __get_key( pos ) ( &node40_ih_at( node, ( unsigned ) ( pos ) ) -> key )
		item_header40 *ih;
		/*
		 * sequential scan. Item headers, and, therefore, keys are
		 * stored at the rightmost part of a node from right to
		 * left. We are trying to access memory from left to right,
		 * and hence, scan in _descending_ order of item numbers.
		 */
		for( left = right, ih = node40_ih_at( node, ( unsigned ) left ) ;
		     left >= 0 ; 
		     ++ ih, prefetch( ih ), -- left ) {
			cmp_t comparison;

			comparison = keycmp( &ih -> key, key );
			if( comparison == GREATER_THAN )
				continue;
			if( comparison == EQUAL_TO ) {
				found = 1;
				if( ! REISER4_NON_UNIQUE_KEYS )
					break;
			} else {
				assert( "nikita-1256", comparison == LESS_THAN );
				if( REISER4_NON_UNIQUE_KEYS && found ) {
					assert( "nikita-1257", left < right );
					++ left;
					assert( "nikita-1258", keyeq( __get_key( left ), key ) );
				}
				break;
			}
		}
	} else do {
		int median;

		median = ( left + right ) / 2;

		assert( "nikita-1084", median >= 0 );
		assert( "nikita-1085", median < node40_num_of_items( node ) );
		switch( keycmp( key, __get_key( median ) ) ) {
		case EQUAL_TO:
			if( ! REISER4_NON_UNIQUE_KEYS ) {
				left = median;
				found = 1;
			} else
				right = median;
			break;
		case LESS_THAN:
			right = median - 1;
			break;
		default:
			wrong_return_value( "nikita-586", "keycmp" );
		case GREATER_THAN:
			left = median;
			break;
		}
		if( abs( left - right ) < 2 ) {
			if( keyle( __get_key( right ), key ) )
				left = right;
			found = keyeq( key, __get_key( left ) );
			break;
		}
#undef __get_key
	} while( ( left < right ) && !found );
	if( right < left )
		left = right;
	if( left < 0 )
		left = 0;

	coord_set_item_pos( coord, ( unsigned ) left );
	coord -> unit_pos = 0;
	coord -> between  = AT_UNIT;

	/*
	 * FIXME-VS: handling of empty node case
	 */
	if( node_is_empty( node ) )
		/* this will set coord in empty node properly */
		coord_init_first_unit( coord, node );

	if( left >= node40_num_of_items( node ) )
		return NS_NOT_FOUND;

	/* key < leftmost key in a mode or node is corrupted and keys
	   are not sorted  */
	bstop = node40_ih_at( node, ( unsigned ) left );
	order = keycmp( &bstop -> key, key );
	if( !found && ( order == GREATER_THAN ) ) {
		/* see reiser4.h for description of this */
		if( REISER4_EXACT_DELIMITING_KEY || ( left != 0 ) ) {
			/* screw up */
			warning( "nikita-587",
				 "Key less than %i key in a node", left );
			print_key( "key", key );
			print_key( "min", &bstop -> key );
			print_znode( "node", node );
			print_coord_content( "coord", coord );
			return NS_IOERROR;
		} else {
			coord -> between = BEFORE_UNIT;
			return NS_NOT_FOUND;
		}
	}
	/* left <= key, ok */
	iplug = item_plugin_by_disk_id( current_tree, &bstop -> plugin_id );

	if( iplug == NULL ) {
		warning( "nikita-588", "Unknown plugin %i",
			 d16tocpu( &bstop -> plugin_id ) );
		print_key( "key", key );
		print_znode( "node", node );
		print_coord_content( "coord", coord );
		return NS_IOERROR;
	}

	coord -> iplug = iplug;

	/*
	 * if exact key from item header was found by binary search, no
	 * further checks are necessary.
	 */
	if( found ) {
		assert( "nikita-1259", order == EQUAL_TO );
		return NS_FOUND;
	}
	if( iplug -> b.max_key_inside != NULL ) {
		reiser4_key max_item_key;

		/* key > max_item_key --- outside of an item */
		if( keygt( key, iplug -> b.max_key_inside( coord, 
							   &max_item_key ) ) ) {
			coord -> unit_pos = 0;
			coord -> between  = AFTER_ITEM;
			/*
			 * FIXME-VS: key we are looking for does not fit into
			 * found item. Return NS_NOT_FOUND then. Without that
			 * the following case does not work: there is extent of
			 * file 10000, 10001. File 10000, 10002 has been just
			 * created. When writing to position 0 in that file -
			 * traverse_tree will stop here on twig level. When we
			 * want it to go down to leaf level
			 */
			return NS_NOT_FOUND;
			return ( bias == FIND_EXACT ) ? NS_NOT_FOUND : NS_FOUND;
		}
	}

	if( iplug -> b.lookup != NULL ) {
		return iplug -> b.lookup( key, bias, coord );
	} else {
		assert( "nikita-1260", order == LESS_THAN );
		coord -> between = AFTER_UNIT;
		return ( bias == FIND_EXACT ) ? NS_NOT_FOUND : NS_FOUND;
	}
}

/* plugin->u.node.estimate
   look for description of this method in plugin/node/node.h
*/
/* Audited by: green(2002.06.12) */
size_t node40_estimate( znode *node )
{
	size_t result;

	assert( "nikita-597", node != NULL );

	result = node40_free_space( node ) - sizeof( item_header40 );

	return ( result > 0 ) ? result : 0;
}


/* plugin->u.node.check
   look for description of this method in plugin/node/node.h
*/
/* Audited by: green(2002.06.12) */
int node40_check( const znode *node /* node to check */, 
		  __u32 flags /* check flags */, 
		  const char **error /* where to store error message */ )
{
	int nr_items;
	int i;
	reiser4_key prev;
	unsigned old_offset;
	coord_t coord;

	assert( "nikita-580", node != NULL );
	assert( "nikita-581", error != NULL );
	trace_stamp( TRACE_NODES );
	
	if( !znode_is_loaded( node ) )
		return 0;

	if( ZF_ISSET( node, JNODE_HEARD_BANSHEE ) )
		return 0;

	assert( "nikita-582", zdata( node ) != NULL );
	
	nr_items = node40_num_of_items( node );
	if( nr_items < 0 ) {
		*error = "Negative number of items";
		return -1;
	}
	    
	if( flags & REISER4_NODE_DKEYS )
		prev = node -> ld_key;
	else
		prev = *min_key();

	if( zload( ( znode *) node ) != 0 )
		return 0;

	old_offset = 0;
	coord_init_zero( &coord );
	coord.node = ( znode * ) node;
	coord.unit_pos = 0;
	coord.between = AT_UNIT;
	for ( i = 0; i < nr_items ; i ++ ) {
		item_header40 *ih;
		reiser4_key unit_key;
		unsigned j;

		ih = node40_ih_at( node, (unsigned)i );
		coord_set_item_pos (&coord, ( unsigned ) i);
		if ( ( ih40_get_offset( ih ) >= 
		       znode_size( node ) - nr_items * sizeof( item_header40 ) ) ||
		     ( ih40_get_offset( ih ) < sizeof( node40_header ) ) ) {
			*error = "Offset is out of bounds";
			return -1;
		}
		if ( ih40_get_offset( ih ) <= old_offset ) {
			*error = "Offsets are in wrong order";
			return -1;
		}
		if ( ( i == 0 ) && 
		     ( ih40_get_offset( ih ) != sizeof( node40_header ) ) ) {
			*error = "Wrong offset of first item";
			return -1;
		}
		old_offset = ih40_get_offset( ih );

		if ( keygt( &prev, &ih -> key ) ) {
			*error = "Keys are in wrong order";
			return -1;
		}
		if( !keyeq( &ih -> key, 
			    unit_key_by_coord( &coord, &unit_key ) ) ) {
			*error = "Wrong key of first unit";
			return -1;
		}
		prev = ih -> key;
		for( j = 0 ; j < coord_num_units( &coord ) ; ++ j ) {
			coord.unit_pos = j;
			unit_key_by_coord( &coord, &unit_key );
			if( keygt( &prev, &unit_key ) ) {
				*error = "Unit keys are in wrong order";
				return -1;
			}
			prev = unit_key;
		}
		coord.unit_pos =  0;
		if( item_plugin_by_coord( &coord ) -> b.check &&
		    item_plugin_by_coord( &coord ) -> b.check( &coord, error ) )
			return -1;
		if( i ) {
			coord_t prev_coord;
			/*
			 * two neighboring items can not be mergeable
			 */
			coord_dup( &prev_coord, &coord );
			coord_prev_item( &prev_coord );
			if( are_items_mergeable( &prev_coord, &coord ) ) {
				*error = "mergeable items in one node";
				return -1;		
			}

		}
	}

	spin_lock_dk( current_tree );
	if( ( flags & REISER4_NODE_DKEYS ) && !node_is_empty( node ) ) {
		coord_t coord;
		item_plugin *iplug;
				
		coord_init_last_unit( &coord, node );
		iplug = item_plugin_by_coord( &coord );
		if( iplug -> b.real_max_key_inside != NULL ) {
			reiser4_key mkey;
			
			if( keygt( iplug -> b.real_max_key_inside( &coord, 
								   &mkey ), 
				   znode_get_rd_key( ( znode * ) node ) ) ) {
				*error = "key of rightmost item is too large";
				return -1;
			}
		}
	}
	if( flags & REISER4_NODE_DKEYS ) {
		spin_lock_tree( current_tree );

		if ( keygt( &prev, &node -> rd_key ) ) {
			reiser4_stat_tree_add( rd_key_skew );
			if( flags & REISER4_NODE_TREE_STABLE ) {
				*error = "Last key is greater than rdkey"; 
				return -1;
			}
		} 
		if( keygt( &node -> ld_key, &node -> rd_key ) ) {
			*error = "ldkey is greater than rdkey"; 
			return -1;
		}
		if( ZF_ISSET( node, JNODE_LEFT_CONNECTED ) && 
		    ( node -> left != NULL ) && 
		    ! ZF_ISSET( node -> left, JNODE_HEARD_BANSHEE )  &&
		    ergo( flags & REISER4_NODE_TREE_STABLE,
			  !keyeq( &node -> left -> rd_key, &node -> ld_key ) ) &&
		    ergo( ! ( flags & REISER4_NODE_TREE_STABLE ),
			  keygt( &node -> left -> rd_key, &node -> ld_key ) ) )
		{
			*error = "left rdkey or ldkey is wrong"; 
			return -1;
		}
		if( ZF_ISSET( node, JNODE_RIGHT_CONNECTED ) && 
		    ( node -> right != NULL ) && 
		    ! ZF_ISSET( node -> right, JNODE_HEARD_BANSHEE )  &&
		    ergo( flags & REISER4_NODE_TREE_STABLE,
			  !keyeq( &node -> rd_key, &node -> right -> ld_key ) ) &&
		    ergo( ! ( flags & REISER4_NODE_TREE_STABLE ),
			  keygt( &node -> rd_key, &node -> right -> ld_key ) ) )
		{
			*error = "rdkey or right ldkey is wrong"; 
			return -1;
		}

		spin_unlock_tree( current_tree );
	}
	spin_unlock_dk( current_tree );

	zrelse( ( znode *) node );
	return 0;
}


/* plugin->u.node.parse
   look for description of this method in plugin/node/node.h
*/
/* Audited by: green(2002.06.12) */
int node40_parse( znode *node /* node to parse */)
{
	node40_header   *header;
	int               result;

	header = node40_node_header( ( znode * ) node );
	result = -EIO;
	if( ( ( __u8 ) znode_get_level( node ) ) != nh40_get_level( header ) )
		warning( "nikita-494", "Wrong level found in node: %i != %i",
			 znode_get_level( node ), nh40_get_level( header ) );
	else if( nh40_get_magic( header ) != REISER4_NODE_MAGIC )
		warning( "nikita-495", 
			 "Wrong magic in tree node: want %x, got %x",
			 REISER4_NODE_MAGIC, nh40_get_magic( header ) );
	else {
		node -> nr_items = node40_num_of_items( node );
		result = 0;
	}
	if( result != 0 )
		print_znode( "node", node );
	return result;
}


/* plugin->u.node.init
   look for description of this method in plugin/node/node.h
*/
/* Audited by: green(2002.06.12) */
int node40_init( znode *node /* node to initialise */)
{
	node40_header *header;

	assert( "nikita-570", node != NULL );
	assert( "nikita-571", znode_is_loaded( node ) );
	assert( "nikita-572", zdata( node ) != NULL );
	assert( "nikita-573", node_plugin_by_node( node ) != NULL );

	header = node40_node_header( node );
	if( REISER4_ZERO_NEW_NODE )
		xmemset( zdata( node ), 0, ( unsigned int ) znode_size( node ) );
	else
		xmemset( header, 0, sizeof (node40_header) );
	nh40_set_free_space (header, znode_size( node ) - sizeof (node40_header));
	nh40_set_free_space_start (header, sizeof (node40_header));
	/* sane hypothesis: 0 in CPU format is 0 in disk format */
	/* items: 0 */
	save_plugin_id (node_plugin_to_plugin (node -> nplug), &header -> common_header.plugin_id);
	nh40_set_level (header, znode_get_level( node ));
	nh40_set_magic (header, REISER4_NODE_MAGIC);
	node -> nr_items = 0;

	/* flags: 0 */
	return 0;
}


/* Audited by: green(2002.06.12) */
int node40_guess( const znode *node /* node to guess plugin of */)
{
	node40_header *nethack;

	assert( "nikita-1058", node != NULL );
	nethack = node40_node_header( node );
	return
		( nh40_get_magic( nethack ) == REISER4_NODE_MAGIC ) &&
		( plugin_by_disk_id( current_tree,
				     REISER4_NODE_PLUGIN_TYPE,
				     &nethack -> common_header.plugin_id ) -> h.id ==
		  NODE40_ID );
}


#if REISER4_DEBUG_OUTPUT
/* Audited by: green(2002.06.12) */
void node40_print( const char *prefix,
		   const znode *node /* node to print */, 
		   __u32 flags UNUSED_ARG /* print flags */ )
{
	node40_header   *header;
	
	
	header = node40_node_header( node );
	info( "%s: BLOCKNR %Lu FREE_SPACE %u, LEVEL %u, ITEM_NUMBER %u\n",
	      prefix,
	      *znode_get_block (node),
	      nh40_get_free_space( header ),
	      nh40_get_level( header ),
	      nh40_get_num_items( header ) );
}
#endif

/* plugin->u.node.chage_item_size
   look for description of this method in plugin/node/node.h
*/
/* Audited by: green(2002.06.13) */
void node40_change_item_size (coord_t * coord, int by)
{
	node40_header * nh;
	item_header40 * ih;
	char * item_data;
	int item_length;
	unsigned i;
	
	node_check (coord->node, REISER4_NODE_PANIC);

	/* make sure that @item is coord of existing item */
	assert ("vs-210", coord_is_existing_item (coord));

	nh = node40_node_header (coord->node);

	item_data = node40_item_by_coord (coord);
	item_length = node40_length_by_coord (coord);


	/* move item bodies */
	ih = node40_ih_at_coord (coord);
	xmemmove (item_data + item_length + by, item_data + item_length,
		 nh40_get_free_space_start (node40_node_header (coord->node)) -
		 (ih40_get_offset (ih) + item_length));

	/* update offsets of moved items */
	for (i = coord->item_pos + 1; i < nh40_get_num_items (nh); i ++) {
		ih = node40_ih_at (coord->node, i);
		ih40_set_offset (ih, ih40_get_offset (ih) + by);
	}

	/* update node header */
	nh40_set_free_space (nh, nh40_get_free_space (nh) - by);
	nh40_set_free_space_start (nh, nh40_get_free_space_start (nh) + by);
}


/* Audited by: green(2002.06.13) */
static int should_notify_parent (const znode *node)
{
	/* FIXME_JMACD This looks equivalent to znode_is_root(), right? -josh */
	return !disk_addr_eq (znode_get_block (node), &current_tree->root_block);
}

/* plugin->u.node.create_item
   look for description of this method in plugin/node/node.h
*/
/* Audited by: green(2002.06.13) */
/* Auditor comments: This function cannot be called with any spinlocks held in
   case passed data is in userspace, because userspace access might schedule */
int node40_create_item (coord_t * target, const reiser4_key * key,
			reiser4_item_data * data, carry_plugin_info *info )
{
	node40_header * nh;
	item_header40 * ih;
	unsigned offset;
	unsigned i;

	node_check (target->node, REISER4_NODE_PANIC);

	nh = node40_node_header (target->node);

	assert ("vs-212", coord_is_between_items (target));
	/* node must have enough free space */
	assert ("vs-254", node40_free_space (target->node) >=
		data->length + sizeof (item_header40));
	
	if (coord_set_to_right (target))
		/* there are not items to the right of @target, so, new item
		   will be inserted after last one */
		coord_set_item_pos (target, nh40_get_num_items (nh));

	if (target->item_pos < nh40_get_num_items (nh)) {
		/* there are items to be moved to prepare space for new
		   item */
		ih = node40_ih_at_coord (target);
		/* new item will start at this offset */
		offset = ih40_get_offset (ih);

		xmemmove (zdata (target->node) + offset + data->length,
			 zdata (target->node) + offset,
			 nh40_get_free_space_start (nh) - offset);
		/* update headers of moved items */
		for (i = target->item_pos; i < nh40_get_num_items (nh); i ++) {
			ih = node40_ih_at (target->node, i);
			ih40_set_offset (ih, ih40_get_offset (ih) + data->length);
		}

		/* @ih is set to item header of the last item, move item headers */
		xmemmove (ih - 1, ih,
			 sizeof (item_header40) * (nh40_get_num_items (nh) - target->item_pos));
	} else {
		/* new item will start at this offset */
		offset = nh40_get_free_space_start (nh);
	}

	/* make item header for the new item */
	ih = node40_ih_at_coord (target);
	xmemcpy (&ih->key, key, sizeof (reiser4_key));
	ih40_set_offset (ih, offset);
	save_plugin_id (item_plugin_to_plugin (data->iplug), &ih->plugin_id);

	/* update node header */
	nh40_set_free_space (nh, nh40_get_free_space (nh) -
			      data->length - sizeof (item_header40));
	nh40_set_free_space_start (nh, nh40_get_free_space_start (nh) + data->length);
	nh40_set_num_items (nh, nh40_get_num_items (nh) + 1);
	target->node->nr_items ++;

	/* FIXME: check how does create_item work when between is set to BEFORE_UNIT */
	target->unit_pos = 0;
	target->between  = AT_UNIT;
	target->iplug    = NULL;

	/* initialise item */
	if (data->iplug->b.init != NULL) {
		data->iplug->b.init (target, data);
	}
	/* copy item body */
	if (data->iplug->b.paste != NULL) {
		data->iplug->b.paste (target, data, info);
	}
	else if (data->data != NULL) {
		if (data->user) {
			/* AUDIT: Are we really should not check that pointer
			 * from userspace was valid and data bytes were
			 * available? How will we return -EFAULT of some kind
			 * without this check? */
			ON_DEBUG_CONTEXT( assert( "green-2", 
						  lock_counters() -> spin_locked == 0 ) );
			/* copy data from user space */
			__copy_from_user (zdata (target->node) + offset,
					  data->data, 
					  (unsigned)data->length);
		} else
			/* copy from kernel space */
			xmemcpy (zdata (target->node) + offset, data->data,
				 (unsigned)data->length);
	}

	if (target->item_pos == 0) {
		/* left delimiting key has to be updated */
		prepare_for_update (NULL, target->node, info);
	}

	if (item_plugin_by_coord (target) -> b.create_hook != NULL) {
		item_plugin_by_coord (target) -> b.create_hook (target,
								data->arg);
	}

	node_check (target->node, REISER4_NODE_PANIC);

	/*
	 * FIXME-VS: remove after debugging
	 */
	if (0) {
		reiser4_key key_1;
		reiser4_key key;
		coord_t crd;

		for_all_items (&crd, target->node) {
			item_key_by_coord (&crd, &key);
			if (crd.item_pos != 0)
				assert ("vs-970", keylt (&key_1, &key));
			key_1 = key;
		}
	}
	/*
	 * FIXME-VS: remove after debugging
	 */
	

	return 0;
}


/* plugin->u.node.update_item_key
   look for description of this method in plugin/node/node.h
*/
/* Audited by: green(2002.06.13) */
void node40_update_item_key (coord_t * target, const reiser4_key * key,
			     carry_plugin_info *info)
{
	item_header40 * ih;

	ih = node40_ih_at_coord (target);
	xmemcpy (&ih->key, key, sizeof (reiser4_key));

	if (target->item_pos == 0) {
		prepare_for_update (NULL, target->node, info);
	}
}


/* Audited by: green(2002.06.13) */
static unsigned cut_units (coord_t * coord, unsigned *from, unsigned *to,
			   int cut,
			   const reiser4_key * from_key,
			   const reiser4_key * to_key,
			   reiser4_key * smallest_removed, void *kill_params)
{
	unsigned cut_size;
	item_plugin * iplug;
	int (*cut_f) (coord_t *, unsigned *, unsigned *,
		       const reiser4_key *, const reiser4_key *,
		       reiser4_key *);


	iplug = item_plugin_by_coord (coord);
	if (cut) {
		cut_f = iplug->b.cut_units;
	} else {
		cut_f = iplug->b.kill_units;
	}

	if (cut_f) {
		/* FIXME-VS:
		 * kill_item_hook for units being cut will be called by
		 * kill_units method, because it is not clear here which units
		 * will be actually cut. To have kill_item_hook here we would
		 * have to pass keys to kill_item_hook */
		cut_size = cut_f (coord, from, to,
				  from_key, to_key, smallest_removed);
	} else {
		/*
		 * cut method is not defined, so there should be request to cut
		 * the single unit of item
		 */
		assert ("vs-302", *from == 0 && *to == 0 &&
			coord->unit_pos == 0 && coord_num_units (coord) == 1);
		/* that item will be removed entirely */
		cut_size = item_length_by_coord (coord);
		if (smallest_removed)
			item_key_by_coord (coord, smallest_removed);
		if (!cut && iplug->b.kill_hook)
			iplug->b.kill_hook (coord, 0, 1, kill_params);
	}

	return cut_size;
}


/* this is auxiliary function used by both cutting methods - cut and
   cut_and_kill. If it is called by cut_and_kill (@cut == 0) special action
   (kill_hook) will be performed on every unit being removed from tree. When
   @info != 0 - it is called not by node40_shift who cares about delimiting
   keys itself - update znode's delimiting keys */
/* Audited by: green(2002.06.13) */
static int cut_or_kill (coord_t * from, coord_t * to,
			const reiser4_key * from_key,
			const reiser4_key * to_key,
			reiser4_key * smallest_removed, carry_plugin_info *info, 
			int cut, void *cut_params, __u32 flags)
{
	znode * node;
	node40_header * nh;
	item_header40 * ih;
	unsigned freed_space_start;/*new_from_end;*/
	unsigned freed_space_end; /*new_to_start;*/
	unsigned first_removed; /* position of first item removed entirely */
	int rightmost_not_moved; /* position of item which is the rightmost of
				  * items which do not change their offset */
	unsigned removed_entirely; /* number of items removed entirely */
	unsigned i;
	unsigned cut_size;
	reiser4_key old_first_key;
	unsigned wrong_item; /* position of item for which may get mismatching
				item key and key of first unit in it */
	unsigned from_unit, to_unit;


	assert ("vs-184", from->node == to->node);
	assert ("vs-297", ergo (from->item_pos == to->item_pos,
				from->unit_pos <= to->unit_pos));
	assert ("vs-312", !node_is_empty (from->node));
	assert ("nikita-1912", ergo (cut, cut_params == NULL));

	node = from->node;
	nh = node40_node_header (node);
	old_first_key = node40_ih_at (node, 0)->key;

	wrong_item = ~0u;
	if (from->item_pos == to->item_pos) {
		/*
		 * cut one item (partially or as whole)
		 */
		first_removed = from->item_pos;
		removed_entirely = 0;
		from_unit = from->unit_pos;
		to_unit = to->unit_pos;
		cut_size = cut_units (from, &from_unit, &to_unit,
				      cut, from_key, to_key, smallest_removed,
				      cut_params);
		if (cut_size == (unsigned)item_length_by_coord (from))
			/*
			 * item will be removed entirely
			 */
			removed_entirely = 1;
		else
			/* this item may have wrong key after cut_units */
			wrong_item = from->item_pos;

		ih = node40_ih_at (node, (unsigned) from->item_pos);
		/*
		 * there are 4 possible cases: cut from the beginning, cut from
		 * the end, cut from the middle and cut whole item
		 */
		if (removed_entirely) {
			/*
			 * whole item is cut
			 */
			freed_space_start = ih40_get_offset (ih);
			freed_space_end = freed_space_start + cut_size;
			rightmost_not_moved = from->item_pos - 1;
		} else if (from_unit == 0) {
			/*
			 * head is cut, freed space is in the beginning
			 */
			freed_space_start = ih40_get_offset (ih);
			freed_space_end = freed_space_start + cut_size;
			rightmost_not_moved = from->item_pos - 1;
			/* item now starts at different place */
			ih40_set_offset (ih, freed_space_end);
			
		} else if (to_unit == coord_last_unit_pos (to)) {
			/*
			 * tail is cut, freed space is in the end
			 */
			freed_space_start = ih40_get_offset (ih) + item_length_by_coord (from) - cut_size;
			freed_space_end = freed_space_start + cut_size;
			rightmost_not_moved = from->item_pos;
		} else {
			/*
			 * cut from the middle
			 * NOTE: cut method of item must leave freed space at
			 * the end of item
			 */
			freed_space_start = ih40_get_offset (ih) + item_length_by_coord (from) - cut_size;
			freed_space_end = freed_space_start + cut_size;
			rightmost_not_moved = from->item_pos;
		}
	} else {
		/*
		 * @from and @to are different items
		 */
		first_removed = from->item_pos + 1;
		removed_entirely = to->item_pos - from->item_pos - 1;
		rightmost_not_moved = from->item_pos;

		if (!cut) {
			/*
			 * for every item being removed entirely between @from
			 * and @to call special kill method
			 */
			coord_t tmp;
			item_plugin * iplug;
			
			/*
			 * FIXME-VS: this iterates items starting not from
			 * 0-th, so it does not use new coord interface
			 */
			tmp.node = node;
			tmp.unit_pos = 0;
			tmp.between = AT_UNIT;
			for (i = 0; i < removed_entirely; i ++) {
				coord_set_item_pos (&tmp, first_removed + i);
				tmp.unit_pos = 0;
				tmp.between = AT_UNIT;
				iplug = item_plugin_by_coord (&tmp);
				if (iplug->b.kill_hook) {
					iplug->b.kill_hook (&tmp, 0, coord_num_units (&tmp), cut_params);
				}
			}
		}

		/*
		 * cut @from item first
		 */
		from_unit = from->unit_pos;
		to_unit = coord_last_unit_pos (from);
		cut_size = cut_units (from, &from_unit, &to_unit,
				      cut, from_key, to_key, smallest_removed,
				      cut_params);
		if (cut_size == (unsigned)item_length_by_coord (from)) {
			/*
			 * whole @from is cut
			 */
			first_removed --;
			removed_entirely ++;
			rightmost_not_moved --;
		}
		ih = node40_ih_at (node, (unsigned) from->item_pos);
		freed_space_start = ih40_get_offset (ih) +
			node40_length_by_coord (from) - cut_size;

		/*
		 * cut @to item
		 */
		from_unit = 0;
		to_unit = to->unit_pos;
		cut_size = cut_units (to, &from_unit, &to_unit,
				      cut, from_key, to_key, 0, cut_params);
		if (cut_size == (unsigned)item_length_by_coord (to))
			/*
			 * whole @to is cut
			 */
			removed_entirely ++;
		else
			/* this item may have wrong key after cut_units */
			wrong_item = to->item_pos;

		ih = node40_ih_at (node, (unsigned) to->item_pos);
		freed_space_end = ih40_get_offset (ih) + cut_size;

		/* item now starts at different place */
		ih40_set_offset (ih, freed_space_end);
	}


	/* move remaining data to left */
	xmemmove (zdata (node) + freed_space_start, zdata (node) + freed_space_end,
		 nh40_get_free_space_start (nh) - freed_space_end);

	/* update item headers of moved items */
	for (i = rightmost_not_moved + 1 + removed_entirely; 
	     (int)i < node40_num_of_items (node); i ++) {
		ih = node40_ih_at (node, i);
		ih40_set_offset (ih, (ih40_get_offset (ih) -
				       (freed_space_end - freed_space_start)));
	}

	/* cut item headers of removed items */
	ih = node40_ih_at (node, (unsigned)node40_num_of_items (node) - 1);
	xmemmove (ih + removed_entirely, ih,
		 sizeof (item_header40) * (node40_num_of_items (node) -
					    removed_entirely - first_removed));

	/* update node header */
	nh40_set_num_items (nh, node40_num_of_items (node) - removed_entirely);
	node->nr_items -= removed_entirely;
	nh40_set_free_space_start (nh, nh40_get_free_space_start (nh) -
				    (freed_space_end - freed_space_start));
	nh40_set_free_space (nh, nh40_get_free_space (nh) +
			      ((freed_space_end - freed_space_start) +
			       sizeof (item_header40) * removed_entirely));

	if (wrong_item != ~0u) {
		coord_t coord;
		reiser4_key unit_key;

		assert ("vs-313", wrong_item >= removed_entirely);
		wrong_item -= removed_entirely;
		assert ("vs-314", (int)wrong_item < node40_num_of_items (node));
		coord.node = node;
		coord_set_item_pos (&coord, wrong_item);
		coord.unit_pos = 0;
		coord.between = AT_UNIT;
		unit_key_by_coord (&coord, &unit_key);
		node40_update_item_key (&coord, &unit_key, 0);
	}

	if (info) {
		/* it is not called by node40_shift, so we have to take care
		   of changes on upper levels */
		if (node_is_empty (node) && !(flags & DELETE_RETAIN_EMPTY))
			/* all contents of node is deleted */
			node40_prepare_for_removal (node, info);
		else if (!keyeq (&node40_ih_at (node, 0)->key,
				 &old_first_key)) {
			/* first key changed */
			prepare_for_update (NULL, node, info);
		}
	}

	/*
	 * FIXME-NIKITA overkill
	 */
	from->iplug = to->iplug = NULL;

	/*print_znode_content (node, ~0u);*/
	return removed_entirely;
}


/* plugin->u.node.cut_and_kill
 */
/* Audited by: green(2002.06.13) */
int node40_cut_and_kill (coord_t * from, coord_t * to,
			 const reiser4_key * from_key,
			 const reiser4_key * to_key,
			 reiser4_key * smallest_removed,
			 carry_plugin_info *info, void *kill_params, __u32 flags)
{
	return cut_or_kill (from, to, from_key, to_key, smallest_removed,
			    info, 0 /* not cut */, kill_params, flags);
}


/* plugin->u.node.cut
 */
/* Audited by: green(2002.06.13) */
int node40_cut (coord_t * from, coord_t * to,
		const reiser4_key * from_key,
		const reiser4_key * to_key,
		reiser4_key * smallest_removed,
		carry_plugin_info *info, __u32 flags)
{
	return cut_or_kill (from, to, from_key, to_key, smallest_removed,
			    info, 1 /* cut */, NULL, flags);
}


/* this structure is used by shift method of node40 plugin */
struct shift_params {
	shift_direction pend; /* when @pend == append - we are shifting to
				 left, when @pend == prepend - to right */
	coord_t wish_stop; /* when shifting to left this is last unit we
				  want shifted, when shifting to right - this
				  is set to unit we want to start shifting
				  from */
	znode * target;
	int everything; /* it is set to 1 if everything we have to shift is
			   shifted, 0 - otherwise */

	/*
	 * FIXME-VS: get rid of read_stop
	 */

	/* these are set by estimate_shift */
	coord_t real_stop; /* this will be set to last unit which will be
			      really shifted */

	/* coordinate in source node before operation of unit which becomes
	 * first after shift to left of last after shift to right */
	union {
		coord_t future_first;
		coord_t future_last;
	} u;

	unsigned merging_units; /* number of units of first item which have to
				   be merged with last item of target node */
	unsigned merging_bytes; /* number of bytes in those units */

	unsigned entire; /* items shifted in their entirety */
	unsigned entire_bytes; /* number of bytes in those items */

	unsigned part_units; /* number of units of partially copied item */
	unsigned part_bytes; /* number of bytes in those units */

	unsigned shift_bytes; /* total number of bytes in items shifted (item
				 headers not included) */

};


/* Audited by: green(2002.06.13) */
static int item_creation_overhead (coord_t * item)
{
	return node_plugin_by_coord (item) -> item_overhead (item->node, 0);
}


/* how many units are there in @source starting from source->unit_pos
   but not further than @stop_coord */
/* Audited by: green(2002.06.13) */
static int wanted_units (coord_t * source, coord_t * stop_coord,
			 shift_direction pend)
{
	if (pend == SHIFT_LEFT) {
		assert ("vs-181", source->unit_pos == 0);
	} else {
		assert ("vs-182", source->unit_pos == coord_last_unit_pos (source));
	}

	if (source->item_pos != stop_coord->item_pos) {
		/* @source and @stop_coord are different items */
		return coord_last_unit_pos (source) + 1;
	}

	if (pend == SHIFT_LEFT) {
		return stop_coord->unit_pos + 1;
	} else {
		return source->unit_pos - stop_coord->unit_pos + 1;
	}
}



/* this calculates what can be copied from @shift->wish_stop.node to
   @shift->target */
/* Audited by: green(2002.06.13) */
static void node40_estimate_shift (struct shift_params * shift)
{
	unsigned target_free_space, size;
	unsigned stop_item; /* item which estimating should not consider */
	unsigned want; /* number of units of item we want shifted */
	coord_t source; /* item being estimated */
	item_plugin * iplug;


	/* shifting to left/right starts from first/last units of
	   @shift->wish_stop.node */
	if (shift->pend == SHIFT_LEFT) {
		coord_init_first_unit (&source, shift->wish_stop.node);
	} else {
		coord_init_last_unit (&source, shift->wish_stop.node);
	}
	shift->real_stop = source;

	/* free space in target node and number of items in source */
	target_free_space = znode_free_space (shift->target);


	shift->everything = 0;
	if (!node_is_empty (shift->target)) {
		/* target node is not empty, check for boundary items
		   mergeability */
		coord_t to;

		/* item we try to merge @source with */
		if (shift->pend == SHIFT_LEFT) {
			coord_init_last_unit (&to, shift->target);
		} else {
			coord_init_first_unit (&to, shift->target);
		}

		if ((shift->pend == SHIFT_LEFT) ?
		    are_items_mergeable (&to, &source) :
		    are_items_mergeable (&source, &to)) {
			/* how many units of @source do we want to merge to
			   item @to */
			want = wanted_units (&source, &shift->wish_stop, shift->pend);

			/* how many units of @source we can merge to item
			   @to */
			iplug = item_plugin_by_coord (&source);
			if (iplug->b.can_shift != NULL)
				shift->merging_units = iplug->b.can_shift (target_free_space, &source,
										shift->target, shift->pend, &size,
										want);
			else {
				shift->merging_units = 0;
				size = 0;
			}
			shift->merging_bytes = size;
			shift->shift_bytes += size;
			/* update stop coord to be set to last unit of @source
			   we can merge to @target */
			if (shift->merging_units)
				/* at least one unit can be shifted */
				shift->real_stop.unit_pos = (shift->merging_units - source.unit_pos - 1) * shift->pend;
			else {
				/* nothing can be shifted */
				if (shift->pend == SHIFT_LEFT)
					coord_init_before_first_item (&shift->real_stop, source.node);
				else
					coord_init_after_last_item (&shift->real_stop, source.node);
			}
			assert ("nikita-2081", shift->real_stop.unit_pos + 1);

			if (shift->merging_units != want) {
				/* we could not copy as many as we want, so,
				   there is no reason for estimating any
				   longer */
				return;
			}

			target_free_space -= size;
			coord_add_item_pos (&source, shift->pend);
		}
	}


	/* number of item nothing of which we want to shift */
	stop_item = shift->wish_stop.item_pos + shift->pend;

	/* calculate how many items can be copied into given free
	   space as whole */
	for (; source.item_pos != stop_item; 
	     coord_add_item_pos (&source, shift->pend)) {
		if (shift->pend == SHIFT_RIGHT)
			source.unit_pos = coord_last_unit_pos (&source);

		/* how many units of @source do we want to copy */
		want = wanted_units (&source, &shift->wish_stop, shift->pend);

		if (want == coord_last_unit_pos (&source) + 1) {
			/* we want this item to be copied entirely */
			size = item_length_by_coord (&source) + item_creation_overhead (&source);
			if (size <= target_free_space) {
				/* item fits into target node as whole */
				target_free_space -= size;
				shift->shift_bytes += size - item_creation_overhead (&source);
				shift->entire_bytes += size - item_creation_overhead (&source);
				shift->entire ++;

				/* update shift->real_stop coord to be set to
				   last unit of @source we can merge to
				   @target */
				shift->real_stop = source;
				if (shift->pend == SHIFT_LEFT)
					shift->real_stop.unit_pos = 
						coord_last_unit_pos (&shift->real_stop);
				else
					shift->real_stop.unit_pos = 0;
				continue;
			}
		}

		/* we reach here only for an item which does not fit into
		   target node in its entirety. This item may be either
		   partially shifted, or not shifted at all. We will have to
		   create new item in target node, so decrease amout of free
		   space by an item creation overhead. We can reach here also
		   if stop coord is in this item */
		if (target_free_space >= (unsigned) item_creation_overhead (&source)) {
			target_free_space -= item_creation_overhead (&source);
			iplug = item_plugin_by_coord (&source);
			if (iplug->b.can_shift) {
				shift->part_units = iplug->b.can_shift (target_free_space, &source,
									     0/*target*/, shift->pend, &size,
									     want);
			} else {
				target_free_space = 0;
				shift->part_units = 0;
				size = 0;
			}
		} else {
			target_free_space = 0;
			shift->part_units = 0;
			size = 0;
		}
		shift->part_bytes = size;
		shift->shift_bytes += size;

		/* set @shift->real_stop to last unit of @source we can merge
		   to @shift->target */
		if (shift->part_units) {
			shift->real_stop = source;
			shift->real_stop.unit_pos = (shift->part_units - source.unit_pos - 1) * shift->pend;
			assert ("nikita-2082", shift->real_stop.unit_pos + 1);
		}

		if (want != shift->part_units)
			/* not everything wanted were shifted */
			return;
		break;
	}

	shift->everything = 1;
}

/* Audited by: green(2002.06.13) */
static void copy_units (coord_t * target, coord_t * source,
			unsigned from, unsigned count, 
			shift_direction dir, unsigned free_space)
{
	item_plugin *iplug;

	assert ("nikita-1463", target != NULL);
	assert ("nikita-1464", source != NULL);
	assert ("nikita-1465", from + count <= coord_num_units (source));

	trace_if (TRACE_COORDS, print_coord ("copy_units source:", source, 0));
	
	iplug = item_plugin_by_coord (source);
	assert ("nikita-1468", iplug == item_plugin_by_coord (target));
	iplug -> b.copy_units (target, source, from, count, dir, free_space);

	if (dir == SHIFT_RIGHT) {
		/*
		 * FIXME-VS: this looks not necessary. update_item_key was
		 * called already by copy_units method
		 */
		reiser4_key split_key;

		assert ("nikita-1469", target->unit_pos == 0);

		unit_key_by_coord (target, &split_key);
		node_plugin_by_coord (target)->update_item_key (target, 
								&split_key, 0);
	}
}

/* copy part of @shift->real_stop.node starting either from its beginning or
   from its end and ending at @shift->real_stop to either the end or the
   beginning of @shift->target */
/* Audited by: green(2002.06.13) */
void node40_copy (struct shift_params * shift)
{
	node40_header * nh;
	coord_t from;
	coord_t to;
	item_header40 * from_ih, * to_ih;
	int free_space_start;
	int new_items;
	unsigned old_items;
	int old_offset;
	unsigned i;

	nh = node40_node_header (shift->target);
	free_space_start = nh40_get_free_space_start (nh);
	old_items = nh40_get_num_items (nh);
	new_items = shift->entire + (shift->part_units ? 1 : 0);
	assert ("vs-185",
		shift->shift_bytes ==
		shift->merging_bytes + shift->entire_bytes + shift->part_bytes);

	from = shift->wish_stop;

	trace_if (TRACE_COORDS, print_coord ("node40_copy from:", & from, 0));

	coord_init_first_unit (& to, shift->target);

	/*
	 * FIXME:NIKITA->VS not sure what I am doing: shift->target is empty,
	 * hence to.between is set to EMPTY_NODE above. Looks like we want it
	 * to be AT_UNIT.
	 *
	 * Oh, wonders of ->betweeness...
	 *
	 */
	to.between = AT_UNIT;

	if (shift->pend == SHIFT_LEFT) {
		/* copying to left */

		coord_set_item_pos (&from, 0);
		from_ih = node40_ih_at (from.node, 0);

		coord_set_item_pos (&to, 
				    (unsigned)node40_num_of_items (to.node) - 1);
		if (shift->merging_units) {
			/* expand last item, so that plugin methods will see
			 * correct data */
			free_space_start += shift->merging_bytes;
			nh40_set_free_space_start (nh, (unsigned) free_space_start);
			nh40_set_free_space (nh, nh40_get_free_space (nh) -
					      shift->merging_bytes);

			trace_if (TRACE_COORDS, print_coord ("before copy_units from:", & from, 0));
			trace_if (TRACE_COORDS, print_coord ("before copy_units to:", & to, 0));

			/* appending last item of @target */
			copy_units (&to, &from, 0, /* starting from 0-th unit */
				    shift->merging_units, 
				    SHIFT_LEFT, shift->merging_bytes);
			coord_inc_item_pos (&from);
			from_ih --;
			coord_inc_item_pos (&to);
		}

		to_ih = node40_ih_at (shift->target, old_items);
		if (shift->entire) {
			/* copy @entire items entirely */

			/* copy item headers */
			xmemcpy (to_ih - shift->entire + 1,
				from_ih - shift->entire + 1,
				shift->entire * sizeof (item_header40));
			/* update item header offset */
			old_offset = ih40_get_offset (from_ih);
			/* AUDIT: Looks like if we calculate old_offset + free_space_start here instead of just old_offset, we can perform one "add" operation less per each iteration */
			for (i = 0; i < shift->entire; i ++, to_ih --, from_ih --)
				ih40_set_offset (to_ih,
						  ih40_get_offset (from_ih) - old_offset + free_space_start );

			/* copy item bodies */
			xmemcpy (zdata (shift->target) + free_space_start,
				zdata (from.node) + old_offset,/*ih40_get_offset (from_ih),*/
				shift->entire_bytes);

			coord_add_item_pos (&from, (int)shift->entire);
			coord_add_item_pos (&to, (int)shift->entire);
		}

		nh40_set_free_space_start (nh, free_space_start + shift->shift_bytes - shift->merging_bytes);
		nh40_set_free_space (nh, nh40_get_free_space (nh) -
				      (shift->shift_bytes - shift->merging_bytes + sizeof (item_header40) * new_items));

		/* update node header */
		nh40_set_num_items (nh, old_items + new_items);
		shift->target->nr_items = old_items + new_items;
		assert ("vs-170",
			nh40_get_free_space (nh) < znode_size (shift->target));

		if (shift->part_units) {
			/* copy heading part (@part units) of @source item as
			   a new item into @target->node */

			/* copy item header of partially copied item */
			coord_set_item_pos (&to, 
					    (unsigned)node40_num_of_items (to.node) - 1);
			xmemcpy (to_ih, from_ih, sizeof (item_header40));
			ih40_set_offset (to_ih, nh40_get_free_space_start (nh) - shift->part_bytes);
			if (item_plugin_by_coord (&to)->b.init)
				item_plugin_by_coord (&to)->b.init(&to, 0);
			copy_units (&to, &from, 0, shift->part_units, 
				    SHIFT_LEFT, shift->part_bytes);
		}

	} else {
		/* copying to right */

		coord_set_item_pos (&from, 
				    (unsigned)node40_num_of_items (from.node) - 1);
		from_ih = node40_ih_at_coord (&from);

		coord_set_item_pos (&to, 0);

		/* prepare space for new items */
		xmemmove (zdata (to.node) + sizeof (node40_header) + shift->shift_bytes,
			 zdata (to.node) + sizeof (node40_header),
			 free_space_start - sizeof (node40_header));
		/* update item headers of moved items */
		to_ih = node40_ih_at (to.node, 0);
		/* first item gets @merging_bytes longer. free space appears
		   at its beginning */
		if (!node_is_empty (to.node))
			ih40_set_offset (to_ih, ih40_get_offset (to_ih) +
					  shift->shift_bytes -
					  shift->merging_bytes);

		for (i = 1; i < old_items; i ++)
			ih40_set_offset (to_ih - i,
					  ih40_get_offset (to_ih - i) + shift->shift_bytes);

		/* move item headers to make space for new items */
		xmemmove (to_ih - old_items + 1 - new_items, to_ih - old_items + 1,
			 sizeof (item_header40) * old_items);
		to_ih -= (new_items - 1);

		nh40_set_free_space_start (nh, free_space_start + shift->shift_bytes);
		nh40_set_free_space (nh, nh40_get_free_space (nh) -
				      (shift->shift_bytes + sizeof (item_header40) * new_items));

		/* update node header */
		nh40_set_num_items (nh, old_items + new_items);
		shift->target->nr_items = old_items + new_items;
		assert ("vs-170",
			nh40_get_free_space (nh) < znode_size (shift->target));

		if (shift->merging_units) {
			coord_add_item_pos (&to, new_items);
			to.unit_pos = 0;
			to.between = AT_UNIT;
			/* prepend first item of @to */
			copy_units (&to, &from, 
				    coord_last_unit_pos (&from) - shift->merging_units + 1,
				    shift->merging_units, 
				    SHIFT_RIGHT, shift->merging_bytes);
			coord_dec_item_pos (&from);
			from_ih ++;
		}

		if (shift->entire) {
			/* copy @entire items entirely */

			/* copy item headers */
			xmemcpy (to_ih, from_ih, shift->entire * sizeof (item_header40));

			/* update item header offset */
			old_offset = ih40_get_offset (from_ih + shift->entire - 1);
			/* AUDIT: old_offset + sizeof (node40_header) + shift->part_bytes calculation can be taken off the loop. */
			for (i = 0; i < shift->entire; i ++, to_ih ++, from_ih ++)
				ih40_set_offset (to_ih,
						  ih40_get_offset (from_ih) - old_offset +
						  sizeof (node40_header) + shift->part_bytes);
			/* copy item bodies */
			coord_add_item_pos (&from, - (int)(shift->entire - 1));
			xmemcpy (zdata (to.node) + sizeof (node40_header) + shift->part_bytes,
				item_body_by_coord (&from), shift->entire_bytes);
			coord_dec_item_pos (&from);
		}

		if (shift->part_units) {
			coord_set_item_pos (&to, 0);
			to.unit_pos = 0;
			to.between = AT_UNIT;
			/* copy heading part (@part units) of @source item as
			   a new item into @target->node */

			/* copy item header of partially copied item */
			xmemcpy (to_ih, from_ih, sizeof (item_header40));
			ih40_set_offset (to_ih, sizeof (node40_header));
			if (item_plugin_by_coord (&to)->b.init)
				item_plugin_by_coord (&to)->b.init(&to, 0);
			copy_units (&to, &from, 
				    coord_last_unit_pos (&from) - shift->part_units + 1,
				    shift->part_units,
				    SHIFT_RIGHT, shift->part_bytes);
		}
	}
}


/* remove everything either before or after @fact_stop. Number of items
   removed completely is returned */
/* Audited by: green(2002.06.13) */
static int node40_delete_copied (struct shift_params * shift)
{
	coord_t from;
	coord_t to;


	if (shift->pend == SHIFT_LEFT) {
		/* we were shifting to left, remove everything from the
		   beginning of @shift->wish_stop->node upto
		   @shift->wish_stop */
		coord_init_first_unit (&from, shift->real_stop.node);
		to = shift->real_stop;

		/* store old coordinate of unit which will be first after
		 * shift to left */
		shift->u.future_first = to;
		coord_next_unit (&shift->u.future_first);
	} else {
		/* we were shifting to right, remove everything from
		   @shift->stop_coord upto to end of
		   @shift->stop_coord->node */
		from = shift->real_stop;
		coord_init_last_unit (&to, from.node);

		/* store old coordinate of unit which will be last after
		 * shift to right */
		shift->u.future_last = from;
		coord_prev_unit (&shift->u.future_last);
	}

	return node40_cut (&from, &to, 0, 0, 0, 0, 0);
}


/* znode has left and right delimiting keys. We moved data between nodes,
   therefore we must update delimiting keys of those znodes */
/* Audited by: green(2002.06.13) */
void update_znode_dkeys (znode * left, znode * right)
{
	reiser4_key key;

	assert ("nikita-1470", spin_dk_is_locked (current_tree));

	leftmost_key_in_node (right, &key );

	if (left == NULL) {
		/* update left delimiting key of @right */
		*znode_get_ld_key (right) = key;
		return;
	} else if (!node_is_empty (left) && !node_is_empty (right)) {
		/* update right delimiting key of @left */
		*znode_get_rd_key (left) = key;
		/* update left delimiting key of @right */
		*znode_get_ld_key (right) = key;
		return;
	} else if (node_is_empty (left) && node_is_empty (right))
		/* AUDIT: there are 2 checks below both stating that both nodes cannot be empty, yet we return success before we even had a chance to check for the error. Perhaps some typo is here? */
		return;
	else if (node_is_empty (left)) {
		assert ("vs-186", !node_is_empty (right));

		/* update right delimiting key of @left */
		*znode_get_rd_key (left) = *znode_get_ld_key (left);

		/* update left delimiting key of @right */
		*znode_get_ld_key (right) = key;
		return;
	}

	if (node_is_empty (right)) {
		assert ("vs-187", !node_is_empty (left));

		/* update right delimiting key of @left */
		*znode_get_rd_key (left) = *znode_get_rd_key (right);

		/* update left delimiting key of @right */
		*znode_get_ld_key (right) = *znode_get_rd_key (right);
		return;
	}
	impossible ("vs-188", "both nodes can not be empty");
}


/* something was moved between @left and @right. Add carry operation to @info
   list to have carry to update delimiting key between them */
/* Audited by: green(2002.06.13) */
static int prepare_for_update (znode * left, znode * right, carry_plugin_info *info)
{
	carry_op * op;
	carry_node * cn;

	if (info == NULL)
		/*
		 * nowhere to send operation to.
		 */
		return 0;

	if (!should_notify_parent (right))
		return 0;

	op = node_post_carry (info, COP_UPDATE, right, 1);
	if (IS_ERR (op) || op == NULL)
		return op ? PTR_ERR (op) : -EIO;

	if (left != NULL) {
		cn = add_carry (info->todo, POOLO_BEFORE, op->node);
		if (IS_ERR (cn))
			return PTR_ERR (cn);
		cn->parent = 1;
		cn->node = left;
		if (ZF_ISSET (left, JNODE_ORPHAN))
			cn->left_before = 1;
		op->u.update.left = cn;
	} else
		op->u.update.left = NULL;
	return 0;
}


/* to delete a pointer to @empty from the tree add corresponding carry
   operation (delete) to @info list */
/* Audited by: green(2002.06.13) */
int node40_prepare_for_removal (znode * empty, carry_plugin_info *info)
{
	carry_op * op;

	if (!should_notify_parent (empty))
		return 0;
	/* already on a road to Styx */
	if (ZF_ISSET (empty, JNODE_HEARD_BANSHEE))
		return 0;
	op = node_post_carry (info, COP_DELETE, empty, 1);
	if (IS_ERR (op) || op == NULL)
		return op ? PTR_ERR (op) : -EIO;

	op->u.delete.child = 0;
	/* fare thee well */
	ZF_SET (empty, JNODE_HEARD_BANSHEE);
	return 0;
}


/* something were shifted from @insert_coord->node to @shift->target, update
   @insert_coord correspondingly */
/* Audited by: green(2002.06.13) */
static void adjust_coord (coord_t * insert_coord,
			  struct shift_params * shift,
			  int removed, int including_insert_coord)
{
	/*
	 * item plugin was invalidated by shifting
	 */
	insert_coord->iplug = NULL;

	if (node_is_empty (shift->wish_stop.node)) {
		assert ("vs-242", shift->everything);
		if (including_insert_coord) {
			if (shift->pend == SHIFT_RIGHT) {
				/* set @insert_coord before first unit of
				   @shift->target node */
				coord_init_before_first_item (insert_coord,
							       shift->target);
			} else {
				/* set @insert_coord after last in target node */
				coord_init_after_last_item (insert_coord,
							     shift->target);
			}
		} else {
			/* set @insert_coord inside of empty node. There is
			 * only one possible coord within an empty
			 * node. init_first_unit will set that coord */
			coord_init_first_unit (insert_coord, shift->wish_stop.node);
		}
		return;
	}

	if (shift->pend == SHIFT_RIGHT) {
		/* there was shifting to right */
		if (shift->everything) {
			/* everything wanted was shifted */
			if (including_insert_coord) {
				/* @insert_coord is set before first unit of
				   @to node */
				coord_init_before_first_item (insert_coord, shift->target);
				insert_coord->between = BEFORE_UNIT;
			} else {
				/* @insert_coord is set after last unit of
				   @insert->node */
				coord_init_last_unit (insert_coord,
						       shift->wish_stop.node);
				insert_coord->between = AFTER_UNIT;
			}
		}
		return;
	}

	/* there was shifting to left */
	if (shift->everything) {
		/* everything wanted was shifted */
		if (including_insert_coord) {
			/* @insert_coord is set after last unit in @to node */
			coord_init_after_last_item (insert_coord, shift->target);
		} else {
			/* @insert_coord is set before first unit in the same
			   node */
			coord_init_before_first_item (insert_coord,
						       shift->wish_stop.node);
		}
		return;
	}


	/*
	 * FIXME-VS: the code below is complicated because with between ==
	 * AFTER_ITEM unit_pos is set to 0
	 */

	if (!removed) {
		/* no items were shifted entirely */
		assert ("vs-195", shift->merging_units == 0 ||
			shift->part_units == 0);

		if (shift->real_stop.item_pos == insert_coord->item_pos) {
			if (shift->merging_units) {
				if (insert_coord->between == AFTER_UNIT) {
					assert( "nikita-1441", insert_coord->unit_pos >=
						shift->merging_units );
					insert_coord->unit_pos -= shift->merging_units;
				} else if (insert_coord->between == BEFORE_UNIT) {
					assert( "nikita-2090", insert_coord->unit_pos >
						shift->merging_units );
					insert_coord->unit_pos -= shift->merging_units;
				}

				assert ("nikita-2083", insert_coord->unit_pos + 1);
			} else {
				if (insert_coord->between == AFTER_UNIT) {
					assert ("nikita-1442",
						insert_coord->unit_pos >= shift->part_units);
					insert_coord->unit_pos -= shift->part_units;
				} else if (insert_coord->between == BEFORE_UNIT) {
					assert ("nikita-2089",
						insert_coord->unit_pos > shift->part_units);
					insert_coord->unit_pos -= shift->part_units;
				}

				assert ("nikita-2084", insert_coord->unit_pos + 1);
			}
		}
		return;
	}
	
	/* we shifted to left and there was no enough space for everything */
	switch (insert_coord->between) {
	case AFTER_UNIT:
	case BEFORE_UNIT:
		if (shift->real_stop.item_pos == insert_coord->item_pos)
			insert_coord->unit_pos -= shift->part_units;
	case AFTER_ITEM:
		coord_add_item_pos (insert_coord, - removed);
		break;
	default:
		impossible ("nikita-2087", "not ready");
	}
	assert ("nikita-2085", insert_coord->unit_pos + 1);
}


/* Audited by: green(2002.06.13) */
static int call_shift_hooks (struct shift_params * shift)
{
	unsigned i, shifted;
	coord_t coord;
	item_plugin *iplug;


	assert ("vs-275", !node_is_empty (shift->target));

	/* number of items shift touches */
	shifted = shift->entire + (shift->merging_units ? 1 : 0) +
		(shift->part_units ? 1 : 0);

	if (shift->pend == SHIFT_LEFT) {
		/* moved items are at the end */
		coord_init_last_unit (&coord, shift->target);
		coord.unit_pos = 0;
		
		assert ("vs-279", shift->pend == 1);
		for (i = 0; i < shifted; i ++) {
			unsigned from, count;

			iplug = item_plugin_by_coord (&coord);
			if (i == 0 && shift->part_units) {
				assert ("vs-277", coord_num_units (&coord) ==
					shift->part_units);
				count = shift->part_units;
				from = 0;
			} else if (i == shifted - 1 && shift->merging_units) {
				count = shift->merging_units;
				from =  coord_num_units (&coord) - count;
			} else {
				count = coord_num_units (&coord);
				from = 0;
			}

			if (iplug->b.shift_hook) {
				iplug->b.shift_hook (&coord, from, count,
						     shift->wish_stop.node);
			}
			coord_add_item_pos (&coord, - shift->pend);
		}
	} else {
		/* moved items are at the beginning */
		coord_init_first_unit (&coord, shift->target);

		assert ("vs-278", shift->pend == -1);
		for (i = 0; i < shifted; i ++) {
			unsigned from, count;

			iplug = item_plugin_by_coord (&coord);
			if (i == 0 && shift->part_units) {
				assert ("vs-277", coord_num_units (&coord) ==
					shift->part_units);
				count = coord_num_units (&coord);
				from = 0;
			} else if (i == shifted - 1 && shift->merging_units) {
				count = shift->merging_units;
				from = 0;
			} else {
				count = coord_num_units (&coord);
				from = 0;
			}

			if (iplug->b.shift_hook) {
				iplug->b.shift_hook (&coord, from, count,
						     shift->wish_stop.node);
			}
			coord_add_item_pos (&coord, - shift->pend);
		}
	}

	return 0;
}


/*
 * shift to left is completed. Return 1 if unit @old was moved to left neighbor
 */
static int unit_moved_left (const struct shift_params * shift,
			    const coord_t * old)
{
	assert ("vs-944", shift->real_stop.node == old->node);

	if (shift->real_stop.item_pos < old->item_pos)
		return 0;
	if (shift->real_stop.item_pos == old->item_pos) {
		if (shift->real_stop.unit_pos < old->unit_pos)
			return 0;
	}
	return 1;
}


/*
 * shift to right is completed. Return 1 if unit @old was moved to right
 * neighbor
 */
static int unit_moved_right (const struct shift_params * shift,
			     const coord_t * old)
{
	assert ("vs-944", shift->real_stop.node == old->node);

	if (shift->real_stop.item_pos > old->item_pos)
		return 0;
	if (shift->real_stop.item_pos == old->item_pos) {
		if (shift->real_stop.unit_pos > old->unit_pos)
			return 0;
	}
	return 1;
}


/*
 * coord @old was set in node from which shift was performed. What was shifted
 * is stored in @shift. Update @old correspondingly to performed shift
 */
static coord_t * adjust_coord2 (const struct shift_params * shift,
				const coord_t * old, coord_t * new)
{
	new->iplug = 0;
	new->between = old->between;


	if (old->node == shift->target) {
		if (shift->pend == SHIFT_LEFT) {
			/* coord which is set inside of left neighbor does not
			 * change during shift to left */
			coord_dup (new, old);
			return new;
		}
		new->node = old->node;
		new->item_pos = old->item_pos + shift->entire + (shift->part_units ? 1 : 0);
		new->unit_pos = old->unit_pos;
		if (old->item_pos == 0 && shift->merging_units)
			new->unit_pos += shift->merging_units;
		return new;
	}


	assert ("vs-977", old->node == shift->wish_stop.node);
	if (shift->pend == SHIFT_LEFT) {
		if (unit_moved_left (shift, old)) {
			/*
			 * unit @old moved to left neighbor. Calculate its
			 * coordinate there
			 */
			new->node = shift->target;
			new->item_pos = node_num_items (shift->target) -
				shift->entire - (shift->part_units ? 1 : 0) + old->item_pos;
			new->unit_pos = old->unit_pos;
			if (shift->merging_units) {
				new->item_pos --;
				if (old->item_pos == 0) {
					/*
					 * unit_pos only changes if item got
					 * merged
					 */
					new->unit_pos = coord_num_units (new) - 
						(shift->merging_units - old->unit_pos);
				}
			}
		} else {
			/*
			 * unit @old did not move to left neighbor.
			 *
			 * Use _nocheck, because @old is outside of its node.
			 */
			coord_dup_nocheck (new, old);
			new->item_pos -= shift->u.future_first.item_pos;
			if (new->item_pos == 0)
				new->unit_pos -= shift->u.future_first.unit_pos;
		}
	} else {
		if (unit_moved_right (shift, old)) {
			/*
			 * unit @old moved to right neighbor
			 */
			new->node = shift->target;
			new->item_pos = old->item_pos - shift->real_stop.item_pos;
			if (new->item_pos == 0) {
				/*
				 * unit @old might change unit pos
				 */
				new->item_pos = old->unit_pos - shift->real_stop.unit_pos;
			}
		} else {
			/*
			 * unit @old did not move to right neighbor, therefore
			 * it did not change
			 */
			coord_dup (new, old);
		}
	}
	new->iplug = item_plugin_by_coord (new);
	return new;
}


/* this is called when shift is completed (something of source node is copied
 * to target and deleted in source) to update all taps set in current
 * context */
static void update_taps (const struct shift_params * shift)
{
	tap_t * tap;
	coord_t new;


	for_all_taps (tap) {
		/* update only taps set to node which is source in the shift */
		if (tap->coord->node == shift->wish_stop.node)
			tap_to_coord (tap, adjust_coord2 (shift, tap->coord, &new));
	}
}


/* plugin->u.node.shift
   look for description of this method in plugin/node/node.h
 */
/* Audited by: green(2002.06.13) */
int node40_shift (coord_t * from, znode * to,
		  shift_direction pend,
		  int delete_child, /* if @from->node becomes empty - it will
				       be deleted from the tree if this is set
				       to 1 */
		  int including_stop_coord /* */,
		  carry_plugin_info *info)
{
	struct shift_params shift;
	int result;
	znode * left, * right;
	znode * source;

	assert ("nikita-2161", coord_check (from));

	xmemset (&shift, 0, sizeof (shift));
	shift.pend = pend;
	shift.wish_stop = *from;
	shift.target = to;

	assert ("nikita-1473", znode_is_write_locked (from->node));
	assert ("nikita-1474", znode_is_write_locked (to));
	node_check (from->node, REISER4_NODE_PANIC);
	node_check (to, REISER4_NODE_PANIC);

	source = from -> node;
	/* set @shift.wish_stop to rightmost/leftmost unit among units we want
	   shifted */
	if (node_is_empty (shift.wish_stop.node))
		result = 1;
	if (pend == SHIFT_LEFT) {
		result = coord_set_to_left (&shift.wish_stop);
		left = to;
		right = from->node;
	} else {
		result = coord_set_to_right (&shift.wish_stop);
		left = from->node;
		right = to;
	}
	if (result) {
		/*
		 * move insertion coord even if there is nothing to move
		 */
		if (including_stop_coord) {
			/* move insertion coord (@from) */
			if (pend == SHIFT_LEFT) {
				/* after last item in target node */
				coord_init_after_last_item (from, to);
			} else {
				/* before first item in target node */
				coord_init_before_first_item (from, to);
			}
		}
		/* there is nothing to shift */
		assert ("nikita-2078", coord_check (from));
		return 0;
	}


	/* when first node plugin with item body compression is implemented,
	   this must be changed to call node specific plugin */

	/* shift->stop_coord is updated to last unit which really will be
	   shifted */
	node40_estimate_shift (&shift);
	if (!shift.shift_bytes) {
		/* we could not shift anything */
		assert ("nikita-2079", coord_check (from));
		return 0;
	}

	trace_if (TRACE_COORDS, print_coord ("shift->wish_stop before copy:", & shift.wish_stop, 0));

	node40_copy (&shift);

	/* item which has been moved from one node to another might want to do
	   something on that event. This can be done by item's shift_hook
	   method, which will be now called for every moved items */
	call_shift_hooks (&shift);

	result = node40_delete_copied (&shift);
	if( result < 0 )
		return result;

	update_taps (&shift);

	/* adjust @from pointer in accordance with @including_stop_coord flag
	   and amount of data which was really shifted */
	adjust_coord (from, &shift, result, including_stop_coord);

	/* add update operation to @info, which is the list of operations to
	   be performed on a higher level */
	result = prepare_for_update (left, right, info);
	if (!result && node_is_empty (source) && delete_child) {
		/* all contents of @from->node is moved to @to and @from->node
		   has to be removed from the tree, so, on higher level we
		   will be removing the pointer to node @from->node */
		result = node40_prepare_for_removal (source, info);
	}

#ifdef DEBUGGING_SHIFT
	dinfo ("SHIFT TO %s: merging %d, entire %d, part %d, size %d\n",
		shift.pend == SHIFT_LEFT ? "LEFT" : "RIGHT",
		shift.merging_units, shift.entire, shift.part_units,
		shift.shift_bytes);
#endif	
	trace_on (TRACE_SHIFT, "shift: [%Li] %s--%s [%Li]: %i\n",
		  *znode_get_block (left), 
		  (shift.pend == SHIFT_LEFT) ? "<" : "",
		  (shift.pend == SHIFT_LEFT) ? "" : ">",
		  *znode_get_block (right), shift.shift_bytes);

	node_check (source, REISER4_NODE_PANIC);
	node_check (to, REISER4_NODE_PANIC);
	assert ("nikita-2080", coord_check (from));

	return result ? result : (int) shift.shift_bytes;
}


/* plugin->u.node.fast_insert() 
   look for description of this method in plugin/node/node.h */
int node40_fast_insert( const coord_t *coord UNUSED_ARG /* node to query */ )
{
	return 1;
}

/* plugin->u.node.fast_paste() 
   look for description of this method in plugin/node/node.h */
int node40_fast_paste( const coord_t *coord UNUSED_ARG /* node to query */ )
{
	return 1;
}

/* plugin->u.node.fast_cut() 
   look for description of this method in plugin/node/node.h */
int node40_fast_cut( const coord_t *coord UNUSED_ARG /* node to query */ )
{
	return 1;
}

/* plugin->u.node.modify - not defined
*/

/* plugin->u.node.max_item_size */
/* Audited by: green(2002.06.13) */
int node40_max_item_size( void )
{
	return reiser4_get_current_sb() -> s_blocksize -
		sizeof( node40_header ) - sizeof( item_header40 );
}




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
