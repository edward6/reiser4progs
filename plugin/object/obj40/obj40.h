/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
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

#define STAT_KEY(o) \
        (&((o)->info.start.key))

#define STAT_PLACE(o) \
        (&((o)->info.start))

typedef struct obj40 {
	/* Info about the object, stat data place, object and parent keys and
	   pointer to the instance of internal libreiser4 tree for modiying
	   purposes. It is passed by reiser4 library during initialization of
	   the file instance. Should be first field to be castable to the 
	   object_entity_t*/
	object_info_t info;

	/* Core operations pointer. */
	reiser4_core_t *core;
} obj40_t;

extern errno_t obj40_fini(obj40_t *obj);
extern errno_t obj40_update(obj40_t *obj);

extern oid_t obj40_objectid(obj40_t *obj);
extern oid_t obj40_locality(obj40_t *obj);
extern uint64_t obj40_ordering(obj40_t *obj);
extern uint64_t obj40_get_size(obj40_t *obj);

extern bool_t obj40_valid_item(reiser4_place_t *place);
extern errno_t obj40_fetch_item(reiser4_place_t *place);

extern int32_t obj40_belong(reiser4_place_t *place, 
			    reiser4_plug_t *plug, 
			    reiser4_key_t *key);

extern lookup_t obj40_find_item(obj40_t *obj, reiser4_key_t *key, 
				lookup_bias_t bias, coll_func_t func, 
				coll_hint_t *hint, reiser4_place_t *place);

extern int64_t obj40_read(obj40_t *obj, trans_hint_t *hint);

extern errno_t obj40_init(obj40_t *obj, object_info_t *info, 
				 reiser4_core_t *core);

extern errno_t obj40_read_ext(reiser4_place_t *place, rid_t id, void *data);

extern errno_t obj40_load_stat(obj40_t *obj, stat_hint_t *hint);

#ifndef ENABLE_MINIMAL
extern errno_t obj40_write_ext(reiser4_place_t *place, rid_t id, void *data);

extern errno_t obj40_touch(obj40_t *obj, uint64_t size, uint64_t bytes);

extern uint64_t obj40_extmask(reiser4_place_t *sd);
extern uint16_t obj40_get_mode(obj40_t *obj);
extern uint32_t obj40_get_nlink(obj40_t *obj);
extern uint32_t obj40_get_atime(obj40_t *obj);
extern uint32_t obj40_get_mtime(obj40_t *obj);
extern uint64_t obj40_get_bytes(obj40_t *obj);

extern errno_t obj40_clobber(obj40_t *obj);

extern errno_t obj40_link(obj40_t *obj);
extern errno_t obj40_unlink(obj40_t *obj);
extern uint32_t obj40_links(obj40_t *obj);

extern errno_t obj40_inc_link(obj40_t *obj, uint32_t value);
extern errno_t obj40_set_mode(obj40_t *obj, uint16_t mode);
extern errno_t obj40_set_size(obj40_t *obj, uint64_t size);
extern errno_t obj40_set_nlink(obj40_t *obj, uint32_t nlink);
extern errno_t obj40_set_atime(obj40_t *obj, uint32_t atime);
extern errno_t obj40_set_mtime(obj40_t *obj, uint32_t mtime);
extern errno_t obj40_set_bytes(obj40_t *obj, uint64_t bytes);

extern errno_t obj40_layout(obj40_t *obj,
			    region_func_t region_func,
			    void *data);

extern errno_t obj40_metadata(obj40_t *obj,
			      place_func_t place_func,
			      void *data);

extern errno_t obj40_remove(obj40_t *obj, reiser4_place_t *place,
			    trans_hint_t *hint);

extern int64_t obj40_insert(obj40_t *obj, reiser4_place_t *place,
			    trans_hint_t *hint, uint8_t level);

extern int64_t obj40_write(obj40_t *obj, trans_hint_t *hint);
extern int64_t obj40_convert(obj40_t *obj, conv_hint_t *hint);
extern int64_t obj40_truncate(obj40_t *obj, trans_hint_t *hint);

extern errno_t obj40_create_stat(obj40_t *obj, uint64_t size, uint64_t bytes, 
				 uint64_t rdev, uint32_t nlink, uint16_t mode, 
				 char *path);

#endif
#endif
