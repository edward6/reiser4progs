/*
 * Copyright 2001, 2002 by Hans Reiser, licensing governed by reiser4/README
 */

/*
 * definition of item plugins.
 */

#include "../../reiser4.h"

static int common_setattr_ok( struct dentry *dentry, struct iattr *attr )
{
	int result;
	struct inode *inode;

	assert( "nikita-2272", dentry != NULL );
	assert( "nikita-2273", attr != NULL );

	inode = dentry -> d_inode;
	assert( "nikita-2274", inode != NULL );

	result = inode_change_ok( inode, attr );
	if( result == 0 ) {
		unsigned int valid;

		valid = attr->ia_valid;
		if( ( valid & ATTR_UID && attr -> ia_uid != inode -> i_uid ) ||
		    ( valid & ATTR_GID && attr -> ia_gid != inode -> i_gid ) )
			result = DQUOT_TRANSFER( inode, attr ) ? -EDQUOT : 0;
	}
	return result;
}

perm_plugin perm_plugins[ LAST_PERM_ID ] = {
	[ RWX_PERM_ID ] = {
		.h = {
			.type_id = REISER4_PERM_PLUGIN_TYPE,
			.id      = RWX_PERM_ID,
			.pops    = NULL,
			.label   = "rwx",
			.desc    = "standard UNIX permissions",
			.linkage = TS_LIST_LINK_ZERO,
		},
		.rw_ok      = NULL,
		.lookup_ok  = NULL,
		.create_ok  = NULL,
		.link_ok    = NULL,
		.unlink_ok  = NULL,
		.delete_ok  = NULL,
		/* smart thing */
		.mask_ok    = vfs_permission,
		.setattr_ok = common_setattr_ok,
		.getattr_ok = NULL,
		.rename_ok  = NULL
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
