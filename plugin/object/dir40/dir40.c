/*
  dir40.c -- reiser4 default directory object plugin.

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

#include "dir40.h"

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t dir40_plugin;

/* Resets internal direntry position at zero */
static errno_t dir40_reset(object_entity_t *entity) {
	dir40_t *dir;
	key_entity_t key;
    
	aal_assert("umka-864", entity != NULL);
    
	dir = (dir40_t *)entity;
	
	/* Preparing key of the first entry in directory */
	key.plugin = dir->obj.key.plugin;
	
	plugin_call(key.plugin->key_ops, build_entry, &key,
		    dir->hash, object40_locality(&dir->obj),
		    object40_objectid(&dir->obj), ".");

	object40_unlock(&dir->obj, &dir->body);
	
	/* Lookup for the first direntry item */
	if (object40_lookup(&dir->obj, &key, LEAF_LEVEL,
			    &dir->body) != LP_PRESENT)
	{
		aal_exception_error("Can't find direntry of object 0x%llx.", 
				    object40_objectid(&dir->obj));
		
		object40_lock(&dir->obj, &dir->body);
		return -1;
	}

	object40_lock(&dir->obj, &dir->body);

	/* Initializing positions */
	dir->offset = 0;
	dir->body.pos.unit = 0;

	return 0;
}

/* Trying to guess hash in use by stat data extention */
static reiser4_plugin_t *dir40_guess(dir40_t *dir) {
	/* 
	   FIXME-UMKA: This function should inspect stat data extentions
	   first. And only if they do not contain a convenient plugin extention
	   (hash plugin), it should use some default hash plugin id.
	*/
	return core->factory_ops.ifind(HASH_PLUGIN_TYPE, HASH_R5_ID);
}

static int dir40_mergeable(item_entity_t *item1,
			   item_entity_t *item2)
{
	reiser4_plugin_t *plugin1;
	reiser4_plugin_t *plugin2;
	
	plugin1 = item1->plugin;
	plugin2 = item2->plugin;

	/* Checking if items are mergeable */
	if (!plugin_equal(plugin1, plugin2))
		return 0;

	/*
	  Calling item's mergeable methods for determining if they are mergeable
	  or not.
	*/
	return plugin_call(plugin1->item_ops, mergeable,
			   item1, item2);
}

/* Switches current dir body item onto next one */
static lookup_t dir40_next(dir40_t *dir) {
	place_t right;
	reiser4_plugin_t *this_plugin;
	reiser4_plugin_t *right_plugin;

	/*
	  Getting the right neighbour node of the node current item lies
	  in. While key40 is in use, next direntry item will lie in the right
	  neighbour node. Probably we should do here like reg40_next does. And
	  namely do not access right neighbour, but rather perform lookup with
	  current hash plus one and then check if found item is mergeable with
	  current one or not.
	*/
	if (core->tree_ops.right(dir->obj.tree, &dir->body, &right))
		return LP_ABSENT;

	if (!dir40_mergeable(&right.item, &dir->body.item))
		return LP_ABSENT;
	
	object40_unlock(&dir->obj, &dir->body);
	object40_lock(&dir->obj, &right);

	dir->body = right;
	dir->body.pos.unit = 0;
	
	return LP_PRESENT;
}

/* Reads n entries to passed buffer buff */
static int32_t dir40_read(object_entity_t *entity, 
			  void *buff, uint32_t n)
{
	dir40_t *dir;
	uint64_t size;
	uint32_t read;
	uint32_t chunk;
	uint32_t units;

	item_entity_t *item;
	reiser4_entry_hint_t *entry;

	aal_assert("umka-844", entity != NULL);
	aal_assert("umka-845", buff != NULL);

	dir = (dir40_t *)entity;

	/* Getting stat data item coord */
	object40_stat(&dir->obj);

	/* Getting size from teh statdata */
	if ((size = object40_get_size(&dir->obj)) == 0)
		return 0;

	if (n > size - dir->offset)
		n = size - dir->offset;
	
	entry = (reiser4_entry_hint_t *)buff;
	
	item = &dir->body.item;

	units = plugin_call(item->plugin->item_ops,
			    units, item);

	/* Loop until requested data is read */
	for (read = 0; read < n; entry += chunk) {
		
		if ((chunk = n - read) == 0)
			return read;

		/* Reading piece of data */
		chunk = plugin_call(item->plugin->item_ops, read,
				    item, entry, dir->body.pos.unit, chunk);

		/* If actual read data is zero, we going out */
		if (chunk == 0)
			return read;

		/* Updating positions */
		read += chunk;
		dir->offset += chunk;
		dir->body.pos.unit += chunk;

		/* Getting next direntry item to be current */
		if (dir->body.pos.unit >= units) {
			
			if (dir40_next(dir) != LP_PRESENT)
				break;

			item = &dir->body.item;

			units = plugin_call(item->plugin->item_ops,
					    units, item);
		}
	}
    
	return read;
}

/* 
   Makes lookup in directory by name. Returns the key of the stat data item,
   entry points to.
*/
static lookup_t dir40_lookup(object_entity_t *entity, 
			     char *name, key_entity_t *key) 
{
	lookup_t res;
	key_entity_t wanted;
	dir40_t *dir = (dir40_t *)entity;
    
	aal_assert("umka-1117", entity != NULL);
	aal_assert("umka-1118", name != NULL);
	aal_assert("umka-1119", key != NULL);

	/*
	  Preparing key to be used for lookup. It is generating from the
	  directory oid, locality and name by menas of using hash plugin.
	*/
	key->plugin = dir->obj.key.plugin;
	
	wanted.plugin = dir->obj.key.plugin;
	
	plugin_call(wanted.plugin->key_ops, build_entry, &wanted,
		    dir->hash, object40_locality(&dir->obj),
		    object40_objectid(&dir->obj), name);

	/* Lookp until needed entry will be found */
	while (1) {
		item_entity_t *item = &dir->body.item;

		/*
		  If needed entry is found, we fetch it into local buffer and
		  get stat data key of the object it points to from it. This key
		  will be used for searching next entry in passed path and so
		  on.
		*/
		if (plugin_call(item->plugin->item_ops, lookup, item, &wanted,
				&dir->body.pos.unit) == LP_PRESENT) 
		{
			reiser4_entry_hint_t entry;

			if (plugin_call(item->plugin->item_ops, read, item,
					&entry, dir->body.pos.unit, 1) != 1)
				return LP_FAILED;

			plugin_call(key->plugin->key_ops, assign, key,
				    &entry.object);
	    
			return LP_PRESENT;
		}

		if ((res = dir40_next(dir)) != LP_PRESENT)
			return res;
	}
    
	return LP_ABSENT;
}

/*
  Initializing dir40 instance by stat data place, resetring directory be means
  of using dir40_reset function and return instance to caller.
*/
static object_entity_t *dir40_open(void *tree, 
				   place_t *place) 
{
	dir40_t *dir;
	key_entity_t *key;

	aal_assert("umka-836", tree != NULL);
	aal_assert("umka-837", place != NULL);
    
	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return NULL;

	key = &place->item.key;

	/* Initializing obj handle for the directory */
	if (object40_init(&dir->obj, &dir40_plugin, key, core, tree))
		goto error_free_dir;

	/* Guessing hash plugin basing on stat data */
	if (!(dir->hash = dir40_guess(dir))) {
                aal_exception_error("Can't guess hash plugin for directory "
				    "%llx.", object40_objectid(&dir->obj));
                goto error_free_dir;
        }

	/* Copying statdata coord and looking node it lies in */
	aal_memcpy(&dir->obj.statdata, place, sizeof(*place));
	object40_lock(&dir->obj, &dir->obj.statdata);
	
	/* Positioning to the first directory unit */
	if (dir40_reset((object_entity_t *)dir)) {
		aal_exception_error("Can't reset directory 0x%llx.", 
				    object40_objectid(&dir->obj));
		goto error_free_dir;
	}
    
	return (object_entity_t *)dir;

 error_free_dir:
	aal_free(dir);
	return NULL;
}

#ifndef ENABLE_ALONE

static char *dir40_empty_dir[2] = { ".", ".." };

/*
  Creates dir40 instance and inserts few item in new directory described by
  passed @hint.
*/
static object_entity_t *dir40_create(void *tree, object_entity_t *parent,
				     reiser4_file_hint_t *hint,
				     place_t *place) {
	uint32_t i;
	dir40_t *dir;

	roid_t parent_locality;
	roid_t objectid, locality;

	reiser4_entry_hint_t *body;
	reiser4_entry_hint_t *entry;
	reiser4_statdata_hint_t stat;
	reiser4_item_hint_t body_hint;
	reiser4_item_hint_t stat_hint;
   
	reiser4_plugin_t *stat_plugin;
	reiser4_plugin_t *body_plugin;
    
	reiser4_sdext_lw_hint_t lw_ext;
	reiser4_sdext_unix_hint_t unix_ext;
	
	aal_assert("umka-835", tree != NULL);
	aal_assert("umka-1739", hint != NULL);

	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return NULL;

	/* Initializing obj handle */
	if (object40_init(&dir->obj, &dir40_plugin, &hint->object, core, tree))
		goto error_free_dir;

	/* Getting hash plugin */
	if (!(dir->hash = core->factory_ops.ifind(HASH_PLUGIN_TYPE, 
						  hint->body.dir.hash)))
	{
		aal_exception_error("Can't find hash plugin by its id 0x%x.", 
				    hint->body.dir.hash);
		goto error_free_dir;
	}

	/* Preparing dir oid and locality and parent locality */
	locality = object40_locality(&dir->obj);
	objectid = object40_objectid(&dir->obj);

	parent_locality = plugin_call(hint->object.plugin->key_ops,
				      get_locality, &hint->parent);

	/* Getting item plugins for statdata and body */
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
   	body_hint.count = sizeof(dir40_empty_dir) / sizeof(char *);
	
	plugin_call(hint->object.plugin->key_ops, build_entry, &body_hint.key,
		    dir->hash, locality, objectid, ".");

	if (!(body = aal_calloc(body_hint.count * sizeof(*body), 0)))
		goto error_free_dir;

	entry = body;
	
	/*
	  Preparing hint for the empty directory. It consists of two entries:
	  dot and dotdot.
	*/
	for (i = 0; i < body_hint.count; i++, entry++) {
		char *name;
		uint64_t loc, oid;

		if (i == 0) {
			loc = locality;
			oid = objectid;
		} else {
			loc = parent_locality;
			oid = locality;
		}

		/* Preparing entry hints */
		name = dir40_empty_dir[i];
		
		aal_strncpy(entry->name, name, aal_strlen(name));

		/*
		  Building key for the statdata of object new entry will point
		  to.
		*/
		entry->object.plugin = hint->object.plugin;

		plugin_call(hint->object.plugin->key_ops, build_generic,
			    &entry->object, KEY_STATDATA_TYPE, loc, oid, 0);

		/* Building key for the hash new entry will have */
		entry->offset.plugin = hint->object.plugin;
		
		plugin_call(hint->object.plugin->key_ops, build_entry,
			    &entry->offset, dir->hash, object40_locality(&dir->obj),
			    object40_objectid(&dir->obj), name);
	}
	
	body_hint.type_specific = body;

	/* Initializing stat data hint */
	stat_hint.count = 1;
	stat_hint.plugin = stat_plugin;
	stat_hint.key.plugin = hint->object.plugin;
    
	plugin_call(hint->object.plugin->key_ops, assign, &stat_hint.key,
		    &hint->object);
    
	/*
	  Initializing stat data item hint. It uses unix extention and light
	  weight one. So we set up the mask in corresponding maner.
	*/
	stat.extmask = (1 << SDEXT_UNIX_ID) | (1 << SDEXT_LW_ID);

	/*
	  Light weight hint initializing. New directory will have two links on
	  it, because of dot entry which points onto directory itself and entry
	  in parent directory, which points to this new directory.
	*/
	lw_ext.mode = S_IFDIR | 0755;
	lw_ext.size = body_hint.count;
	lw_ext.nlink = (parent != NULL ? 2 : 3);

	/* Unix extention hint initializing */
	unix_ext.rdev = 0;
	unix_ext.uid = getuid();
	unix_ext.gid = getgid();
	unix_ext.atime = time(NULL);
	unix_ext.mtime = time(NULL);
	unix_ext.ctime = time(NULL);

	/*
	  Estimating body item and setting up "bytes" field from the unix
	  extetion.
	*/
	if (plugin_call(body_plugin->item_ops, estimate, NULL, &body_hint, ~0ul, 1)) {
		aal_exception_error("Can't estimate directory item.");
		goto error_free_body;
	}
    
	unix_ext.bytes = body_hint.len;
    
	aal_memset(&stat.ext, 0, sizeof(stat.ext));
    
	stat.ext[SDEXT_LW_ID] = &lw_ext;
	stat.ext[SDEXT_UNIX_ID] = &unix_ext;

	stat_hint.type_specific = &stat;

	/* Inserting stat data and body into the tree */
	if (object40_insert(&dir->obj, &stat_hint, LEAF_LEVEL, place))
		goto error_free_body;
	
	/* Saving stat data coord insert function has returned */
	aal_memcpy(&dir->obj.statdata, place, sizeof(*place));
	object40_lock(&dir->obj, &dir->obj.statdata);
    
	/* Inserting the direntry item into the tree */
	if (object40_insert(&dir->obj, &body_hint, LEAF_LEVEL, &dir->body))
		goto error_free_body;
	
	object40_lock(&dir->obj, &dir->body);
	aal_free(body);

	if (parent) {
		plugin_call(parent->plugin->file_ops, link,
			    parent);
	}
	
	return (object_entity_t *)dir;

 error_free_body:
	aal_free(body);
 error_free_dir:
	aal_free(dir);
 error:
	return NULL;
}

static errno_t dir40_truncate(object_entity_t *entity,
			      uint64_t n)
{
	return -1;
}

static errno_t dir40_link(object_entity_t *entity) {
	aal_assert("umka-1908", entity != NULL);
	
	return object40_link(&((dir40_t *)entity)->obj, 1);
}

static errno_t dir40_unlink(object_entity_t *entity) {
	dir40_t *dir;
	uint64_t size;
	
	aal_assert("umka-1907", entity != NULL);

	dir = (dir40_t *)entity;
	
	if (object40_link(&dir->obj, -1))
		return -1;

	if (object40_get_nlink(&dir->obj) > 0)
		return 0;
	
	/* Removing directory when nlink became zero */
	if (object40_stat(&dir->obj))
		return -1;

	if (dir40_reset(entity))
		return -1;
		
	size = object40_get_size(&dir->obj);

	aal_assert("umka-1909", size > 0);

	if (dir40_truncate(entity, size))
		return -1;

	return object40_remove(&dir->obj, &dir->obj.key, 1);
}

static errno_t dir40_remove(object_entity_t *entity,
			    key_entity_t *key)
{
	dir40_t *dir = (dir40_t *)entity;
	
	aal_assert("umka-1922", entity != NULL);
	aal_assert("umka-1923", key != NULL);

	return object40_remove(&dir->obj, key, 1);
}

/* Writes @n number of entries described by @buff to passed directory entity */
static int32_t dir40_write(object_entity_t *entity, 
			   void *buff, uint32_t n) 
{
	uint32_t i;
	uint64_t size;
	uint32_t atime;

	place_t place;
	key_entity_t *key;
	
	reiser4_item_hint_t hint;
	reiser4_entry_hint_t *entry;
	dir40_t *dir = (dir40_t *)entity;
    
	aal_assert("umka-844", dir != NULL);
	aal_assert("umka-845", buff != NULL);
   
	key = &dir->obj.key;
	entry = (reiser4_entry_hint_t *)buff;
	
	aal_memset(&hint, 0, sizeof(hint));
	
	hint.count = 1;
	hint.key.plugin = key->plugin;
	hint.plugin = dir->body.item.plugin;

	/* Loop until told number of entries is written */
	for (i = 0; i < n; i++, entry++) {
		hint.type_specific = (void *)entry;

		/* Building key of the new entry */
		plugin_call(key->plugin->key_ops, build_entry, &hint.key,
			    dir->hash, object40_locality(&dir->obj),
			    object40_objectid(&dir->obj), entry->name);
	
		entry->offset.plugin = key->plugin;
			
		plugin_call(key->plugin->key_ops, assign, &entry->offset,
			    &hint.key);

		/* Inserting entry */
		if (object40_insert(&dir->obj, &hint, LEAF_LEVEL, &place)) {
			aal_exception_error("Can't insert entry %s.",
					    entry->name);
			return -1;
		}
	}
	
	/* Updating size field in stat data */
	if (object40_stat(&dir->obj))
		return -1;
	
	size = object40_get_size(&dir->obj);

	if (object40_set_size(&dir->obj, size + n))
		return -1;

	atime = time(NULL);
	
	if (object40_set_atime(&dir->obj, atime))
		return -1;

	if (object40_set_mtime(&dir->obj, atime))
		return -1;

	return i;
}

struct layout_hint {
	object_entity_t *entity;
	block_func_t func;
	void *data;
};

typedef struct layout_hint layout_hint_t;

static errno_t callback_item_data(item_entity_t *item, uint64_t start,
				  uint64_t count, void *data)
{
	blk_t blk;
	errno_t res;
	
	layout_hint_t *hint = (layout_hint_t *)data;

	for (blk = start; blk < start + count; blk++) {
		if ((res = hint->func(hint->entity, blk, hint->data)))
			return res;
	}

	return 0;
}

/*
  Layout function implementation. It is needed for printing, fragmentation
  calculating, etc.
*/
static errno_t dir40_layout(object_entity_t *entity,
			    block_func_t func,
			    void *data)
{
	errno_t res;
	dir40_t *dir;
	layout_hint_t hint;

	aal_assert("umka-1473", entity != NULL);
	aal_assert("umka-1474", func != NULL);

	hint.func = func;
	hint.data = data;
	hint.entity = entity;

	dir = (dir40_t *)entity;
	
	while (1) {
		item_entity_t *item = &dir->body.item;
		
		if (item->plugin->item_ops.layout) {
			if ((res = item->plugin->item_ops.layout(item, 
								 callback_item_data, 
								 &hint)))
				return res;
		} else {
			if ((res = callback_item_data(item, item->con.blk, 1, &hint)))
				return res;
		}
		
		if (dir40_next(dir) != LP_PRESENT)
			break;
	}
    
	return 0;
}

/*
  Metadata function implementation. It traverses all directory items and calls
  @func for each of them. It is needed for printing, fragmentation calculating,
  etc.
*/
static errno_t dir40_metadata(object_entity_t *entity,
			      place_func_t func,
			      void *data)
{
	errno_t res = 0;
	dir40_t *dir = (dir40_t *)entity;
	
	aal_assert("umka-1712", entity != NULL);
	aal_assert("umka-1713", func != NULL);
	
	if ((res = func(entity, &dir->obj.statdata, data)))
		return res;

	while (1) {
		if ((res = func(entity, &dir->body, data)))
			return res;
		
		if (dir40_next(dir) != LP_PRESENT)
			break;
			
	}
	
	return res;
}

#endif

/*
  Freeing dir40 instance. That is unlocking nodes current statdata and body lie
  in and freeing all occpied memory.
*/
static void dir40_close(object_entity_t *entity) {
	dir40_t *dir = (dir40_t *)entity;
	
	aal_assert("umka-750", entity != NULL);

	object40_unlock(&dir->obj, &dir->obj.statdata);
	object40_unlock(&dir->obj, &dir->body);
	
	aal_free(entity);
}

static uint64_t dir40_offset(object_entity_t *entity) {
	aal_assert("umka-874", entity != NULL);
	return ((dir40_t *)entity)->offset;
}

static reiser4_plugin_t dir40_plugin = {
	.file_ops = {
		.h = {
			.handle = EMPTY_HANDLE,
			.id = FILE_DIRTORY40_ID,
			.group = DIRTORY_FILE,
			.type = FILE_PLUGIN_TYPE,
			.label = "dir40",
			.desc = "Compound directory for reiserfs 4.0, ver. " VERSION,
		},
		
#ifndef ENABLE_ALONE
		.create	    = dir40_create,
		.write	    = dir40_write,
		.layout     = dir40_layout,
		.metadata   = dir40_metadata,
		.link       = dir40_link,
		.unlink     = dir40_unlink,
		.truncate   = dir40_truncate,
		.remove     = dir40_remove,
#else
		.create	    = NULL,
		.write	    = NULL,
		.layout     = NULL,
		.metadata   = NULL,
		.link       = NULL,
		.unlink     = NULL,
		.truncate   = NULL,
		.remove     = NULL,
#endif
		.follow     = NULL,
		.valid	    = NULL,
		.seek	    = NULL,
		
		.open	    = dir40_open,
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

