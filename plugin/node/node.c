/*
 *
 * Copyright 2000, 2001, 2002 by Hans Reiser, licensing governed by reiserfs/README
 *
 */

/* Node plugin interface.

   Description: The tree provides the abstraction of flows, which it
   internally fragments into items which it stores in nodes.

   A key_atom is a piece of data bound to a single key.

   For reasonable space efficiency to be achieved it is often
   necessary to store key_atoms in the nodes in the form of items, where
   an item is a sequence of key_atoms of the same or similar type. It is
   more space-efficient, because the item can implement (very)
   efficient compression of key_atom's bodies using internal knowledge
   about their semantics, and it can often avoid having a key for each
   key_atom. Each type of item has specific operations implemented by its
   item handler (see balance.c).

   Rationale: the rest of the code (specifically balancing routines)
   accesses leaf level nodes through this interface. This way we can
   implement various block layouts and even combine various layouts
   within the same tree. Balancing/allocating algorithms should not
   care about peculiarities of splitting/merging specific item types,
   but rather should leave that to the item's item handler.

   Items, including those that provide the abstraction of flows, have
   the property that if you move them in part or in whole to another
   node, the balancing code invokes their is_left_mergeable()
   item_operation to determine if they are mergeable with their new
   neighbor in the node you have moved them to.  For some items the
   is_left_mergeable() function always returns null.

   When moving the bodies of items from one node to another:

   * if a partial item is shifted to another node the balancing code invokes
     an item handler method to handle the item splitting.

   * if the balancing code needs to merge with an item in the node it
     is shifting to, it will invoke an item handler method to handle
     the item merging.

   * if it needs to move whole item bodies unchanged, the balancing code uses xmemcpy()
     adjusting the item headers after the move is done using the node handler.
*/

#include "../../reiser4.h"

/**
 * return starting key of the leftmost item in the @node
 */
/* Audited by: green(2002.06.12) */
reiser4_key *leftmost_key_in_node( const znode *node /* node to query */, 
				   reiser4_key *key /* resulting key */ )
{
	assert( "nikita-1634", node != NULL );
	assert( "nikita-1635", key != NULL );

	if( !node_is_empty( node ) ) {
		coord_t first_item;

		coord_init_first_unit( &first_item, ( znode * ) node );
		item_key_by_coord( &first_item, key );
	} else
		*key = *max_key();
	return key;
}

/** helper function: convert 4 bit integer to its hex representation */
/* Audited by: green(2002.06.12) */
static char hex_to_ascii( const int hex /* hex digit */ )
{
	assert( "nikita-1081", ( 0 <= hex ) && ( hex < 0x10 ) );

	if( hex < 10 )
		return '0' + hex;
	else
		return 'a' + hex - 10;
}


/** helper function used to indent output during recursive tree printing */
/* Audited by: green(2002.06.12) */
void indent (unsigned indentation)
{
	unsigned i;

	for (i = 0 ; i < indentation ; ++ i)
		info ("%.1i........", indentation - i);
}

/** helper function used to indent output for @node during recursive tree
 * printing */
/* Audited by: green(2002.06.12) */
void indent_znode( const znode *node /* current node */ )
{
	if( current_tree -> height < znode_get_level( node ) )
		indent( 0 );
	else
		indent( current_tree -> height - znode_get_level( node ) );
}

/**
 * debugging aid: output human readable information about @node
 */
void print_node_content( const char *prefix /* output prefix */, 
			 const znode *node /* node to print */, 
			 __u32 flags /* print flags */ )
{
	unsigned i;
	coord_t coord;
	item_plugin *iplug;
	reiser4_key key;


	if( !znode_is_loaded( node ) ) {
		print_znode( "znode is not loaded\n", node );
		return;
	}
	if( ( flags & REISER4_NODE_PRINT_HEADER ) &&
	    ( node_plugin_by_node( node ) -> print != NULL ) ) {
		indent_znode (node);
		node_plugin_by_node( node ) -> print( prefix, node, flags );

		indent_znode (node);
		print_key ("LDKEY", &node->ld_key);

		indent_znode (node);
		print_key ("RDKEY", &node->rd_key);
	}

	/*if( flags & REISER4_NODE_SILENT ) {return;}*/

	coord.node = ( znode * ) node;
	coord.unit_pos = 0;
	coord.between = AT_UNIT;
	/*indent_znode (node);*/
	for( i = 0; i < node_num_items( node ); i ++ ) {
		indent_znode (node);info( "%d: ", i );

		coord_set_item_pos (&coord, i);
		
		iplug = item_plugin_by_coord( &coord );
		if( flags & REISER4_NODE_PRINT_PLUGINS ) {
			print_plugin( "\titem plugin", item_plugin_to_plugin (iplug) ); indent_znode (node);
		}
		if( flags & REISER4_NODE_PRINT_KEYS ) {
			item_key_by_coord( &coord, &key );
			print_key( "\titem key", &key );
		}

		if( ( flags & REISER4_NODE_PRINT_ITEMS ) &&
		    ( iplug -> common.print ) ) {
			indent_znode (node);
			info ("\tlength %d\n", item_length_by_coord( &coord ) );
			indent_znode (node); 
			iplug -> common.print( "\titem", &coord );
		}
		if( flags & REISER4_NODE_PRINT_DATA ) {
			int   j;
			int   length;
			char *data;

			data = item_body_by_coord( &coord );
			length = item_length_by_coord( &coord );
			indent_znode( node );
			info( "\titem length: %i, offset: %i\n",
			      length, data - zdata( node ) );
			for( j = 0 ; j < length ; ++j ) {
				char datum;
				
				if( ( j % 16 ) == 0 ) {
					/*
					 * next 16 bytes
					 */					
					if( j == 0 ) {
						indent_znode( node );
						info( "\tdata % .2i: ", j );
					} else {
						info( "\n" );
						indent_znode( node );
						info( "\t     % .2i: ", j );
					}
				}
				datum = data[ j ];
				info( "%c", 
				      hex_to_ascii( ( datum & 0xf0 ) >> 4 ) );
				info( "%c ", hex_to_ascii( datum & 0xf ) );
			}
			info( "\n" ); indent_znode (node);
		}
		info( "======================\n" );
	}
	info( "\n" );
}

#if REISER4_DEBUG
/** debugging aid: check consistency of @node content */
void node_check( const znode *node /* node to check */, 
		 __u32 flags /* check flags */ )
{
	const char * mes;
	
	if( znode_is_loaded( node ) && 
	    ( node_plugin_by_node( node ) -> check ) &&
	    node_plugin_by_node( node ) -> check( node, flags, &mes ) ) {
		info( "%s\n", mes );
		print_node_content( "check", node, ~0u );
		if( flags & REISER4_NODE_PANIC )
			rpanic( "vs-273", "node corrupted" );
	}
}
#endif


reiser4_plugin node_plugins[ LAST_NODE_ID ] = {
	[ NODE40_ID ] = {
		.node = {
			.h = {
				.type_id = REISER4_NODE_PLUGIN_TYPE,
				.id      = NODE40_ID,
				.pops    = NULL,
				.label   = "unified",
				.desc    = "unified node layout",
				.linkage = TS_LIST_LINK_ZERO,
			},
			.item_overhead    = node40_item_overhead,
			.free_space       = node40_free_space,
			.lookup           = node40_lookup,
			.num_of_items     = node40_num_of_items,
			.item_by_coord	  = node40_item_by_coord,
			.length_by_coord  = node40_length_by_coord,
			.plugin_by_coord  = node40_plugin_by_coord,
			.key_at           = node40_key_at,
			.estimate         = node40_estimate,
			.check            = node40_check,
			.parse            = node40_parse,
			.init             = node40_init,
#ifdef GUESS_EXISTS
			.guess            = node40_guess,
#endif
			.print            = node40_print,
			.change_item_size = node40_change_item_size,
			.create_item      = node40_create_item,
			.update_item_key  = node40_update_item_key, 
			.cut_and_kill     = node40_cut_and_kill,
			.cut              = node40_cut, 
			.shift            = node40_shift,
			.fast_insert      = node40_fast_insert,
			.fast_paste       = node40_fast_paste,
			.fast_cut         = node40_fast_cut,
#ifdef MODIFY_EXISTS
			.modify           = NULL,
#endif
			.max_item_size    = node40_max_item_size
		}
	},
};


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
