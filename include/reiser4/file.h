/*
  file.h -- reiser4 file functions.
    
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef OBJECT_H
#define OBJECT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

extern reiser4_file_t *reiser4_file_open(reiser4_fs_t *fs, const char *name);
extern reiser4_file_t *reiser4_file_begin(reiser4_fs_t *fs, reiser4_coord_t *coord);
extern errno_t reiser4_file_read(reiser4_file_t *file, void *buff, uint64_t n);
extern void reiser4_file_close(reiser4_file_t *file);

#ifndef ENABLE_COMPACT

extern reiser4_file_t *reiser4_file_create(reiser4_fs_t *fs, reiser4_file_hint_t *hint,
					   reiser4_file_t *parent,  const char *name);

extern errno_t reiser4_file_write(reiser4_file_t *file, void *buff, uint64_t n);
extern errno_t reiser4_file_truncate(reiser4_file_t *file, uint64_t n);

extern errno_t reiser4_file_layout(reiser4_file_t *file, file_action_func_t func,
				   void *data);

#endif

extern errno_t reiser4_file_reset(reiser4_file_t *object);
extern uint32_t reiser4_file_offset(reiser4_file_t *object);
extern errno_t reiser4_file_seek(reiser4_file_t *object, uint32_t offset);

#endif

