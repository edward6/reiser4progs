/*
 * Copyright 2001, 2002 by Hans Reiser, licensing governed by reiser4/README
 */

/*
 * Handling of "pseudo" files representing unified access to meta data in
 * reiser4.
 */

/*
 * See http://namesys.com/v4/v4.html, and especially
 * http://namesys.com/v4/v4.html#syscall for basic information about reiser4
 * pseudo files, access to meta-data, reiser4() system call, etc.
 *
 * Pseudo files should be accessible from both reiser4() system call and
 * normal POSIX calls.
 *
 * IMPLEMENTATION
 *
 *   When assigning inode numbers to pseudo files, we use the last 2^62 oids
 *   of the 64 oid space.
 *
 *   We implement each type of pseudo file ("..key", "..foo") as new object plugin in
 *   addition to the standard object plugins (regular file, directory, pipe,
 *   and so on).
 *
 *   Pseudo file inodes require a pointer to the "host" object inode.
 *
 *   Adding yet another field to the generic reiser4_inode that will
 *   only be used for the pseudo file seems to be an excess. Moreover, each pseudo file type can
 *   require its own additional state, which is hard to predict in advance,
 *   and ability to freely add new pseudo file types with rich and widely different
 *   semantics seems to be important for the reiser4.
 *
 *   This difficulty can be solved by observing that some fields in reiser4
 *   private part of inode are meaningless for pseudo files. Examples are: tail, hash,
 *   and stat-data plugin pointers, etc.
 *
 * PROPOSED SOLUTION
 *
 *   So, for now, we can reuse locality_id in reiser4 private part of inode to
 *   store number of host inode of pseudo file, and create special object plugin for
 *   each pseudo file type. 
 *
 *   That plugin will use some other field in reiser4 private
 *   inode part that is meaning less for pseudo files (->extmask?) to
 *   determine type of pseudo file and use appopriate low level pseudo file operations
 *   (pseudo_ops) to implement VFS operations.
 *
Use the pluginid field?

 *
 *   All this (hack-like) machinery has of course nothing to do with reiser4()
 *   system call that will use pseudo_ops directly.

 We do however need to define the methods that the reiser4 system call will use.

 *
 * NOTES
 *
 *  Special flag in inodes_plugins->flags to detect pseudo file.
(The plugin id?)
 *  Mark pseudo file inode as loaded: ->flags | REISER4_LOADED
 *
 */

#if YOU_CAN_COMPILE_PSEUDO_CODE
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
