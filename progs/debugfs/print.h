/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   print.h -- filesystem print related functions declaration. */

#ifndef DEBUGFS_PRINT_H
#define DEBUGFS_PRINT_H

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include <reiser4/libreiser4.h>

extern errno_t debugfs_print_file(reiser4_fs_t *fs,
				  char *filename,
				  uint32_t flags);

extern errno_t debugfs_print_block(reiser4_fs_t *fs,
				   blk_t blk);

extern void debugfs_print_oid(reiser4_fs_t *fs);
extern void debugfs_print_tree(reiser4_fs_t *fs);
extern void debugfs_print_alloc(reiser4_fs_t *fs);
extern void debugfs_print_master(reiser4_fs_t *fs);
extern void debugfs_print_status(reiser4_fs_t *fs);
extern void debugfs_print_format(reiser4_fs_t *fs);
extern void debugfs_print_journal(reiser4_fs_t *fs);
extern void debugfs_print_node(reiser4_node_t *node);

extern errno_t debugfs_print_stream(aal_stream_t *stream);
#endif
