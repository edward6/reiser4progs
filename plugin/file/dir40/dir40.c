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
	dir40_t *dir;
	key_entity_t key;
    
	aal_assert("umka-864", entity != NULL, return -1);
    
	dir = (dir40_t *)entity;
	
	/* Preparing key of the first entry in directory */
	key.plugin = dir->file.key.plugin;
	
	plugin_call(return -1, key.plugin->key_ops, build_entry,
		    &key, dir->hash, file40_locality(&dir->file),
		    file40_objectid(&dir->file), ".");

	file40_unlock(&dir->file, &dir->body);
	
	if (core->tree_ops.lookup(dir->file.tree, &key,
				  LEAF_LEVEL, &dir->body) != PRESENT)
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
	uint32_t units;
	item_entity_t *item;

	reiser4_place_t right;
	reiser4_plugin_t *this_plugin;
	reiser4_plugin_t *right_plugin;

	item = &dir->body.item;
	
	units = plugin_call(return -1, item->plugin->item_ops,
			    units, item);
	
	if (dir->body.pos.unit < units)
		return 0;
	
	/*
	  Getting the right neighbour. While key40 is using, next direntry item
	  will lie in the right neighbour node.
	*/
	if (core->tree_ops.right(dir->file.tree, &dir->body, &right))
		return 0;

	right_plugin = right.item.plugin;
	this_plugin = dir->body.item.plugin;

	/* Checking if items are mergeable */
	if (!plugin_equal(this_plugin, right_plugin))
		return 0;
	
	if (!plugin_call(return 0, this_plugin->item_ops, mergeable,
			 &right.item, &dir->body.item))
		return 0;
	
	file40_unlock(&dir->file, &dir->body);
	file40_lock(&dir->file, &right);

	dir->body = right;
	dir->body.pos.unit = 0;
	
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

	file40_realize(&dir->file);
	
	if ((size = file40_get_size(&dir->file.statdata)) == 0)
		return 0;

	if (n > size - dir->offset)
		n = size - dir->offset;
	
	entry = (reiser4_entry_hint_t *)buff;

	for (read = 0; read < n; read += chunk) {
		uint32_t units;
		
		item = &dir->body.item;
		
		if ((chunk = n - read) == 0)
			return read;

		chunk = plugin_call(return -1, item->plugin->item_ops, fetch,
				    item, entry, dir->body.pos.unit, chunk);

		if (chunk == 0)
			return read;

		entry += chunk;
		dir->offset += chunk;
		dir->body.pos.unit += chunk;

		dir40_next(dir);
	}
    
	return read;
}

/* 
   Makes lookup in directory by name. Returns the key of the stat data item,
   entry points to.
*/
static int dir40_lookup(object_entity_t *entity, 
			char *name, key_entity_t *key) 
{
	key_entity_t wanted;
	dir40_t *dir = (dir40_t *)entity;
    
	aal_assert("umka-1117", entity != NULL, return -1);
	aal_assert("umka-1118", name != NULL, return -1);

	aal_assert("umka-1119", key != NULL, return -1);
	aal_assert("umka-1120", key->plugin != NULL, return -1);

	wanted.plugin = dir->file.key.plugin;
	
	plugin_call(return -1, wanted.plugin->key_ops, build_entry,
		    &wanted, dir->hash, file40_locality(&dir->file),
		    file40_objectid(&dir->file), name);
    
	while (1) {
		item_entity_t *item = &dir->body.item;

		if (plugin_call(return -1, item->plugin->item_ops, lookup, 
				item, &wanted, &dir->body.pos.unit) == PRESENT) 
		{
			reiser4_entry_hint_t entry;

			if (plugin_call(return -1, item->plugin->item_ops, fetch,
					item, &entry, dir->body.pos.unit, 1) != 1)
				return -1;

			plugin_call(return -1, key->plugin->key_ops, assign,
				    key, &entry.object);
	    
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
	key_entity_t *key;

	aal_assert("umka-836", tree != NULL, return NULL);
	aal_assert("umka-837", place != NULL, return NULL);
    
	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return NULL;

	key = &place->item.key;
	
	if (file40_init(&dir->file, key, &dir40_plugin, tree, core))
		goto error_free_dir;

	if (!(dir->hash = dir40_guess(dir))) {
                aal_exception_error("Can't guess hash plugin for directory "
				    "%llx.", file40_objectid(&dir->file));
                goto error_free_dir;
        }

	aal_memcpy(&dir->file.statdata, place, sizeof(*place));
	file40_lock(&dir->file, &dir->file.statdata);
	
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
	roid_t parent_locality;
	roid_t objectid, locality;

	reiser4_statdata_hint_t stat;
	reiser4_direntry_hint_t body;
	reiser4_item_hint_t stat_hint;
	reiser4_item_hint_t body_hint;
   
	reiser4_plugin_t *stat_plugin;
	reiser4_plugin_t *body_plugin;
    
	reiser4_sdext_lw_hint_t lw_ext;
	reiser4_sdext_unix_hint_t unix_ext;
	
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

	parent_locality = plugin_call(return NULL, hint->object.plugin->key_ops,
				      get_locality, &hint->parent);
    
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
		    build_entry, &body_hint.key, dir->hash, locality,
		    objectid, ".");

	if (!(body.unit = aal_calloc(body.count * sizeof(*body.unit), 0)))
		goto error_free_dir;

	/* Preparing hist for the empty directory */
	for (i = 0; i < body.count; i++) {
		char *name;
		uint64_t loc, oid;
			
		if (i == 0) {
			loc = locality;
			oid = objectid;
		} else {
			loc = parent_locality;
			oid = locality;
		}
		
		name = dir40_empty_dir[i];
		
		aal_strncpy(body.unit[i].name, name, aal_strlen(name));

		body.unit[i].object.plugin = hint->object.plugin;
		
		plugin_call(goto error_free_body, hint->object.plugin->key_ops,
			    build_generic, &body.unit[i].object, KEY_STATDATA_TYPE,
			    loc, oid, 0);
	
		body.unit[i].offset.plugin = hint->object.plugin;
		
		plugin_call(goto error_free_body, hint->object.plugin->key_ops,
			    build_entry, &body.unit[i].offset, dir->hash,
			    file40_locality(&dir->file), file40_objectid(&dir->file),
			    body.unit[i].name);
	}
	
	body_hint.hint = &body;

	/* Initializing stat data hint */
	stat_hint.plugin = stat_plugin;
	stat_hint.key.plugin = hint->object.plugin;
    
	plugin_call(goto error_free_body, hint->object.plugin->key_ops,
		    assign, &stat_hint.key, &hint->object);
    
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
	if (file40_insert(&dir->file, &stat_hint, LEAF_LEVEL, &place))
		goto error_free_body;
	
	/* Saving stat data coord insert function has returned */
	aal_memcpy(&dir->file.statdata, &place, sizeof(place));
	file40_lock(&dir->file, &dir->file.statdata);
    
	/* Inserting the direntry item into the tree */
	if (file40_insert(&dir->file, &body_hint, LEAF_LEVEL, &place))
		goto error_free_body;
	
	/* Saving directory start in local body coord */
	aal_memcpy(&dir->body, &place, sizeof(place));
	file40_lock(&dir->file, &dir->body);
	
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
	reiser4_entry_hint_t *entry;
	dir40_t *dir = (dir40_t *)entity;
	reiser4_direntry_hint_t body_hint;
    
	aal_assert("umka-844", dir != NULL, return -1);
	aal_assert("umka-845", buff != NULL, return -1);
   
	entry = (reiser4_entry_hint_t *)buff;
	
	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&body_hint, 0, sizeof(body_hint));
	
	body_hint.count = 1;

	if (!(body_hint.unit = aal_calloc(sizeof(*entry), 0)))
		return -1;
    
	hint.hint = &body_hint;
  
	for (i = 0; i < n; i++) {
		key_entity_t *key = &dir->file.key;
		
		hint.key.plugin = key->plugin;
		hint.plugin = dir->body.item.plugin;

		aal_memcpy(&body_hint.unit[0], entry, sizeof(*entry));
		
		plugin_call(return -1, key->plugin->key_ops, build_entry,
			    &hint.key, dir->hash, file40_locality(&dir->file),
			    file40_objectid(&dir->file), entry->name);

		body_hint.unit[0].offset.plugin = key->plugin;
			
		plugin_call(return -1, key->plugin->key_ops, assign,
			    &body_hint.unit[0].offset, &hint.key);

		if (file40_insert(&dir->file, &hint, LEAF_LEVEL, &place)) {
			aal_exception_error("Can't insert entry %s.",
					    entry->name);
			return -1;
		}
		
		entry++;
	}

	/* Updating size field */
	file40_realize(&dir->file);
	
	size = file40_get_size(&dir->file.statdata);

	if (file40_set_size(&dir->file.statdata, size + n))
		return -1;
	
	aal_free(body_hint.unit);
	
	return i;
}

struct layout_hint {
	object_entity_t *entity;
	block_func_t func;
	void *data;
};

typedef struct layout_hint layout_hint_t;

static errno_t callback_item_data(item_entity_t *item,
				  blk_t blk, void *data)
{
	layout_hint_t *hint = (layout_hint_t *)data;
	return hint->func(hint->entity, blk, hint->data);
}

static errno_t dir40_layout(object_entity_t *entity,
			    block_func_t func,
			    void *data)
{
	errno_t res;
	dir40_t *dir;
	layout_hint_t hint;

	aal_assert("umka-1473", entity != NULL, return -1);
	aal_assert("umka-1474", func != NULL, return -1);

	hint.func = func;
	hint.data = data;
	hint.entity = entity;

	dir = (dir40_t *)entity;
	
	while (1) {
		item_entity_t *item = &dir->body.item;
		
		if ((res = plugin_call(return -1, item->plugin->item_ops, layout,
				       item, callback_item_data, &hint)))
			return res;
		
		if (dir40_next(dir) != PRESENT)
			break;
	}
    
	return 0;
}

static errno_t dir40_metadata(object_entity_t *entity,
			      place_func_t func,
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
	mode = file40_get_mode(place);
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

