/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   browse.h -- filesystem browse code.*/

#ifndef DEBUGFS_BROWSE_H
#define DEBUGFS_BROWSE_H

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

extern errno_t debugfs_browse(reiser4_fs_t *fs, char *filename);

#endif
