/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key_common.c -- reiser4 common for all keys functions. */

#include "key_common.h"

/* Minimal possible key */
static key_entity_t minimal_key = {
	.plugin = NULL,
	.body   = {0ull, 0ull, 0ull, 0ull}
};

/* Maximal possible key */
static key_entity_t maximal_key = {
	.plugin = NULL,
	.body   = {~0ull, ~0ull, ~0ull, ~0ull}
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
		return 0xff;
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
		return 0xff;
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
key_entity_t *key_common_minimal(reiser4_plugin_t *plugin) {
	minimal_key.plugin = plugin;
	return &minimal_key;
}

/* Returns maximal key */
key_entity_t *key_common_maximal(reiser4_plugin_t *plugin) {
	maximal_key.plugin = plugin;
	return &maximal_key;
}

