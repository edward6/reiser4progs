/*
 * Copyright 2001, 2002 by Hans Reiser, licensing governed by reiser4/README
 */

/*
 * Perm (short for "permissions") plugins common stuff.
 */

#if !defined( __REISER4_PERM_H__ )
#define __REISER4_PERM_H__


/**
 * interface for perm plugin.
 *
 * Perm plugin method can be implemented through:
 *
 *  1. consulting ->i_mode bits in stat data
 *
 *  2. obtaining acl from the tree and inspecting it
 *
 *  3. asking some kernel module or user-level program to authorize access.
 *
 * This allows for integration with things like capabilities, SELinux-style
 * secutiry contexts, etc.
 *
 */
typedef struct perm_plugin {
	/* generic fields */
	plugin_header           h;

	/** check permissions for read/write */
	int ( *rw_ok )( struct file *file, const char *buf, 
			size_t size, loff_t *off, rw_op op );

	/** check permissions for lookup */
	int ( *lookup_ok )( struct inode *parent, struct dentry *dentry );

	/** check permissions for create */
	int ( *create_ok )( struct inode *parent, struct dentry *dentry, 
			    reiser4_object_create_data *data );

	/** check permissions for linking @where to @existing */
	int ( *link_ok )( struct dentry *existing, struct inode *parent, 
			  struct dentry *where );

	/** check permissions for unlinking @victim from @parent */
	int ( *unlink_ok )( struct inode *parent, struct dentry *victim );

	/** check permissions for deletion of @object whose last reference is
	 * by @parent */
	int ( *delete_ok )( struct inode *parent, struct dentry *victim );
	int ( *mask_ok )( struct inode *inode, int mask );
	/** check whether attribute change is acceptable */
	int ( *setattr_ok )( struct dentry *dentry, struct iattr *attr );

	/** check whether stat(2) is allowed */
	int ( *getattr_ok )( struct vfsmount *mnt UNUSED_ARG,
			     struct dentry *dentry, struct kstat *stat );
} perm_plugin;

/** call ->check_ok method of perm plugin for inode */
#define perm_chk( inode, check, args... )		\
({							\
	perm_plugin *perm;				\
							\
	perm = inode_perm_plugin( inode );		\
	( ( perm != NULL ) &&				\
	  ( perm -> ## check ## _ok != NULL ) &&	\
	    perm -> ## check ## _ok( ##args ) );	\
})

typedef enum { RWX_PERM_ID, LAST_PERM_ID } reiser4_perm_id;

/* __REISER4_PERM_H__ */
#endif

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
