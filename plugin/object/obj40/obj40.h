/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   obj40.h -- reiser4 file plugins common structures. */

#ifndef OBJ40_H
#define OBJ40_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

#define STAT_KEY(o) \
        (&((o)->info.start.key))

#define STAT_PLACE(o) \
        (&((o)->info.start))

struct obj40 {
	/* File plugin refference. Should be first field due to be castable to
	   object_entity_t */
	reiser4_plug_t *plug;
    
	/* Info about the object, stat data place, object and parent keys and
	   pointer to the instance of internal libreiser4 tree for modiying
	   purposes. It is passed by reiser4 library durring initialization of
	   the file instance. */
	object_info_t info;

	/* Core operations pointer. */
	reiser4_core_t *core;
};

typedef struct obj40 obj40_t;

extern errno_t obj40_fetch(obj40_t *obj,
			   place_t *place);

extern errno_t obj40_fini(obj40_t *obj);
extern errno_t obj40_update(obj40_t *obj);

extern oid_t obj40_objectid(obj40_t *obj);
extern oid_t obj40_locality(obj40_t *obj);
extern uint64_t obj40_ordering(obj40_t *obj);
extern uint64_t obj40_get_size(obj40_t *obj);

extern reiser4_plug_t *obj40_plug(obj40_t *obj, rid_t type,
				  char *name);

extern rid_t obj40_pid(obj40_t *obj, rid_t type, char *name);

extern lookup_res_t obj40_lookup(obj40_t *obj, key_entity_t *key,
				 uint8_t level, lookup_mod_t mode,
				 place_t *place);

extern errno_t obj40_init(obj40_t *obj, reiser4_plug_t *plug,
			  reiser4_core_t *core, object_info_t *info);

extern errno_t obj40_read_ext(place_t *place, rid_t id, void *data);

#ifndef ENABLE_STAND_ALONE
typedef errno_t (*key_func_t) (obj40_t *);
typedef errno_t (*stat_func_t) (place_t *);

typedef void (*mode_func_t) (obj40_t *, uint16_t *);
typedef void (*nlink_func_t) (obj40_t *, uint32_t *);
typedef void (*size_func_t) (obj40_t *, uint64_t *, uint64_t);

extern errno_t obj40_write_ext(place_t *place,
			       rid_t id, void *data);

extern errno_t obj40_touch(obj40_t *obj, uint64_t size,
			   uint64_t bytes, uint32_t atime);

extern errno_t obj40_create_stat(obj40_t *obj, rid_t pid,
				 uint64_t mask, uint64_t size,
				 uint64_t bytes, uint32_t nlink,
				 uint16_t mode, char *path);

extern uint64_t obj40_extmask(place_t *sd);
extern uint16_t obj40_get_mode(obj40_t *obj);
extern uint32_t obj40_get_nlink(obj40_t *obj);
extern uint32_t obj40_get_atime(obj40_t *obj);
extern uint32_t obj40_get_mtime(obj40_t *obj);
extern uint64_t obj40_get_bytes(obj40_t *obj);

extern errno_t obj40_link(obj40_t *obj,
			  uint32_t value);

extern errno_t obj40_set_mode(obj40_t *obj,
			      uint16_t mode);

extern errno_t obj40_set_size(obj40_t *obj,
			      uint64_t size);

extern errno_t obj40_set_nlink(obj40_t *obj,
			       uint32_t nlink);

extern errno_t obj40_set_atime(obj40_t *obj,
			       uint32_t atime);

extern errno_t obj40_set_mtime(obj40_t *obj,
			       uint32_t mtime);

extern errno_t obj40_set_bytes(obj40_t *obj,
			       uint64_t bytes);

extern errno_t obj40_recognize(obj40_t *obj,
			       stat_func_t stat_func);

extern errno_t obj40_remove(obj40_t *obj, place_t *place,
			    trans_hint_t *hint);

extern errno_t obj40_insert(obj40_t *obj, place_t *place,
			    trans_hint_t *hint, uint8_t level);

extern int32_t obj40_write(obj40_t *obj, place_t *place,
			   trans_hint_t *hint, uint8_t level);

extern errno_t obj40_ukey(obj40_t *obj, place_t *place, 
			  key_entity_t *key, uint8_t mode);

extern errno_t obj40_check_stat(obj40_t *obj, nlink_func_t nlink_func,
				mode_func_t mode_func, size_func_t size_func,
				uint64_t size, uint64_t bytes, uint8_t mode);

extern errno_t obj40_stat_launch(obj40_t *obj, stat_func_t stat_func, 
				 uint64_t mask, uint32_t nlink, 
				 uint16_t objmode, uint8_t mode);

extern reiser4_plug_t *obj40_plug_recognize(obj40_t *obj, 
					    rid_t type, 
					    char *name);

#endif
#endif
