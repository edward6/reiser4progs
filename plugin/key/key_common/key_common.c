/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key_common.c -- reiser4 common for all keys functions. */

#include "key_common.h"

/* Minimal possible key */
static reiser4_key_t minimal_key = {
	.plug = NULL,
	.body = {0ull, 0ull, 0ull, 0ull},
#ifndef ENABLE_MINIMAL
	.adjust = 0
#endif
};

/* Maximal possible key */
static reiser4_key_t maximal_key = {
	.plug = NULL,
	.body = {MAX_UINT64, MAX_UINT64, MAX_UINT64, MAX_UINT64},
#ifndef ENABLE_MINIMAL
	.adjust = 0
#endif
};

/* Translates key type from libreiser4 type to key_common one */
key_minor_t key_common_type2minor(key_type_t type) {
	switch (type) {
	case KEY_FILENAME_TYPE:
		return KEY_FILENAME_MINOR;
	case KEY_STATDATA_TYPE:
		return KEY_STATDATA_MINOR;
	case KEY_ATTRNAME_TYPE:
		return KEY_ATTRNAME_MINOR;
	case KEY_ATTRBODY_TYPE:
		return KEY_ATTRBODY_MINOR;
	case KEY_FILEBODY_TYPE:
		return KEY_FILEBODY_MINOR;
	default:
		return MAX_UINT8;
	}
}

/* Translates key type from key_common to libreiser4 one */
key_type_t key_common_minor2type(key_minor_t minor) {
	switch (minor) {
	case KEY_FILENAME_MINOR:
		return KEY_FILENAME_TYPE;
	case KEY_STATDATA_MINOR:
		return KEY_STATDATA_TYPE;
	case KEY_ATTRNAME_MINOR:
		return KEY_ATTRNAME_TYPE;
	case KEY_ATTRBODY_MINOR:
		return KEY_ATTRBODY_TYPE;
	case KEY_FILEBODY_MINOR:
		return KEY_FILEBODY_TYPE;
	default:
		return MAX_UINT8;
	}
}

/* Key minor names. They are used key_print() function */
static const char *const minor_names[] = {
	"NAME", "SD", "AN", "AB", "FB", "?"
};

/* Translates passed minor into corresponding name */
const char *key_common_minor2name(key_minor_t type) {
	if (type > KEY_LAST_MINOR)
		type = KEY_LAST_MINOR;
    
	return minor_names[type];
}

/* Returns minimal key */
reiser4_key_t *key_common_minimal(reiser4_plug_t *plug) {
	minimal_key.plug = plug;
	return &minimal_key;
}

/* Returns maximal key */
reiser4_key_t *key_common_maximal(reiser4_plug_t *plug) {
	maximal_key.plug = plug;
	return &maximal_key;
}

