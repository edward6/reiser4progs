/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   object.h -- reiser4 common object functions (regular file, directory,
   etc). */

#ifndef REISER4_OBJECT_H
#define REISER4_OBJECT_H

#include <reiser4/types.h>

extern void reiser4_object_close(reiser4_object_t *object);
extern uint64_t reiser4_object_size(reiser4_object_t *object);
extern errno_t reiser4_object_refresh(reiser4_object_t *object);

extern errno_t reiser4_object_seek(reiser4_object_t *object,
				   uint32_t offset);

extern int64_t reiser4_object_read(reiser4_object_t *object,
				   void *buff, uint64_t n);

extern errno_t reiser4_object_readdir(reiser4_object_t *object,
				      entry_hint_t *entry);

extern errno_t reiser4_object_resolve(reiser4_object_t *object,
				      char *path, bool_t follow);

extern reiser4_object_t *reiser4_object_open(reiser4_tree_t *tree,
					     char *path, bool_t follow);

extern reiser4_object_t *reiser4_object_guess(reiser4_tree_t *tree, 
					      reiser4_object_t *parent,	
					      reiser4_key_t *object,
					      place_t *place,
					      object_init_t init_func);

extern reiser4_object_t *reiser4_object_realize(reiser4_tree_t *tree,
						reiser4_object_t *parent,
						place_t *place);

extern reiser4_object_t *reiser4_object_launch(reiser4_tree_t *tree,
					       reiser4_object_t *parent,
					       reiser4_key_t *key);

#ifndef ENABLE_STAND_ALONE
extern reiser4_object_t *reiser4_object_create(reiser4_tree_t *tree,
					       entry_hint_t *entry,
					       object_hint_t *hint);

extern errno_t reiser4_object_seekdir(reiser4_object_t *object,
				      reiser4_key_t *offset);

extern errno_t reiser4_object_telldir(reiser4_object_t *object,
				      reiser4_key_t *offset);

extern lookup_t reiser4_object_lookup(reiser4_object_t *object,
				      const char *name,
				      entry_hint_t *entry);

extern errno_t reiser4_object_stat(reiser4_object_t *object,
				   statdata_hint_t *hint);

extern errno_t reiser4_object_update(reiser4_object_t *object,
				     statdata_hint_t *hint);

extern errno_t reiser4_object_link(reiser4_object_t *object,
				   reiser4_object_t *child,
				   entry_hint_t *entry);

extern errno_t reiser4_object_unlink(reiser4_object_t *object,
				     entry_hint_t *entry);

extern uint32_t reiser4_object_links(reiser4_object_t *object);

extern int64_t reiser4_object_write(reiser4_object_t *object,
				    void *buff, uint64_t n);

extern errno_t reiser4_object_add_entry(reiser4_object_t *object,
					entry_hint_t *entry);

extern errno_t reiser4_object_rem_entry(reiser4_object_t *object,
					entry_hint_t *entry);

extern errno_t reiser4_object_truncate(reiser4_object_t *object,
				       uint64_t n);

extern errno_t reiser4_object_layout(reiser4_object_t *object,
				     region_func_t region_func,
				     void *data);

extern errno_t reiser4_object_metadata(reiser4_object_t *object,
				       place_func_t place_func,
				       void *data);

extern reiser4_object_t *reiser4_dir_create(reiser4_fs_t *fs,
					    reiser4_object_t *parent,
					    const char *name);

extern reiser4_object_t *reiser4_reg_create(reiser4_fs_t *fs,
					    reiser4_object_t *parent,
					    const char *name);

extern reiser4_object_t *reiser4_sym_create(reiser4_fs_t *fs,
					    reiser4_object_t *parent,
					    const char *name,
					    const char *target);

extern reiser4_object_t *reiser4_spl_create(reiser4_fs_t *fs,
					    reiser4_object_t *parent,
					    const char *name,
					    uint32_t mode,
					    uint64_t rdev);

extern errno_t reiser4_object_traverse(reiser4_object_t *object, 
				       object_open_func_t open_func,
				       void *data);
#endif

extern errno_t reiser4_object_reset(reiser4_object_t *object);
extern uint32_t reiser4_object_offset(reiser4_object_t *object);
#define object_start(object) ((place_t *)&(object)->info->start)

#endif
