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

/*
  Updates stat data place as it might be moved due to rebalancing and then gets
  size field from it.
*/
static uint64_t dir40_size(object_entity_t *entity) {
	dir40_t *dir;

	dir = (dir40_t *)entity;
	
	/* Getting stat data item place */
	obj40_stat(&dir->obj);
	
	return obj40_get_size(&dir->obj);
}

static errno_t dir40_telldir(object_entity_t *entity,
			     key_entity_t *offset)
{
	dir40_t *dir = (dir40_t *)entity;
	
	aal_assert("umka-1985", entity != NULL);
	aal_assert("umka-1986", offset != NULL);

	aal_memcpy(offset, &dir->offset, sizeof(*offset));
	return 0;
}

static errno_t dir40_seekdir(object_entity_t *entity,
			     key_entity_t *offset)
{
	dir40_t *dir = (dir40_t *)entity;
	
	aal_assert("umka-1983", entity != NULL);
	aal_assert("umka-1984", offset != NULL);

	aal_memcpy(&dir->offset, offset, sizeof(*offset));
	
	if (dir->body.node != NULL)
		obj40_unlock(&dir->obj, &dir->body);
	
	/* Lookup for the first direntry item */
	if (obj40_lookup(&dir->obj, &dir->offset, LEAF_LEVEL,
			 &dir->body) == LP_PRESENT)
	{
		obj40_lock(&dir->obj, &dir->body);

		/*
		  Correcting of the unit position for the case we are at the
		  start of directory item.
		*/
		if (dir->body.pos.unit == ~0ul)
			dir->body.pos.unit = 0;
				
		return 0;
	}

	dir->body.node = NULL;
	return -1;
}

/* Resets internal direntry position at zero */
static errno_t dir40_reset(object_entity_t *entity) {
	dir40_t *dir;
	key_entity_t key;
    
	aal_assert("umka-864", entity != NULL);
    
	dir = (dir40_t *)entity;
	
	/* Preparing key of the first entry in directory */
	key.plugin = dir->obj.key.plugin;
	
	plugin_call(key.plugin->key_ops, build_entry, &key,
		    dir->hash, obj40_locality(&dir->obj),
		    obj40_objectid(&dir->obj), ".");

	return dir40_seekdir(entity, &key);
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
	return plugin_call(plugin1->item_ops, mergeable, item1, item2);
}

/* Switches current dir body item onto next one */
static lookup_t dir40_next(dir40_t *dir) {
	place_t next;
	item_entity_t *item;
	reiser4_entry_hint_t entry;

	/* Getting next directory item */
	if (core->tree_ops.next(dir->obj.tree, &dir->body, &next))
		return LP_ABSENT;

	if (!dir40_mergeable(&next.item, &dir->body.item))
		return LP_ABSENT;
	
	obj40_unlock(&dir->obj, &dir->body);
	obj40_lock(&dir->obj, &next);

	dir->body = next;
	dir->body.pos.unit = 0;

	item = &dir->body.item;

	/* Updating current position by entry offset key */
	if (plugin_call(item->plugin->item_ops, read, item,
			&entry, dir->body.pos.unit, 1) == 1)
	{
		aal_memcpy(&dir->offset, &entry.offset,
			   sizeof(dir->offset));
	}
	
	return LP_PRESENT;
}

/* Reads n entries to passed buffer buff */
static errno_t dir40_readdir(object_entity_t *entity, 
			     reiser4_entry_hint_t *entry)
{
	dir40_t *dir;
	uint64_t size;
	uint32_t units;
	item_entity_t *item;

	aal_assert("umka-844", entity != NULL);
	aal_assert("umka-845", entry != NULL);

	dir = (dir40_t *)entity;
	item = &dir->body.item;

	/* Getting size from teh statdata */
	if ((size = dir40_size(entity)) == 0)
		return -1;

	units = plugin_call(item->plugin->item_ops,
			    units, item);

	if (dir->body.pos.unit >= units)
		return -1;
	
	/* Reading piece of data */
	if (plugin_call(item->plugin->item_ops, read, item,
			entry, dir->body.pos.unit, 1) == 1)
	{
		/* Updating positions */
		dir->body.pos.unit++;
    
		/* Getting next direntry item */
		if (dir->body.pos.unit >= units)
			dir40_next(dir);
	
		return 0;
	}

	return -1;
}

/* 
  Makes lookup in directory by name. Fills passed buff by found entry fields
  (offset key, object key, etc).
*/
static lookup_t dir40_lookup(object_entity_t *entity, 
			     char *name, reiser4_entry_hint_t *entry) 
{
	lookup_t res;
	key_entity_t wanted;
	dir40_t *dir = (dir40_t *)entity;
    
	aal_assert("umka-1117", entity != NULL);
	aal_assert("umka-1118", name != NULL);
	aal_assert("umka-1924", entry != NULL);

	/*
	  Preparing key to be used for lookup. It is generating from the
	  directory oid, locality and name by menas of using hash plugin.
	*/
	wanted.plugin = dir->obj.key.plugin;
	
	plugin_call(wanted.plugin->key_ops, build_entry,
		    &wanted,dir->hash, obj40_locality(&dir->obj),
		    obj40_objectid(&dir->obj), name);

	entry->object.plugin = wanted.plugin;
	entry->offset.plugin = wanted.plugin;
	
	/* Lookp until needed entry will be found */
	while (1) {
		uint32_t units;
		item_entity_t *item = &dir->body.item;

		/*
		  If needed entry is found, we fetch it into local buffer and
		  get stat data key of the object it points to from it. This key
		  will be used for searching next entry in passed path and so
		  on.
		*/
		if (plugin_call(item->plugin->item_ops, lookup, item,
				&wanted, &dir->body.pos.unit) == LP_PRESENT) 
		{
			if (plugin_call(item->plugin->item_ops, read, item,
					entry, dir->body.pos.unit, 1) != 1)
			{
				aal_exception_error("Can't read %lu entry "
						    "from object 0x%llx.",
						    dir->body.pos.unit,
						    obj40_objectid(&dir->obj));
				return LP_FAILED;
			}

			/* Handling possible hash collision */
			if (aal_strncmp(entry->name, name,
					aal_strlen(name)) == 0)
			{
				aal_memcpy(&dir->offset, &wanted,
					   sizeof(dir->offset));
				
				return LP_PRESENT;
			}

			aal_exception_warn("Hash collision is detected between "
					   "%s and %s. Sequentional search has "
					   "been started.", entry->name, name);

			if (!item->plugin->item_ops.units)
				return LP_FAILED;
			
			units = item->plugin->item_ops.units(item);

			/* Sequentional search of the needed entry by its name */
			for (; dir->body.pos.unit < units; dir->body.pos.unit++) {
				
				if (plugin_call(item->plugin->item_ops, read, item,
						entry, dir->body.pos.unit, 1) != 1)
				{
					aal_exception_error("Can't read %lu entry "
							    "from object 0x%llx.",
							    dir->body.pos.unit,
							    obj40_objectid(&dir->obj));
					return LP_FAILED;
				}

				if (aal_strncmp(entry->name, name,
						aal_strlen(name)) == 0)
				{
					aal_memcpy(&dir->offset, &wanted,
						   sizeof(dir->offset));
				
					return LP_PRESENT;
				}
			}
				
			return LP_ABSENT;
		}

		/* Getting next direntry item */
		if ((res = dir40_next(dir)) != LP_PRESENT)
			return res;
	}
    
	return LP_ABSENT;
}

/*
  Initializing dir40 instance by stat data place, resetring directory be means
  of using dir40_reset function and return instance to caller.
*/
static object_entity_t *dir40_open(void *tree, place_t *place) {
	dir40_t *dir;
	key_entity_t *key;

	aal_assert("umka-836", tree != NULL);
	aal_assert("umka-837", place != NULL);
    
	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return NULL;

	key = &place->item.key;

	/* Initializing obj handle for the directory */
	if (obj40_init(&dir->obj, &dir40_plugin, key, core, tree))
		goto error_free_dir;

	/* Guessing hash plugin basing on stat data */
	if (!(dir->hash = dir40_guess(dir))) {
                aal_exception_error("Can't guess hash plugin for directory "
				    "%llx.", obj40_objectid(&dir->obj));
                goto error_free_dir;
        }

	/* Copying statdata place and looking node it lies in */
	aal_memcpy(&dir->obj.statdata, place, sizeof(*place));
	obj40_lock(&dir->obj, &dir->obj.statdata);
	
	/* Positioning to the first directory unit */
	if (dir40_reset((object_entity_t *)dir)) {
		aal_exception_error("Can't reset directory 0x%llx.", 
				    obj40_objectid(&dir->obj));
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
				     reiser4_object_hint_t *hint,
				     place_t *place)
{
	uint32_t i;
	dir40_t *dir;

	oid_t parent_locality;
	oid_t objectid, locality;

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
	if (obj40_init(&dir->obj, &dir40_plugin, &hint->object, core, tree))
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
	locality = obj40_locality(&dir->obj);
	objectid = obj40_objectid(&dir->obj);

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
	body_hint.flags = HF_FORMATD;
	body_hint.plugin = body_plugin;
	body_hint.key.plugin = hint->object.plugin; 
   	body_hint.count = sizeof(dir40_empty_dir) / sizeof(char *);
	
	plugin_call(hint->object.plugin->key_ops, build_entry,
		    &body_hint.key, dir->hash, locality, objectid, ".");

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
			    &entry->offset, dir->hash, obj40_locality(&dir->obj),
			    obj40_objectid(&dir->obj), name);
	}
	
	body_hint.type_specific = body;

	/* Initializing stat data hint */
	stat_hint.count = 1;
	stat_hint.flags = HF_FORMATD;
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
	lw_ext.nlink = 1;
	lw_ext.mode = S_IFDIR | 0755;
	lw_ext.size = body_hint.count;

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
	if (plugin_call(body_plugin->item_ops, estimate, NULL, &body_hint,
			~0ul, 1))
	{
		aal_exception_error("Can't estimate directory item.");
		goto error_free_body;
	}
    
	unix_ext.bytes = body_hint.len;
    
	aal_memset(&stat.ext, 0, sizeof(stat.ext));
    
	stat.ext[SDEXT_LW_ID] = &lw_ext;
	stat.ext[SDEXT_UNIX_ID] = &unix_ext;

	stat_hint.type_specific = &stat;

	/* Inserting stat data and body into the tree */
	if (obj40_insert(&dir->obj, &stat_hint, LEAF_LEVEL, place))
		goto error_free_body;
	
	/* Saving stat data place insert function has returned */
	aal_memcpy(&dir->obj.statdata, place, sizeof(*place));
	obj40_lock(&dir->obj, &dir->obj.statdata);
    
	/* Inserting the direntry item into the tree */
	if (obj40_insert(&dir->obj, &body_hint, LEAF_LEVEL, &dir->body))
		goto error_free_body;

	obj40_lock(&dir->obj, &dir->body);
	
	aal_free(body);

	if (parent) {
		plugin_call(parent->plugin->object_ops, link,
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

/* Removes all directory body items */
static errno_t dir40_truncate(object_entity_t *entity,
			      uint64_t n)
{
	errno_t res;
	uint64_t offset;
	uint64_t objectid;
	
	key_entity_t key;
	key_entity_t *maxkey;

	dir40_t *dir = (dir40_t *)entity;

	aal_assert("umka-1925", entity != NULL);

	key.plugin = dir->obj.key.plugin;

	/*
	  Generating last item key by means of using maximal one (Nikita's
	  idea). So, we remove whole directory starting from the last item step
	  by step.
	*/
	maxkey = plugin_call(key.plugin->key_ops, maximal,);
	offset = plugin_call(key.plugin->key_ops, get_offset, maxkey);
	objectid = plugin_call(key.plugin->key_ops, get_objectid, maxkey);
	
	plugin_call(key.plugin->key_ops, build_generic, &key,
		    KEY_FILENAME_TYPE, obj40_objectid(&dir->obj),
		    objectid, offset);

	while (1) {
		place_t place;

		/* Looking for the last directory item */
		if ((obj40_lookup(&dir->obj, &key, LEAF_LEVEL,
				  &place)) == LP_FAILED)
		{
			aal_exception_error("Lookup failed while searching "
					    "the last directory item durring "
					    "truncate the directory 0x%llx.",
					    obj40_objectid(&dir->obj));
			return -1;
		}

		/* Checking if found item belongs this directory */
		if (!dir40_mergeable(&dir->body.item, &place.item))
			return 0;

		/* Removing item from the tree */
		if ((res = dir->obj.core->tree_ops.remove(dir->obj.tree,
							  &place, 1)))
		{
			aal_exception_error("Can't remove directory item "
					    "from directory 0x%llx.",
					    obj40_objectid(&dir->obj));
			return res;
		}
	}
	
	return 0;
}

static errno_t dir40_link(object_entity_t *entity) {
	aal_assert("umka-1908", entity != NULL);
	return obj40_link(&((dir40_t *)entity)->obj, 1);
}

static errno_t dir40_unlink(object_entity_t *entity) {
	errno_t res;
	dir40_t *dir;
	
	aal_assert("umka-1907", entity != NULL);

	dir = (dir40_t *)entity;
	
	if ((res = obj40_stat(&dir->obj)))
		return res;
	
	if ((res = obj40_link(&dir->obj, -1)))
		return res;

	/*
	  Checking if nlink reached 1. It is right for directory because it has
	  first entry refferenced to itself.
	*/
	if (obj40_get_nlink(&dir->obj) > 1)
		return 0;
	
	/* Removing directory when nlink became zero */
	if ((res = dir40_reset(entity)))
		return res;
		
	if ((res = dir40_truncate(entity, 0)))
		return res;

	if ((res = obj40_stat(&dir->obj)))
		return res;
	
	return obj40_remove(&dir->obj, &dir->obj.key, 1);
}

/* Removing entry from the directory */
static errno_t dir40_rem_entry(object_entity_t *entity,
			       reiser4_entry_hint_t *entry)
{
	errno_t res;
	dir40_t *dir;
	uint64_t size;
	uint32_t atime;
	key_entity_t *key;
	
	aal_assert("umka-1922", entity != NULL);
	aal_assert("umka-1923", entry != NULL);

	dir = (dir40_t *)entity;
	key = &dir->obj.key;

	/* Generating key of the entry to be removed */
	entry->offset.plugin = dir->obj.key.plugin;
	
	plugin_call(key->plugin->key_ops, build_entry, &entry->offset,
		    dir->hash, obj40_locality(&dir->obj),
		    obj40_objectid(&dir->obj), entry->name);
	
	if ((res = obj40_remove(&dir->obj, &entry->offset, 1)))
		return res;

	/* Updating size field in stat data */
	if ((res = obj40_stat(&dir->obj)))
		return res;
	
	size = obj40_get_size(&dir->obj);

	if ((res = obj40_set_size(&dir->obj, size - 1)))
		return res;

	atime = time(NULL);
	
	if ((res = obj40_set_atime(&dir->obj, atime)))
		return res;

	return obj40_set_mtime(&dir->obj, atime);
}

/* Adding new entry  */
static errno_t dir40_add_entry(object_entity_t *entity, 
			       reiser4_entry_hint_t *entry)
{
	uint64_t size;
	uint32_t atime;

	dir40_t *dir;
	place_t place;
	key_entity_t *key;
	
	reiser4_item_hint_t hint;

	aal_assert("umka-844", entity != NULL);
	aal_assert("umka-845", entry != NULL);

	dir = (dir40_t *)entity;
	key = &dir->obj.key;
	
	aal_memset(&hint, 0, sizeof(hint));
	
	hint.count = 1;
	hint.flags = HF_FORMATD;
	hint.key.plugin = key->plugin;
	hint.plugin = dir->body.item.plugin;

	hint.type_specific = (void *)entry;

	/* Building key of the new entry */
	plugin_call(key->plugin->key_ops, build_entry, &hint.key,
		    dir->hash, obj40_locality(&dir->obj),
		    obj40_objectid(&dir->obj), entry->name);
	
	plugin_call(key->plugin->key_ops, assign, &entry->offset,
		    &hint.key);

	/* Inserting entry */
	if (obj40_insert(&dir->obj, &hint, LEAF_LEVEL, &place)) {
		aal_exception_error("Can't insert entry %s.",
				    entry->name);
		return -1;
	}
	
	/* Updating size field in stat data */
	if (obj40_stat(&dir->obj))
		return -1;
	
	size = obj40_get_size(&dir->obj);

	if (obj40_set_size(&dir->obj, size + 1))
		return -1;

	atime = time(NULL);
	
	if (obj40_set_atime(&dir->obj, atime))
		return -1;

	if (obj40_set_mtime(&dir->obj, atime))
		return -1;

	return 0;
}

struct layout_hint {
	object_entity_t *entity;
	block_func_t func;
	void *data;
};

typedef struct layout_hint layout_hint_t;

static errno_t callback_item_data(void *object, uint64_t start,
				  uint64_t count, void *data)
{
	item_entity_t *item = (item_entity_t *)object;
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
			    block_func_t block_func,
			    void *data)
{
	errno_t res;
	dir40_t *dir;
	layout_hint_t hint;

	aal_assert("umka-1473", entity != NULL);
	aal_assert("umka-1474", block_func != NULL);

	hint.data = data;
	hint.entity = entity;
	hint.func = block_func;

	dir = (dir40_t *)entity;
	
	while (1) {
		item_entity_t *item = &dir->body.item;
		
		if (item->plugin->item_ops.layout) {
			
			/* Calling item's layout method */
			res = plugin_call(item->plugin->item_ops, layout,
					  item, callback_item_data, &hint);

			if (res != 0)
				return res;
		} else {
			if ((res = callback_item_data(item, item->context.blk,
						      1, &hint)))
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

	if (dir->obj.statdata.node != NULL)
		obj40_unlock(&dir->obj, &dir->obj.statdata);

	if (dir->body.node != NULL)
		obj40_unlock(&dir->obj, &dir->body);
	
	aal_free(entity);
}

static reiser4_plugin_t dir40_plugin = {
	.object_ops = {
		.h = {
			.handle = EMPTY_HANDLE,
			.id = OBJECT_DIRTORY40_ID,
			.group = DIRTORY_OBJECT,
			.type = OBJECT_PLUGIN_TYPE,
			.label = "dir40",
			.desc = "Compound directory for reiserfs 4.0, ver. " VERSION,
		},
		
#ifndef ENABLE_ALONE
		.create	      = dir40_create,
		.layout       = dir40_layout,
		.metadata     = dir40_metadata,
		.link         = dir40_link,
		.unlink       = dir40_unlink,
		.truncate     = dir40_truncate,
		.add_entry    = dir40_add_entry,
		.rem_entry    = dir40_rem_entry,
		.write        = NULL,
#endif
		.follow       = NULL,
		.valid	      = NULL,
		.seek	      = NULL,
		.read         = NULL,
		.offset       = NULL,
		
		.open	      = dir40_open,
		.close	      = dir40_close,
		.reset	      = dir40_reset,
		.lookup	      = dir40_lookup,
		.size	      = dir40_size,
		.readdir      = dir40_readdir,
		.telldir      = dir40_telldir,
		.seekdir      = dir40_seekdir
	}
};

static reiser4_plugin_t *dir40_start(reiser4_core_t *c) {
	core = c;
	return &dir40_plugin;
}

plugin_register(dir40_start, NULL);

