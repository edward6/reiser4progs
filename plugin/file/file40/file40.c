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
			   get_objectid, file->key.body);
}

roid_t file40_locality(file40_t *file) {
	aal_assert("umka-839", file != NULL, return 0);
    
	return plugin_call(return 0, file->key.plugin->key_ops, 
			   get_locality, file->key.body);
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
uint16_t file40_get_mode(reiser4_place_t *place) {
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;
	reiser4_sdext_lw_hint_t lw_hint;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.hint = &stat;
	stat.ext[SDEXT_LW_ID] = &lw_hint;

	item = &place->entity;

	if (!item->plugin->item_ops.open)
		return 0;

	if (item->plugin->item_ops.open(item, &hint))
		return 0;

	return lw_hint.mode;
}

errno_t file40_set_mode(reiser4_place_t *place, uint16_t mode) {
	return -1;
}

/* Gets size field from the stat data */
uint64_t file40_get_size(reiser4_place_t *place) {
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;
	reiser4_sdext_lw_hint_t lw_hint;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));

	hint.hint = &stat;
	stat.ext[SDEXT_LW_ID] = &lw_hint;

	item = &place->entity;

	if (!item->plugin->item_ops.open)
		return 0;

	if (item->plugin->item_ops.open(item, &hint))
		return 0;
	
	return lw_hint.size;
}

/* Updates size field in the stat data */
errno_t file40_set_size(reiser4_place_t *place, uint64_t size) {
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;
	reiser4_sdext_lw_hint_t lw_hint;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.hint = &stat;

	stat.extmask = 1 << SDEXT_LW_ID;
	stat.ext[SDEXT_LW_ID] = &lw_hint;

	item = &place->entity;

	if (!item->plugin->item_ops.open)
		return -1;

	if (item->plugin->item_ops.open(item, &hint))
		return -1;

	lw_hint.size = size;
	
	if (!item->plugin->item_ops.insert)
		return -1;

	return item->plugin->item_ops.insert(item, &hint, 0);
}

errno_t file40_init(file40_t *file, reiser4_key_t *key,
		    reiser4_plugin_t *plugin, const void *tree,
		    reiser4_core_t *core)
{
	aal_assert("umka-1574", file != NULL, return -1);

	file->plugin = plugin;
	file->tree = tree;

	file->key.plugin = key->plugin;
	plugin_call(return -1, key->plugin->key_ops, assign,
		    file->key.body, key->body);

	file->core = core;
	
	return 0;
}

errno_t file40_realize(file40_t *file) {
	reiser4_level_t stop = {LEAF_LEVEL, LEAF_LEVEL};
	
	aal_assert("umka-857", file != NULL, return -1);	

	plugin_call(return -1, file->key.plugin->key_ops, build_generic, 
		    file->key.body, KEY_STATDATA_TYPE, file40_locality(file), 
		    file40_objectid(file), 0);

	if (file->statdata.node)
		file->core->tree_ops.unlock(file->tree, &file->statdata);
	
	if (file->core->tree_ops.lookup(file->tree, &file->key, &stop,
					&file->statdata) != PRESENT) 
	{
		aal_exception_error("Can't find stat data of file 0x%llx.", 
				    file40_objectid(file));

		if (file->statdata.node)
			file->core->tree_ops.lock(file->tree, &file->statdata);
		
		return -1;
	}
	
	file->core->tree_ops.lock(file->tree, &file->statdata);
	
	return 0;
}

/*
  Inserts passed item hint into the tree. After function is finished, place
  contains the coord of the inserted item.
*/
errno_t file40_insert(file40_t *file, reiser4_item_hint_t *hint,
		      reiser4_level_t *stop, reiser4_place_t *place)
{
	rpid_t objectid = file40_objectid(file);
	
	switch (file->core->tree_ops.lookup(file->tree, &hint->key,
				      stop, place))
	{
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
