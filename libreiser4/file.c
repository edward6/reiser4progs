/*
  file.c -- common code for all the files (regular ones, directories, symlinks,
  etc).
  
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
	item = &file->coord.item;
		
	if (!item->plugin->item_ops.belongs) {
		aal_exception_error("Method \"belongs\" is not "
				    "implemented. Can't find file plugin.");
		return NULL;
	}

	return item->plugin->item_ops.belongs(item);
}

static errno_t reiser4_file_stat(reiser4_file_t *file) {
	
	/* Setting up the file key to statdata one */
	reiser4_key_set_offset(&file->key, 0);
	reiser4_key_set_type(&file->key, KEY_STATDATA_TYPE);

	/* Performing lookup for statdata of current directory */
	if (reiser4_tree_lookup(file->fs->tree, &file->key, 
				LEAF_LEVEL, &file->coord) != PRESENT) 
	{
		/* Stat adta is not found. getting us out */
		return -1;
	}

	/* Initializing item at @file->coord */
	if (reiser4_coord_realize(&file->coord))
		return -1;

	return reiser4_item_get_key(&file->coord, &file->key);
}

/* Callback function for finding statdata of the current directory */
static errno_t callback_find_statdata(char *track, char *entry, void *data) {
	reiser4_file_t *file;
	reiser4_place_t *place;
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
		place = (reiser4_place_t *)&file->coord;
		
		if (!(entity = plugin_call(plugin->file_ops, open, 
					   file->fs->tree, place)))
		{
			aal_exception_error("Can't open parent of %s.", track);
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
	reiser4_file_t *file;
	reiser4_place_t *place;
	object_entity_t *entity;
	reiser4_plugin_t *plugin;
	
	file = (reiser4_file_t *)data;

	/* Getting file plugin */
	if (!(plugin = reiser4_file_plugin(file))) {
		aal_exception_error("Can't find file plugin for %s.",
				    track);
		return -1;
	}

	/* Opening currect diretory */
	place = (reiser4_place_t *)&file->coord;
		
	if (!(entity = plugin_call(plugin->file_ops, open, 
				   file->fs->tree, place)))
	{
		aal_exception_error("Can't open parent of directory "
				    "%s.", track);
		return -1;
	}

	/* Looking up for @enrty in current directory */
	if (plugin_call(plugin->file_ops, lookup, entity,
			entry, &file->key) != PRESENT)
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
   Performs lookup of file statdata by its name. Result is stored in passed
   object fileds. Returns error code or 0 if there are no errors. This function
   also supports symlinks and it rather might be called "stat".
*/
static errno_t reiser4_file_lookup(
	reiser4_file_t *file,	    /* file lookup will be performed in */
	char *name)	            /* name to be parsed */
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

/* This function opens file by its @coord */
reiser4_file_t *reiser4_file_begin(
	reiser4_fs_t *fs,		/* fs object will be opened on */
	reiser4_coord_t *coord)		/* statdata key of file to be opened */
{
	reiser4_file_t *file;
	reiser4_place_t *place;
	reiser4_plugin_t *plugin;
	
	aal_assert("umka-1508", fs != NULL);
	aal_assert("umka-1509", coord != NULL);

	if (!(file = aal_calloc(sizeof(*file), 0)))
		return NULL;
    
	file->fs = fs;
	
	aal_memcpy(&file->coord, coord, sizeof(*coord));
	
	if (reiser4_item_get_key(&file->coord, &file->key)) {
		aal_exception_error("Node (%llu), item (%u), unit(%u): Can't "
				    "get item key.", coord->node->blk, 
				    coord->pos.item, coord->pos.unit);
		goto error_free_file;
	}

	aal_snprintf(file->name, sizeof(file->name), "file %llx",
		     reiser4_key_get_objectid(&file->key));

	/* Guessing file plugin */
	if (!(plugin = reiser4_file_plugin(file))) {
		aal_exception_error("Can't find file plugin for %s.",
				    file->name);
		goto error_free_file;
	}

	place = (reiser4_place_t *)&file->coord;
		
	if (!(file->entity = plugin_call(plugin->file_ops, open,
					 fs->tree, place)))
	{
		aal_exception_error("Can't open %s.", file->name);
		goto error_free_file;
	}
	
	return file;
	
 error_free_file:
	aal_free(file);
	return NULL;
}

/* This function opens file by its name */
reiser4_file_t *reiser4_file_open(
	reiser4_fs_t *fs,		/* fs object will be opened on */
	char *name)		        /* name of file to be opened */
{
	reiser4_file_t *file;
	reiser4_place_t *place;
	reiser4_plugin_t *plugin;
    
	aal_assert("umka-678", fs != NULL);
	aal_assert("umka-789", name != NULL);

	if (!fs->tree) {
		aal_exception_error("Can't created file without "
				    "initialized tree.");
		return NULL;
	}
    
	if (!(file = aal_calloc(sizeof(*file), 0)))
		return NULL;
    
	file->fs = fs;
	aal_strncpy(file->name, name, sizeof(file->name));

	reiser4_key_assign(&file->key, &fs->tree->key);
	reiser4_key_assign(&file->dir, &fs->tree->key);
    
	/* 
	   Getting the file's stat data key by means of parsing its path. I
	   assume, that name is absolute one. So, user, who will call this
	   method should convert name previously into absolute one by getcwd
	   function.
	*/
	if (reiser4_file_lookup(file, name))
		goto error_free_file;
    
	if (!(plugin = reiser4_file_plugin(file))) {
		aal_exception_error("Can't find file plugin for %s.", name);
		goto error_free_file;
	}
    
	place = (reiser4_place_t *)&file->coord;
	
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
	reiser4_fs_t *fs,		    /* filesystem dir will be created on */
	reiser4_file_t *parent,	            /* parent file */
	reiser4_file_hint_t *hint,	    /* directory hint */
	const char *name)		    /* name of entry */
{
	reiser4_file_t *file;
	reiser4_plugin_t *plugin;
    
	roid_t objectid, locality;
    
	aal_assert("umka-790", fs != NULL);
	aal_assert("umka-1128", hint != NULL);
	aal_assert("umka-1152", name != NULL);

	if (!fs->tree) {
		aal_exception_error("Can't created file without "
				    "initialized tree.");
		return NULL;
	}
    
	/* Getting plugin will be used for file creating */
	if (!(plugin = hint->plugin)) {
		if (!parent) {
			aal_exception_error("Can't find plugin for "
					    "file creating.");
			return NULL;
		}
	
		plugin = parent->entity->plugin;
	}
    
	/* Allocating the memory for obejct instance */
	if (!(file = aal_calloc(sizeof(*file), 0)))
		return NULL;

	/* Initializing fileds and preparing keys */
	file->fs = fs;

	if (parent) {
		aal_strncat(file->name, parent->name, sizeof(file->name));

		if (file->name[aal_strlen(file->name) - 1] != '/')
			aal_strncat(file->name, "/", sizeof(file->name));
		
		aal_strncat(file->name, name, sizeof(file->name));
	} else
		aal_strncpy(file->name, name, sizeof(file->name));
    
	/* 
	   This is a special case. In the case parent is NULL, we are trying to
	   create root directory.
	*/
	if (parent) {
		reiser4_key_assign(&hint->parent, &parent->key);
		objectid = reiser4_oid_allocate(fs->oid);
	} else {
		roid_t root_locality = reiser4_oid_root_locality(fs->oid);
		roid_t hyper_locality = reiser4_oid_hyper_locality(fs->oid);
		
		hint->parent.plugin = fs->tree->key.plugin;
		reiser4_key_build_generic(&hint->parent, KEY_STATDATA_TYPE, 
					  hyper_locality, root_locality, 0);

		objectid = reiser4_oid_root_objectid(fs->oid);
	}

	locality = reiser4_key_get_objectid(&hint->parent);
    
	/* Building stat data key of directory */
	hint->object.plugin = hint->parent.plugin;
	
	reiser4_key_build_generic(&hint->object, KEY_STATDATA_TYPE,
				  locality, objectid, 0);
    
	reiser4_key_assign(&file->key, &hint->object);
    
	/* Creating entry in parent */
	if (parent) {
		reiser4_entry_hint_t entry;

		/* 
		   Creating entry in parent directory. It should be done first,
		   because if such directory exist we preffer just return error
		   and do not delete inserted file stat data and some kind of
		   body.
		*/
		aal_memset(&entry, 0, sizeof(entry));
	
		reiser4_key_assign(&entry.object, &hint->object);
		aal_strncpy(entry.name, (char *)name, sizeof(entry.name));

		if (reiser4_file_write(parent, &entry, 1) != 1) {
			aal_exception_error("Can't add entry %s.", name);
			goto error_free_file;
		}
	}

	if (!(file->entity = plugin_call(plugin->file_ops,
					 create, fs->tree, hint)))
	{
		aal_exception_error("Can't create file with oid 0x%llx.", 
				    reiser4_key_get_objectid(&file->key));
		goto error_free_file;
	}

	/* FIXME-UMKA: Updating parent will be here (nlink, size, etc) */
    
	return file;

 error_free_file:
	aal_free(file);
	return NULL;
}

static errno_t callback_process_place(
	object_entity_t *entity,   /* file to be inspected */
	reiser4_place_t *place,    /* next file block */
	void *data)                /* user-specified data */
{
	aal_stream_t *stream = (aal_stream_t *)data;
	reiser4_coord_t *coord = (reiser4_coord_t *)place;
	
	if (reiser4_item_print(coord, stream)) {
		aal_exception_error("Can't print item %lu in node %llu.",
				    coord->pos.item, coord->node->blk);
		return -1;
	}
		
	aal_stream_write(stream, "\n", 1);
	return 0;
}

/* Prints file items into passed stream */
errno_t reiser4_file_print(reiser4_file_t *file, aal_stream_t *stream) {
	return reiser4_file_metadata(file, callback_process_place, stream);
}

#endif

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
	reiser4_file_t *file,	    /* file where position shopuld be chnaged */
	uint32_t offset)	    /* offset for seeking */
{
	aal_assert("umka-1129", file != NULL);
	aal_assert("umka-1153", file->entity != NULL);
    
	return plugin_call(file->entity->plugin->file_ops, 
			   seek, file->entity, offset);
}

errno_t reiser4_file_layout(
	reiser4_file_t *file,       /* file we working with */
	block_func_t func,          /* layout callback */
	void *data)                 /* user-spaecified data */
{
	aal_assert("umka-1469", file != NULL);
	aal_assert("umka-1470", func != NULL);

	if (!file->entity->plugin->file_ops.layout)
		return 0;
	
	return file->entity->plugin->file_ops.layout(file->entity,
						     func, data);
}

errno_t reiser4_file_metadata(
	reiser4_file_t *file,       /* file we working with */
	place_func_t func,          /* metadata layout callback */
	void *data)                 /* user-spaecified data */
{
	aal_assert("umka-1714", file != NULL);
	aal_assert("umka-1715", func != NULL);

	if (!file->entity->plugin->file_ops.metadata)
		return 0;
	
	return file->entity->plugin->file_ops.metadata(file->entity,
						       func, data);
}

