/*
 * Copyright 2002 by Hans Reiser, licensing governed by reiser4/README
 */
/*
 * Seals implemenation.
 */
/*
 * Seals are "weak" tree pointers. They are analogous to tree coords in
 * allowing to bypass tree traversal. But normal usage of coords implies that
 * node pointed to by coord is locked, whereas seals don't keep a lock (or
 * even a reference) to znode. In stead, each znode contains a version number,
 * increased on each znode modification. This version number is copied into a
 * seal when seal is created. Later, one can "validate" seal by calling
 * seal_validate(). If znode is in cache and its version number is still the
 * same, seal is "pristine" and coord associated with it can be re-used
 * immediately.
 *
 * If, on the other hand, znode is out of cache, or it is obviously different
 * one from the znode seal was initially attached to (for example, it is on
 * the different level, or is being removed from the tree), seal is
 * irreparably invalid ("burned") and tree traversal has to be repeated.
 *
 * Otherwise, there is some hope, that while znode was modified (and seal was
 * "broken" as a result), key attached to the seal is still in the node. This
 * is checked by first comparing this key with delimiting keys of node and, if
 * key is ok, doing intra-node lookup.
 *
 *
 */

#include "reiser4.h"

static znode *seal_node( const seal_t *seal );
static int seal_matches( const seal_t *seal, znode *node );
static int seal_search_node( seal_t *seal, tree_coord *coord, 
			     znode *node, reiser4_key *key, lookup_bias bias,
			     tree_level level );

/** 
 * initialise seal. This can be called several times on the same seal. @coord
 * and @key can be NULL. 
 */
void seal_init( seal_t      *seal /* seal to initialise */, 
		tree_coord  *coord /* coord @seal will be attached to */, 
		const reiser4_key *key UNUSED_ARG /* key @seal will be
						   * attached to */ )
{
	assert( "nikita-1886", seal != NULL );
	xmemset( seal, 0, sizeof *seal );
	if( coord != NULL ) {
		znode *node;

		node = coord -> node;
		assert( "nikita-1987", node != NULL );
		spin_lock_znode( node );
		seal -> version = node -> version;
		assert( "nikita-1988", seal -> version != 0 );
		seal -> block   = *znode_get_block( node );
#if REISER4_DEBUG
		seal -> coord = *coord;
		if( key != NULL )
			seal -> key = *key;
#endif
		spin_unlock_znode( node );
	}
}

/** finish with seal */
void seal_done( seal_t *seal )
{
	assert( "nikita-1887", seal != NULL );
	if( REISER4_DEBUG )
		seal -> version = 0;
}

/** true if seal was initialised */
int seal_is_set( const seal_t *seal /* seal to query */ )
{
	assert( "nikita-1890", seal != NULL );
	return seal -> version != 0;
}

/**
 * (re-)validate seal.
 *
 * Checks whether seal is pristine, and try to revalidate it if possible.
 *
 * If seal was burned, or broken irreparably, return -EAGAIN.
 *
 */
int seal_validate( seal_t            *seal  /* seal to validate */, 
		   tree_coord        *coord /* coord to validate against */, 
		   reiser4_key       *key   /* key to validate against */, 
		   tree_level         level /* level of node */,
		   lock_handle       *lh    /* resulting lock handle */, 
		   lookup_bias        bias  /* search bias */,
		   znode_lock_mode    mode  /* lock node */,
		   znode_lock_request request /* locking priority */ )
{
	znode *node;
	int    result;

	assert( "nikita-1889", seal != NULL );
	assert( "nikita-1881", seal_is_set( seal ) );
	assert( "nikita-1882", key != NULL );
	assert( "nikita-1883", coord != NULL );
	assert( "nikita-1884", lh != NULL );
	assert( "nikita-1885", keyeq( &seal -> key, key ) );
	assert( "nikita-1989", !memcmp( &seal -> coord, coord, sizeof *coord ) );

	/* obtain znode by block number */
	node = seal_node( seal );
	if( node != NULL ) {
		/* znode was in cache, lock it */
		result = longterm_lock_znode( lh, node, mode, request );
		zput( node );
		if( result == 0 ) {
			if( seal_matches( seal, node ) ) {
				/* if seal version and znode version
				 * coincide */
				assert( "nikita-1990", 
					node == seal -> coord.node );
				assert( "nikita-1898", 
					({ 
						reiser4_key ukey;

						coord_of_unit( coord ) && 
						keyeq( key, 
						       unit_key_by_coord( coord,
									  &ukey ) );
					}) );
				reiser4_stat_seal_add( perfect_match );
				result = 0;
			} else if( znode_contains_key_lock( node, key ) )
				/* 
				 * seal is broken, but there is a hope that
				 * key is still in @node
				 */
				result = seal_search_node( seal, coord, node, 
							   key, bias, level );
			else {
				/* key is not in @node */
				reiser4_stat_seal_add( key_drift );
				result = -EAGAIN;
			}
		}
		if( result != 0 )
			/* unlock node on failure */
			done_lh( lh );
	} else {
		/* znode wasn't in cache */
		reiser4_stat_seal_add( out_of_cache );
		result = -EAGAIN;
	}
	return result;
}

/* helpers functions */

/** obtain reference to znode seal points to, if in cache */
static znode *seal_node( const seal_t *seal /* seal to query */ )
{
	assert( "nikita-1891", seal != NULL );
	return zlook( current_tree, &seal -> block );
}

/** true if @seal version and @node version coincide */
static int seal_matches( const seal_t *seal /* seal to check */, 
			 znode *node /* node to check */ )
{
	int result;

	assert( "nikita-1991", seal != NULL );
	assert( "nikita-1993", node != NULL );

	spin_lock_znode( node );
	result = ( seal -> version == node -> version );
	spin_unlock_znode( node );
	return result;
}

/** intranode search */
static int seal_search_node( seal_t      *seal  /* seal to repair */, 
			     tree_coord  *coord /* coord attached to @seal */, 
			     znode       *node  /* node to search in */, 
			     reiser4_key *key   /* key attached to @seal */, 
			     lookup_bias  bias  /* search bias */,
			     tree_level   level /* node level */ )
{
	int         result;
	reiser4_key unit_key;

	assert( "nikita-1888", seal != NULL );
	assert( "nikita-1994", coord != NULL );
	assert( "nikita-1892", node != NULL );
	assert( "nikita-1893", znode_is_any_locked( node ) );

	if( ( znode_get_level( node ) != level ) ||
	    ZF_ISSET( node, ZNODE_HEARD_BANSHEE ) ||
	    ZF_ISSET( node, ZNODE_IS_DYING ) ) {
		reiser4_stat_seal_add( wrong_node );
		return -EAGAIN;
	}

	result = zload( node );
	if( result != 0 )
		return result;
	
	if( coord_of_unit( coord ) && 
	    keyeq( key, unit_key_by_coord( coord, &unit_key ) ) ) {
		/* coord is still at the same position in the @node */
		reiser4_stat_seal_add( didnt_move );
		result = 0;
	} else {
		result = node_plugin_by_node( node ) -> lookup( node, key, 
								bias, coord );
		if( result == NS_FOUND ) {
			/* renew seal */
			reiser4_stat_seal_add( found );
			seal_init( seal, coord, key );
		} else
			result = -ENOENT;
	}
	zrelse( node );
	return result;
}

#if REISER4_DEBUG
void print_seal( const char *prefix, const seal_t *seal )
{
	if( seal == NULL ) {
		info( "%s: null seal\n", prefix );
	} else {
		info( "%s: version: %llu, block: %llu\n",
		      prefix, seal -> version, seal -> block );
		print_key( "seal key", &seal -> key );
		print_coord( "seal coord", &seal -> coord, 1 );
	}
}
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
