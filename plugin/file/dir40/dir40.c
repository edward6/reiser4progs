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
	reiser4_level_t level = {LEAF_LEVEL, LEAF_LEVEL};
    
	aal_assert("umka-864", dir != NULL, return -1);
    
	/* Preparing key of the first entry in directory */
	key.plugin = dir->file.key.plugin;
	plugin_call(return -1, key.plugin->key_ops, build_direntry, key.body, dir->hash,
		    file40_locality(&dir->file), file40_objectid(&dir->file), ".");

	file40_unlock(&dir->file, &dir->body);
	
	if (core->tree_ops.lookup(dir->file.tree, &key, &level, &dir->body) != 1) {
		
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

static int dir40_next(object_entity_t *entity) {
	reiser4_place_t right;
	reiser4_plugin_t *this_plugin;
	reiser4_plugin_t *right_plugin;
	dir40_t *dir = (dir40_t *)entity;

	/* Getting the right neighbour */
	if (core->tree_ops.right(dir->file.tree, &dir->body, &right))
		return 0;

	right_plugin = right.entity.plugin;
	this_plugin = dir->body.entity.plugin;
	
	if (!plugin_equal(this_plugin, right_plugin))
		return 0;
	
	if (!plugin_call(return 0, this_plugin->item_ops, mergeable,
			 &right.entity, &dir->body.entity))
		return 0;
	
	file40_unlock(&dir->file, &dir->body);
	file40_lock(&dir->file, &right);

	dir->body = right;
	return 1;
}

/* Reads n entries to passed buffer buff */
static int32_t dir40_read(object_entity_t *entity, 
			  void *buff, uint32_t n)
{
	uint32_t i, units;
	item_entity_t *item;
	reiser4_entry_hint_t *entry;
    
	dir40_t *dir = (dir40_t *)entity;
	entry = (reiser4_entry_hint_t *)buff;

	aal_assert("umka-844", dir != NULL, return -1);
	aal_assert("umka-845", entry != NULL, return -1);

	item = &dir->body.entity;
	
	/* Getting the number of entries */
	if (!(units = plugin_call(return -1, item->plugin->item_ops,
				  units, item)))
		return -1;
    
	for (i = 0; i < n; i++) {
		
		/* Check if we should get next item in right neighbour */
		if (dir->body.pos.unit >= units && dir40_next(entity) != 1)
			break;

		item = &dir->body.entity;

		/* Getting next entry from the current direntry item */
		if (plugin_call(break, item->plugin->item_ops, fetch, item,
				dir->body.pos.unit, entry++, 1))
			break;

		dir->offset++; 
		dir->body.pos.unit++; 
	}
    
	return i;
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

	/* Forming the directory key by passed @name*/
	wanted.plugin = dir->file.key.plugin;
	plugin_call(return -1, wanted.plugin->key_ops, build_direntry,
		    wanted.body, dir->hash, file40_locality(&dir->file),
		    file40_objectid(&dir->file), name);
    
	while (1) {
		item_entity_t *item = &dir->body.entity;

		if (plugin_call(return -1, item->plugin->item_ops, lookup, 
				item, &wanted, &dir->body.pos.unit) == 1) 
		{
			roid_t locality;
			reiser4_entry_hint_t entry;

			if (plugin_call(return -1, item->plugin->item_ops, fetch,
					item, dir->body.pos.unit, &entry, 1))
				return -1;

			locality = plugin_call(return -1, key->plugin->key_ops,
					       get_locality, &entry.objid);
	    
			plugin_call(return -1, key->plugin->key_ops, build_generic,
				    key->body, KEY_STATDATA_TYPE, locality,
				    entry.objid.objectid, 0);
	    
			return 1;
		}
	
		if (dir40_next(entity) != 1)
			return 0;
	}
    
	return 0;
}

static object_entity_t *dir40_open(const void *tree, 
				   reiser4_key_t *object) 
{
	dir40_t *dir;

	aal_assert("umka-836", tree != NULL, return NULL);
	aal_assert("umka-837", object != NULL, return NULL);
	aal_assert("umka-838", object->plugin != NULL, return NULL);
    
	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return NULL;

	if (file40_init(&dir->file, object, &dir40_plugin, tree, core))
		goto error_free_dir;

	if (!(dir->hash = dir40_guess(dir))) {
                aal_exception_error("Can't guess hash plugin for directory %llx.",
                                    file40_objectid(&dir->file));
                goto error_free_dir;
        }
  
	/* Grabbing stat data */
	if (file40_realize(&dir->file)) {
		aal_exception_error("Can't grab stat data of directory 0x%llx.", 
				    file40_objectid(&dir->file));
		goto error_free_dir;
	}

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

static object_entity_t *dir40_create(const void *tree, reiser4_key_t *parent,
				     reiser4_key_t *object, reiser4_file_hint_t *hint) 
{
	dir40_t *dir;
	reiser4_place_t place;
    
	reiser4_statdata_hint_t stat;
	reiser4_direntry_hint_t body;
	reiser4_item_hint_t stat_hint;
	reiser4_item_hint_t body_hint;
   
	reiser4_sdext_lw_hint_t lw_ext;
	reiser4_sdext_unix_hint_t unix_ext;

	int lookup;
	rpid_t body_pid;
	roid_t parent_locality;
	roid_t objectid, locality;

	reiser4_plugin_t *stat_plugin;
	reiser4_plugin_t *body_plugin;
    
	reiser4_level_t level = {LEAF_LEVEL, LEAF_LEVEL};
	
	aal_assert("umka-835", tree != NULL, return NULL);
	aal_assert("umka-743", parent != NULL, return NULL);
	aal_assert("umka-744", object != NULL, return NULL);
	aal_assert("umka-881", object->plugin != NULL, return NULL);

	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return NULL;
    
	if (file40_init(&dir->file, object, &dir40_plugin, tree, core))
		goto error_free_dir;
	
	if (!(dir->hash = core->factory_ops.ifind(HASH_PLUGIN_TYPE, 
						  hint->body.dir.hash_pid)))
	{
		aal_exception_error("Can't find hash plugin by its id 0x%x.", 
				    hint->body.dir.hash_pid);
		goto error_free_dir;
	}
    
	locality = file40_locality(&dir->file);
	objectid = file40_objectid(&dir->file);
    
	parent_locality = plugin_call(return NULL, object->plugin->key_ops,
				      get_locality, parent->body);
    
	if (!(stat_plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, 
						    hint->statdata_pid)))
	{
		aal_exception_error("Can't find stat data item plugin by its id 0x%x.", 
				    hint->statdata_pid);

		goto error_free_dir;
	}
   
	body_pid = hint->body.dir.direntry_pid;

	if (!(body_plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, 
						    body_pid)))
	{
		aal_exception_error("Can't find direntry item plugin by its id 0x%x.", 
				    body_pid);
		goto error_free_dir;
	}
    
	/* 
	   Initializing direntry item hint. This should be done earlier than
	   initializing of the stat data item hint, because we will need size of
	   direntry item durring stat data initialization.
	*/
	aal_memset(&body_hint, 0, sizeof(body_hint));

	body.count = 2;
	body_hint.plugin = body_plugin;
	body_hint.key.plugin = object->plugin; 
   
	plugin_call(goto error_free_dir, object->plugin->key_ops,
		    build_direntry, body_hint.key.body, dir->hash,
		    locality, objectid, ".");

	if (!(body.entry = aal_calloc(body.count*sizeof(*body.entry), 0)))
		goto error_free_dir;
    
	/* Preparing dot entry */
	body.entry[0].name = ".";
    
	plugin_call(goto error_free_body, object->plugin->key_ops,
		    build_objid, &body.entry[0].objid, KEY_STATDATA_TYPE,
		    locality, objectid);
	
	plugin_call(goto error_free_body, object->plugin->key_ops,
		    build_entryid, &body.entry[0].entryid, dir->hash,
		    body.entry[0].name);
    
	/* Preparing dot-dot entry */
	body.entry[1].name = "..";
    
	plugin_call(goto error_free_body, object->plugin->key_ops,
		    build_objid, &body.entry[1].objid, KEY_STATDATA_TYPE,
		    parent_locality, locality);
	
	plugin_call(goto error_free_body, object->plugin->key_ops,
		    build_entryid, &body.entry[1].entryid, dir->hash,
		    body.entry[1].name);
    
	body_hint.hint = &body;

	/* Initializing stat data hint */
	aal_memset(&stat_hint, 0, sizeof(stat_hint));
    
	stat_hint.plugin = stat_plugin;
	stat_hint.key.plugin = object->plugin;
    
	plugin_call(goto error_free_body, object->plugin->key_ops,
		    assign, stat_hint.key.body, object->body);
    
	/* Initializing stat data item hint. */
	stat.extmask = 1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID;
    
	lw_ext.mode = S_IFDIR | 0755;
	lw_ext.nlink = 2;
	lw_ext.size = 2;
    
	unix_ext.uid = getuid();
	unix_ext.gid = getgid();
	unix_ext.atime = time(NULL);
	unix_ext.mtime = time(NULL);
	unix_ext.ctime = time(NULL);
	unix_ext.rdev = 0;

	if (plugin_call(goto error_free_body, body_plugin->item_ops, estimate, 
			NULL, ~0ul, &body_hint))
	{
		aal_exception_error("Can't estimate directory item.");
		goto error_free_body;
	}
    
	unix_ext.bytes = body_hint.len;
    
	aal_memset(&stat.ext, 0, sizeof(stat.ext));
    
	stat.ext[SDEXT_LW_ID] = &lw_ext;
	stat.ext[SDEXT_UNIX_ID] = &unix_ext;

	stat_hint.hint = &stat;
    
	/* Calling balancing code in order to insert statdata item into the tree */
	if ((lookup = core->tree_ops.lookup(tree, &stat_hint.key,
					    &level, &place)) == FAILED)
		goto error_free_body;

	if (lookup == PRESENT) {
		aal_exception_error("Stat data key of file 0x%llx already exists in "
				    "the tree.", objectid);
		goto error_free_body;
	}
	
	if (core->tree_ops.insert(tree, &place, &stat_hint)) {
		aal_exception_error("Can't insert stat data item of file 0x%llx "
				    "into the tree.", objectid);
		goto error_free_body;
	}
    
	/* Inserting the direntry item into the tree */
	if ((lookup = core->tree_ops.lookup(tree, &body_hint.key,
					    &level, &place)) == FAILED)
		goto error_free_body;

	if (lookup == PRESENT) {
		aal_exception_error("Body key of file 0x%llx already exists in "
				    "the tree.", objectid);
		goto error_free_body;
	}
	
	if (core->tree_ops.insert(tree, &place, &body_hint)) {
		aal_exception_error("Can't insert direntry item of file 0x%llx "
				    "into the tree.", objectid);
		goto error_free_body;
	}

	if (file40_realize(&dir->file)) {
		aal_exception_error("Can't open stat data item of directory 0x%llx.",
				    file40_objectid(&dir->file));
		goto error_free_body;
	}
    
	if (dir40_reset((object_entity_t *)dir)) {
		aal_exception_error("Can't open body of directory 0x%llx.",
				    file40_objectid(&dir->file));
		goto error_free_body;
	}

	aal_free(body.entry);
    
	return (object_entity_t *)dir;

 error_free_body:
	aal_free(body.entry);
 error_free_dir:
	aal_free(dir);
 error:
	return NULL;
}

static errno_t dir40_truncate(object_entity_t *entity, uint64_t n) {
	/* Sorry, not implemented yet! */
	return -1;
}

/* Adds n entries from buff to passed entity */
static int32_t dir40_write(object_entity_t *entity, 
			   void *buff, uint32_t n) 
{
	uint64_t i;
	int lookup;

	reiser4_place_t place;
	reiser4_item_hint_t hint;
	dir40_t *dir = (dir40_t *)entity;
	reiser4_direntry_hint_t body_hint;
    
	reiser4_level_t level = {LEAF_LEVEL, LEAF_LEVEL};
	reiser4_entry_hint_t *entry = (reiser4_entry_hint_t *)buff;
    
	aal_assert("umka-844", dir != NULL, return -1);
	aal_assert("umka-845", entry != NULL, return -1);
   
	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&body_hint, 0, sizeof(body_hint));

	body_hint.count = 1;

	if (!(body_hint.entry = aal_calloc(sizeof(*entry), 0)))
		return 0;
    
	hint.hint = &body_hint;
  
	for (i = 0; i < n; i++) {
		plugin_call(break, dir->file.key.plugin->key_ops, build_objid,
			    &entry->objid, KEY_STATDATA_TYPE, entry->objid.locality,
			    entry->objid.objectid);
	
		plugin_call(break, dir->file.key.plugin->key_ops, build_entryid,
			    &entry->entryid, dir->hash, entry->name);
    
		aal_memcpy(&body_hint.entry[0], entry, sizeof(*entry));
    
		hint.key.plugin = dir->file.key.plugin;
		plugin_call(break, hint.key.plugin->key_ops, build_direntry,
			    hint.key.body, dir->hash, file40_locality(&dir->file),
			    file40_objectid(&dir->file), entry->name);
    
		hint.plugin = dir->body.entity.plugin;

		/* Inserting the entry to the tree */
		if ((lookup = core->tree_ops.lookup(dir->file.tree, &hint.key,
						    &level, &place)) == FAILED)
			break;

		if (lookup == PRESENT) {
			aal_exception_error("Entry key already exists in "
					    "the tree.");
			break;
		}
		
		if (core->tree_ops.insert(dir->file.tree, &place, &hint)) {
			aal_exception_error("Can't add entry %s to the tree.", 
					    entry->name);
			
			break;
		}

		entry++;
	}
    
	aal_free(body_hint.entry);
	return i;
}

static errno_t dir40_layout(object_entity_t *entity, file_action_func_t func,
			    void *data)
{
	errno_t res;
	dir40_t *dir = (dir40_t *)entity;

	aal_assert("umka-1473", dir != NULL, return -1);
	aal_assert("umka-1474", func != NULL, return -1);

	while (1) {
		blk_t blk = dir->body.entity.con.blk;
		
		if ((res = func(entity, blk, data)))
			return res;
		
		if (dir40_next(entity) != 1)
			break;
			
	}
    
	return 0;
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

static errno_t dir40_seek(object_entity_t *entity, 
			  uint64_t offset) 
{
	dir40_t *dir = (dir40_t *)entity;
    
	aal_assert("umka-1130", entity != NULL, return 0);

	/* FIXME-UMKA: Not implemented yet! */

	dir->offset = offset;
	return -1;
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
		.truncate   = dir40_truncate,
		.layout     = dir40_layout,
#else
		.create	    = NULL,
		.write	    = NULL,
		.truncate   = NULL,
		.layout     = NULL,
#endif
		.valid	    = NULL,
		
		.open	    = dir40_open,
		.confirm    = dir40_confirm,
		.close	    = dir40_close,
		.reset	    = dir40_reset,
		.offset	    = dir40_offset,
		.seek	    = dir40_seek,
		.lookup	    = dir40_lookup,
		.read	    = dir40_read
	}
};

static reiser4_plugin_t *dir40_start(reiser4_core_t *c) {
	core = c;
	return &dir40_plugin;
}

plugin_register(dir40_start, NULL);

