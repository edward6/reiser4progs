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

extern roid_t object40_objectid(object40_t *file);
extern roid_t object40_locality(object40_t *file);
extern errno_t object40_stat(object40_t *file);

extern uint16_t object40_get_mode(object40_t *file);
extern errno_t object40_set_mode(object40_t *file, uint16_t mode);

extern uint64_t object40_get_size(object40_t *file);
extern errno_t object40_set_size(object40_t *file, uint64_t size);

extern errno_t object40_get_sym(object40_t *file, char *data);
extern errno_t object40_set_sym(object40_t *file, char *data);

extern errno_t object40_lock(object40_t *file, place_t *place);
extern errno_t object40_unlock(object40_t *file, place_t *place);

extern errno_t object40_init(object40_t *file, reiser4_plugin_t *plugin,
			     key_entity_t *key, reiser4_core_t *core,
			     void *tree);

extern errno_t object40_fini(object40_t *file);

extern errno_t object40_lookup(object40_t *file, key_entity_t *key,
			       uint8_t stop, place_t *place);

extern errno_t object40_insert(object40_t *file, reiser4_item_hint_t *hint,
			       uint8_t stop, place_t *place);

#endif
