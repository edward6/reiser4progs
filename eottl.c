/*
 * Copyright 2002 by Hans Reiser, licensing governed by reiser4/README
 */

#include "reiser4.h"

/*
 * Extents on the twig level (EOTTL) handling.
 *
 * EOTTL poses some problems to the tree traversal, that are better
 * explained by example.
 *
 * Suppose we have block B1 on the twig level with the following items:
 * 
 * 0. internal item I0 with key (0:0:0:0) (locality, key-type, object-id, offset)
 * 1. extent item E1 with key (1:4:100:0), having 10 blocks of 4k each
 * 2. internal item I2 with key (10:0:0:0)
 * 
 * We are trying to insert item with key (5:0:0:0). Lookup finds node
 * B1, and then intra-node lookup is done. This lookup finished on the
 * E1, because the key we are looking for is larger than the key of E1
 * and is smaller than key the of I2.
 * 
 * Here search is stuck.
 * 
 * After some thought it is clear what is wrong here: extents on the
 * twig level break some basic property of the *search* tree (on the
 * pretext, that they restore property of balanced tree).
 * 
 * Said property is the following: if in the internal node of the search
 * tree we have [ ... Key1 Pointer Key2 ... ] then, all data that are or
 * will be keyed in the tree with the Key such that Key1 <= Key < Key2
 * are accessible through the Pointer.
 * 
 * This is not true, when Pointer is Extent-Pointer, simply because
 * extent cannot expand indefinitely to the right to include any item
 * with
 *   
 *   Key1 <= Key <= Key2. 
 * 
 * For example, our E1 extent is only responsible for the data with keys
 * 
 *   (1:4:100:0) <= key <= (1:4:100:0xffffffffffffffff), and
 * 
 * so, key range 
 * 
 *   ( (1:4:100:0xffffffffffffffff), (10:0:0:0) ) 
 * 
 * is orphaned: there is no way to get there from the tree root.
 * 
 * In other words, extent pointers are different than normal child
 * pointers as far as search tree is concerned, and this creates such
 * problems.
 * 
 * Possible solution for this problem is to insert our item into node
 * pointed to by I2. There are some problems through:
 *
 * (1) I2 can be in a different node.
 * (2) E1 can be immediately followed by another extent E2.
 *
 * (1) is solved by calling reiser4_get_right_neighbor() and accounting
 * for locks/coords as necessary.
 *
 * (2) is more complex. Solution here is to insert new empty leaf node
 * and insert internal item between E1 and E2 pointing to said leaf
 * node. This is further complicated by possibility that E2 is in a
 * different node, etc.
 *
 * Problems:
 *
 * (1) if there was internal item I2 immediately on the right of an
 * extent E1 we and we decided to insert new item S1 into node N2
 * pointed to by I2, then key of S1 will be less than smallest key in
 * the N2. Normally, search key checks that key we are looking for is in
 * the range of keys covered by the node key is being looked in. To work
 * around of this situation, while preserving useful consistency check
 * new flag CBK_TRUST_DK was added to the cbk falgs bitmask. This flag
 * is automatically set on entrance to the coord_by_key() and is only
 * cleared when we are about to enter situation described above.
 *
 * (2) If extent E1 is immediately followed by another extent E2 and we
 * are searching for the key that is between E1 and E2 we only have to
 * insert new empty leaf node when coord_by_key was called for
 * insertion, rather than just for lookup. To distinguish these cases,
 * new flag CBK_FOR_INSERT was added to the cbk falgs bitmask. This flag
 * is automatically set by coord_by_key calls performed by
 * insert_by_key() and friends.
 *
 * (3) Insertion of new empty leaf node (possibly) requires
 * balancing. In any case it requires modification of node content which
 * is only possible under write lock. It may well happen that we only
 * have read lock on the node where new internal pointer is to be
 * inserted (common case: lookup of non-existent stat-data that fells
 * between two extents). If only read lock is held, tree traversal is
 * restarted with lock_level modified so that next time we hit this
 * problem, write lock will be held. Once we have write lock, balancing
 * will be performed.
 *
 *
 *
 *
 *
 *
 */

/*
 * look to the right of @coord. If it is an item of internal type - 1 is
 * returned. If that item is in right neighbor and it is internal - @coord and
 * @lh are switched to that node: move lock handle, zload right neighbor and
 * zrelse znode coord was set to at the beginning
 */
/* Audited by: green(2002.06.15) */
static int is_next_item_internal( coord_t *coord,  lock_handle *lh )
{
	int result;


	if( coord -> item_pos != node_num_items( coord -> node ) - 1 ) {
		/*
		 * next item is in the same node
		 */
		coord_t right;

		coord_dup (&right, coord);
		check_me ("vs-742", coord_next_item (&right) == 0);
		if( item_is_internal( &right ) ) {
			coord_dup (coord, &right);
			return 1;
		}
		return 0;
	} else {
		/*
		 * look for next item in right neighboring node
		 */
		lock_handle right_lh;
		coord_t right;


		init_lh( &right_lh );
		result = reiser4_get_right_neighbor( &right_lh,
						     coord -> node,
						     ZNODE_READ_LOCK,
						     GN_DO_READ);
		if( result && result != -ENAVAIL ) {
			/* error occured */
			/*
			 * FIXME-VS: error code is not returned. Just that
			 * there is no right neighbor
			 */
			done_lh( &right_lh );
			return 0;
		}
		if( !result && ( result = zload( right_lh.node ) ) == 0 ) {
			coord_init_first_unit( &right, right_lh.node );
			if( item_is_internal( &right ) ) {
				/*
				 * switch to right neighbor
				 */
				zrelse( coord -> node );
				done_lh( lh );

				coord_init_zero( coord );
				coord_dup( coord, &right );
				move_lh( lh, &right_lh );

				return 1;
			}
			/* zrelse right neighbor */
			zrelse( right_lh.node );
		}
		/* item to the right of @coord either does not exist or is not
		   of internal type */
		done_lh( &right_lh );
		return 0;
	}
}


/*
 * inserting empty leaf after (or between) item of not internal type we have to
 * know which right delimiting key corresponding znode has to be inserted with
 */
/* Audited by: green(2002.06.15) */
static reiser4_key *rd_key( coord_t *coord, reiser4_key *key )
{
	coord_t dup;

	assert( "nikita-2281", coord_is_between_items( coord ) );
	coord_dup( &dup, coord );

	spin_lock_dk( current_tree );

	if( coord_set_to_right( &dup ) == 0 )
		/*
		 * get right delimiting key from an item to the right of @coord
		 */
		unit_key_by_coord( &dup, key );
	else
		/*
		 * use right delimiting key of parent znode
		 */
		*key = *znode_get_rd_key( coord -> node );

	spin_unlock_dk( current_tree );
	return key;
}


/*
 * this is used to insert empty node into leaf level if tree lookup can not go
 * further down because it stopped between items of not internal type
 */
/* Audited by: green(2002.06.15) */
static int add_empty_leaf( coord_t *insert_coord, lock_handle *lh,
			   const reiser4_key *key, const reiser4_key *rdkey )
{
	int result;
	carry_pool        pool;
	carry_level       todo;
	carry_op         *op;
	znode            *node;
	reiser4_item_data item;
	carry_insert_data cdata;
	__u64             grabbed;

	init_carry_pool( &pool );
	init_carry_level( &todo, &pool );
	ON_STATS( todo.level_no = TWIG_LEVEL );

	grabbed = get_current_context() -> grabbed_blocks;
	result = reiser4_grab_space1( (__u64)1 );
	if( result != 0 )
		return result;

	node = new_node( insert_coord -> node, LEAF_LEVEL );
	reiser4_release_grabbed_space
		( get_current_context() -> grabbed_blocks - grabbed );

	if( IS_ERR( node ) )
		return PTR_ERR( node );
	/*
	 * setup delimiting keys for node being inserted
	 */
	spin_lock_dk( current_tree );
	*znode_get_ld_key( node ) = *key;
	*znode_get_rd_key( node ) = *rdkey;
	spin_unlock_dk( current_tree );

	zrelse( insert_coord -> node );
	op = post_carry( &todo, COP_INSERT, insert_coord -> node, 0 );
	if( !IS_ERR( op ) ) {
		cdata.coord = insert_coord;
		cdata.key   = key;
		cdata.data  = &item;
		op -> u.insert.d = &cdata;
		op -> u.insert.type = COPT_ITEM_DATA;
		build_child_ptr_data( node, &item );
		item.arg = NULL;
		/*
		 * have @insert_coord to be set at inserted item after
		 * insertion is done
		 */
		op -> node -> track = 1;
		op -> node -> tracked = lh;
		
		result = carry( &todo, 0 );
	} else
		result = PTR_ERR( op );
	zput( node );
	done_carry_pool( &pool );
	if( result == 0 )
		result = zload( insert_coord -> node );
	return result;
}

/**
 * handle extent-on-the-twig-level cases in tree traversal
 */
int handle_eottl( cbk_handle *h /* cbk handle */, 
		  int *outcome /* how traversal should proceed */ )
{
	int result;
	reiser4_key key;

	if( h -> level != TWIG_LEVEL ) {
		/*
		 * Continue to traverse tree downward.
		 */
		assert( "nikita-2341", h -> level > TWIG_LEVEL );
		return 0;
	}

	/*
	 * FIXME-NIKITA not yet. Callers are not ready to set CBK_FOR_INSERT.
	 */
	if( 0 && !( h -> flags & CBK_FOR_INSERT ) ) {
		/*
		 * tree traversal is not for insertion. Just return
		 * CBK_COORD_NOTFOUND.
		 */
		h -> result = CBK_COORD_NOTFOUND;
		*outcome = LOOKUP_DONE;
		return 1;
	}

	/*
	 * FIXME-VS: work around twig thing: h->coord can be set such that
	 * item_plugin can not be taken (h->coord->between == AFTER_ITEM)
	 */
	if( !coord_is_existing_item( h -> coord ) || 
	    !item_is_internal( h -> coord ) ) {
		/* strange item type found on non-stop level?!  Twig
		   horrors? */
		assert( "vs-356", h -> level == TWIG_LEVEL );
		assert( "vs-357",
			({
				coord_t coord;

				coord_dup( &coord, h -> coord );
				check_me( "vs-733", coord_set_to_left( &coord ) == 0);
				item_id_by_coord( &coord ) == EXTENT_POINTER_ID;
			}));

		if( *outcome == NS_FOUND ) {
			/*
			 * we have found desired key on twig level in extent item
			 */
			h -> result = CBK_COORD_FOUND;
			reiser4_stat_tree_add( cbk_found );
			*outcome = LOOKUP_DONE;
			return 1;
		}

		/* take a look at the item to the right of h -> coord */
		result = is_next_item_internal( h -> coord, h -> active_lh );
		if( result < 0 ) {
			/*
			 * error occured while we were trying to look at the
			 * item to the right
			 */
			h -> error = "could not check next item";
			h -> result = result;
			*outcome = LOOKUP_DONE;
			return 1;
		} else if( result == 0 ) {
		
			/*
			 * item to the right is not internal one. Allocate a new
			 * node and insert pointer to it after item h -> coord.
			 *
			 * This is a result of extents being located at the twig
			 * level. For explanation, see comment just above
			 * is_next_item_internal().
			 */
			if( cbk_lock_mode( h -> level, h ) != ZNODE_WRITE_LOCK ) {
				/*
				 * we got node read locked, restart
				 * coord_by_key to have write lock on twig
				 * level
				 */
				h -> lock_level = TWIG_LEVEL;
				h -> lock_mode  = ZNODE_WRITE_LOCK;
				*outcome = LOOKUP_REST;
				return 1;
			}
			
			result = add_empty_leaf( h -> coord, h -> active_lh,
						 h -> key, rd_key( h -> coord, &key ) );
			if( result ) {
				h -> error = "could not add empty leaf";
				h -> result = result;
				*outcome = LOOKUP_DONE;
				return 1;
			}
			assert( "vs-358", keyeq( h -> key, item_key_by_coord( h -> coord, &key ) ) );
		} else {
			/* 
			 * this is special case mentioned in the comment on
			 * tree.h:cbk_flags. We have found internal item
			 * immediately on the right of extent, and we are
			 * going to insert new item there. Key of item we are
			 * going to insert is smaller than leftmost key in the
			 * node pointed to by said internal item (otherwise
			 * search wouldn't come to the extent in the first
			 * place).
			 *
			 * This is a result of extents being located at the
			 * twig level. For explanation, see comment just above
			 * is_next_item_internal().
			 */
			h -> flags &= ~CBK_TRUST_DK;
		}
	}
	assert( "vs-362", item_is_internal( h -> coord ) );
	return 0;
}

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
