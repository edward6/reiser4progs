/*
  measure.h -- filesystem measurement related functions declaration.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef DEBUGFS_MEASURE_H
#define DEBUGFS_MEASURE_H

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include <reiser4/reiser4.h>

extern errno_t debugfs_tree_frag(reiser4_fs_t *fs);
extern errno_t debugfs_tree_stat(reiser4_fs_t *fs);

extern errno_t debugfs_data_frag(reiser4_fs_t *fs,
				 uint32_t flags);

extern errno_t debugfs_file_frag(reiser4_fs_t *fs,
				 char *filename);

#endif
