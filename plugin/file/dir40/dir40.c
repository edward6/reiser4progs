/*
  dir40.c -- reiser4 default directory object plugin.

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

#include "dir40.h"

extern reiser4_plugin_t dir40_plugin;

static reiser4_core_t *core = NULL;

static errno_t dir40_reset(object_entity_t *entity) {
	reiser4_key_t key;
	
	dir40_t *dir = (dir40_t *)entity;
	reiser4_level_t stop = {LEAF_LEVEL, LEAF_LEVEL};
    
	aal_assert("umka-864", dir != NULL, return -1);
    
	/* Preparing key of the first entry in directory */
	key.plugin = dir->file.key.plugin;
	
	plugin_call(return -1, key.plugin->key_ops, build_direntry,
		    key.body, dir->hash, file40_locality(&dir->file),
		    file40_objectid(&dir->file), ".");

	file40_unlock(&dir->file, &dir->body);
	
	if (core->tree_ops.lookup(dir->file.tree, &key,
				  &stop, &dir->body) != PRESENT)
	{
		aal_exception_error("Can't find direntry of object 0x%llx.", 
				    file40_objectid(&dir->file));
		
		file40_lock(&dir->file, &dir->body);
		return -1;
	}

	file40_lock(&dir->file, &dir->body);
	
	dir->offset = 0;
	dir->body.pos.unit = 0;

	return 0;
}

/* Trying to guess hash in use by stat  dfata extention */
static reiser4_plugin_t *dir40_guess(dir40_t *dir) {
	/* 
	   FIXME-UMKA: This function should inspect stat data extentions
	   first. And only if they do not contain a convenient plugin extention
	   (hash plugin), it should use some default hash plugin id.
	*/
	return core->factory_ops.ifind(HASH_PLUGIN_TYPE, HASH_R5_ID);
}

static int dir40_next(dir40_t *dir) {
	reiser4_place_t right;

	reiser4_plugin_t *this_plugin;
	reiser4_plugin_t *right_plugin;

	/*
	  Getting the right neighbour. While key40 is using, next direntry item
	  will lie in the right neighbour node.
	*/
	if (core->tree_ops.right(dir->file.tree, &dir->body, &right))
		return 0;

	right_plugin = right.entity.plugin;
	this_plugin = dir->body.entity.plugin;

	/* Checking if items are mergeable */
	if (!plugin_equal(this_plugin, right_plugin))
		return 0;
	
	if (!plugin_call(return 0, this_plugin->item_ops, mergeable,
			 &right.entity, &dir->body.entity))
		return 0;
	
	file40_unlock(&dir->file, &dir->body);
	file40_lock(&dir->file, &right);

	dir->body = right;
	return PRESENT;
}

/* Reads n entries to passed buffer buff */
static int32_t dir40_read(object_entity_t *entity, 
			  void *buff, uint32_t n)
{
	dir40_t *dir;
	uint64_t size;
	uint32_t read;
	uint32_t chunk;

	item_entity_t *item;
	reiser4_entry_hint_t *entry;

	aal_assert("umka-844", entity != NULL, return -1);
	aal_assert("umka-845", buff != NULL, return -1);

	dir = (dir40_t *)entity;

	if (file40_get_size(&dir->file.statdata, &size))
		return -1;

	if (size == 0)
		return 0;

/*	if (n > size - dir->offset)
		n = size - dir->offset;*/
	
	entry = (reiser4_entry_hint_t *)buff;

	for (read = 0; read < n; read += chunk) {
		uint32_t units;
		
		item = &dir->body.entity;
		
		if ((chunk = n - read) == 0)
			return read;

		units = plugin_call(return -1, item->plugin->item_ops,
				    units, item);
		
		if (dir->body.pos.unit >= units) {
			if (dir40_next(dir) != PRESENT)
				return read;
		}
		
		chunk = plugin_call(return -1, item->plugin->item_ops, fetch,
				    item, entry, dir->body.pos.unit, chunk);

		if (chunk == 0)
			return read;

		entry += chunk;
		dir->offset += chunk;
		dir->body.pos.unit += chunk;
	}
    
	return read;
}

/* 
   Makes lookup in directory by name. Returns the key of the stat data item,
   entry points to.
*/
static int dir40_lookup(object_entity_t *entity, 
			char *name, reiser4_key_t *key) 
{
	reiser4_key_t wanted;
	dir40_t *dir = (dir40_t *)entity;
    
	aal_assert("umka-1117", entity != NULL, return -1);
	aal_assert("umka-1118", name != NULL, return -1);

	aal_assert("umka-1119", key != NULL, return -1);
	aal_assert("umka-1120", key->plugin != NULL, return -1);

	/* Forming the directory key by passed @name */
	wanted.plugin = dir->file.key.plugin;
	
	plugin_call(return -1, wanted.plugin->key_ops, build_direntry,
		    wanted.body, dir->hash, file40_locality(&dir->file),
		    file40_objectid(&dir->file), name);
    
	while (1) {
		item_entity_t *item = &dir->body.entity;

		if (plugin_call(return -1, item->plugin->item_ops, lookup, 
				item, &wanted, &dir->body.pos.unit) == PRESENT) 
		{
			roid_t locality;
			reiser4_entry_hint_t entry;

			if (plugin_call(return -1, item->plugin->item_ops, fetch,
					item, &entry, dir->body.pos.unit, 1) != 1)
				return -1;

			locality = plugin_call(return -1, key->plugin->key_ops,
					       get_locality, &entry.objid);
	    
			plugin_call(return -1, key->plugin->key_ops, build_generic,
				    key->body, KEY_STATDATA_TYPE, locality,
				    entry.objid.objectid, 0);
	    
			return 1;
		}
	
		if (dir40_next(dir) != PRESENT)
			return 0;
	}
    
	return 0;
}

static object_entity_t *dir40_open(const void *tree, 
				   reiser4_place_t *place) 
{
	dir40_t *dir;
	reiser4_key_t *pkey;

	aal_assert("umka-836", tree != NULL, return NULL);
	aal_assert("umka-837", place != NULL, return NULL);
    
	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return NULL;

	pkey = &place->entity.key;
	
	if (file40_init(&dir->file, pkey, &dir40_plugin, tree, core))
		goto error_free_dir;

	if (!(dir->hash = dir40_guess(dir))) {
                aal_exception_error("Can't guess hash plugin for directory "
				    "%llx.", file40_objectid(&dir->file));
                goto error_free_dir;
        }

	aal_memcpy(&dir->file.statdata, place, sizeof(*place));
	dir->file.core->tree_ops.lock(tree, &dir->file.statdata);
	
	/* Positioning to the first directory unit */
	if (dir40_reset((object_entity_t *)dir)) {
		aal_exception_error("Can't reset directory 0x%llx.", 
				    file40_objectid(&dir->file));
		goto error_free_dir;
	}
    
	return (object_entity_t *)dir;

 error_free_dir:
	aal_free(dir);
	return NULL;
}

#ifndef ENABLE_COMPACT

static char *dir40_empty_dir[2] = { ".", ".." };

static object_entity_t *dir40_create(const void *tree,
				     reiser4_file_hint_t *hint) 
{
	uint32_t i;
	dir40_t *dir;

	reiser4_place_t place;
	roid_t objectid, locality;

	reiser4_statdata_hint_t stat;
	reiser4_direntry_hint_t body;
	reiser4_item_hint_t stat_hint;
	reiser4_item_hint_t body_hint;
   
	reiser4_plugin_t *stat_plugin;
	reiser4_plugin_t *body_plugin;
    
	reiser4_sdext_lw_hint_t lw_ext;
	reiser4_sdext_unix_hint_t unix_ext;

	reiser4_level_t stop = {LEAF_LEVEL, LEAF_LEVEL};
	
	aal_assert("umka-835", tree != NULL, return NULL);
	aal_assert("umka-1739", hint != NULL, return NULL);

	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return NULL;
    
	if (file40_init(&dir->file, &hint->object, &dir40_plugin, tree, core))
		goto error_free_dir;
	
	if (!(dir->hash = core->factory_ops.ifind(HASH_PLUGIN_TYPE, 
						  hint->body.dir.hash)))
	{
		aal_exception_error("Can't find hash plugin by its id 0x%x.", 
				    hint->body.dir.hash);
		goto error_free_dir;
	}
    
	locality = file40_locality(&dir->file);
	objectid = file40_objectid(&dir->file);
    
	if (!(stat_plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, 
						    hint->statdata)))
	{
		aal_exception_error("Can't find stat data item plugin by its "
				    "id 0x%x.", hint->statdata);

		goto error_free_dir;
	}
   
	if (!(body_plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, 
						    hint->body.dir.direntry)))
	{
		aal_exception_error("Can't find direntry item plugin by its id 0x%x.", 
				    hint->body.dir.direntry);
		goto error_free_dir;
	}
    
	aal_memset(&stat_hint, 0, sizeof(stat_hint));
	aal_memset(&body_hint, 0, sizeof(body_hint));
	
	/* 
	   Initializing direntry item hint. This should be done before the stat
	   data item hint, because we will need size of direntry item durring
	   stat data initialization.
	*/
	body_hint.plugin = body_plugin;
	body_hint.key.plugin = hint->object.plugin; 
   
	body.count = sizeof(dir40_empty_dir) / sizeof(char *);
	
	plugin_call(goto error_free_dir, hint->object.plugin->key_ops,
		    build_direntry, body_hint.key.body, dir->hash,
		    locality, objectid, ".");

	if (!(body.unit = aal_calloc(body.count * sizeof(*body.unit), 0)))
		goto error_free_dir;

	/* Preparing hist for the empty directory */
	for (i = 0; i < body.count; i++) {
		char *name = dir40_empty_dir[i];
			
		aal_strncpy(body.unit[i].name, name, aal_strlen(name));
    
		plugin_call(goto error_free_body, hint->object.plugin->key_ops,
			    build_objid, &body.unit[i].objid, KEY_STATDATA_TYPE,
			    locality, objectid);
	
		plugin_call(goto error_free_body, hint->object.plugin->key_ops,
			    build_entryid, &body.unit[i].entryid, dir->hash,
			    body.unit[i].name);
	}
	
	body_hint.hint = &body;

	/* Initializing stat data hint */
	stat_hint.plugin = stat_plugin;
	stat_hint.key.plugin = hint->object.plugin;
    
	plugin_call(goto error_free_body, hint->object.plugin->key_ops,
		    assign, stat_hint.key.body, hint->object.body);
    
	/* Initializing stat data item hint. */
	stat.extmask = 1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID;
    
	lw_ext.nlink = 2;
	lw_ext.size = body.count;
	lw_ext.mode = S_IFDIR | 0755;
    
	unix_ext.rdev = 0;
	unix_ext.uid = getuid();
	unix_ext.gid = getgid();
	unix_ext.atime = time(NULL);
	unix_ext.mtime = time(NULL);
	unix_ext.ctime = time(NULL);

	if (plugin_call(goto error_free_body, body_plugin->item_ops,
			estimate, NULL, &body_hint, ~0ul))
	{
		aal_exception_error("Can't estimate directory item.");
		goto error_free_body;
	}
    
	unix_ext.bytes = body_hint.len;
    
	aal_memset(&stat.ext, 0, sizeof(stat.ext));
    
	stat.ext[SDEXT_LW_ID] = &lw_ext;
	stat.ext[SDEXT_UNIX_ID] = &unix_ext;

	stat_hint.hint = &stat;

	/* Inserting stat data and body into the tree */
	if (file40_insert(&dir->file, &stat_hint, &stop, &place))
		goto error_free_body;
	
	/* Saving stat data coord insert function has returned */
	aal_memcpy(&dir->file.statdata, &place, sizeof(place));
	dir->file.core->tree_ops.lock(dir->file.tree, &dir->file.statdata);
    
	/* Inserting the direntry item into the tree */
	if (file40_insert(&dir->file, &body_hint, &stop, &place))
		goto error_free_body;
	
	/* Saving directory start in local body coord */
	aal_memcpy(&dir->body, &place, sizeof(place));
	dir->file.core->tree_ops.lock(dir->file.tree, &dir->body);
	
	aal_free(body.unit);
	return (object_entity_t *)dir;

 error_free_body:
	aal_free(body.unit);
 error_free_dir:
	aal_free(dir);
 error:
	return NULL;
}

/* Adds n entries from buff to passed entity */
static int32_t dir40_write(object_entity_t *entity, 
			   void *buff, uint32_t n) 
{
	uint32_t i;
	uint64_t size;
	
	reiser4_place_t place;
	reiser4_item_hint_t hint;
	dir40_t *dir = (dir40_t *)entity;
	reiser4_direntry_hint_t body_hint;
    
	reiser4_level_t stop = {LEAF_LEVEL, LEAF_LEVEL};
	reiser4_entry_hint_t *entry = (reiser4_entry_hint_t *)buff;
    
	aal_assert("umka-844", dir != NULL, return -1);
	aal_assert("umka-845", entry != NULL, return -1);
   
	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&body_hint, 0, sizeof(body_hint));
	
	body_hint.count = 1;

	if (!(body_hint.unit = aal_calloc(sizeof(*entry), 0)))
		return -1;
    
	hint.hint = &body_hint;
  
	for (i = 0; i < n; i++) {
		
		plugin_call(break, dir->file.key.plugin->key_ops, build_objid,
			    &entry->objid, KEY_STATDATA_TYPE, entry->objid.locality,
			    entry->objid.objectid);
	
		plugin_call(break, dir->file.key.plugin->key_ops, build_entryid,
			    &entry->entryid, dir->hash, entry->name);
    
		aal_memcpy(&body_hint.unit[0], entry, sizeof(*entry));
    
		hint.key.plugin = dir->file.key.plugin;
		
		plugin_call(break, hint.key.plugin->key_ops, build_direntry,
			    hint.key.body, dir->hash, file40_locality(&dir->file),
			    file40_objectid(&dir->file), entry->name);
    
		hint.plugin = dir->body.entity.plugin;

		if (file40_insert(&dir->file, &hint, &stop, &place)) {
			aal_exception_error("Can't insert entry %s.", entry->name);
			return -1;
		}
		
		entry++;
	}

	file40_realize(&dir->file);
	
	/* Updating size field */
	if (file40_get_size(&dir->file.statdata, &size))
		return -1;

	size += n;

	if (file40_set_size(&dir->file.statdata, &size))
		return -1;
	
	aal_free(body_hint.unit);
	return i;
}

static errno_t dir40_layout(object_entity_t *entity,
			    action_func_t func,
			    void *data)
{
	blk_t blk;
	errno_t res = 0;
	dir40_t *dir = (dir40_t *)entity;

	aal_assert("umka-1473", dir != NULL, return -1);
	aal_assert("umka-1474", func != NULL, return -1);

	while (1) {
		blk = dir->body.entity.con.blk;
		
		if ((res = func(entity, blk, data)))
			return res;
		
		if (dir40_next(dir) != PRESENT)
			break;
			
	}
    
	return res;
}

static errno_t dir40_metadata(object_entity_t *entity,
			      metadata_func_t func,
			      void *data)
{
	errno_t res = 0;
	dir40_t *dir = (dir40_t *)entity;
	
	aal_assert("umka-1712", entity != NULL, return -1);
	aal_assert("umka-1713", func != NULL, return -1);
	
	if ((res = func(entity, &dir->file.statdata, data)))
		return res;

	while (1) {
		if ((res = func(entity, &dir->body, data)))
			return res;
		
		if (dir40_next(dir) != PRESENT)
			break;
			
	}
	
	return res;
}

#endif

static void dir40_close(object_entity_t *entity) {
	dir40_t *dir = (dir40_t *)entity;
	
	aal_assert("umka-750", entity != NULL, return);

	file40_unlock(&dir->file, &dir->file.statdata);
	file40_unlock(&dir->file, &dir->body);
	
	aal_free(entity);
}

static uint64_t dir40_offset(object_entity_t *entity) {
	aal_assert("umka-874", entity != NULL, return 0);
	return ((dir40_t *)entity)->offset;
}

/* Detecting the object plugin by extentions or mode */
static int dir40_confirm(reiser4_place_t *place) {
	uint16_t mode;
    
	aal_assert("umka-1417", place != NULL, return 0);

	/* 
	   FIXME-UMKA: Here we should inspect all extentions and try to find out
	   if non-standard file plugin is in use.
	*/

	/* 
	   Guessing plugin type and plugin id by mode field from the stat data 
	   item. Here we return default plugins for every file type.
	*/
	if (file40_get_mode(place, &mode)) {
		aal_exception_error("Can't get mode from stat data while probing %s.",
				    dir40_plugin.h.label);
		return 0;
	}
    
	return S_ISDIR(mode);
}

static reiser4_plugin_t dir40_plugin = {
	.file_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.id = FILE_DIRTORY40_ID,
			.group = DIRTORY_FILE,
			.type = FILE_PLUGIN_TYPE,
			.label = "dir40",
			.desc = "Compound directory for reiserfs 4.0, ver. " VERSION,
		},
#ifndef ENABLE_COMPACT
		.create	    = dir40_create,
		.write	    = dir40_write,
		.layout     = dir40_layout,
		.metadata   = dir40_metadata,
#else
		.create	    = NULL,
		.write	    = NULL,
		.layout     = NULL,
		.metadata   = NULL,
#endif
		.truncate   = NULL,
		.valid	    = NULL,
		.seek	    = NULL,
		
		.open	    = dir40_open,
		.confirm    = dir40_confirm,
		.close	    = dir40_close,
		.reset	    = dir40_reset,
		.offset	    = dir40_offset,
		.lookup	    = dir40_lookup,
		.read	    = dir40_read
	}
};

static reiser4_plugin_t *dir40_start(reiser4_core_t *c) {
	core = c;
	return &dir40_plugin;
}

plugin_register(dir40_start, NULL);

