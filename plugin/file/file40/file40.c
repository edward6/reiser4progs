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

/* Gets mode field from the stat data */
errno_t file40_get_mode(item_entity_t *item, uint16_t *mode) {
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;
	reiser4_sdext_lw_hint_t lw_hint;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.hint = &stat;
	stat.ext[SDEXT_LW_ID] = &lw_hint;

	if (plugin_call(return -1, item->plugin->item_ops, open, item, &hint))
		return -1;

	*mode = lw_hint.mode;
	return 0;
}

/* Gets size field from the stat data */
errno_t file40_get_size(item_entity_t *item, uint64_t *size) {
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;
	reiser4_sdext_lw_hint_t lw_hint;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));

	hint.hint = &stat;
	stat.ext[SDEXT_LW_ID] = &lw_hint;

	if (plugin_call(return -1, item->plugin->item_ops, open, item, &hint))
		return -1;

	*size = lw_hint.size;
	return 0;
}

/* Updates size field in the stat data */
errno_t file40_set_size(item_entity_t *item, uint64_t size) {
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;
	reiser4_sdext_lw_hint_t lw_hint;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.hint = &stat;
	stat.ext[SDEXT_LW_ID] = &lw_hint;

	if (plugin_call(return -1, item->plugin->item_ops, open, item, &hint))
		return -1;

	lw_hint.size = size;
	return plugin_call(return -1, item->plugin->item_ops,
			   init, item, &hint);
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
	reiser4_level_t level = {LEAF_LEVEL, LEAF_LEVEL};
	
	aal_assert("umka-857", file != NULL, return -1);	

	plugin_call(return -1, file->key.plugin->key_ops, build_generic, 
		    file->key.body, KEY_STATDATA_TYPE, file40_locality(file), 
		    file40_objectid(file), 0);

	if (file->statdata.data)
		file->core->tree_ops.unlock(file->tree, &file->statdata);
	
	if (file->core->tree_ops.lookup(file->tree, &file->key, &level,
					&file->statdata) != 1) 
	{
		aal_exception_error("Can't find stat data of file 0x%llx.", 
				    file40_objectid(file));

		if (file->statdata.data)
			file->core->tree_ops.lock(file->tree, &file->statdata);
		
		return -1;
	}
	
	file->core->tree_ops.lock(file->tree, &file->statdata);
	
	return 0;
}
