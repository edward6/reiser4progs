/*
  dir40.h -- reiser4 hashed directory plugin structures.

  Copyright (C) 2001, 2002 by Hans Reiser, licencing governed by
  reiser4progs/COPYING.
*/

#ifndef DIR40_H
#define DIR40_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

/* Compaund directory structure */
struct dir40 {
	reiser4_plugin_t *plugin;
    
	/* 
	   Poiter to the instance of internal libreiser4 tree, dir opened on
	   stored here for lookup and modiying purposes. It is passed by reiser4
	   library durring initialization of the directory instance.
	*/
	const void *tree;

	/* 
	   The key of stat data (or just first item if stat data doesn't exists) 
	   for this directory.
	*/
	reiser4_key_t key;

	/* Stat data item coord */
	reiser4_place_t statdata;

	/* Current body item coord */
	reiser4_place_t body;

	/* Current position in the directory */
	uint32_t offset;

	/* Hash plugin in use */
	reiser4_plugin_t *hash;
};

typedef struct dir40 dir40_t;

#endif

