/*
  file40.h -- reiser4 file plugins common structures.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef FILE40_H
#define FILE40_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

/* Compaund directory structure */
struct file40 {

	/*
	  File plugin refference. Shoudl be first field due to be castable to
	  object_entity_t
	*/
	reiser4_plugin_t *plugin;
    
	/* 
	   Poiter to the instance of internal libreiser4 tree, file opened on
	   stored here for lookup and modiying purposes. It is passed by reiser4
	   library durring initialization of the fileinstance.
	*/
	const void *tree;

	/* 
	   The key of stat data (or just first item if stat data doesn't exists)
	   for this directory.
	*/
	reiser4_key_t key;

	/* Stat data coord stored here */
	reiser4_place_t statdata;

	/* Core operations pointer */
	reiser4_core_t *core;
};

typedef struct file40 file40_t;

extern roid_t file40_objectid(file40_t *file);
extern roid_t file40_locality(file40_t *file);
extern errno_t file40_realize(file40_t *file);

extern errno_t file40_lock(file40_t *file, reiser4_place_t *place);
extern errno_t file40_unlock(file40_t *file, reiser4_place_t *place);
extern errno_t file40_get_mode(reiser4_place_t *place, uint16_t *mode);
extern errno_t file40_set_mode(reiser4_place_t *place, uint16_t mode);
extern errno_t file40_get_size(reiser4_place_t *place,uint64_t *size);
extern errno_t file40_set_size(reiser4_place_t *place, uint64_t size);

extern errno_t file40_init(file40_t *file, reiser4_key_t *key,
			   reiser4_plugin_t *plugin, const void *tree,
			   reiser4_core_t *core);

extern errno_t file40_insert(file40_t *file, reiser4_item_hint_t *hint,
			 reiser4_level_t *stop, reiser4_place_t *place);

#endif
