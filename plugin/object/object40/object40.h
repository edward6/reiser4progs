/*
  object40.h -- reiser4 file plugins common structures.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef OBJECT40_H
#define OBJECT40_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

struct object40 {

	/*
	  File plugin refference. Should be first field due to be castable to
	  object_entity_t
	*/
	reiser4_plugin_t *plugin;
    
	/* 
	  The key of stat data (or just first item if stat data doesn't exist)
	  for this object.
	*/
	key_entity_t key;

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

typedef struct object40 object40_t;

extern roid_t object40_objectid(object40_t *object);
extern roid_t object40_locality(object40_t *object);
extern errno_t object40_stat(object40_t *object);

extern uint16_t object40_get_mode(object40_t *object);
extern errno_t object40_set_mode(object40_t *object, uint16_t mode);

extern uint64_t object40_get_size(object40_t *object);
extern errno_t object40_set_size(object40_t *object, uint64_t size);

extern uint32_t object40_get_nlink(object40_t *object);
extern errno_t object40_set_nlink(object40_t *object, uint32_t nlink);

extern uint32_t object40_get_atime(object40_t *object);
extern errno_t object40_set_atime(object40_t *object, uint32_t atime);

extern uint32_t object40_get_mtime(object40_t *object);
extern errno_t object40_set_mtime(object40_t *object, uint32_t mtime);

extern errno_t object40_get_sym(object40_t *object, char *data);
extern errno_t object40_set_sym(object40_t *object, char *data);

extern errno_t object40_lock(object40_t *object, place_t *place);
extern errno_t object40_unlock(object40_t *object, place_t *place);

extern errno_t object40_link(object40_t *object, uint32_t value);

extern errno_t object40_init(object40_t *object, reiser4_plugin_t *plugin,
			     key_entity_t *key, reiser4_core_t *core,
			     void *tree);

extern errno_t object40_fini(object40_t *object);

extern lookup_t object40_lookup(object40_t *object, key_entity_t *key,
				uint8_t stop, place_t *place);

extern errno_t object40_insert(object40_t *object, reiser4_item_hint_t *hint,
			       uint8_t stop, place_t *place);

#endif
