/*
  file.c -- common code for all reiser4 files (regular ones, directories,
  symlinks, etc).
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aux/aux.h>
#include <reiser4/reiser4.h>

static reiser4_plugin_t *reiser4_file_plugin(reiser4_file_t *file) {
	item_entity_t *item;

	/* Getting file plugin */
	item = &file->place.item;
		
	if (!item->plugin->item_ops.belongs) {
		aal_exception_error("Method \"belongs\" is not "
				    "implemented. Can't find file plugin.");
		return NULL;
	}

	return item->plugin->item_ops.belongs(item);
}

uint64_t reiser4_file_size(reiser4_file_t *file) {
	aal_assert("umka-1961", file != NULL);

	return plugin_call(file->entity->plugin->file_ops,
			   size, file->entity);
}

/* Looks up for the file stat data place in tree */
errno_t reiser4_file_stat(reiser4_file_t *file) {
	
	/* Setting up the file key to statdata one */
	reiser4_key_set_offset(&file->key, 0);
	reiser4_key_set_type(&file->key, KEY_STATDATA_TYPE);

	/* Performing lookup for statdata of current directory */
	if (reiser4_tree_lookup(file->fs->tree, &file->key,
				LEAF_LEVEL, &file->place) != LP_PRESENT) 
	{
		/* Stat data is not found. getting us out */
		return -1;
	}

	/* Initializing item at @file->place */
	if (reiser4_place_realize(&file->place))
		return -1;

	return reiser4_item_get_key(&file->place, &file->key);
}

/* Callback function for finding statdata of the current directory */
static errno_t callback_find_statdata(char *track, char *entry, void *data) {
	place_t *place;
	reiser4_file_t *file;
	object_entity_t *entity;
	reiser4_plugin_t *plugin;

	file = (reiser4_file_t *)data;

	if (reiser4_file_stat(file)) {
		aal_exception_error("Can't find stat data of %s.",
				    track);
		return -1;
	}

	/* Getting file plugin */
	if (!(plugin = reiser4_file_plugin(file))) {
		aal_exception_error("Can't find file plugin for %s.",
				    track);
		return -1;
	}

	/* Symlinks handling. Method "follow" should be implemented */
	if (plugin->file_ops.follow) {

		/* Opening file */
		place = (place_t *)&file->place;
		
		if (!(entity = plugin_call(plugin->file_ops, open, 
					   file->fs->tree, place)))
		{
			aal_exception_error("Can't open parent of %s.",
					    track);
			return -1;
		}

		if (plugin->file_ops.follow(entity, &file->key)) {
			aal_exception_error("Can't follow %s.", track);
			goto error_free_entity;
		}
		
		plugin_call(plugin->file_ops, close, entity);
	}
	
	reiser4_key_assign(&file->dir, &file->key);
	return 0;

 error_free_entity:
	plugin_call(plugin->file_ops, close, entity);
	return -1;
}

/* Callback function for finding passed @entry inside the current directory */
static errno_t callback_find_entry(char *track, char *entry, void *data) {
	place_t *place;
	reiser4_file_t *file;
	object_entity_t *entity;
	reiser4_plugin_t *plugin;
	reiser4_entry_hint_t entry_hint;
	
	file = (reiser4_file_t *)data;

	if (reiser4_file_stat(file)) {
		aal_exception_error("Can't find stat data of %s.",
				    track);
		return -1;
	}
	
	/* Getting file plugin */
	if (!(plugin = reiser4_file_plugin(file))) {
		aal_exception_error("Can't find file plugin for %s.",
				    track);
		return -1;
	}

	/* Opening currect directory */
	place = (place_t *)&file->place;
		
	if (!(entity = plugin_call(plugin->file_ops, open, 
				   file->fs->tree, place)))
	{
		aal_exception_error("Can't open parent of directory "
				    "%s.", track);
		return -1;
	}

	aal_memset(&entry_hint, 0, sizeof(entry_hint));
	
	/* Looking up for @enrty in current directory */
	if (plugin_call(plugin->file_ops, lookup, entity,
			entry, &entry_hint) != LP_PRESENT)
	{
		aal_exception_error("Can't find %s.", track);
		goto error_free_entity;
	}

	plugin_call(plugin->file_ops, close, entity);
	reiser4_key_assign(&file->key, &entry_hint.object);
	
	return 0;
	
 error_free_entity:
	plugin_call(plugin->file_ops, close, entity);
	return -1;
}

/* 
  Performs lookup of file statdata by its name. Result is stored in passed
  object fileds. Returns error code or 0 if there are no errors. This function
  also supports symlinks and it rather might be called "stat".
*/
static errno_t reiser4_file_search(
	reiser4_file_t *file,	    /* file lookup will be performed in */
	const char *name)           /* name to be parsed */
{
	aal_assert("umka-682", file != NULL);
	aal_assert("umka-681", name != NULL);

	/*
	  Parsing path and finding actual stat data key. I've said actual,
	  because there may be a symlink.
	*/
	if (aux_parse_path(name, callback_find_statdata,
			   callback_find_entry, (void *)file))
		return -1;

	/*
	  As the last part of path may be a symlink, we need position onto
	  actual stat data item.
	*/
	return reiser4_file_stat(file);
}

/* This function opens file by its name */
reiser4_file_t *reiser4_file_open(
	reiser4_fs_t *fs,		/* fs object will be opened on */
	const char *name)               /* name of file to be opened */
{
	place_t *place;
	reiser4_file_t *file;
	reiser4_plugin_t *plugin;
    
	aal_assert("umka-678", fs != NULL);
	aal_assert("umka-789", name != NULL);

	if (!fs->tree) {
		aal_exception_error("Can't open file without "
				    "initialized tree.");
		return NULL;
	}
    
	if (!(file = aal_calloc(sizeof(*file), 0)))
		return NULL;
    
	file->fs = fs;

#ifndef ENABLE_ALONE
	aal_strncpy(file->name, name, sizeof(file->name));
#endif

	reiser4_key_assign(&file->key, &fs->tree->key);
	reiser4_key_assign(&file->dir, &fs->tree->key);
    
	/* 
	  Parsing path and looking for file's stat data. We assume, that name is
	  absolute one. So, user, who calls this method should convert name
	  previously into absolute one by means of using getcwd function.
	*/
	if (reiser4_file_search(file, name))
		goto error_free_file;
    
	if (!(plugin = reiser4_file_plugin(file))) {
		aal_exception_error("Can't find file plugin for %s.", name);
		goto error_free_file;
	}
    
	place = (place_t *)&file->place;
	
	if (!(file->entity = plugin_call(plugin->file_ops,
					 open, fs->tree, place)))
	{
		aal_exception_error("Can't open %s.", name);
		goto error_free_file;
	}
    
	return file;
    
 error_free_file:
	aal_free(file);
	return NULL;
}

#ifndef ENABLE_ALONE

/* This function opens file by its @place */
reiser4_file_t *reiser4_file_begin(
	reiser4_fs_t *fs,		/* fs object will be opened on */
	reiser4_place_t *place)		/* statdata key of file to be opened */
{
	place_t *p;
	reiser4_file_t *file;
	reiser4_plugin_t *plugin;
	
	aal_assert("umka-1508", fs != NULL);
	aal_assert("umka-1509", place != NULL);

	if (!(file = aal_calloc(sizeof(*file), 0)))
		return NULL;
    
	file->fs = fs;
	
	aal_memcpy(&file->place, place, sizeof(*place));
	
	if (reiser4_item_get_key(&file->place, &file->key)) {
		aal_exception_error("Node (%llu), item (%u), unit(%u): Can't "
				    "get item key.", place->node->blk, 
				    place->pos.item, place->pos.unit);
		goto error_free_file;
	}

	reiser4_key_string(&file->key, file->name);

	/* Guessing file plugin */
	if (!(plugin = reiser4_file_plugin(file))) {
		aal_exception_error("Can't find file plugin for %s.",
				    file->name);
		goto error_free_file;
	}

	p = (place_t *)&file->place;
		
	if (!(file->entity = plugin_call(plugin->file_ops, open,
					 fs->tree, p)))
	{
		aal_exception_error("Can't open %s.", file->name);
		goto error_free_file;
	}
	
	return file;
	
 error_free_file:
	aal_free(file);
	return NULL;
}

errno_t reiser4_file_truncate(
	reiser4_file_t *file,	            /* file for truncating */
	uint64_t n)			    /* the number of entries */
{
	aal_assert("umka-1154", file != NULL);
	aal_assert("umka-1155", file->entity != NULL);
    
	return plugin_call(file->entity->plugin->file_ops, 
			   truncate, file->entity, n);
}

/* Adds speficied entry into passed opened dir */
int32_t reiser4_file_write(
	reiser4_file_t *file,	            /* file for writing */
	void *buff,			    /* new entries buffer */
	uint64_t n)			    /* the number of entries to be created */
{
	aal_assert("umka-862", file != NULL);
	aal_assert("umka-863", file->entity != NULL);
    
	return plugin_call(file->entity->plugin->file_ops, 
			   write, file->entity, buff, n);
}

/* Creates new file on specified filesystem */
reiser4_file_t *reiser4_file_create(
	reiser4_fs_t *fs,		    /* fs obejct will be created on */
	reiser4_file_t *parent,             /* parent file */
	reiser4_file_hint_t *hint)	    /* object hint */
{
	reiser4_file_t *file;
	reiser4_plugin_t *plugin;
    
	roid_t objectid, locality;
    
	aal_assert("umka-790", fs != NULL);
	aal_assert("umka-1128", hint != NULL);
	aal_assert("umka-1917", hint->plugin != NULL);

	if (!fs->tree) {
		aal_exception_error("Can't create file without "
				    "initialized tree.");
		return NULL;
	}
    
	plugin = hint->plugin;
	
	/* Allocating the memory for object instance */
	if (!(file = aal_calloc(sizeof(*file), 0)))
		return NULL;

	/* Initializing fields and preparing the keys */
	file->fs = fs;

	/* 
	  This is the special case. In the case parent is NULL, we are trying to
	  create root directory.
	*/
	if (parent) {
		reiser4_key_assign(&hint->parent, &parent->key);
		objectid = reiser4_oid_allocate(fs->oid);
	} else {
		hint->parent.plugin = fs->tree->key.plugin;
		
		if (reiser4_fs_hyper_key(fs, &hint->parent))
			goto error_free_file;
		
		objectid = reiser4_oid_root_objectid(fs->oid);
	}

	locality = reiser4_key_get_objectid(&hint->parent);
    
	/* Building stat data key of the new object */
	hint->object.plugin = hint->parent.plugin;
	
	reiser4_key_build_generic(&hint->object, KEY_STATDATA_TYPE,
				  locality, objectid, 0);
    
	reiser4_key_assign(&file->key, &hint->object);
	reiser4_key_string(&file->key, file->name);
	
	if (!(file->entity = plugin_call(plugin->file_ops, create, fs->tree,
					 (parent ? parent->entity : NULL),
					 hint, (place_t *)&file->place)))
	{
		aal_exception_error("Can't create file with oid 0x%llx.", 
				    reiser4_key_get_objectid(&file->key));
		goto error_free_file;
	}

	return file;

 error_free_file:
	aal_free(file);
	return NULL;
}

static errno_t callback_process_place(
	object_entity_t *entity,   /* file to be inspected */
	place_t *place,            /* next file block */
	void *data)                /* user-specified data */
{
	aal_stream_t *stream = (aal_stream_t *)data;
	reiser4_place_t *p = (reiser4_place_t *)place;
	
	if (reiser4_item_print(p, stream)) {
		aal_exception_error("Can't print item %lu in node %llu.",
				    p->pos.item, p->node->blk);
		return -1;
	}
		
	aal_stream_write(stream, "\n", 1);
	return 0;
}

/* Links @child to @file if it is a directory */
errno_t reiser4_file_link(reiser4_file_t *file,
			  reiser4_file_t *child,
			  const char *name)
{
	reiser4_entry_hint_t entry_hint;
	
	aal_assert("umka-1944", file != NULL);
	aal_assert("umka-1945", child != NULL);
	aal_assert("umka-1946", name != NULL);
	
	aal_memset(&entry_hint, 0, sizeof(entry_hint));
	
	reiser4_key_assign(&entry_hint.object, &child->key);
	aal_strncpy(entry_hint.name, name, sizeof(entry_hint.name));

	if (reiser4_file_write(file, &entry_hint, 1) != 1) {
		aal_exception_error("Can't add entry %s to %s.",
				    name, file->name);
		return -1;
	}

	if (plugin_call(file->entity->plugin->file_ops, link,
			file->entity))
	{
		aal_exception_error("Can't link %s to %s.",
				    name, file->name);
		return -1;
	}

	return 0;
}

/* Removes entry from the @file if it is a directory */
errno_t reiser4_file_unlink(reiser4_file_t *file,
			    const char *name)
{
	reiser4_file_t *child;
	reiser4_place_t place;
	reiser4_entry_hint_t entry;
	
	aal_assert("umka-1910", file != NULL);
	aal_assert("umka-1918", name != NULL);

	/* Getting child statdata key */
	if (reiser4_file_lookup(file, name, &entry) != LP_PRESENT) {
		aal_exception_error("Can't find entry %s in %s.",
				    name, file->name);
		return -1;
	}

	if (file->entity->plugin->file_ops.remove) {
		if (plugin_call(file->entity->plugin->file_ops,
				remove, file->entity, &entry.offset))
		{
			aal_exception_error("Can't remove entry %s in %s.",
					    name, file->name);
			return -1;
		}
	}
	
	if (reiser4_tree_lookup(file->fs->tree, &entry.object,
				LEAF_LEVEL, &place) != LP_PRESENT)
	{
		aal_exception_error("Can't find stat data of %s/%s. "
				    "Entry %s points to nowere.",
				    file->name, name, name);
		return -1;
	}
	
	if (!(child = reiser4_file_begin(file->fs, &place))) {
		aal_exception_error("Can't open %s/%s.",
				    file->name, name);
		return -1;
	}
	
	if (plugin_call(child->entity->plugin->file_ops, unlink,
			child->entity))
	{
		aal_exception_error("Can't unlink %s/%s.",
				    file->name, name);
		goto error_free_child;
	}

	reiser4_file_close(child);
	return 0;
	
 error_free_child:
	reiser4_file_close(child);
	return -1;
}

/* Prints file items into passed stream */
errno_t reiser4_file_print(reiser4_file_t *file,
			   aal_stream_t *stream)
{
	place_func_t place_func = callback_process_place;
	return reiser4_file_metadata(file, place_func, stream);
}

errno_t reiser4_file_layout(
	reiser4_file_t *file,       /* file we working with */
	block_func_t func,          /* layout callback */
	void *data)                 /* user-spaecified data */
{
	reiser4_plugin_t *plugin;
	
	aal_assert("umka-1469", file != NULL);
	aal_assert("umka-1470", func != NULL);

	plugin = file->entity->plugin;
	
	if (!plugin->file_ops.layout)
		return 0;
	
	return plugin->file_ops.layout(file->entity,
				       func, data);
}

errno_t reiser4_file_metadata(
	reiser4_file_t *file,       /* file we working with */
	place_func_t func,          /* metadata layout callback */
	void *data)                 /* user-spaecified data */
{
	reiser4_plugin_t *plugin;
	
	aal_assert("umka-1714", file != NULL);
	aal_assert("umka-1715", func != NULL);

	plugin = file->entity->plugin;
	
	if (!plugin->file_ops.metadata)
		return 0;
	
	return plugin->file_ops.metadata(file->entity,
					 func, data);
}

#endif

lookup_t reiser4_file_lookup(reiser4_file_t *file,
			     const char *name,
			     reiser4_entry_hint_t *entry)
{
	aal_assert("umka-1919", file != NULL);
	aal_assert("umka-1920", name != NULL);
	aal_assert("umka-1921", entry != NULL);

	if (!file->entity->plugin->file_ops.lookup)
		return LP_FAILED;
	
	return plugin_call(file->entity->plugin->file_ops, lookup,
			   file->entity, (char *)name, (void *)entry);
}

/* Closes specified file */
void reiser4_file_close(
	reiser4_file_t *file)	    /* file to be closed */
{
	aal_assert("umka-680", file != NULL);
	aal_assert("umka-1149", file->entity != NULL);

	plugin_call(file->entity->plugin->file_ops,
		    close, file->entity);
    
 error_free_file:
	aal_free(file);
}

/* Resets directory position */
errno_t reiser4_file_reset(
	reiser4_file_t *file)	    /* dir to be rewinded */
{
	aal_assert("umka-842", file != NULL);
	aal_assert("umka-843", file->entity != NULL);

	return plugin_call(file->entity->plugin->file_ops, 
			   reset, file->entity);
}

int32_t reiser4_file_read(
	reiser4_file_t *file,	    /* dir entry will be read from */
	void *buff,		    /* buffer result will be stored in */
	uint64_t n)                 /* buffer size */
{
	aal_assert("umka-860", file != NULL);
	aal_assert("umka-861", file->entity != NULL);

	return plugin_call(file->entity->plugin->file_ops, 
			   read, file->entity, buff, n);
}

/* Retutns current position in directory */
uint32_t reiser4_file_offset(
	reiser4_file_t *file)	    /* dir position will be obtained from */
{
	aal_assert("umka-875", file != NULL);
	aal_assert("umka-876", file->entity != NULL);

	return plugin_call(file->entity->plugin->file_ops, 
			   offset, file->entity);
}

/* Seeks directory current position to passed pos */
errno_t reiser4_file_seek(
	reiser4_file_t *file,	    /* file where position shopuld be changed */
	uint32_t offset)	    /* offset for seeking */
{
	aal_assert("umka-1129", file != NULL);
	aal_assert("umka-1153", file->entity != NULL);
    
	return plugin_call(file->entity->plugin->file_ops, 
			   seek, file->entity, offset);
}
