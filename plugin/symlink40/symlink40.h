/*
  symlink40.h -- reiser4 symlink plugin structures.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef SYMLINK40_H
#define SYMLINK40_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

/* Compaund directory structure */
struct symlink40 {
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
};

typedef struct symlink40 symlink40_t;

#endif

