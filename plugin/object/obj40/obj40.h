/*
  obj40.h -- reiser4 file plugins common structures.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef OBJ40_H
#define OBJ40_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

static reiser4_core_t *core = NULL;

#define STAT_KEY(o) \
        (&((o)->statdata.item.key))

#define STAT_ITEM(o) \
        (&((o)->statdata.item))

struct obj40 {

	/*
	  File plugin refference. Should be first field due to be castable to
	  object_entity_t
	*/
	reiser4_plugin_t *plugin;
    
	/* Stat data coord stored here */
	place_t statdata;

	/* Core operations pointer */
	reiser4_core_t *core;

	/* 
	  Pointer to the instance of internal libreiser4 tree, file opened on
	  stored here for lookup and modiying purposes. It is passed by reiser4
	  library durring initialization of the file instance.
	*/
	void *tree;
};

typedef struct obj40 obj40_t;

extern errno_t obj40_stat(obj40_t *obj);
extern oid_t obj40_objectid(obj40_t *obj);
extern oid_t obj40_locality(obj40_t *obj);
extern rid_t obj40_pid(item_entity_t *item);

extern errno_t obj40_init(obj40_t *obj, reiser4_plugin_t *plugin,
			  key_entity_t *key, reiser4_core_t *core,
			  void *tree);

extern lookup_t obj40_lookup(obj40_t *obj, key_entity_t *key,
			     uint8_t level, place_t *place);

extern errno_t obj40_fini(obj40_t *obj);

extern errno_t obj40_read_lw(item_entity_t *item,
			     sdext_lw_hint_t *lw_hint);

#ifdef ENABLE_SYMLINKS_SUPPORT
extern errno_t obj40_get_sym(obj40_t *obj, char *data);
#endif

#ifndef ENABLE_STAND_ALONE
extern errno_t obj40_write_lw(item_entity_t *item,
			      sdext_lw_hint_t *lw_hint);

extern errno_t obj40_read_unix(item_entity_t *item,
			       sdext_unix_hint_t *unix_hint);

extern errno_t obj40_write_unix(item_entity_t *item,
				sdext_unix_hint_t *unix_hint);

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

extern errno_t obj40_remove(obj40_t *obj, key_entity_t *key,
			    uint64_t count);

extern errno_t obj40_insert(obj40_t *obj, create_hint_t *hint,
			    uint8_t level, place_t *place);

typedef bool_t (*obj40_realize_func_t) (uint16_t);

extern errno_t obj40_realize(object_info_t *info, 
			     obj40_realize_func_t mode_func, 
			     obj40_realize_func_t type_func);
#endif

extern uint64_t obj40_get_size(obj40_t *obj);

extern void obj40_relock(obj40_t *obj, place_t *curr,
			 place_t *next);

extern errno_t obj40_lock(obj40_t *obj, place_t *place);
extern errno_t obj40_unlock(obj40_t *obj, place_t *place);

#endif
