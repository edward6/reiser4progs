/*
  file40.c -- reiser4 file plugins common code.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/stat.h>

#ifndef ENABLE_COMPACT
#  include <time.h>
#  include <unistd.h>
#endif

#include "file40.h"

roid_t file40_objectid(file40_t *file) {
	aal_assert("umka-839", file != NULL, return 0);
    
	return plugin_call(return 0, file->key.plugin->key_ops, 
			   get_objectid, &file->key);
}

roid_t file40_locality(file40_t *file) {
	aal_assert("umka-839", file != NULL, return 0);
    
	return plugin_call(return 0, file->key.plugin->key_ops, 
			   get_locality, &file->key);
}

errno_t file40_lock(file40_t *file, reiser4_place_t *place) {
	if (place->node)
		return file->core->tree_ops.lock(file->tree, place);

	return 0;
}

errno_t file40_unlock(file40_t *file, reiser4_place_t *place) {
	
	if (place->node)
		return file->core->tree_ops.unlock(file->tree, place);

	return 0;
}

/* Gets mode field from the stat data */
uint16_t file40_get_mode(file40_t *file) {
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;
	reiser4_sdext_lw_hint_t lw_hint;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.hint = &stat;
	stat.ext[SDEXT_LW_ID] = &lw_hint;

	item = &file->statdata.item;

	if (!item->plugin->item_ops.open)
		return 0;

	if (item->plugin->item_ops.open(item, &hint)) {
		aal_exception_error("Can't open statdata item.");
		return 0;
	}

	return lw_hint.mode;
}

errno_t file40_set_mode(file40_t *file, uint16_t mode) {
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;
	reiser4_sdext_lw_hint_t lw_hint;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.hint = &stat;

	stat.extmask = 1 << SDEXT_LW_ID;
	stat.ext[SDEXT_LW_ID] = &lw_hint;

	item = &file->statdata.item;

	if (!item->plugin->item_ops.open)
		return -1;

	if (item->plugin->item_ops.open(item, &hint)) {
		aal_exception_error("Can't open statdata item.");
		return -1;
	}

	lw_hint.mode = mode;
	
	if (!item->plugin->item_ops.insert)
		return -1;

	return item->plugin->item_ops.insert(item, &hint, 0);
}

/* Gets size field from the stat data */
uint64_t file40_get_size(file40_t *file) {
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;
	reiser4_sdext_lw_hint_t lw_hint;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));

	hint.hint = &stat;
	stat.ext[SDEXT_LW_ID] = &lw_hint;

	item = &file->statdata.item;

	if (!item->plugin->item_ops.open)
		return 0;

	if (item->plugin->item_ops.open(item, &hint)) {
		aal_exception_error("Can't open statdata item.");
		return 0;
	}
	
	return lw_hint.size;
}

/* Updates size field in the stat data */
errno_t file40_set_size(file40_t *file, uint64_t size) {
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;
	reiser4_sdext_lw_hint_t lw_hint;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.hint = &stat;

	stat.extmask = 1 << SDEXT_LW_ID;
	stat.ext[SDEXT_LW_ID] = &lw_hint;

	item = &file->statdata.item;

	if (!item->plugin->item_ops.open)
		return -1;

	if (item->plugin->item_ops.open(item, &hint)) {
		aal_exception_error("Can't open statdata item.");
		return -1;
	}

	lw_hint.size = size;
	
	if (!item->plugin->item_ops.insert)
		return -1;

	return item->plugin->item_ops.insert(item, &hint, 0);
}

/* Gets symlink from the stat data */
errno_t file40_get_symlink(file40_t *file, char *data) {
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.hint = &stat;
	stat.ext[SDEXT_SYMLINK_ID] = data;

	item = &file->statdata.item;

	if (!item->plugin->item_ops.open)
		return -1;

	if (item->plugin->item_ops.open(item, &hint)) {
		aal_exception_error("Can't open statdata item.");
		return -1;
	}

	return 0;
}

/* Updates symlink data */
errno_t file40_set_symlink(file40_t *file, char *data) {
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.hint = &stat;
	stat.ext[SDEXT_SYMLINK_ID] = data;

	item = &file->statdata.item;

	if (!item->plugin->item_ops.insert)
		return -1;

	if (item->plugin->item_ops.insert(item, &hint, 0)) {
		aal_exception_error("Can't update symlink.");
		return -1;
	}

	return 0;
}

errno_t file40_init(file40_t *file, reiser4_plugin_t *plugin,
		    key_entity_t *key, reiser4_core_t *core,
		    void *tree)
{
	aal_assert("umka-1574", file != NULL, return -1);
	aal_assert("umka-1756", plugin != NULL, return -1);
	aal_assert("umka-1757", tree != NULL, return -1);

	file->tree = tree;
	file->core = core;
	file->plugin = plugin;

	file->key.plugin = key->plugin;
	
	return plugin_call(return -1, key->plugin->key_ops, assign,
			   &file->key, key);
}

errno_t file40_realize(file40_t *file) {
	aal_assert("umka-857", file != NULL, return -1);	

	plugin_call(return -1, file->key.plugin->key_ops, build_generic, 
		    &file->key, KEY_STATDATA_TYPE, file40_locality(file), 
		    file40_objectid(file), 0);

	if (file->statdata.node)
		file40_unlock(file, &file->statdata);
	
	if (file->core->tree_ops.lookup(file->tree, &file->key, LEAF_LEVEL,
					&file->statdata) != PRESENT) 
	{
		aal_exception_error("Can't find stat data of file 0x%llx.", 
				    file40_objectid(file));

		if (file->statdata.node)
			file40_lock(file, &file->statdata);
		
		return -1;
	}
	
	file40_lock(file, &file->statdata);
	
	return 0;
}

/* Performs lookup and returns result to caller */
errno_t file40_lookup(file40_t *file, key_entity_t *key,
		      uint8_t stop, reiser4_place_t *place)
{
	return file->core->tree_ops.lookup(file->tree, key,
					   stop, place);
}

/*
  Inserts passed item hint into the tree. After function is finished, place
  contains the coord of the inserted item.
*/
errno_t file40_insert(file40_t *file, reiser4_item_hint_t *hint,
		      uint8_t stop, reiser4_place_t *place)
{
	roid_t objectid = file40_objectid(file);
	
	switch (file40_lookup(file, &hint->key, stop, place)) {
	case PRESENT:
		aal_exception_error("Key already exists in the tree.");
		return -1;
	case FAILED:
		aal_exception_error("Lookup is failed while trying to insert "
				    "new item into file 0x%llx.", objectid);
		return -1;
	}

	if (file->core->tree_ops.insert(file->tree, place, hint)) {
		aal_exception_error("Can't insert new item of file "
				    "0x%llx into the tree.", objectid);
		return -1;
	}

	return 0;
}
