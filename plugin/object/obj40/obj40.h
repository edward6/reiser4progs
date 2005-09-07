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

extern errno_t obj40_fini(reiser4_object_t *obj);
extern errno_t obj40_update(reiser4_object_t *obj);

extern oid_t obj40_objectid(reiser4_object_t *obj);
extern oid_t obj40_locality(reiser4_object_t *obj);
extern uint64_t obj40_ordering(reiser4_object_t *obj);
extern uint64_t obj40_get_size(reiser4_object_t *obj);

extern bool_t obj40_valid_item(reiser4_place_t *place);
extern errno_t obj40_fetch_item(reiser4_place_t *place);

extern int32_t obj40_belong(reiser4_place_t *place, 
			    reiser4_plug_t *plug, 
			    reiser4_key_t *key);

extern lookup_t obj40_find_item(reiser4_object_t *obj, 
				reiser4_key_t *key, 
				lookup_bias_t bias, 
				coll_func_t func, 
				coll_hint_t *hint, 
				reiser4_place_t *place);

extern int64_t obj40_read(reiser4_object_t *obj, trans_hint_t *hint);

extern errno_t obj40_read_ext(reiser4_object_t *obj, rid_t id, void *data);

extern errno_t obj40_load_stat(reiser4_object_t *obj, stat_hint_t *hint);
extern errno_t obj40_save_stat(reiser4_object_t *obj, stat_hint_t *hint);

extern errno_t obj40_open(reiser4_object_t *obj);
extern errno_t obj40_seek(reiser4_object_t *obj, uint64_t offset);
extern uint64_t obj40_size(reiser4_object_t *obj);
extern errno_t obj40_reset(reiser4_object_t *obj);
extern uint64_t obj40_offset(reiser4_object_t *obj);
extern errno_t obj40_create(reiser4_object_t *obj, object_hint_t *hint);

#ifndef ENABLE_MINIMAL
extern errno_t obj40_write_ext(reiser4_place_t *place, rid_t id, void *data);

extern errno_t obj40_touch(reiser4_object_t *obj, uint64_t size, uint64_t bytes);

extern uint64_t obj40_extmask(reiser4_place_t *sd);
extern uint16_t obj40_get_mode(reiser4_object_t *obj);
extern uint32_t obj40_get_nlink(reiser4_object_t *obj);
extern uint32_t obj40_get_atime(reiser4_object_t *obj);
extern uint32_t obj40_get_mtime(reiser4_object_t *obj);
extern uint64_t obj40_get_bytes(reiser4_object_t *obj);

extern errno_t obj40_clobber(reiser4_object_t *obj);

extern errno_t obj40_link(reiser4_object_t *obj);
extern errno_t obj40_unlink(reiser4_object_t *obj);
extern uint32_t obj40_links(reiser4_object_t *obj);
extern bool_t obj40_linked(reiser4_object_t *obj);

extern errno_t obj40_inc_link(reiser4_object_t *obj, uint32_t value);
extern errno_t obj40_set_mode(reiser4_object_t *obj, uint16_t mode);
extern errno_t obj40_set_size(reiser4_object_t *obj, uint64_t size);
extern errno_t obj40_set_nlink(reiser4_object_t *obj, uint32_t nlink);
extern errno_t obj40_set_atime(reiser4_object_t *obj, uint32_t atime);
extern errno_t obj40_set_mtime(reiser4_object_t *obj, uint32_t mtime);
extern errno_t obj40_set_bytes(reiser4_object_t *obj, uint64_t bytes);

extern errno_t obj40_layout(reiser4_object_t *obj,
			    region_func_t region_func,
			    void *data);

extern errno_t obj40_metadata(reiser4_object_t *obj,
			      place_func_t place_func,
			      void *data);

extern errno_t obj40_remove(reiser4_object_t *obj, 
			    reiser4_place_t *place,
			    trans_hint_t *hint);

extern int64_t obj40_insert(reiser4_object_t *obj, 
			    reiser4_place_t *place,
			    trans_hint_t *hint, 
			    uint8_t level);

extern int64_t obj40_write(reiser4_object_t *obj, trans_hint_t *hint);
extern int64_t obj40_convert(reiser4_object_t *obj, conv_hint_t *hint);
extern int64_t obj40_truncate(reiser4_object_t *obj, trans_hint_t *hint);

extern errno_t obj40_create_stat(reiser4_object_t *obj, 
				 uint64_t size, uint64_t bytes, 
				 uint64_t rdev, uint32_t nlink, 
				 uint16_t mode, char *path);

#endif
#endif
