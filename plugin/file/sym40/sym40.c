/*
  sym40.c -- reiser4 symlink file plugin.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_ALONE
#  include <time.h>
#  include <unistd.h>
#endif

#include "sym40.h"

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t sym40_plugin;

/* Reads @n bytes to passed buffer @buff */
static int32_t sym40_read(object_entity_t *entity, 
			  void *buff, uint32_t n)
{
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	sym40_t *sym = (sym40_t *)entity;

	aal_assert("umka-1570", entity != NULL);
	aal_assert("umka-1571", buff != NULL);

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));

	hint.hint = &stat;
	stat.ext[SDEXT_SYMLINK_ID] = buff;

	item = &sym->file.statdata.item;

	if (!item->plugin->item_ops.read)
		return -1;

	if (item->plugin->item_ops.read(item, &hint, 0, 1) != 1)
		return -1;

	return aal_strlen(buff);
}

/* Opens symlink and returns initialized instance to the caller */
static object_entity_t *sym40_open(void *tree, reiser4_place_t *place) {
	key_entity_t *key;
	sym40_t *sym;

	aal_assert("umka-1163", tree != NULL);
	aal_assert("umka-1164", place != NULL);
    
	if (!(sym = aal_calloc(sizeof(*sym), 0)))
		return NULL;

	key = &place->item.key;

	/* Initalizing file handle */
	if (file40_init(&sym->file, &sym40_plugin, key, core, tree))
		goto error_free_sym;

	/* Saving statdata coord and locking the node it lies in */
	aal_memcpy(&sym->file.statdata, place, sizeof(*place));
	file40_lock(&sym->file, &sym->file.statdata);

	/* Initializing parent key from the root one */
	sym->file.core->tree_ops.rootkey(sym->file.tree,
					 &sym->parent);
	
	return (object_entity_t *)sym;

 error_free_sym:
	aal_free(sym);
	return NULL;
}

#ifndef ENABLE_ALONE

/* Creates symlink and returns initialized instance to the caller */
static object_entity_t *sym40_create(void *tree, reiser4_file_hint_t *hint) {
	roid_t objectid;
	roid_t locality;
	roid_t parent_locality;

	sym40_t *sym;
	reiser4_place_t place;
	reiser4_plugin_t *stat_plugin;
    
	reiser4_statdata_hint_t stat;
	reiser4_item_hint_t stat_hint;
    
	reiser4_sdext_lw_hint_t lw_ext;
	reiser4_sdext_unix_hint_t unix_ext;
	
	aal_assert("umka-1741", tree != NULL);
	aal_assert("umka-1740", hint != NULL);

	if (!(sym = aal_calloc(sizeof(*sym), 0)))
		return NULL;

	/* Inizializes file handle */
	file40_init(&sym->file, &sym40_plugin, &hint->object, 
		    core, tree);

	/* Initializing parent key from the parent field of passed @hint */
	plugin_call(hint->object.plugin->key_ops, assign,
		    &sym->parent, &hint->parent);
	
	locality = file40_locality(&sym->file);
	objectid = file40_objectid(&sym->file);

	parent_locality = plugin_call(hint->object.plugin->key_ops, 
				      get_locality, &hint->parent);

	/* Getting statdata plugin */
	if (!(stat_plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, 
						    hint->statdata)))
	{
		aal_exception_error("Can't find stat data item plugin by "
				    "its id 0x%x.", hint->statdata);
		goto error_free_sym;
	}
    
	/* Initializing the stat data hint */
	aal_memset(&stat_hint, 0, sizeof(stat_hint));
	stat_hint.plugin = stat_plugin;
    
	stat_hint.key.plugin = hint->object.plugin;
	
	plugin_call(hint->object.plugin->key_ops, assign,
		    &stat_hint.key, &hint->object);
    
	/*
	  Initializing stat data item hint. Here we set up the extentions mask
	  to unix extention, light weight and symlink ones.
	*/
	stat.extmask = 1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID |
		1 << SDEXT_SYMLINK_ID;

	/* Lightweigh extention hint setup */
	lw_ext.mode = S_IFLNK | 0755;
	lw_ext.nlink = 2;
	lw_ext.size = 0;

	/* Unix extention hint setup */
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
	stat.ext[SDEXT_SYMLINK_ID] = hint->body.sym;

	stat_hint.hint = &stat;

	/* Inserting stat data into the tree */
	if (file40_insert(&sym->file, &stat_hint, LEAF_LEVEL, &place))
		goto error_free_sym;

	/* Saving statdata coord and locking the node it lies in */
	aal_memcpy(&sym->file.statdata, &place, sizeof(place));
	file40_lock(&sym->file, &sym->file.statdata);
		
	return (object_entity_t *)sym;

 error_free_sym:
	aal_free(sym);
	return NULL;
}

/* Writes "n" bytes from "buff" to passed file. */
static int32_t sym40_write(object_entity_t *entity, 
			   void *buff, uint32_t n) 
{
	sym40_t *sym;

	aal_assert("umka-1777", buff != NULL);
	aal_assert("umka-1776", entity != NULL);

	/*
	  FIXME-UMKA: What about expanding node (and stat data item) first. What
	  we have do here?
	*/
	sym = (sym40_t *)entity;
	return file40_set_sym(&sym->file, buff);
}

/* Calls function @func for each symlink item (statdata only) */
static errno_t sym40_metadata(object_entity_t *entity,
			      place_func_t func,
			      void *data)
{
	sym40_t *sym;

	aal_assert("umka-1719", func != NULL);
	aal_assert("umka-1718", entity != NULL);

	sym = (sym40_t *)entity;
	return func(entity, &sym->file.statdata, data);
}

/* Calls function @func for each block symlink items lie in */
static errno_t sym40_layout(object_entity_t *entity,
			    block_func_t func,
			    void *data)
{
	blk_t blk;
	sym40_t *sym;

	aal_assert("umka-1721", func != NULL);
	aal_assert("umka-1720", entity != NULL);

	sym = (sym40_t *)entity;
	blk = sym->file.statdata.item.con.blk;
		
	return func(entity, blk, data);
}

#endif

/* Callback function for searching statdata item while parsing symlink */
static errno_t callback_find_statdata(char *track,
				      char *entry,
				      void *data)
{
	sym40_t *sym;
	file40_t *file;
	key_entity_t *key;
	item_entity_t *item;

	reiser4_place_t *place;
	object_entity_t *entity;
	reiser4_plugin_t *plugin;

	sym = (sym40_t *)data;

	file = &sym->file;
	key = &sym->file.key;
	
	place = &sym->file.statdata;
	item = &sym->file.statdata.item;
		
	/* Setting up the file key */
	plugin_call(key->plugin->key_ops, set_type, key, KEY_STATDATA_TYPE);
	plugin_call(key->plugin->key_ops, set_offset, key, 0);

	/* Performing lookup for statdata of current directory */
	if (file40_lookup(file, key, LEAF_LEVEL, &file->statdata) != PRESENT) {
		aal_exception_error("Can't find stat data of %s.",
				    track);
		return -1;
	}

	if (file->core->tree_ops.realize(file->tree,
					 &file->statdata))
		return -1;
	
	/* Getting file plugin */
	if (!(plugin = item->plugin->item_ops.belongs(item))) {
		aal_exception_error("Can't find file plugin for %s.",
				    track);
		return -1;
	}

	/* Symlinks handling. Method "follow" should be implemented */
	if (plugin->file_ops.follow) {
		
		if (!(entity = plugin_call(plugin->file_ops, open, 
					   sym->file.tree, place)))
		{
			aal_exception_error("Can't open parent of directory "
					    "%s.", track);
			return -1;
		}

		if (plugin->file_ops.follow(entity, &sym->file.key)) {
			aal_exception_error("Can't follow %s.", track);
			goto error_free_entity;
		}

		plugin_call(plugin->file_ops, close, entity);
	}
	
	plugin_call(sym->file.key.plugin->key_ops,
		    assign, &sym->parent, &sym->file.key);

	return 0;

 error_free_entity:
	plugin_call(plugin->file_ops, close, entity);
	return -1;
}

/* Callback for searching entry inside current directory */
static errno_t callback_find_entry(char *track, char *entry,
				   void *data)
{
	sym40_t *sym;
	item_entity_t *item;
	reiser4_place_t *place;
	object_entity_t *entity;
	reiser4_plugin_t *plugin;
	
	sym = (sym40_t *)data;
	place = &sym->file.statdata;
	item = &sym->file.statdata.item;

	/* Getting file plugin */
	if (!(plugin = item->plugin->item_ops.belongs(item))) {
		aal_exception_error("Can't find file plugin for %s.",
				    track);
		return -1;
	}

	/* Opening currect diretory */
	if (!(entity = plugin_call(plugin->file_ops, open, 
				   sym->file.tree, place)))
	{
		aal_exception_error("Can't open parent of directory "
				    "%s.", track);
		return -1;
	}

	/* Looking up for @enrty in current directory */
	if (plugin_call(plugin->file_ops, lookup, entity,
			entry, &sym->file.key) != PRESENT)
	{
		aal_exception_error("Can't find %s.", track);
		goto error_free_entity;
	}

	plugin_call(plugin->file_ops, close, entity);
	return 0;
	
 error_free_entity:
	plugin_call(plugin->file_ops, close, entity);
	return -1;

}

/*
  This function reads symlink and parses it by means of using aux_parse_path
  with applying corresponding callback fucntions for searching stat data and
  searchig entry. It returns stat data key of the object symlink points to.
*/
static errno_t sym40_follow(object_entity_t *entity,
			    key_entity_t *key)
{
	errno_t res;
	char path[4096];

	sym40_t *sym;
	reiser4_plugin_t *plugin;
	
	aal_assert("umka-1774", entity != NULL);
	aal_assert("umka-1775", key != NULL);

	sym = (sym40_t *)entity;
	aal_memset(path, 0, sizeof(path));
	
	if (file40_get_sym(&sym->file, path))
		return -1;

	plugin = sym->file.key.plugin;
		
	/*
	  Assigning parent key to root one of path symlink has is beginning from
	  the slash or assigning it to the parent key otherwise.
	*/
	if (path[0] == '/') {
		sym->file.core->tree_ops.rootkey(sym->file.tree,
						 &sym->file.key);
	} else {
		plugin_call(plugin->key_ops, assign,
			    &sym->file.key, &sym->parent);
	}

	res = aux_parse_path(path, callback_find_statdata,
			     callback_find_entry, (void *)entity);

	/* If there is no errors, we assign result ot passed @key */
	if (res == 0) {
		plugin_call(plugin->key_ops, assign, key,
			    &sym->file.key);
	}

	return res;
}

/* Releases passed @entity */
static void sym40_close(object_entity_t *entity) {
	sym40_t *sym = (sym40_t *)entity;
		
	aal_assert("umka-1170", entity != NULL);

	/* Unlocking statdata and body */
	file40_unlock(&sym->file, &sym->file.statdata);
	aal_free(entity);
}

static reiser4_plugin_t sym40_plugin = {
	.file_ops = {
		.h = {
			.handle = empty_handle,
			.id = FILE_SYMLINK40_ID,
			.group = SYMLINK_FILE,
			.type = FILE_PLUGIN_TYPE,
			.label = "sym40",
			.desc = "Symlink for reiserfs 4.0, ver. " VERSION,
		},
		
#ifndef ENABLE_ALONE
		.create	    = sym40_create,
		.write	    = sym40_write,
		.layout     = sym40_layout,
		.metadata   = sym40_metadata,
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
		
		.follow     = sym40_follow,
		.open	    = sym40_open,
		.close	    = sym40_close,
		.read	    = sym40_read
	}
};

static reiser4_plugin_t *sym40_start(reiser4_core_t *c) {
	core = c;
	return &sym40_plugin;
}

plugin_register(sym40_start, NULL);
