/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key_common.h -- reiser4 common for all keys functions. */

#ifndef KEY_COMMON_H
#define KEY_COMMON_H

#include <aal/aal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>

typedef enum {
	/* File name key type */
	KEY_FILENAME_MINOR = 0,
    
	/* Stat-data key type */
	KEY_STATDATA_MINOR = 1,
    
	/* File attribute name */
	KEY_ATTRNAME_MINOR = 2,
    
	/* File attribute value */
	KEY_ATTRBODY_MINOR = 3,
    
	/* File body (tail or extent) */
	KEY_FILEBODY_MINOR = 4,
	KEY_LAST_MINOR
} key_minor_t;

extern key_minor_t key_common_type2minor(key_type_t type);
extern key_type_t key_common_minor2type(key_minor_t minor);
extern const char *key_common_minor2name(key_minor_t type);
extern key_entity_t *key_common_minimal(reiser4_plug_t *plug);
extern key_entity_t *key_common_maximal(reiser4_plug_t *plug);

#endif
