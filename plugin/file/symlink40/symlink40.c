/*
  symlink40.c -- reiser4 symlink file plugin.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_COMPACT
#  include <time.h>
#  include <unistd.h>
#endif

#include "symlink40.h"

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t symlink40_plugin;

/* Reads @n bytes to passed buffer @buff */
static int32_t symlink40_read(object_entity_t *entity, 
			      void *buff, uint32_t n)
{
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	symlink40_t *symlink = (symlink40_t *)entity;

	aal_assert("umka-1570", entity != NULL, return 0);
	aal_assert("umka-1571", buff != NULL, return 0);

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));

	hint.hint = &stat;
	stat.ext[SDEXT_SYMLINK_ID] = buff;

	item = &symlink->file.statdata.item;
	
	if (plugin_call(return -1, item->plugin->item_ops, open, item, &hint))
		return -1;

	return aal_strlen(buff);
}

static object_entity_t *symlink40_open(void *tree, 
				       reiser4_place_t *place) 
{
	key_entity_t *key;
	symlink40_t *symlink;

	aal_assert("umka-1163", tree != NULL, return NULL);
	aal_assert("umka-1164", place != NULL, return NULL);
    
	if (!(symlink = aal_calloc(sizeof(*symlink), 0)))
		return NULL;

	key = &place->item.key;
		
	if (file40_init(&symlink->file, &symlink40_plugin, key, core, tree))
		goto error_free_symlink;
	
	aal_memcpy(&symlink->file.statdata, place, sizeof(*place));
	file40_lock(&symlink->file, &symlink->file.statdata);
    
	return (object_entity_t *)symlink;

 error_free_symlink:
	aal_free(symlink);
	return NULL;
}

/* Gets symlink from the stat data */
static errno_t symlink40_get_data(reiser4_place_t *place,
				  char *data)
{
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.hint = &stat;
	stat.ext[SDEXT_SYMLINK_ID] = data;

	item = &place->item;

	if (!item->plugin->item_ops.open)
		return -1;

	if (item->plugin->item_ops.open(item, &hint)) {
		aal_exception_error("Can't open statdata item.");
		return -1;
	}

	return 0;
}

#ifndef ENABLE_COMPACT

static object_entity_t *symlink40_create(void *tree, 
					 reiser4_file_hint_t *hint) 
{
	roid_t objectid;
	roid_t locality;
	roid_t parent_locality;

	symlink40_t *symlink;
	reiser4_place_t place;
	reiser4_plugin_t *stat_plugin;
    
	reiser4_statdata_hint_t stat;
	reiser4_item_hint_t stat_hint;
    
	reiser4_sdext_lw_hint_t lw_ext;
	reiser4_sdext_unix_hint_t unix_ext;
	
	aal_assert("umka-1741", tree != NULL, return NULL);
	aal_assert("umka-1740", hint != NULL, return NULL);

	if (!(symlink = aal_calloc(sizeof(*symlink), 0)))
		return NULL;
    
	file40_init(&symlink->file, &symlink40_plugin, &hint->object, 
		    core, tree);
	
	locality = file40_locality(&symlink->file);
	objectid = file40_objectid(&symlink->file);

	parent_locality = plugin_call(return NULL, hint->object.plugin->key_ops, 
				      get_locality, &hint->parent);
    
	if (!(stat_plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, 
						    hint->statdata)))
	{
		aal_exception_error("Can't find stat data item plugin by "
				    "its id 0x%x.", hint->statdata);
		goto error_free_symlink;
	}
    
	/* Initializing the stat data hint */
	aal_memset(&stat_hint, 0, sizeof(stat_hint));
	stat_hint.plugin = stat_plugin;
    
	stat_hint.key.plugin = hint->object.plugin;
	
	plugin_call(goto error_free_symlink, hint->object.plugin->key_ops,
		    assign, &stat_hint.key, &hint->object);
    
	/* Initializing stat data item hint. */
	stat.extmask = 1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID |
		1 << SDEXT_SYMLINK_ID;
    
	lw_ext.mode = S_IFLNK | 0755;
	lw_ext.nlink = 2;

	/* This should be modifyed by write */
	lw_ext.size = 0;
    
	unix_ext.uid = getuid();
	unix_ext.gid = getgid();
	unix_ext.atime = time(NULL);
	unix_ext.mtime = time(NULL);
	unix_ext.ctime = time(NULL);
	unix_ext.rdev = 0;
	unix_ext.bytes = 0;

	aal_memset(&stat.ext, 0, sizeof(stat.ext));
    
	stat.ext[SDEXT_LW_ID] = &lw_ext;
	stat.ext[SDEXT_UNIX_ID] = &unix_ext;
	stat.ext[SDEXT_SYMLINK_ID] = hint->body.symlink;

	stat_hint.hint = &stat;

	if (file40_insert(&symlink->file, &stat_hint, LEAF_LEVEL, &place))
		goto error_free_symlink;

	aal_memcpy(&symlink->file.statdata, &place, sizeof(place));
	file40_lock(&symlink->file, &symlink->file.statdata);
		
	return (object_entity_t *)symlink;

 error_free_symlink:
	aal_free(symlink);
	return NULL;
}

/* Writes "n" bytes from "buff" to passed file. */
static int32_t symlink40_write(object_entity_t *entity, 
			   void *buff, uint32_t n) 
{
	/* Sorry, not implemented yet! */
	return 0;
}

static errno_t symlink40_metadata(object_entity_t *entity,
			      place_func_t func,
			      void *data)
{
	symlink40_t *symlink;

	aal_assert("umka-1719", func != NULL, return -1);
	aal_assert("umka-1718", entity != NULL, return -1);

	symlink = (symlink40_t *)entity;
	return func(entity, &symlink->file.statdata, data);
}

static errno_t symlink40_layout(object_entity_t *entity,
				block_func_t func,
				void *data)
{
	blk_t blk;
	symlink40_t *symlink;

	aal_assert("umka-1721", func != NULL, return -1);
	aal_assert("umka-1720", entity != NULL, return -1);

	symlink = (symlink40_t *)entity;
	blk = symlink->file.statdata.item.con.blk;
		
	return func(entity, blk, data);
}

#endif

static errno_t callback_find_statdata(char *track, char *entry,
				      void *data)
{
	file40_t *file;
	symlink40_t *symlink = (symlink40_t *)data;
	key_entity_t *key = &symlink->file.key;

	file = &symlink->file;
		
	/* Setting up the file key */
	plugin_call(return -1, key->plugin->key_ops, set_type,
		    key, KEY_STATDATA_TYPE);
	
	plugin_call(return -1, key->plugin->key_ops, set_offset,
		    key, 0);

	/* Performing lookup for statdata of current directory */
	if (file40_lookup(file, key, LEAF_LEVEL, &file->statdata) != PRESENT) {
		aal_exception_error("Can't find stat data of %s.",
				    track);
		return -1;
	}

	return file->core->tree_ops.realize(file->tree,
					    &file->statdata);
}

static errno_t callback_find_entry(char *track, char *entry,
				   void *data)
{
	item_entity_t *item;
	symlink40_t *symlink;
	reiser4_place_t *place;
	object_entity_t *entity;
	reiser4_plugin_t *plugin;
	
	symlink = (symlink40_t *)data;
	place = &symlink->file.statdata;
	item = &symlink->file.statdata.item;

	/* Getting file plugin */
	if (!(plugin = item->plugin->item_ops.belongs(item))) {
		aal_exception_error("Can't find file plugin for %s.",
				    track);
		return -1;
	}

	/* Opening currect diretory */
	if (!(entity = plugin_call(return -1, plugin->file_ops, open, 
				   symlink->file.tree, place)))
	{
		aal_exception_error("Can't open parent of directory "
				    "%s.", track);
		return -1;
	}

	/* Symlinks handling. Method "follow" should be implemented */
	if (plugin->file_ops.follow) {
		if (plugin->file_ops.follow(entity, &symlink->file.key)) {
			aal_exception_error("Can't follow %s.", track);
			goto error_free_entity;
		}
	}

	/* Updating parent key will be here */
	
	/* Looking up for @enrty in current directory */
	if (plugin_call(goto error_free_entity, plugin->file_ops, lookup,
			entity, entry, &symlink->file.key) != PRESENT)
	{
		aal_exception_error("Can't find %s.", track);
		goto error_free_entity;
	}

	plugin_call(return -1, plugin->file_ops, close, entity);
	return 0;
	
 error_free_entity:
	plugin_call(return -1, plugin->file_ops, close, entity);
	return -1;

}

static errno_t symlink40_follow(object_entity_t *entity,
				key_entity_t *key)
{
	char path[4096];
	symlink40_t *symlink;
	
	aal_assert("umka-1774", entity != NULL, return -1);
	aal_assert("umka-1775", key != NULL, return -1);

	symlink = (symlink40_t *)entity;
	
	if (symlink40_get_data(&symlink->file.statdata, path))
		return -1;

	/*
	  Getting the parent or root key will be here. Actually, we should set
	  up the key to root one if got symlink data is a absolute path and to
	  parent key otherwise.
	*/
	
	return aux_parse_path(path, callback_find_statdata,
			      callback_find_entry, (void *)entity);
}

static void symlink40_close(object_entity_t *entity) {
	symlink40_t *symlink = (symlink40_t *)entity;
		
	aal_assert("umka-1170", entity != NULL, return);

	/* Unlocking statdata and body */
	file40_unlock(&symlink->file, &symlink->file.statdata);
	
	aal_free(entity);
}

static reiser4_plugin_t symlink40_plugin = {
	.file_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.id = FILE_SYMLINK40_ID,
			.group = SYMLINK_FILE,
			.type = FILE_PLUGIN_TYPE,
			.label = "symlink40",
			.desc = "Symlink for reiserfs 4.0, ver. " VERSION,
		},
		
#ifndef ENABLE_COMPACT
		.create	    = symlink40_create,
		.write	    = symlink40_write,
		.layout     = symlink40_layout,
		.metadata   = symlink40_metadata,
#else
		.create	    = NULL,
		.write	    = NULL,
		.layout     = NULL,
		.metadata   = NULL,
#endif
		.truncate   = NULL,
		.valid	    = NULL,
		.lookup	    = NULL,
		.reset	    = NULL,
		.offset	    = NULL,
		.seek	    = NULL,
		
		.follow     = symlink40_follow,
		.open	    = symlink40_open,
		.close	    = symlink40_close,
		.read	    = symlink40_read
	}
};

static reiser4_plugin_t *symlink40_start(reiser4_core_t *c) {
	core = c;
	return &symlink40_plugin;
}

plugin_register(symlink40_start, NULL);
