/*
  object.h -- reiser4 common object functions (regular file, directory, etc).
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef REISER4_OBJECT_H
#define REISER4_OBJECT_H

#include <reiser4/types.h>

extern reiser4_object_t *reiser4_object_open(reiser4_fs_t *fs,
					     char *filename,
					     bool_t follow);

extern int32_t reiser4_object_read(reiser4_object_t *object,
				   void *buff, uint32_t n);

extern errno_t reiser4_object_readdir(reiser4_object_t *object,
				      entry_hint_t *entry);

extern void reiser4_object_close(reiser4_object_t *object);
extern errno_t reiser4_object_stat(reiser4_object_t *object);
extern uint64_t reiser4_object_size(reiser4_object_t *object);

extern errno_t reiser4_object_seek(reiser4_object_t *object,
				   uint32_t offset);

#ifndef ENABLE_STAND_ALONE

extern errno_t reiser4_object_seekdir(reiser4_object_t *object,
				      reiser4_key_t *offset);

extern errno_t reiser4_object_telldir(reiser4_object_t *object,
				      reiser4_key_t *offset);

extern lookup_t reiser4_object_lookup(reiser4_object_t *object,
				      const char *name,
				      entry_hint_t *entry);

extern errno_t reiser4_object_seek(reiser4_object_t *object,
				   uint32_t offset);

extern reiser4_object_t *reiser4_object_access(reiser4_fs_t *fs,
					       reiser4_place_t *place);

extern reiser4_object_t *reiser4_object_create(reiser4_fs_t *fs,
					       reiser4_object_t *parent,
					       object_hint_t *hint);

extern errno_t reiser4_object_print(reiser4_object_t *object,
				    aal_stream_t *stream);

extern errno_t reiser4_object_link(reiser4_object_t *object,
				   reiser4_object_t *child,
				   const char *name);

extern errno_t reiser4_object_unlink(reiser4_object_t *object,
				     const char *name);

extern int32_t reiser4_object_write(reiser4_object_t *object,
				    void *buff, uint64_t n);

extern errno_t reiser4_object_add_entry(reiser4_object_t *object,
					entry_hint_t *entry);

extern errno_t reiser4_object_rem_entry(reiser4_object_t *object,
					entry_hint_t *entry);

extern errno_t reiser4_object_truncate(reiser4_object_t *object,
				       uint64_t n);

extern errno_t reiser4_object_layout(reiser4_object_t *object,
				     block_func_t func, void *data);

extern errno_t reiser4_object_metadata(reiser4_object_t *object,
				       place_func_t func, void *data);
#endif

extern errno_t reiser4_object_reset(reiser4_object_t *object);

extern uint32_t reiser4_object_offset(reiser4_object_t *object);

#endif
