/*
 * Copyright 2001, 2002 by Hans Reiser, licensing governed by reiser4/README
 */

/*
 * Directory entry implementation
 */

#include "../../reiser4.h"

/* Audited by: green(2002.06.14) */
void de_print( const char *prefix /* prefix to print */, 
	       coord_t *coord /* item to print */ )
{
	assert( "nikita-1456", prefix != NULL );
	assert( "nikita-1457", coord != NULL );

	if( item_length_by_coord( coord ) < 
	    ( int ) sizeof( directory_entry_format ) ) {
		info( "%s: wrong size: %i < %i\n", prefix,
		      item_length_by_coord( coord ), 
		      sizeof( directory_entry_format ) );
	} else {
		reiser4_key  sdkey;
		char        *name;

		de_extract_key( coord, &sdkey );
		name = de_extract_name( coord );
		info( "%s: name: %s\n", prefix, name );
		print_key( "\tsdkey", &sdkey );
	}
}

/**
 * ->extract_key() method of simple directory item plugin.
 */
/* Audited by: green(2002.06.14) */
int de_extract_key( const coord_t *coord /* coord of item */,
		    reiser4_key *key /* resulting key */ )
{
	directory_entry_format *dent;

	assert( "nikita-1458", coord != NULL );
	assert( "nikita-1459", key != NULL );

	dent = ( directory_entry_format * ) item_body_by_coord( coord );
	assert( "nikita-1158", item_length_by_coord( coord ) >=
		( int ) sizeof *dent );
	return extract_key_from_id( &dent -> id, key );
}

int de_update_key( const coord_t *coord, 
		   const reiser4_key *key, lock_handle *lh UNUSED_ARG )
{
	directory_entry_format *dent;
	obj_key_id              obj_id;
	int                     result;

	assert( "nikita-2342", coord != NULL );
	assert( "nikita-2343", key != NULL );

	dent = ( directory_entry_format * ) item_body_by_coord( coord );
	result = build_obj_key_id( key, &obj_id );
	if( result == 0 ) {
		dent -> id = obj_id;
		znode_set_dirty( coord -> node );
	}
	return 0;
}

/**
 * ->extract_name() method of simple directory item plugin.
 */
/* Audited by: green(2002.06.14) */
char *de_extract_name( const coord_t *coord /* coord of item */ )
{
	directory_entry_format *dent;

	assert( "nikita-1460", coord != NULL );

	dent = ( directory_entry_format * ) item_body_by_coord( coord );
	assert( "nikita-1160", item_length_by_coord( coord ) >= 
		( int ) sizeof *dent );
	return ( char * ) dent -> name;
}

/**
 * ->extract_file_type() method of simple directory item plugin.
 */
unsigned de_extract_file_type( const coord_t *coord UNUSED_ARG /* coord of
								   * item */ )
{
	assert( "nikita-1764", coord != NULL );
	/*
	 * we don't store file type in the directory entry yet.
	 *
	 * But see comments at kassign.h:obj_key_id
	 */
	return DT_UNKNOWN;
}

/* Audited by: green(2002.06.14) */
int de_add_entry( struct inode *dir /* directory of item */, 
		  coord_t *coord /* coord of item */, 
		  lock_handle *lh /* insertion lock handle */, 
		  const struct dentry *name /* name to add */, 
		  reiser4_dir_entry_desc *entry /* parameters of new directory
						 * entry */ )
{
	reiser4_item_data       data;
	directory_entry_format *dent;
	int                     result;

	data.length = sizeof *dent + name -> d_name.len + 1;
	data.data   = NULL;
	data.user   = 0;
	data.iplug  = item_plugin_by_id( SIMPLE_DIR_ENTRY_ID );
	
	result = insert_by_coord( coord, &data, &entry -> key, lh,
				  inter_syscall_ra( dir ), NO_RAP, 0/*flags*/ );
	if( result != 0 )
		return result;

	dent = ( directory_entry_format * ) item_body_by_coord( coord );
	build_inode_key_id( entry -> obj, &dent -> id );
	assert( "nikita-1163", 
		strlen( name -> d_name.name ) == name -> d_name.len );
	/* AUDIT: The length of source is known, so using of memcpy
	   would be much cheaper here */
	strcpy( ( unsigned char * ) dent -> name, name -> d_name.name );
	cputod8( 0, &dent -> name[ name -> d_name.len ] );

	dir -> i_size += 1;

	return 0;
}

/* Audited by: green(2002.06.14) */
int de_rem_entry( struct inode *dir /* directory of item */, 
		  coord_t *coord /* coord of item */,
		  lock_handle *lh UNUSED_ARG /* lock handle for
						      * removal */, 
		  reiser4_dir_entry_desc *entry UNUSED_ARG /* parameters of
							    * directory entry
							    * being removed */ )
{
	int     result;
	coord_t shadow;

	/*
	 * cut_node() is supposed to take pointers to _different_
	 * coords, because it will modify them without respect to
	 * possible aliasing. To work around this, create temporary copy
	 * of @coord.
	 */
	coord_dup( &shadow, coord );
	result = cut_node( coord, &shadow, NULL, NULL, NULL, DELETE_KILL, 0 );

	if( result == 0 ) {
		if( dir -> i_size >= 1 )
			dir -> i_size -= 1;
		else {
			warning( "nikita-2509", "Dir %llu is runt",
				 get_inode_oid( dir ) );
			result = -EIO;
		}
	}
	return result;
}

/* Audited by: green(2002.06.14) */
int de_max_name_len( const struct inode *dir )
{
	return 
		tree_by_inode( dir ) -> nplug -> max_item_size() - 
		sizeof( directory_entry_format ) - 2;
}


/* 
 * Make Linus happy.
 * Local variables:
 * c-indentation-style: "K&R"
 * mode-name: "LC"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 120
 * End:
 */
