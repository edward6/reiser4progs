/*
 * Copyright 2001, 2002 by Hans Reiser, licensing governed by reiser4/README
 */

/*
 * definition of item plugins.
 */

#include "../../reiser4.h"

/* return pointer to item body */
/* Audited by: green(2002.06.15) */
void *item_body_by_coord( const coord_t *coord /* coord to query */ )
{
	assert( "nikita-324", coord != NULL );
	assert( "nikita-325", coord -> node != NULL );
	assert( "nikita-326", znode_is_loaded( coord -> node ) );
	trace_stamp( TRACE_TREE );

	return node_plugin_by_node( coord -> node ) -> item_by_coord( coord );
}

/** return length of item at @coord */
/* Audited by: green(2002.06.15) */
int item_length_by_coord( const coord_t *coord /* coord to query */ )
{
	assert( "nikita-327", coord != NULL );
	assert( "nikita-328", coord -> node != NULL );
	assert( "nikita-329", znode_is_loaded( coord -> node ) );
	trace_stamp( TRACE_TREE );

	return node_plugin_by_node( coord -> node ) -> length_by_coord( coord );
}

/** return plugin of item at @coord */
/* Audited by: green(2002.06.15) */
item_plugin *item_plugin_by_coord( const coord_t *coord /* coord to query */ )
{
	assert( "nikita-330", coord != NULL );
	assert( "nikita-331", coord -> node != NULL );
	assert( "nikita-332", znode_is_loaded( coord -> node ) );
	trace_stamp( TRACE_TREE );

	if( coord -> iplug == NULL )
		( ( coord_t * ) coord ) -> iplug = node_plugin_by_node( coord -> node ) -> plugin_by_coord( coord );
	assert( "nikita-2479", 
		coord -> iplug == node_plugin_by_node( coord -> node ) -> plugin_by_coord( coord ) );
	return coord -> iplug;
}

/** return type of item at @coord */
/* Audited by: green(2002.06.15) */
item_type_id item_type_by_coord( const coord_t *coord /* coord to query */ )
{
	assert( "nikita-333", coord != NULL );
	assert( "nikita-334", coord -> node != NULL );
	assert( "nikita-335", znode_is_loaded( coord -> node ) );
	assert( "nikita-336", item_plugin_by_coord( coord ) != NULL );

	trace_stamp( TRACE_TREE );

	return item_plugin_by_coord( coord ) -> common.item_type;
}

/* return id of item */
/* Audited by: green(2002.06.15) */
item_id item_id_by_coord( const coord_t *coord /* coord to query */ )
{
	assert( "vs-539", coord != NULL );
	assert( "vs-538", coord -> node != NULL );
	assert( "vs-537", znode_is_loaded( coord -> node ) );
	assert( "vs-536", item_plugin_by_coord( coord ) != NULL );

	trace_stamp( TRACE_TREE );

	assert( "vs-540",
		item_id_by_plugin( item_plugin_by_coord( coord ) ) < LAST_ITEM_ID );
	return item_id_by_plugin( item_plugin_by_coord( coord ) );
}

/** return key of item at @coord */
/* Audited by: green(2002.06.15) */
reiser4_key *item_key_by_coord( const coord_t *coord /* coord to query */, 
				reiser4_key *key /* result */ )
{
	assert( "nikita-338", coord != NULL );
	assert( "nikita-339", coord -> node != NULL );
	assert( "nikita-340", znode_is_loaded( coord -> node ) );
	trace_stamp( TRACE_TREE );

	return node_plugin_by_node( coord -> node ) -> key_at( coord, key );
}

/** return key of unit at @coord */
/* Audited by: green(2002.06.15) */
reiser4_key *unit_key_by_coord( const coord_t *coord /* coord to query */, 
				reiser4_key *key /* result */ )
{
	assert( "nikita-772", coord != NULL );
	assert( "nikita-774", coord -> node != NULL );
	assert( "nikita-775", znode_is_loaded( coord -> node ) );
	trace_stamp( TRACE_TREE );

	if( item_plugin_by_coord( coord )->common.unit_key != NULL )
		return item_plugin_by_coord( coord )->common.unit_key
			( coord, key );
	else
		return item_key_by_coord( coord, key );
}


/** ->max_key_inside() method for items consisting of exactly one key (like
    stat-data) */
/* Audited by: green(2002.06.14) */
static reiser4_key *single_key( const coord_t *coord /* coord of item */, 
				reiser4_key *result /* resulting key */ )
{
	assert( "nikita-604", coord != NULL );

	/* coord -> key is starting key of this item and it has to be already
	   filled in */
	return unit_key_by_coord( coord, result );
}

/** ->nr_units() method for items consisting of exactly one unit always */
/* Audited by: green(2002.06.14) */
static unsigned int single_unit( const coord_t *coord UNUSED_ARG /* coord
								       of
								       item */ )
{
	return 1;
}

/** default ->fast_paste() method */
int agree_to_fast_op( const coord_t *coord UNUSED_ARG /* coord of item */ )
{
	return 1;
}

/* Audited by: green(2002.06.14) */
int item_can_contain_key( const coord_t *item /* coord of item */, 
			  const reiser4_key *key /* key to check */,
			  const reiser4_item_data *data /* parameters of item
							 * being created */ )
{
	item_plugin *iplug;
	reiser4_key min_key_in_item;
	reiser4_key max_key_in_item;
	
	assert( "nikita-1658", item != NULL );
	assert( "nikita-1659", key != NULL );

	iplug = item_plugin_by_coord( item );
	if( iplug -> common.can_contain_key != NULL )
		return iplug -> common.can_contain_key( item, key, data );
	else {
		assert( "nikita-1681", iplug -> common.max_key_inside != NULL );
		item_key_by_coord( item, &min_key_in_item );
		iplug -> common.max_key_inside( item, &max_key_in_item );
	
		/*
		 * can contain key if 
		 *    min_key_in_item <= key &&
		 *    key <= max_key_in_item
		 */
		return 
			keyle( &min_key_in_item, key ) &&
			keyle( key, &max_key_in_item );
	}
}

/* return 0 if @item1 and @item2 are not mergeable, !0 - otherwise */
/* Audited by: green(2002.06.14) */
int are_items_mergeable( const coord_t *i1 /* coord of first item */, 
			 const coord_t *i2 /* coord of second item */ )
{
	item_plugin *iplug;
	reiser4_key k1;
	reiser4_key k2;

	assert( "nikita-1336", i1 != NULL );
	assert( "nikita-1337", i2 != NULL );

	iplug = item_plugin_by_coord( i1 );
	assert( "nikita-1338", iplug != NULL );

	trace_if( TRACE_NODES, print_key( "k1", item_key_by_coord( i1, &k1 ) ) );
	trace_if( TRACE_NODES, print_key( "k2", item_key_by_coord( i2, &k2 ) ) );

	/*
	 * FIXME-NIKITA are_items_mergeable() is also called by assertions in
	 * shifting code when nodes are in "suspended" state.
	 */
	assert( "nikita-1663", 
		keyle( item_key_by_coord( i1, &k1 ), 
		       item_key_by_coord( i2, &k2 ) ) );

	if( iplug -> common.mergeable != NULL ) {
		return iplug -> common.mergeable( i1, i2 );
	} else if( iplug -> common.max_key_inside != NULL ) {
		iplug -> common.max_key_inside( i1, &k1 );
		item_key_by_coord( i2, &k2 );

		/*
		 * mergeable if ->max_key_inside() >= key of i2;
		 */
		return keyge( iplug -> common.max_key_inside( i1, &k1 ), 
			      item_key_by_coord( i2, &k2 ) );
	} else {
		item_key_by_coord( i1, &k1 );
		item_key_by_coord( i2, &k2 );

		return 
			( get_key_locality( &k1 ) == get_key_locality( &k2 ) ) &&
			( get_key_objectid( &k1 ) == get_key_objectid( &k2 ) ) &&
			( iplug == item_plugin_by_coord( i2 ) );
	}
}

/* Audited by: green(2002.06.14) */
int item_is_extent (const coord_t *item)
{
	assert ("vs-482", coord_is_existing_item (item));
	return item_id_by_coord (item) == EXTENT_POINTER_ID;
}

/* this returns true if item is of internal type */
/* Audited by: green(2002.06.14) */
int item_is_internal (const coord_t *item)
{
	assert ("vs-483", coord_is_existing_item (item));
	return item_type_by_coord (item) == INTERNAL_ITEM_TYPE;
}


/* Audited by: green(2002.06.14) */
int item_is_statdata (const coord_t *item)
{
	assert ("vs-516", coord_is_existing_item (item));
	return item_type_by_coord (item) == STAT_DATA_ITEM_TYPE;
}

reiser4_plugin item_plugins[ LAST_ITEM_ID ] = {
	[ STATIC_STAT_DATA_ID ] = {
		.item = {
			.h = {
				.type_id   = REISER4_ITEM_PLUGIN_TYPE,
				.id        = STATIC_STAT_DATA_ID,
				.pops      = NULL,
				.label     = "sd",
				.desc      = "stat-data",
				.linkage   = TS_LIST_LINK_ZERO
			},
			.common = {
				.item_type               = STAT_DATA_ITEM_TYPE,
				.max_key_inside          = single_key,
				.can_contain_key         = NULL,
				.mergeable               = NULL,
				.print                   = sd_print,
				.check                   = NULL,
				.nr_units                = single_unit,
				/* to need for ->lookup method */
				.lookup                  = NULL,
				.init                    = NULL,
				.paste                   = NULL,
				.fast_paste              = NULL,
				.can_shift               = NULL,
				.copy_units              = NULL,
				.create_hook             = NULL,
				.kill_hook               = NULL,
				.shift_hook              = NULL,
				.cut_units               = NULL,
				.kill_units              = NULL,
				.unit_key                = NULL,
				.estimate                = NULL,
				.item_data_by_flow       = NULL,
				.utmost_child            = NULL,
				.utmost_child_real_block = NULL,
				.real_max_key_inside     = NULL,
				.key_in_item             = NULL,
				.key_in_unit             = NULL,
				.item_stat               = sd_item_stat
			},
			.s = {
				.sd = {
					.init_inode = sd_load,
					.save_len   = sd_len,
					.save       = sd_save
				}
			}
		}
	},
	[ SIMPLE_DIR_ENTRY_ID ] = {
		.item = {
			.h = {
				.type_id = REISER4_ITEM_PLUGIN_TYPE,
				.id      = SIMPLE_DIR_ENTRY_ID,
				.pops    = NULL,
				.label   = "de",
				.desc    = "directory entry",
				.linkage = TS_LIST_LINK_ZERO
			},
			.common = {
				.item_type               = DIR_ENTRY_ITEM_TYPE,
				.max_key_inside          = single_key,
				.can_contain_key         = NULL,
				.mergeable               = NULL,
				.print                   = de_print,
				.check                   = NULL,
				.nr_units                = single_unit,
				/* to need for ->lookup method */
				.lookup                  = NULL,
				.init                    = NULL,
				.paste                   = NULL,
				.fast_paste              = NULL,
				.can_shift               = NULL,
				.copy_units              = NULL,
				.create_hook             = NULL,
				.kill_hook               = NULL,
				.shift_hook              = NULL,
				.cut_units               = NULL,
				.kill_units              = NULL,
				.unit_key                = NULL,
				.estimate                = NULL,
				.item_data_by_flow       = NULL,
				.utmost_child            = NULL,
				.utmost_child_real_block = NULL,
				.real_max_key_inside     = NULL,
				.key_in_item             = NULL,
				.key_in_unit             = NULL,
				.item_stat               = NULL
			},
			.s = {
				.dir = {
					.extract_key       = de_extract_key,
					.update_key        = de_update_key,
					.extract_name      = de_extract_name,
					.extract_file_type = de_extract_file_type,
					.add_entry         = de_add_entry,
					.rem_entry         = de_rem_entry,
					.max_name_len      = de_max_name_len
				}
			}
		}
	},
	[ COMPOUND_DIR_ID ] = {
		.item = {
			.h = {
				.type_id = REISER4_ITEM_PLUGIN_TYPE,
				.id      = COMPOUND_DIR_ID,
				.pops    = NULL,
				.label   = "cde",
				.desc    = "compressed directory entry",
				.linkage = TS_LIST_LINK_ZERO
			},
			.common = {
				.item_type               = DIR_ENTRY_ITEM_TYPE,
				.max_key_inside          = cde_max_key_inside,
				.can_contain_key         = cde_can_contain_key,
				.mergeable               = cde_mergeable,
				.print                   = cde_print,
				.check                   = cde_check,
				.nr_units                = cde_nr_units,
				.lookup                  = cde_lookup,
				.init                    = cde_init,
				.paste                   = cde_paste,
				.fast_paste              = agree_to_fast_op,
				.can_shift               = cde_can_shift,
				.copy_units              = cde_copy_units,
				.create_hook             = NULL,
				.kill_hook               = NULL,
				.shift_hook              = NULL,
				.cut_units               = cde_cut_units,
				.kill_units              = cde_cut_units,
				.unit_key                = cde_unit_key,
				.estimate                = cde_estimate,
				.item_data_by_flow       = NULL,
				.utmost_child            = NULL,
				.utmost_child_real_block = NULL,
				.real_max_key_inside     = NULL,
				.key_in_item             = NULL,
				.key_in_unit             = NULL,
				.item_stat               = NULL
			},
			.s = {
				.dir = {
					.extract_key       = cde_extract_key,
					.update_key        = cde_update_key,
					.extract_name      = cde_extract_name,
					.extract_file_type = de_extract_file_type,
					.add_entry         = cde_add_entry,
					.rem_entry         = de_rem_entry,
					.max_name_len      = cde_max_name_len
				}
			}
		}
	},
	[ NODE_POINTER_ID ] = {
		.item = {
			.h = {
				.type_id = REISER4_ITEM_PLUGIN_TYPE,
				.id      = NODE_POINTER_ID,
				.pops    = NULL,
				.label   = "internal",
				.desc    = "internal item",
				.linkage = TS_LIST_LINK_ZERO
			},
			.common = {
				.item_type               = INTERNAL_ITEM_TYPE,
				.max_key_inside          = NULL,
				.can_contain_key         = NULL,
				.mergeable               = internal_mergeable,
				.print                   = internal_print,
				.check                   = NULL,
				.nr_units                = single_unit,
				.lookup                  = internal_lookup,
				.init                    = NULL,
				.paste                   = NULL,
				.fast_paste              = NULL,
				.can_shift               = NULL,
				.copy_units              = NULL,
				.create_hook             = internal_create_hook,
				.kill_hook               = internal_kill_hook,
				.shift_hook              = internal_shift_hook,
				.cut_units               = NULL,
				.kill_units              = NULL,
				.unit_key                = NULL,
				.estimate                = NULL,
				.item_data_by_flow       = NULL,
				.utmost_child            = internal_utmost_child,
				.utmost_child_real_block = internal_utmost_child_real_block,
				.real_max_key_inside     = NULL,
				.key_in_item             = NULL,
				.key_in_unit             = NULL,
				.item_stat               = NULL
			},
			.s = {
				.internal = {
					.down_link      = internal_down_link,
					.has_pointer_to = internal_has_pointer_to
				}
			}
		}
	},
	[ EXTENT_POINTER_ID ] = {
		.item = {
			.h = {
				.type_id = REISER4_ITEM_PLUGIN_TYPE,
				.id      = EXTENT_POINTER_ID,
				.pops    = NULL,
				.label   = "extent",
				.desc    = "extent item",
				.linkage = TS_LIST_LINK_ZERO
			},
			.common = {
				.item_type               = ORDINARY_FILE_METADATA_TYPE,
				.max_key_inside          = extent_max_key_inside,
				.can_contain_key         = extent_can_contain_key,
				.mergeable               = extent_mergeable,
				.print                   = extent_print,
				.check                   = extent_check,
				.nr_units                = extent_nr_units,
				.lookup                  = extent_lookup,
				.init                    = NULL,
				.paste                   = extent_paste,
				.fast_paste              = agree_to_fast_op,
				.can_shift               = extent_can_shift,
				.create_hook             = extent_create_hook,
				.copy_units              = extent_copy_units,
				.kill_hook               = extent_kill_item_hook,
				.shift_hook              = NULL,
				.cut_units               = extent_cut_units,
				.kill_units              = extent_kill_units,
				.unit_key                = extent_unit_key,
				.estimate                = NULL,
				.item_data_by_flow       = NULL,
				.utmost_child            = extent_utmost_child,
				.utmost_child_real_block = extent_utmost_child_real_block,
				.real_max_key_inside     = extent_max_key,
				.key_in_item             = extent_key_in_item,
				.key_in_unit             = extent_key_in_unit,
				.item_stat               = extent_item_stat

			},
			.s = {
				.file = {
					.write     = extent_write,
					.read      = extent_read,
					.readpage  = extent_readpage,
					.writepage = extent_writepage,
					.page_cache_readahead = extent_page_cache_readahead,
				}
			}
		}
	},
	[ TAIL_ID ] = {
		.item = {
			.h = {
				.type_id = REISER4_ITEM_PLUGIN_TYPE,
				.id      = TAIL_ID,
				.pops    = NULL,
				.label   = "body",
				.desc    = "body (or tail?) item",
				.linkage = TS_LIST_LINK_ZERO
			},
			.common = {
				.item_type               = ORDINARY_FILE_METADATA_TYPE,
				.max_key_inside          = tail_max_key_inside,
				.can_contain_key         = tail_can_contain_key,
				.mergeable               = tail_mergeable,
				.print                   = NULL,
				.check                   = NULL,
				.nr_units                = tail_nr_units,
				.lookup                  = tail_lookup,
				.init                    = NULL,
				.paste                   = tail_paste,
				.fast_paste              = agree_to_fast_op,
				.can_shift               = tail_can_shift,
				.create_hook             = NULL,
				.copy_units              = tail_copy_units,
				.kill_hook               = NULL,
				.shift_hook              = NULL,
				.cut_units               = tail_cut_units,
				.kill_units              = tail_cut_units,
				.unit_key                = tail_unit_key,
				.estimate                = NULL,
				.item_data_by_flow       = NULL,
				.utmost_child            = NULL,
				.utmost_child_real_block = NULL,
				.real_max_key_inside     = tail_max_key,
				.key_in_item             = tail_key_in_item,
				.key_in_unit             = NULL,
				.item_stat               = NULL
			},
			.s = {
				.file = {
					.write     = tail_write,
					.read      = tail_read,
					.readpage  = NULL,
					.writepage = NULL,
					.page_cache_readahead = NULL
				}
			}
		}
	}
};



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
