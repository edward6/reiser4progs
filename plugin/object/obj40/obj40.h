/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   obj40.h -- reiser4 file plugins common structures. */

#ifndef OBJ40_H
#define OBJ40_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

static reiser4_core_t *core = NULL;

#define STAT_KEY(o) \
        (&((o)->info.start.key))

#define STAT_ITEM(o) \
        (&((o)->info.start))

struct obj40 {

	/* File plugin refference. Should be first field due to be castable to
	   object_entity_t */
	reiser4_plug_t *plug;
    
	/* Core operations pointer */
	reiser4_core_t *core;
	
	/* Info about the object, SD place, object and parent keys and 
	   pointer to the instance of internal libreiser4 tree also for 
	   modiying purposes. It is passed by reiser4 library durring 
	   initialization of the file instance. */
	object_info_t info;
};

typedef struct obj40 obj40_t;

extern oid_t obj40_objectid(obj40_t *obj);
extern oid_t obj40_locality(obj40_t *obj);
extern uint64_t obj40_ordering(obj40_t *obj);

extern errno_t obj40_stat(obj40_t *obj);
extern rid_t obj40_pid(place_t *place);

extern errno_t obj40_init(obj40_t *obj, reiser4_plug_t *plug,
			  reiser4_core_t *core, object_info_t *info);

extern lookup_t obj40_lookup(obj40_t *obj, key_entity_t *key,
			     uint8_t level, place_t *place);

extern errno_t obj40_fini(obj40_t *obj);

extern errno_t obj40_read_ext(place_t *place, rid_t id, void *data);

#ifndef ENABLE_STAND_ALONE
extern errno_t obj40_write_ext(place_t *place, rid_t id, void *data);
extern uint64_t obj40_extmask(place_t *sd);

extern uint16_t obj40_get_mode(obj40_t *obj);
extern uint32_t obj40_get_nlink(obj40_t *obj);
extern uint32_t obj40_get_atime(obj40_t *obj);
extern uint32_t obj40_get_mtime(obj40_t *obj);
extern uint64_t obj40_get_bytes(obj40_t *obj);

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

extern errno_t obj40_link(obj40_t *obj, uint32_t value);

extern errno_t obj40_insert(obj40_t *obj, create_hint_t *hint,
			    uint8_t level, place_t *place);

extern errno_t obj40_remove(obj40_t *obj, place_t *place,
			    uint32_t count);

typedef errno_t (*realize_func_t) (place_t *);
typedef errno_t (*realize_key_func_t) (obj40_t *);

extern errno_t obj40_realize(obj40_t *obj, realize_func_t sd_func,
			     realize_key_func_t key_func, uint64_t types);

errno_t obj40_check_sd(place_t *sd);

#endif

extern uint64_t obj40_get_size(obj40_t *obj);
#endif
