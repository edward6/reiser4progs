/*
  file.c -- common code for files and directories.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/reiser4.h>
#include <sys/stat.h>

/* Callback function for probing all file plugins */
static errno_t callback_guess_file(
	reiser4_plugin_t *plugin,	    /* plugin to be checked */
	void *data)			    /* item ot be checked */
{
	if (plugin->h.type == FILE_PLUGIN_TYPE) {
		return plugin_call(return 0, plugin->file_ops, confirm,
				   (reiser4_place_t *)data);
	}
    
	return 0;
}

/* 
   Tries to guess object plugin type passed first item plugin and item
   body. Most probably, that passed item body is stat data body.
*/
static reiser4_plugin_t *reiser4_file_guess(reiser4_coord_t *coord) {
	aal_assert("umka-1296", coord != NULL, return NULL);
	return libreiser4_factory_cfind(callback_guess_file, (void *)coord);
}

/* 
   Performs lookup of file statdata by its name. result of lookuping are stored
   in passed object fileds. Returns error code or 0 if there is no errors. This
   function also supports symlinks and it rather might be called "stat", by
   means of work it performs.
*/
static errno_t reiser4_file_realize(
	reiser4_file_t *file,	    /* file lookup will be performed in */
	const char *name)	    /* name to be parsed */
{
	object_entity_t *entity;
	reiser4_plugin_t *plugin;
	reiser4_level_t level = {LEAF_LEVEL, LEAF_LEVEL};

	char track[4096], path[4096];
	char *pointer = NULL, *entryname = NULL;

	aal_assert("umka-682", file != NULL, return -1);
	aal_assert("umka-681", name != NULL, return -1);
    
	aal_memset(path, 0, sizeof(path));
	aal_memset(track, 0, sizeof(track));
    
	aal_strncpy(path, name, sizeof(path));
    
	if (path[0] != '.' || path[0] == '/')
		track[aal_strlen(track)] = '/';
  
	pointer = path[0] == '/' ? &path[1] : &path[0];

	while (1) {
		reiser4_key_set_type(&file->key, KEY_STATDATA_TYPE);
		reiser4_key_set_offset(&file->key, 0);
	
		if (reiser4_tree_lookup(file->fs->tree, &file->key, 
					&level, &file->coord) != 1) 
		{
			aal_exception_error("Can't find stat data of directory %s.", track);
			return -1;
		}
	
		if (!(entryname = aal_strsep(&pointer, "/")))
			break;
		
		if (!aal_strlen(entryname))
			continue;
	
		aal_strncat(track, entryname, aal_strlen(entryname));
	
		if (!(plugin = reiser4_file_guess(&file->coord))) {
			aal_exception_error("Can't guess file plugin for "
					    "parent of %s.", track);
			return -1;
		}
	
		if (!(entity = plugin_call(return -1, plugin->file_ops, open, 
					   file->fs->tree, &file->key)))
		{
			aal_exception_error("Can't open parent of directory %s.", track);
			return -1;
		}
	    
		if (!plugin->file_ops.lookup) {
			aal_exception_error("Method \"lookup\" is not implemented in %s plugin.", 
					    plugin->h.label);
		
			plugin_call(return -1, plugin->file_ops, close, entity);
			return -1;
		}
	
		if (plugin->file_ops.lookup(entity, entryname, &file->key) != 1) {
			plugin_call(return -1, plugin->file_ops, close, entity);
			return -1;
		}
	    
		plugin_call(return -1, plugin->file_ops, close, entity);
		track[aal_strlen(track)] = '/';
	}
    
	return 0;
}

/* This function opens file by its @coord */
reiser4_file_t *reiser4_file_begin(
	reiser4_fs_t *fs,		/* fs object will be opened on */
	reiser4_coord_t *coord)		/* statdata key of file to be opened */
{
	reiser4_file_t *file;
	reiser4_plugin_t *plugin;
	
	aal_assert("umka-1508", fs != NULL, return NULL);
	aal_assert("umka-1509", coord != NULL, return NULL);

	if (!(file = aal_calloc(sizeof(*file), 0)))
		return NULL;
    
	file->fs = fs;
	file->coord = *coord;
	
	reiser4_key_init(&file->key, coord->entity.key.plugin,
			 &coord->entity.key.body);

#ifndef ENABLE_COMPACT
	{
		aal_stream_t stream;
		aal_stream_init(&stream);
		
		reiser4_key_print(&file->key, &stream);
		aal_strncpy(file->name, (char *)stream.data,
			    sizeof(file->name));
		
		aal_stream_fini(&stream);
	}
#else
	aal_snprintf(file->name, sizeof(file->name), "%llx",
		     reiser4_key_objectid(&file->key));
#endif
	
	if (!(plugin = reiser4_file_guess(&file->coord))) {
		aal_exception_error("Can't find file plugin for %s.",
				    file->name);
		goto error_free_file;
	}
    
	if (!(file->entity = plugin_call(goto error_free_file, plugin->file_ops,
					 open, fs->tree, &file->key)))
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
	const char *name)		/* name of file to be opened */
{
	reiser4_file_t *file;
	reiser4_key_t *root_key;
	reiser4_plugin_t *plugin;
    
	aal_assert("umka-678", fs != NULL, return NULL);
	aal_assert("umka-789", name != NULL, return NULL);

	if (!fs->tree) {
		aal_exception_error("Can't created file without "
				    "initialized tree.");
		return NULL;
	}
    
	if (!(file = aal_calloc(sizeof(*file), 0)))
		return NULL;
    
	file->fs = fs;
	aal_strncpy(file->name, name, sizeof(file->name));

	root_key = &fs->tree->key;
	reiser4_key_init(&file->key, root_key->plugin, root_key->body);
    
	/* 
	   Getting file's stat data key by means of parsing its path. I assume
	   that name is absolute name. So, user, who will call this method
	   should convert name previously into absolute one by getcwd function.
	*/
	if (reiser4_file_realize(file, name)) {
		aal_exception_error("Can't find file %s.", name);
		goto error_free_file;
	}
    
	if (!(plugin = reiser4_file_guess(&file->coord))) {
		aal_exception_error("Can't find file plugin for %s.", name);
		goto error_free_file;
	}
    
	if (!(file->entity = plugin_call(goto error_free_file, plugin->file_ops,
					 open, fs->tree, &file->key)))
	{
		aal_exception_error("Can't open %s.", name);
		goto error_free_file;
	}
    
	return file;
    
 error_free_file:
	aal_free(file);
	return NULL;
}

#ifndef ENABLE_COMPACT

errno_t reiser4_file_truncate(
	reiser4_file_t *file,	            /* file for truncating */
	uint64_t n)			    /* the number of entries */
{
	aal_assert("umka-1154", file != NULL, return -1);
	aal_assert("umka-1155", file->entity != NULL, return -1);
    
	return plugin_call(return -1, file->entity->plugin->file_ops, 
			   truncate, file->entity, n);
}

/* Adds speficied entry into passed opened dir */
errno_t reiser4_file_write(
	reiser4_file_t *file,	            /* file for writing */
	void *buff,			    /* new entries buffer */
	uint64_t n)			    /* the number of entries to be created */
{
	aal_assert("umka-862", file != NULL, return -1);
	aal_assert("umka-863", file->entity != NULL, return -1);
    
	return plugin_call(return -1, file->entity->plugin->file_ops, 
			   write, file->entity, buff, n);
}

/* Creates new file on specified filesystem */
reiser4_file_t *reiser4_file_create(
	reiser4_fs_t *fs,		    /* filesystem dir will be created on */
	reiser4_file_hint_t *hint,	    /* directory hint */
	reiser4_file_t *parent,	            /* parent file */
	const char *name)		    /* name of entry */
{
	reiser4_file_t *file;
	reiser4_plugin_t *plugin;
    
	roid_t objectid, locality;
	reiser4_key_t parent_key, file_key;
    
	aal_assert("umka-790", fs != NULL, return NULL);
	aal_assert("umka-1128", hint != NULL, return NULL);
	aal_assert("umka-1152", name != NULL, return NULL);

	if (!fs->tree) {
		aal_exception_error("Can't created file without initialized tree.");
		return NULL;
	}
    
	/* Getting plugin will be used for file creating */
	if (!(plugin = hint->plugin)) {
		if (!parent) {
			aal_exception_error("Can't find plugin for file creating.");
			return NULL;
		}
	
		plugin = parent->entity->plugin;
	}
    
	/* Allocating the memory for obejct instance */
	if (!(file = aal_calloc(sizeof(*file), 0)))
		return NULL;

	/* Initializing fileds and preparing keys */
	file->fs = fs;

	aal_strncpy(file->name, name, sizeof(file->name));
    
	/* 
	   This is a special case. In the case parent is NULL, we are trying to
	   create root directory.
	*/
	if (parent) {
		reiser4_key_init(&parent_key, parent->key.plugin, parent->key.body);
		objectid = reiser4_oid_allocate(parent->fs->oid);
	} else {
		roid_t root_locality = reiser4_oid_root_locality(fs->oid);
		roid_t root_parent_locality = reiser4_oid_root_parent_locality(fs->oid);
		
		parent_key.plugin = fs->tree->key.plugin;
		reiser4_key_build_generic(&parent_key, KEY_STATDATA_TYPE, 
					  root_parent_locality, root_locality, 0);

		objectid = reiser4_oid_root_objectid(fs->oid);
	}

	locality = reiser4_key_get_objectid(&parent_key);
    
	/* Building stat data key of directory */
	file_key.plugin = parent_key.plugin;
	reiser4_key_build_generic(&file_key, KEY_STATDATA_TYPE,
				  locality, objectid, 0);
    
	reiser4_key_init(&file->key, file_key.plugin, file_key.body);
    
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
	
		entry.objid.objectid = reiser4_key_get_objectid(&file->key);
		entry.objid.locality = reiser4_key_get_locality(&file->key);
		entry.name = (char *)name;

		if (!reiser4_file_write(parent, (char *)&entry, 1)) {
			aal_exception_error("Can't add entry %s.", name);
			goto error_free_file;
		}
	}

	if (!(file->entity = plugin_call(goto error_free_file, plugin->file_ops,
					 create, fs->tree, &parent_key, &file_key, hint)))
	{
		aal_exception_error("Can't create file with oid 0x%llx.", 
				    reiser4_key_get_objectid(&file_key));
		goto error_free_file;
	}

	/* FIXME-UMKA: Updating parent will be here (nlink, size, etc) */
    
	return file;

 error_free_file:
	aal_free(file);
	return NULL;
}

#endif

/* Closes specified file */
void reiser4_file_close(
	reiser4_file_t *file)	    /* file to be closed */
{
	aal_assert("umka-680", file != NULL, return);
	aal_assert("umka-1149", file->entity != NULL, return);

	plugin_call(goto error_free_file, file->entity->plugin->file_ops,
		    close, file->entity);
    
 error_free_file:
	aal_free(file);
}

/* Resets directory position */
errno_t reiser4_file_reset(
	reiser4_file_t *file)	    /* dir to be rewinded */
{
	aal_assert("umka-842", file != NULL, return -1);
	aal_assert("umka-843", file->entity != NULL, return -1);

	return plugin_call(return -1, file->entity->plugin->file_ops, 
			   reset, file->entity);
}

errno_t reiser4_file_read(
	reiser4_file_t *file,	    /* dir entry will be read from */
	void *buff,		    /* buffer result will be stored in */
	uint64_t n)                 /* buffer size */
{
	aal_assert("umka-860", file != NULL, return -1);
	aal_assert("umka-861", file->entity != NULL, return -1);

	return plugin_call(return -1, file->entity->plugin->file_ops, 
			   read, file->entity, buff, n);
}

/* Retutns current position in directory */
uint32_t reiser4_file_offset(
	reiser4_file_t *file)	    /* dir position will be obtained from */
{
	aal_assert("umka-875", file != NULL, return -1);
	aal_assert("umka-876", file->entity != NULL, return -1);

	return plugin_call(return -1, file->entity->plugin->file_ops, 
			   offset, file->entity);
}

/* Seeks directory current position to passed pos */
errno_t reiser4_file_seek(
	reiser4_file_t *file,	    /* file where position shopuld be chnaged */
	uint32_t offset)	    /* offset for seeking */
{
	aal_assert("umka-1129", file != NULL, return -1);
	aal_assert("umka-1153", file->entity != NULL, return -1);
    
	return plugin_call(return -1, file->entity->plugin->file_ops, 
			   seek, file->entity, offset);
}

errno_t reiser4_file_layout(
	reiser4_file_t *file,       /* file we working with */
	file_action_func_t func,    /* layout callback */
	void *data)                 /* user-spaecified data */
{
	aal_assert("umka-1469", file != NULL, return -1);
	aal_assert("umka-1470", func != NULL, return -1);

	if (!file->entity->plugin->file_ops.layout)
		return 0;
	
	return file->entity->plugin->file_ops.layout(file->entity, func, data);
}

