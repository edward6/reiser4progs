/*
 * Copyright 2001, 2002 by Hans Reiser, licensing governed by reiser4/README
 */

/*
 * Functions to add/delete new nodes to/from the tree
 */

#include "reiser4.h"

static int add_child_ptr( znode *parent, znode *child );


/**
 * warning only issued if error is not -EAGAIN
 */
#define ewarning( error, ... )			\
	if( ( error ) != -EAGAIN )		\
		warning( __VA_ARGS__ )

/**
 * allocate new node on the @level and immediately on the right of @brother.
 *
 */
/* Audited by: umka (2002.06.15) */
znode *new_node( znode *brother /* existing left neighbor of new node */, 
		 tree_level level /* tree level at which new node is to
				   * be allocated */ )
{
	znode *result;
	int    retcode;
	reiser4_block_nr blocknr;

	assert( "nikita-930", brother != NULL );
	
	/* AUDIT: In this point passed "level" should be checked for validness */
	assert( "umka-264", level < REAL_MAX_ZTREE_HEIGHT );

	retcode = assign_fake_blocknr( &blocknr, 1/*formatted*/ );
	if( retcode == 0 ) {
		result = zget( current_tree, &blocknr, NULL, level, GFP_KERNEL );
		if( IS_ERR( result ) ) {
			ewarning( PTR_ERR( result ), "nikita-929",
				  "Cannot allocate znode for carry: %li",
				  PTR_ERR( result ) );
			return result;
		}

		if( !znode_just_created( result ) ) {
			warning( "nikita-2213", 
				 "Allocated already existing block: %llu",
				 blocknr );
			zput( result );
			return ERR_PTR( -EIO );
		}

		/*
		 * @result is created and inserted into hash-table. There is
		 * no need to lock it: nobody can access it yet anyway.
		 *
		 * FIXME_JMACD zget() should add additional checks to panic if attempt to
		 * access orphaned znode is made. -- is this comment still
		 * accurate? nikita: yes, it is, I think.  want to add those
		 * checks? -josh
		 */
		assert( "nikita-931", result != NULL );

		result -> nplug = current_tree -> nplug;
		assert( "nikita-933", result -> nplug != NULL );
			
		retcode = zinit_new( result );
		if( retcode == 0 ) {
			ZF_SET( result, JNODE_CREATED );
			zrelse( result );
		} else {
			zput( result );
			result = ERR_PTR( retcode );
		}
	} else {
		/*
		 * failure to allocate new node during balancing.
		 * This should never happen. Ever. Returning -EAGAIN
		 * is not viable solution, because "out of disk space"
		 * is not transient error that will go away by itself.
		 */
		ewarning( retcode, "nikita-928",
			  "Cannot allocate block for carry: %i", retcode );
		zput( result );
		result = ERR_PTR( retcode );
	}
	assert( "nikita-1071", result != NULL );
	return result;
}

/**
 * allocate new root and add it to the tree
 *
 * This helper function is called by add_new_root().
 *
 */
/* Audited by: umka (2002.06.15) */
znode *add_tree_root( znode *old_root /* existing tree root */, 
		      znode *fake /* "fake" znode */ )
{
	reiser4_tree *tree = current_tree;
	znode        *new_root;
	int           result;

	assert( "nikita-1069", old_root != NULL );
	assert( "umka-262", fake != NULL );
	assert( "umka-263", tree != NULL );

	/*
	 * "fake" znode---one always hanging just above current root. This
	 * node is locked when new root is created or existing root is
	 * deleted. Downward tree traversal takes lock on it before taking
	 * lock on a root node. This avoids race conditions with root
	 * manipulations.
	 *
	 */
	assert( "nikita-1348", znode_above_root( fake ) );
	assert( "nikita-1211", znode_is_root( old_root ) );

	result = 0;
	if( tree -> height >= REAL_MAX_ZTREE_HEIGHT ) {
		warning( "nikita-1344", "Tree is too tall: %i", tree -> height );
		/*
		 * ext2 returns -ENOSPC when it runs out of free inodes with a
		 * following comment (fs/ext2/ialloc.c:441): Is it really
		 * ENOSPC?
		 *
		 * -EXFULL? -EINVAL?
		 */
		result = -ENOSPC; 
	} else {
		/*
		 * Allocate block for new root. It's not that
		 * important where it will be allocated, as root is
		 * almost always in memory. Moreover, allocate on
		 * flush can be going here.
		 */
		assert( "nikita-1448", znode_is_root( old_root ) );
		new_root = new_node( fake, tree -> height + 1 );
		if( !IS_ERR( new_root ) ) {
			lock_handle rlh;

			init_lh( &rlh );
			/*
			 * FIXME-NIKITA pass lock handle from add_new_root()
			 * in stead
			 */
			result = longterm_lock_znode( &rlh, new_root, 
						      ZNODE_WRITE_LOCK, 
						      ZNODE_LOCK_LOPRI );
			if( result == 0 ) {
				coord_t *in_parent;
				++ tree -> height;
				tree -> root_block = *znode_get_block( new_root );
				znode_set_dirty (fake);
				/* new root is a child of "fake" node */
				spin_lock_tree( tree );
				in_parent = &new_root -> in_parent;
				in_parent -> node = fake;
				coord_set_item_pos( in_parent, ~0u );
				in_parent -> between = AT_UNIT;
				spin_unlock_tree( tree );

				/*
				 * insert into new root pointer to the
				 * @old_root.
				 */
				assert( "nikita-1110", 
					WITH_DATA( new_root, 
						   node_is_empty( new_root ) ) );
				spin_lock_dk( current_tree );
				*znode_get_ld_key( new_root ) = *min_key();
				*znode_get_rd_key( new_root ) = *max_key();
				spin_unlock_dk( current_tree );
				sibling_list_insert( new_root, NULL );
				result = add_child_ptr( new_root, old_root );
				done_lh( &rlh );
			}
		}
	}
	if( result != 0 )
		new_root = ERR_PTR( result );
	return new_root;
}

/**
 * build &reiser4_item_data for inserting child pointer
 *
 * Build &reiser4_item_data that can be later used to insert pointer to @child
 * in its parent.
 *
 */
/* Audited by: umka (2002.06.15) */
void build_child_ptr_data( znode *child /* node pointer to which will be
					 * inserted */, 
			   reiser4_item_data *data /* where to store result */ )
{
	assert( "nikita-1116", child != NULL );
	assert( "nikita-1117", data  != NULL );

	/* this is subtle assignment to meditate upon */
	data -> data = ( char * ) znode_get_block( child );
	/* data -> data is kernel space */
	data -> user = 0;
	data -> length = sizeof( reiser4_block_nr );
	/* FIXME-VS: hardcoded internal item? */

	/* AUDIT: Is it possible that "item_plugin_by_id" may find nothing? */
	data -> iplug = item_plugin_by_id( NODE_POINTER_ID );
}

/**
 * add pointer to @child into empty @parent.
 *
 * This is used when pointer to old root is inserted into new root which is
 * empty.
 */
/* Audited by: umka (2002.06.15) */
static int add_child_ptr( znode *parent, znode *child )
{
	coord_t       coord;
	reiser4_item_data data;
	int               result;
	reiser4_key      *key;

	assert( "nikita-1111", parent != NULL );
	assert( "nikita-1112", child != NULL );
	assert( "nikita-1115", znode_get_level( parent ) == znode_get_level( child ) + 1 );

	result = zload( parent );
	if( result != 0 )
		return result;
	assert( "nikita-1113", node_is_empty( parent ) );
	coord_init_first_unit( &coord, parent );

	build_child_ptr_data( child, &data );
	data.arg = NULL;

	key = UNDER_SPIN( dk, current_tree, znode_get_ld_key( child ) );
	result = node_plugin_by_node( parent ) -> create_item( &coord, key, 
							       &data, NULL );
	znode_set_dirty( parent );
	zrelse( parent );
	return result;
}

/**
 * actually remove tree root
 */
/* Audited by: umka (2002.06.15) */
static int kill_root( reiser4_tree *tree /* tree from which root is being
					  * removed */, 
		      znode *old_root /* root node that is being removed */, 
		      znode *new_root /* new root---sole child of *
				       * @old_root */, 
		      const reiser4_block_nr *new_root_blk /* disk address of
							    * @new_root */ )
{
	znode *fake;
	int    result;

	assert( "umka-265", tree != NULL );
	assert( "nikita-1198", new_root != NULL );
	assert( "nikita-1199", znode_get_level( new_root ) + 1 == znode_get_level( old_root ) );

	assert( "nikita-1201", znode_is_write_locked( old_root ) );

	assert( "nikita-1203", disk_addr_eq( new_root_blk, 
					     znode_get_block( new_root ) ) );

	result = 0;
	/* obtain and lock "fake" znode protecting changes in tree height. */
	fake = zget( tree, &FAKE_TREE_ADDR, NULL, 0, GFP_KERNEL );
	if( !IS_ERR( fake ) ) {
		lock_handle handle_for_fake;

		init_lh( &handle_for_fake );
		result = longterm_lock_znode( &handle_for_fake, 
					      fake, ZNODE_WRITE_LOCK, 
					      ZNODE_LOCK_HIPRI );
		zput( fake );
		if( result == 0 ) {
			tree -> root_block = *new_root_blk;
			-- tree -> height;
			assert( "nikita-1202", 
				tree -> height = znode_get_level( new_root ) );

			znode_set_dirty( fake );

			/*
			 * don't take long term lock a @new_root. Take
			 * spinlock.
			 */
			
			spin_lock_tree( tree );

			/* new root is child on "fake" node */
			new_root -> in_parent.node = fake;
			coord_set_item_pos( &new_root -> in_parent, ~0u );
			new_root -> in_parent.between = AT_UNIT;
			atomic_inc( &fake -> c_count );

			sibling_list_insert_nolock( new_root, NULL );
			spin_unlock_tree( tree );

			/* reinitialise old root. */
			result = node_plugin_by_node( old_root ) -> init( old_root );
			if( result == 0 ) {
				assert( "nikita-1279", 
					node_is_empty( old_root ) );
				ZF_SET( old_root, JNODE_HEARD_BANSHEE );
				atomic_set( &old_root -> c_count, 0 );
			}
		}
		done_lh( &handle_for_fake );
	} else
		result = PTR_ERR( fake );
	return result;
}

/**
 * remove tree root
 *
 * This function removes tree root, decreasing tree height by one.  Tree root
 * and its only child (that is going to become new tree root) are write locked
 * at the entry.
 *
 * To remove tree root we need to take lock on special "fake" znode that
 * protects changes of tree height. See comments in add_tree_root() for more
 * on this.
 *
 * Also parent pointers have to be updated in
 * old and new root. To simplify code, function is split into two parts: outer
 * kill_tree_root() collects all necessary arguments and calls kill_root()
 * to do the actual job.
 *
 */
/* Audited by: umka (2002.06.15) */
int kill_tree_root( znode *old_root /* tree root that we are removing */ )
{
	int           result;
	coord_t       down_link;
	znode        *new_root;
	
	assert( "umka-266", current_tree != NULL );
	assert( "nikita-1194", old_root != NULL );
	assert( "nikita-1196", znode_is_root( old_root ) );
	assert( "nikita-1200", node_num_items( old_root ) == 1 );
	assert( "nikita-1401", znode_is_write_locked( old_root ) );

	coord_init_first_unit( &down_link, old_root );

	new_root = UNDER_SPIN( dk, current_tree,
			       child_znode( &down_link, old_root, 0, 1 ) );
	if( !IS_ERR( new_root ) ) {
		result = kill_root( current_tree, old_root, new_root, 
				    znode_get_block( new_root ) );
		zput( new_root );
	} else
		result = PTR_ERR( new_root );

	return result;
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
