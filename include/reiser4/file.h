/*
  file.h -- reiser4 file functions.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef REISER4_FILE_H
#define REISER4_FILE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/types.h>

extern reiser4_file_t *reiser4_file_open(reiser4_fs_t *fs,
					 char *name);

extern reiser4_file_t *reiser4_file_begin(reiser4_fs_t *fs,
					  reiser4_place_t *place);

extern int32_t reiser4_file_read(reiser4_file_t *file, void *buff,
				 uint64_t n);

extern void reiser4_file_close(reiser4_file_t *file);
extern errno_t reiser4_file_stat(reiser4_file_t *file);

#ifndef ENABLE_ALONE

extern errno_t reiser4_file_print(reiser4_file_t *file,
				  aal_stream_t *stream);

extern reiser4_file_t *reiser4_file_create(reiser4_fs_t *fs,
					   reiser4_file_t *parent,
					   reiser4_file_hint_t *hint,
					   const char *name);

extern errno_t reiser4_file_nlink(reiser4_file_t *file);

extern int32_t reiser4_file_write(reiser4_file_t *file,
				  void *buff,
				  uint64_t n);

extern errno_t reiser4_file_truncate(reiser4_file_t *file,
				     uint64_t n);

extern errno_t reiser4_file_layout(reiser4_file_t *file,
				   block_func_t func,
				   void *data);

extern errno_t reiser4_file_metadata(reiser4_file_t *file,
				     place_func_t func,
				     void *data);
#endif

extern errno_t reiser4_file_reset(reiser4_file_t *object);
extern uint32_t reiser4_file_offset(reiser4_file_t *object);
extern errno_t reiser4_file_seek(reiser4_file_t *object,
				 uint32_t offset);

#endif

