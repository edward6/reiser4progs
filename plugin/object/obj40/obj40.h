/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   obj40.h -- reiser4 file plugins common structures. */

#ifndef OBJ40_H
#define OBJ40_H

#include <sys/stat.h>

#ifndef ENABLE_MINIMAL
#  include <time.h>
#  include <unistd.h>
#endif

#include "reiser4/plugin.h"

extern reiser4_core_t *obj40_core;

#define STAT_PLACE(o) \
        (&((o)->info.start))

/* Returns the file position offset. */
static inline uint64_t obj40_offset(reiser4_object_t *obj) {
	return objcall(&obj->position, get_offset);
}

extern errno_t obj40_fini(reiser4_object_t *obj);
extern errno_t obj40_update(reiser4_object_t *obj);

extern uint64_t obj40_get_size(reiser4_object_t *obj);

extern bool_t obj40_valid_item(reiser4_place_t *place);
extern errno_t obj40_fetch_item(reiser4_place_t *place);

extern lookup_t obj40_belong(reiser4_place_t *place, reiser4_key_t *key);

extern lookup_t obj40_find_item(reiser4_object_t *obj, 
				reiser4_key_t *key, 
				lookup_bias_t bias, 
				coll_func_t func, 
				coll_hint_t *hint, 
				reiser4_place_t *place);

extern int64_t obj40_read(reiser4_object_t *obj, trans_hint_t *hint,
			  void *buff, uint64_t off, uint64_t count);

extern errno_t obj40_read_ext(reiser4_object_t *obj, rid_t id, void *data);

extern errno_t obj40_load_stat(reiser4_object_t *obj, stat_hint_t *hint);
extern errno_t obj40_save_stat(reiser4_object_t *obj, stat_hint_t *hint);

extern errno_t obj40_open(reiser4_object_t *obj);
extern errno_t obj40_seek(reiser4_object_t *obj, uint64_t offset);
extern errno_t obj40_reset(reiser4_object_t *obj);

typedef errno_t (*obj_func_t) (reiser4_object_t *, void *);

extern lookup_t obj40_update_body(reiser4_object_t *obj, 
				  obj_func_t adjust_func);

extern lookup_t obj40_next_item(reiser4_object_t *obj);

#ifndef ENABLE_MINIMAL
extern errno_t obj40_write_ext(reiser4_object_t *obj, rid_t id, void *data);

extern errno_t obj40_touch(reiser4_object_t *obj, int64_t size, int64_t bytes);

extern uint64_t obj40_extmask(reiser4_place_t *sd);
extern uint16_t obj40_get_mode(reiser4_object_t *obj);
extern int64_t  obj40_get_nlink(reiser4_object_t *obj, int update);
extern uint32_t obj40_get_atime(reiser4_object_t *obj);
extern uint32_t obj40_get_mtime(reiser4_object_t *obj);
extern uint64_t obj40_get_bytes(reiser4_object_t *obj);

extern errno_t obj40_clobber(reiser4_object_t *obj);

extern errno_t obj40_link(reiser4_object_t *obj);
extern errno_t obj40_unlink(reiser4_object_t *obj);
extern bool_t obj40_linked(reiser4_object_t *obj);

extern errno_t obj40_set_mode(reiser4_object_t *obj, uint16_t mode);
extern errno_t obj40_set_size(reiser4_object_t *obj, uint64_t size);
extern errno_t obj40_set_nlink(reiser4_object_t *obj, uint32_t nlink);
extern errno_t obj40_set_atime(reiser4_object_t *obj, uint32_t atime);
extern errno_t obj40_set_mtime(reiser4_object_t *obj, uint32_t mtime);
extern errno_t obj40_set_bytes(reiser4_object_t *obj, uint64_t bytes);

extern errno_t obj40_layout(reiser4_object_t *obj,
			    region_func_t region_func,
			    obj_func_t obj_func,
			    void *data);

extern errno_t obj40_metadata(reiser4_object_t *obj,
			      place_func_t place_func,
			      void *data);

extern errno_t obj40_traverse(reiser4_object_t *obj, 
			      place_func_t place_func, 
			      obj_func_t obj_func,
			      void *data);

extern errno_t obj40_remove(reiser4_object_t *obj, 
			    reiser4_place_t *place,
			    trans_hint_t *hint);

extern int64_t obj40_insert(reiser4_object_t *obj, 
			    reiser4_place_t *place,
			    trans_hint_t *hint, 
			    uint8_t level);

extern int64_t obj40_write(reiser4_object_t *obj, 
			   trans_hint_t *hint,
			   void *buff,
			   uint64_t off, 
			   uint64_t count, 
			   reiser4_item_plug_t *item_plug, 
			   place_func_t func,
			   void *data);

extern int64_t obj40_convert(reiser4_object_t *obj, conv_hint_t *hint);

extern int64_t obj40_cut(reiser4_object_t *obj, 
			 trans_hint_t *hint, 
			 uint64_t off, uint64_t count,
			 region_func_t func, void *data);

extern int64_t obj40_truncate(reiser4_object_t *obj, uint64_t n, 
			      reiser4_item_plug_t *item_plug);

extern errno_t obj40_stat_unix_init(stat_hint_t *stat, 
				    sdhint_unix_t *unixh, 
				    uint64_t bytes, 
				    uint64_t rdev);

extern errno_t obj40_stat_lw_init(reiser4_object_t *obj, 
				  stat_hint_t *stat, 
				  sdhint_lw_t *lwh, 
				  uint64_t size,  
				  uint32_t nlink, 
				  uint16_t mode);


extern errno_t obj40_inherit(object_info_t *info, object_info_t *parent);

extern errno_t obj40_create(reiser4_object_t *obj, object_hint_t *hint);

extern errno_t obj40_create_stat(reiser4_object_t *obj, 
				 uint64_t size, uint64_t bytes, 
				 uint64_t rdev, uint32_t nlink, 
				 uint16_t mode, char *path);

#endif
#endif
