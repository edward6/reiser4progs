/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40.c -- reiser4 default directory object plugin. */

#ifndef ENABLE_STAND_ALONE
#  include <time.h>
#  include <unistd.h>
#endif

#include "dir40.h"

extern reiser4_plug_t dir40_plug;

/* Gets size from the object stat data */
static uint64_t dir40_size(object_entity_t *entity) {
	dir40_t *dir = (dir40_t *)entity;

	aal_assert("umka-2277", entity != NULL);
	
#ifndef ENABLE_STAND_ALONE
	/* Updating stat data place */
	if (obj40_stat(&dir->obj))
		return 0;
#endif

	return obj40_get_size(&dir->obj);
}

#ifndef ENABLE_STAND_ALONE
static errno_t dir40_telldir(object_entity_t *entity,
			     key_entity_t *offset)
{
	dir40_t *dir = (dir40_t *)entity;
	
	aal_assert("umka-1985", entity != NULL);
	aal_assert("umka-1986", offset != NULL);

	aal_memcpy(offset, &dir->offset,
		   sizeof(*offset));
	
	return 0;
}
#endif

static int dir40_mergeable(object_entity_t *entity,
			   place_t *place)
{
	dir40_t *dir = (dir40_t *)entity;

	/* Checking if item component in @place->pos is valid one */
	if (!core->tree_ops.valid(dir->obj.tree, place))
		return 0;

	/* Initializing item entity at @next place */
	if (core->tree_ops.fetch(dir->obj.tree, place))
		return 0;
	
	/* Checking if item plugins are mergeable */
	if (!plug_equal(dir->body.plug, place->plug))
		return 0;

	/* Calling item mergeable() method in order to determine if they are
	   mergeable. */
	return plug_call(dir->body.plug->o.item_ops,
			 mergeable, &dir->body, place);
}

static errno_t dir40_seekdir(object_entity_t *entity,
			     key_entity_t *offset)
{
	dir40_t *dir;
	place_t next;
	lookup_t res;

	uint64_t locality;
	
	aal_assert("umka-1983", entity != NULL);
	aal_assert("umka-1984", offset != NULL);

	dir = (dir40_t *)entity;

	switch (obj40_lookup(&dir->obj, offset,
			     LEAF_LEVEL, &next))
	{
	case ABSENT:
#ifndef ENABLE_STAND_ALONE
		/* Checking if item component in @item->pos is valid one */
		if (!core->tree_ops.valid(dir->obj.tree, &next))
			return -EINVAL;
		
                /* Initializing item entity at @next place */
                if (core->tree_ops.fetch(dir->obj.tree, &next))
                        return -EINVAL;
                                                                                    
                locality = plug_call(STAT_KEY(&dir->obj)->plug->o.key_ops,
				     get_locality, &next.key);
                                                                                    
                /* Items are not mergeable */
                if (locality != obj40_objectid(&dir->obj))
                        return -EINVAL;
#endif
	case FAILED:
		return -EINVAL;
	default:
		break;
	}
		
#ifndef ENABLE_STAND_ALONE
	aal_memcpy(&dir->offset, offset, sizeof(*offset));
#endif
	
	obj40_relock(&dir->obj, &dir->body, &next);
	aal_memcpy(&dir->body, &next, sizeof(dir->body));

	dir->unit = dir->body.pos.unit;
	
	if (dir->unit == MAX_UINT32)
		dir->unit = 0;

	return 0;
}

/* Resets internal direntry position at zero */
static errno_t dir40_reset(object_entity_t *entity) {
	errno_t res;
	dir40_t *dir;
	key_entity_t key;
    
	aal_assert("umka-864", entity != NULL);
    
	dir = (dir40_t *)entity;
	
	/* Preparing key of the first entry in directory */
	plug_call(STAT_KEY(&dir->obj)->plug->o.key_ops,
		  build_entry, &key, dir->hash,
		  obj40_locality(&dir->obj),
		  obj40_objectid(&dir->obj), ".");

	return dir40_seekdir(entity, &key);
}

/* Trying to guess hash in use by stat data extention */
static reiser4_plug_t *dir40_hash(object_entity_t *entity, rid_t pid) {
	dir40_t *dir;
	
	dir = (dir40_t *)entity;
	
	if (pid == INVAL_PID) {
		/* This function should inspect stat data extentions first. And
		   only if they do not contain a convenient plugin extention
		   (hash plugin), it should use some default hash plugin id. */
		return core->factory_ops.ifind(HASH_PLUG_TYPE, HASH_R5_ID);
	} else {
		/* Getting hash plugin by its id */
		return core->factory_ops.ifind(HASH_PLUG_TYPE, pid);
	}
}

/* Switches current dir body item onto next one */
static lookup_t dir40_next(object_entity_t *entity) {
	dir40_t *dir;
	place_t next;

#ifndef ENABLE_STAND_ALONE
	place_t *place;
#endif

	entry_hint_t entry;

	aal_assert("umka-2063", entity != NULL);
	
	dir = (dir40_t *)entity;
	dir->body.pos.unit = dir->unit;
	
	/* Getting next directory item */
	if (core->tree_ops.next(dir->obj.tree,
				&dir->body, &next))
	{
		return ABSENT;
	}

	if (!dir40_mergeable(entity, &next))
		return ABSENT;

	dir->unit = 0;
	
	obj40_relock(&dir->obj, &dir->body, &next);
	aal_memcpy(&dir->body, &next, sizeof(next));
	
#ifndef ENABLE_STAND_ALONE
	place = &dir->body;
	
	/* Updating current position by entry offset key */
	if (plug_call(place->plug->o.item_ops, read,
		      place, &entry, dir->unit, 1) == 1)
	{
		aal_memcpy(&dir->offset, &entry.offset,
			   sizeof(dir->offset));
	}
#endif
	
	return PRESENT;
}

/* Reads n entries to passed buffer buff */
static errno_t dir40_readdir(object_entity_t *entity, 
			     entry_hint_t *entry)
{
	dir40_t *dir;
	uint32_t units;
	place_t *place;

	aal_assert("umka-844", entity != NULL);
	aal_assert("umka-845", entry != NULL);

	dir = (dir40_t *)entity;
	place = &dir->body;

	/* Getting directory size from statdata item */
	if (dir40_size(entity) == 0)
		return -EINVAL;

	units = plug_call(place->plug->o.item_ops,
			  units, place);

	if (dir->unit >= units)
		return -EINVAL;

	/* Reading piece of data */
	if (plug_call(place->plug->o.item_ops, read,
		      place, entry, dir->unit, 1) == 1)
	{
#ifndef ENABLE_STAND_ALONE
		entry->type = ET_NAME;
		
		if (aal_strlen(entry->name) == 1 &&
		    !aal_strncmp(entry->name, ".", 1))
		{
			entry->type = ET_SELF;
		}

		if (aal_strlen(entry->name) == 2 &&
			   !aal_strncmp(entry->name, "..",2))
		{
			entry->type = ET_PARENT;
		}
#endif
		
		/* Updating positions */
		dir->unit++;
		
		/* Getting next direntry item */
		if (dir->unit >= units)
			dir40_next(entity);
#ifndef ENABLE_STAND_ALONE
		else {
			entry_hint_t ent;
			
			if (plug_call(place->plug->o.item_ops, read,
				      place, &ent, dir->unit, 1) != 1)
			{
				return -EINVAL;
			}

			aal_memcpy(&dir->offset, &ent.offset,
				   sizeof(dir->offset));
		}
#endif
	
		return 0;
	}

	return -EINVAL;
}

/* Makes lookup in directory by name. Fills passed buff by found entry fields
   (offset key, object key, etc). */
lookup_t dir40_lookup(object_entity_t *entity,
		      char *name, entry_hint_t *entry) 
{
	dir40_t *dir;
	place_t next;
	lookup_t res;

	oid_t locality;
	oid_t objectid;

	place_t *place;
	key_entity_t key;

	aal_assert("umka-1118", name != NULL);
	aal_assert("umka-1117", entity != NULL);

	dir = (dir40_t *)entity;

	locality = obj40_locality(&dir->obj);
	objectid = obj40_objectid(&dir->obj);

	/* Preparing key to be used for lookup. It is generating from the
	   directory oid, locality and name by menas of using hash plugin. */
	plug_call(STAT_KEY(&dir->obj)->plug->o.key_ops, build_entry,
		  &key, dir->hash, locality, objectid, name);

	/* Looking for @wan */
	switch ((res = obj40_lookup(&dir->obj, &key,
				    LEAF_LEVEL, &next)))
	{
	case ABSENT:
	case FAILED:
		return res;
	default:
		break;
	}

	obj40_relock(&dir->obj, &dir->body, &next);
	aal_memcpy(&dir->body, &next, sizeof(dir->body));

#ifndef ENABLE_STAND_ALONE
	if (entry) {
		plug_call(key.plug->o.key_ops, assign,
			  &dir->offset, &entry->offset);
	}
#endif

	dir->unit = dir->body.pos.unit;
	
	if (dir->unit == MAX_UINT32)
		dir->unit = 0;

	/* Entry is null, it is probebly the case when we want just know, that
	   passed @name exists. */
	if (entry == NULL)
		return PRESENT;

	place = &dir->body;

	/* If needed entry is found, we fetch it into local buffer and get stat
	   data key of the object it points to from it. This key will be used
	   for searching next entry in passed path and so on. */
	entry->object.plug = key.plug;
	entry->offset.plug = key.plug;
	
	if (plug_call(place->plug->o.item_ops, read,
		      place, entry, dir->unit, 1) != 1)
	{
		aal_exception_error("Can't read %u entry from "
				    "object 0x%llx.", dir->unit,
				    obj40_objectid(&dir->obj));
		return FAILED;
	}

#ifdef ENABLE_COLLISIONS
	if (aal_strncmp(entry->name, name, aal_strlen(name)) == 0) {
		aal_memcpy(&dir->offset, &key, sizeof(dir->offset));
		return PRESENT;
	}

	aal_exception_warn("Hash collision is detected between "
			   "%s and %s. Sequentional search is "
			   "started.", entry->name, name);

	if (!place->plug->o.item_ops->units)
		return FAILED;
			
	/* Sequentional search of the needed entry by its name */
	for (; dir->unit < place->plug->o.item_ops->units(place);
	     dir->unit++)
	{
		if (plug_call(place->plug->o.item_ops, read,
			      place, entry, dir->unit, 1) != 1)
		{
			aal_exception_error("Can't read %u entry "
					    "from object 0x%llx.",
					    dir->unit, objectid);
			return FAILED;
		}

		if (!aal_strncmp(entry->name, name, aal_strlen(name))) {
			aal_memcpy(&dir->offset, &key, sizeof(key));
			return PRESENT;
		}
	}
				
	return ABSENT;
#else
	return PRESENT;
#endif
}

/* Initializing dir40 instance by stat data place, resetring directory be means
   of using dir40_reset function and return instance to caller. */
static object_entity_t *dir40_open(object_info_t *info) {
	dir40_t *dir;

	aal_assert("umka-836", info != NULL);
	aal_assert("umka-837", info->tree != NULL);
	
	if (info->start.plug->id.group != STATDATA_ITEM)
		return NULL;
	
	if (obj40_pid(&info->start) != dir40_plug.id.id)
		return NULL;

	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return NULL;

	/* Initializing obj handle for the directory */
	obj40_init(&dir->obj, &dir40_plug, &info->start.key, 
		   core, info->tree);

	/* Guessing hash plugin basing on stat data */
	if (!(dir->hash = dir40_hash((object_entity_t *)dir, INVAL_PID))) {
                aal_exception_error("Can't guess hash plugin for directory "
				    "%llx.", obj40_objectid(&dir->obj));
                goto error_free_dir;
        }

	/* Initialziing statdata place */
	aal_memcpy(&dir->obj.statdata, &info->start,
		   sizeof(info->start));
	
	obj40_lock(&dir->obj, &dir->obj.statdata);

	/* Positioning to the first directory unit */
	if (dir40_reset((object_entity_t *)dir))
		goto error_free_dir;
	
	return (object_entity_t *)dir;

 error_free_dir:
	aal_free(dir);
	return NULL;
}

#ifndef ENABLE_STAND_ALONE
/* Creates dir40 instance and inserts few item in new directory described by
   passed @hint. */
static object_entity_t *dir40_create(object_info_t *info,
				     object_hint_t *hint)
{
	dir40_t *dir;

	entry_hint_t *body;
	entry_hint_t *entry;
	statdata_hint_t stat;

	create_hint_t body_hint;
	create_hint_t stat_hint;
   
	uint64_t ordering;
	sdext_lw_hint_t lw_ext;
	oid_t objectid, locality;
	sdext_unix_hint_t unix_ext;
	
	reiser4_plug_t *stat_plug;
	reiser4_plug_t *body_plug;
    
	aal_assert("umka-835", info != NULL);
	aal_assert("umka-1739", hint != NULL);
	aal_assert("vpf-1095", info->tree != NULL);

	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return NULL;
	
	/* Preparing dir oid and locality */
	locality = plug_call(info->object.plug->o.key_ops,
			     get_locality, &info->object);
	
	objectid = plug_call(info->object.plug->o.key_ops,
			     get_objectid, &info->object);

	ordering = plug_call(info->object.plug->o.key_ops,
			     get_ordering, &info->object);
	
	/* Key contains valid locality and objectid only, build start key */
	plug_call(info->object.plug->o.key_ops, build_gener,
		  &info->object, KEY_STATDATA_TYPE, locality,
		  ordering, objectid, 0);

	/* Initializing obj handle */
	obj40_init(&dir->obj, &dir40_plug, &info->object,
		   core, info->tree);

	/* Getting hash plugin */
	if (!(dir->hash = dir40_hash((object_entity_t *)dir,
				     hint->body.dir.hash)))
	{
		aal_exception_error("Can't find hash plugin by its "
				    "id 0x%x.", hint->body.dir.hash);
		goto error_free_dir;
	}

	/* Getting item plugins for statdata and body */
	if (!(stat_plug = core->factory_ops.ifind(ITEM_PLUG_TYPE, 
						  hint->statdata)))
	{
		aal_exception_error("Can't find stat data item plugin "
				    "by its id 0x%x.", hint->statdata);

		goto error_free_dir;
	}
   
	if (!(body_plug = core->factory_ops.ifind(ITEM_PLUG_TYPE, 
						  hint->body.dir.direntry)))
	{
		aal_exception_error("Can't find direntry item plugin by "
				    "its id 0x%x.", hint->body.dir.direntry);
		goto error_free_dir;
	}
    
	aal_memset(&stat_hint, 0, sizeof(stat_hint));
	aal_memset(&body_hint, 0, sizeof(body_hint));
	
	/* Initializing direntry item hint. This should be done before the stat
	   data item hint, because we will need size of direntry item durring
	   stat data initialization. */
   	body_hint.count = 1;
	body_hint.plug = body_plug;
	
	plug_call(info->object.plug->o.key_ops, build_entry,
		  &body_hint.key, dir->hash, locality, objectid, ".");

	if (!(body = aal_calloc(body_hint.count * sizeof(*body), 0)))
		goto error_free_dir;

	entry = body;
	
	/* Preparing hint for the empty directory. It consists only "." for
	   unlinked directories. */
	aal_strncpy(entry->name, ".", 1);

	/* Building key for the statdata of object new entry will point to. */
	plug_call(info->object.plug->o.key_ops, build_gener,
		  &entry->object, KEY_STATDATA_TYPE, locality,
		  ordering, objectid, 0);

	/* Building key for the hash new entry will have */
	plug_call(info->object.plug->o.key_ops, build_entry,
		  &entry->offset, dir->hash, locality, objectid,
		  entry->name);
	
	body_hint.type_specific = body;

	/* Initializing stat data hint */
	stat_hint.count = 1;
	stat_hint.plug = stat_plug;
    
	plug_call(info->object.plug->o.key_ops, assign,
		  &stat_hint.key, &info->object);
    
	/* Initializing stat data item hint. It uses unix extention and light
	   weight one. So we set up the mask in corresponding maner. */
	stat.extmask = (1 << SDEXT_UNIX_ID) | (1 << SDEXT_LW_ID);

	/* Light weight hint initializing. New directory will have two links on
	   it, because of dot entry which points onto directory itself and entry
	   in parent directory, which points to this new directory. */
	lw_ext.nlink = 1;
	lw_ext.mode = S_IFDIR | 0755;
	lw_ext.size = body_hint.count;

	/* Unix extention hint initializing */
	unix_ext.rdev = 0;
	unix_ext.uid = getuid();
	unix_ext.gid = getgid();
	
	unix_ext.atime = time(NULL);
	unix_ext.mtime = unix_ext.atime;
	unix_ext.ctime = unix_ext.atime;

	/* Estimating body item and setting up "bytes" field from the unix
	   extetion. */
	if (plug_call(body_plug->o.item_ops, estimate_insert,
		      NULL, &body_hint, MAX_UINT32))
	{
		aal_exception_error("Can't estimate directory item.");
		goto error_free_body;
	}
    
	unix_ext.bytes = body_hint.len;
    
	aal_memset(&stat.ext, 0, sizeof(stat.ext));
    
	stat.ext[SDEXT_LW_ID] = &lw_ext;
	stat.ext[SDEXT_UNIX_ID] = &unix_ext;

	stat_hint.type_specific = &stat;

	/* Looking for place to insert directory stat data */
	switch (obj40_lookup(&dir->obj, &stat_hint.key,
			     LEAF_LEVEL, &dir->obj.statdata))
	{
	case FAILED:
	case PRESENT:
		goto error_free_body;
	default:
		break;
	}
	
	/* Inserting stat data and body into the tree */
	if (obj40_insert(&dir->obj, &stat_hint,
			 LEAF_LEVEL, &dir->obj.statdata))
	{
		goto error_free_body;
	}
	
	/* Saving stat data place insert function has returned */
	aal_memcpy(&info->start, &dir->obj.statdata,
		   sizeof(info->start));
	
	obj40_lock(&dir->obj, &dir->obj.statdata);

	/* Looking for place to insert directory body */
	switch (obj40_lookup(&dir->obj, &body_hint.key,
			     LEAF_LEVEL, &dir->body))
	{
	case FAILED:
	case PRESENT:
		goto error_free_body;
	default:
		break;
	}
	
	/* Inserting the direntry item into the tree */
	if (obj40_insert(&dir->obj, &body_hint,
			 LEAF_LEVEL, &dir->body))
	{
		goto error_free_body;
	}

	/* Initializing @dir->offset by key of the "." entry */
	plug_call(STAT_KEY(&dir->obj)->plug->o.key_ops,
		  build_entry, &dir->offset, dir->hash,
		  locality, objectid, ".");
	
	obj40_lock(&dir->obj, &dir->body);
	aal_free(body);

	return (object_entity_t *)dir;

 error_free_body:
	aal_free(body);
 error_free_dir:
	aal_free(dir);
	return NULL;
}

/* Removes all directory body items */
static errno_t dir40_truncate(object_entity_t *entity,
			      uint64_t n)
{
	errno_t res;
	dir40_t *dir;
	key_entity_t key;

	aal_assert("umka-1925", entity != NULL);

	dir = (dir40_t *)entity;

	/* Releasing current body item */
	obj40_relock(&dir->obj, &dir->body, NULL);
	
	/* Getting maximal possible key form directory item. We will use it for
	   removing last item and so on util directro contains no items. Thanks
	   to Nikita for this idea. */
	plug_call(dir->body.plug->o.item_ops, maxposs_key, &dir->body, &key);

	while (1) {
		place_t place;

		/* Looking for the last directory item */
		if ((obj40_lookup(&dir->obj, &key, LEAF_LEVEL,
				  &place)) == FAILED)
		{
			return -EINVAL;
		}

		/* Checking if found item belongs this directory */
		if (!dir40_mergeable(entity, &place))
			return 0;

		/* Removing item from the tree */
		if ((res = obj40_remove(&dir->obj, &place, 1)))
			return res;
	}
	
	return 0;
}

static errno_t dir40_clobber(object_entity_t *entity) {
	errno_t res;
	dir40_t *dir;
	
	aal_assert("umka-2298", entity != NULL);

	if ((res = dir40_truncate(entity, 0)))
		return res;

	dir = (dir40_t *)entity;

	if (obj40_stat(&dir->obj))
		return -EINVAL;

	return obj40_remove(&dir->obj, &dir->obj.statdata, 1);
}

static key_entity_t *dir40_origin(object_entity_t *entity) {
	return STAT_KEY(&((dir40_t *)entity)->obj);
}

static errno_t dir40_attach(object_entity_t *entity,
			    object_entity_t *parent)
{
	errno_t res;
	dir40_t *dir;
	key_entity_t *obj;
	entry_hint_t entry;
	
	aal_assert("umka-2289", entity != NULL);
	aal_assert("umka-2359", parent != NULL);

	dir = (dir40_t *)entity;
	aal_strncpy(entry.name, "..", 2);

	if (dir40_lookup(entity, entry.name, NULL) == ABSENT) {

		/* Getting first item key from the parent object */
		if (!(obj = plug_call(parent->plug->o.object_ops,
				      origin, parent)))
		{
			return -EINVAL;
		}
		
		/* Adding ".." to child */
		plug_call(STAT_KEY(&dir->obj)->plug->o.key_ops,
			  assign, &entry.object, obj);

		if ((res = plug_call(entity->plug->o.object_ops,
				     add_entry, entity, &entry)))
		{
			return res;
		}
	} else
		return -EINVAL;

	/* Increasing parent's @nlink by one */
	return plug_call(parent->plug->o.object_ops, link, parent);
}

static errno_t dir40_detach(object_entity_t *entity,
			    object_entity_t *parent)
{
	errno_t res;
	dir40_t *dir;
	entry_hint_t entry;

	aal_assert("umka-2291", entity != NULL);

	dir = (dir40_t *)entity;

	/* Removing ".." from child if it is found */
	switch (plug_call(entity->plug->o.object_ops,
			  lookup, entity, "..", &entry))
	{
	case PRESENT:
		plug_call(entity->plug->o.object_ops,
			  rem_entry, entity, &entry);

	default:
		break;
	}

	if (parent) {
		/* Decreasing parent's @nlink by one */
		return plug_call(parent->plug->o.object_ops,
				 unlink, parent);
	}

	return 0;
}

static uint32_t dir40_links(object_entity_t *entity) {
	aal_assert("umka-2294", entity != NULL);
	return obj40_get_nlink(&((dir40_t *)entity)->obj);
}

static errno_t dir40_link(object_entity_t *entity) {
	dir40_t *dir;
	
	aal_assert("umka-1908", entity != NULL);

	dir = (dir40_t *)entity;
	
	/* Updating stat data place */
	if (obj40_stat(&dir->obj))
		return -EINVAL;
	
	return obj40_link(&dir->obj, 1);
}

static errno_t dir40_unlink(object_entity_t *entity) {
	dir40_t *dir;
	
	aal_assert("umka-1907", entity != NULL);

	dir = (dir40_t *)entity;
	
	if (obj40_stat(&dir->obj))
		return -EINVAL;
	
	return obj40_link(&dir->obj, -1);
}

/* Estimates directory operations (add_entry, rem_entry) */
static uint32_t dir40_estimate(object_entity_t *entity, 
			       entry_hint_t *entry)
{
	dir40_t *dir;
	create_hint_t hint;

	dir = (dir40_t *)entity;
	aal_memset(&hint, 0, sizeof(hint));

	hint.count = 1;
	hint.type_specific = entry;
	hint.plug = dir->body.plug;
	hint.key.plug = STAT_KEY(&dir->obj)->plug;

	if (plug_call(hint.plug->o.item_ops,
			estimate_insert, NULL, &hint, 0))
	{
		aal_exception_error("Can't estimate directory "
				    "operation.");
	}

	return hint.len;
}

/* Updates body item */
static errno_t dir40_update(object_entity_t *entity) {
	dir40_t *dir;
	place_t place;
	key_entity_t *key;
	
	dir = (dir40_t *)entity;
	key = &dir->body.key;
	
	/* Looking for stat data place by */
	switch (obj40_lookup(&dir->obj, key,
			     LEAF_LEVEL, &place))
	{
	case PRESENT:
		obj40_relock(&dir->obj, &dir->body,
			     &place);

		aal_memcpy(&dir->body, &place,
			   sizeof(place));
		return 0;
	default:
		return -EINVAL;
	}
}

/* Adding new entry */
static errno_t dir40_add_entry(object_entity_t *entity, 
			       entry_hint_t *entry)
{
	errno_t res;
	dir40_t *dir;
	place_t place;
	uint64_t size;
	uint32_t atime;

	key_entity_t *key;
	create_hint_t hint;
	
	oid_t locality, objectid;
	sdext_unix_hint_t unix_hint;

	aal_assert("umka-844", entity != NULL);
	aal_assert("umka-845", entry != NULL);

	dir = (dir40_t *)entity;
	key = STAT_KEY(&dir->obj);
	
	aal_memset(&hint, 0, sizeof(hint));
	
	hint.count = 1;
	hint.plug = dir->body.plug;
	hint.type_specific = (void *)entry;

	/* Building key of the new entry */
	locality = obj40_locality(&dir->obj);
	objectid = obj40_objectid(&dir->obj);
	
	plug_call(key->plug->o.key_ops, build_entry, &hint.key,
		  dir->hash, locality, objectid, entry->name);
	
	plug_call(key->plug->o.key_ops, assign, &entry->offset,
		  &hint.key);

	/* Looking for place to insert directory entry */
	switch (obj40_lookup(&dir->obj, &hint.key,
			     LEAF_LEVEL, &place))
	{
	case PRESENT:
		aal_exception_error("Entry %s already exists.",
				    entry->name);
		return -EINVAL;
	case FAILED:
		aal_exception_error("Lookup failed while adding "
				    "entry.");
		return -EINVAL;
	default:
		break;
	}
	
	/* Inserting entry */
	if ((res = obj40_insert(&dir->obj, &hint,
				LEAF_LEVEL, &place)))
	{
		aal_exception_error("Can't insert entry %s to "
				    "tree.", entry->name);
		return res;
	}

	/* Updating stat data place */
	if ((res = obj40_stat(&dir->obj)))
		return res;
	
	/* Updating stat data fields */
	size = obj40_get_size(&dir->obj);

	if ((res = obj40_set_size(&dir->obj, size + 1)))
		return res;

	if ((res = obj40_read_ext(&dir->obj.statdata, SDEXT_UNIX_ID, 
				  &unix_hint)))
		return res;
	
	atime = time(NULL);

	unix_hint.atime = atime;
	unix_hint.mtime = atime;
	unix_hint.bytes += dir40_estimate(entity, entry);

	if ((res = obj40_write_ext(&dir->obj.statdata, SDEXT_UNIX_ID, 
				   &unix_hint)))
		return res;

	/* Updating body place */
	return dir40_update(entity);
}

/* Removing entry from the directory */
static errno_t dir40_rem_entry(object_entity_t *entity,
			       entry_hint_t *entry)
{
	errno_t res;
	dir40_t *dir;
	uint64_t size;
	uint32_t units;
	uint32_t atime;

	entry_hint_t ent;

	place_t place;
	key_entity_t *key;

	oid_t locality, objectid;
	sdext_unix_hint_t unix_hint;
	
	aal_assert("umka-1922", entity != NULL);
	aal_assert("umka-1923", entry != NULL);

	dir = (dir40_t *)entity;
	key = STAT_KEY(&dir->obj);

	/* Generating key of the entry to be removed */
	locality = obj40_locality(&dir->obj);
	objectid = obj40_objectid(&dir->obj);
	
	plug_call(key->plug->o.key_ops, build_entry,
		  &entry->offset, dir->hash, locality,
		  objectid, entry->name);

	/* Looking for place to insert directory entry */
	switch (obj40_lookup(&dir->obj, &entry->offset,
			     LEAF_LEVEL, &place))
	{
	case FAILED:
	case ABSENT:
		return -EINVAL;
	default:
		break;
	}
	
	/* Removing one unit from directory */
	if ((res = obj40_remove(&dir->obj, &place, 1)))
		return res;

	/* Updating directory body  */
	dir40_update(entity);
	
	units = plug_call(dir->body.plug->o.item_ops,
			  units, &dir->body);

	/* Getting next direntry item */
	if (dir->unit >= units)
		dir40_next(entity);
	else {
		if (place.pos.unit == dir->unit) {
			/* Updating current position by entry offset key */
			if (plug_call(dir->body.plug->o.item_ops, read,
				      &dir->body, &ent, dir->unit, 1) == 1)
			{
				aal_memcpy(&dir->offset, &ent.offset,
					   sizeof(dir->offset));
			}
		}
		
		if (place.pos.unit < dir->unit)
			dir->unit--;
	}

	/* Updating stat data place */
	if ((res = obj40_stat(&dir->obj)))
		return res;
	
	/* Updating size field */
	size = obj40_get_size(&dir->obj);

	if ((res = obj40_set_size(&dir->obj, size - 1)))
		return res;

	if ((res = obj40_read_ext(&dir->obj.statdata, SDEXT_UNIX_ID, 
				  &unix_hint)))
		return res;
	
	atime = time(NULL);
	
	unix_hint.atime = atime;
	unix_hint.mtime = atime;
	unix_hint.bytes -= dir40_estimate(entity, entry);

	return obj40_write_ext(&dir->obj.statdata, SDEXT_UNIX_ID, &unix_hint);
}

struct layout_hint {
	object_entity_t *entity;
	block_func_t block_func;
	void *data;
};

typedef struct layout_hint layout_hint_t;

static errno_t callback_item_data(void *object, uint64_t start,
				  uint64_t count, void *data)
{
	blk_t blk;
	errno_t res;

	place_t *place = (place_t *)object;
	layout_hint_t *hint = (layout_hint_t *)data;

	for (blk = start; blk < start + count; blk++) {
		if ((res = hint->block_func(hint->entity, blk,
					    hint->data)))
		{
			return res;
		}
	}

	return 0;
}

/* Layout function implementation. It is needed for printing, fragmentation
   calculating, etc. */
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
	hint.block_func = block_func;

	dir = (dir40_t *)entity;
	
	while (1) {
		place_t *place = &dir->body;
		
		if (place->plug->o.item_ops->layout) {
			/* Calling item's layout method */
			if ((res = plug_call(place->plug->o.item_ops, layout,
					     place, callback_item_data, &hint)))
			{
				return res;
			}
		} else {
			if ((res = callback_item_data(place, place->con.blk,
						      1, &hint)))
			{
				return res;
			}
		}

		dir->unit = MAX_UINT32;
		
		if (dir40_next(entity) != PRESENT)
			return 0;
	}
    
	return 0;
}

/* Metadata function implementation. It traverses all directory items and calls
   @func for each of them. It is needed for printing, fragmentation calculating,
   etc. */
static errno_t dir40_metadata(object_entity_t *entity,
			      place_func_t place_func,
			      void *data)
{
	errno_t res = 0;
	dir40_t *dir = (dir40_t *)entity;
	
	aal_assert("umka-1712", entity != NULL);
	aal_assert("umka-1713", place_func != NULL);
	
	if ((res = place_func(entity, &dir->obj.statdata, data)))
		return res;

	while (1) {
		if ((res = place_func(entity, &dir->body, data)))
			return res;
		
		dir->unit = MAX_UINT32;
		
		if (dir40_next(entity) != PRESENT)
			return 0;
	}
	
	return res;
}

extern object_entity_t *dir40_realize(object_info_t *info);

extern errno_t dir40_check_attach(object_entity_t *object, 
				  object_entity_t *parent, 
				  uint8_t mode);
#endif

/* Freeing dir40 instance. That is unlocking nodes current statdata and body lie
   in and freeing all occpied memory. */
static void dir40_close(object_entity_t *entity) {
	dir40_t *dir = (dir40_t *)entity;
	
	aal_assert("umka-750", entity != NULL);

	obj40_relock(&dir->obj, &dir->obj.statdata, NULL);
	obj40_relock(&dir->obj, &dir->body, NULL);

	aal_free(entity);
}

static reiser4_object_ops_t dir40_ops = {
#ifndef ENABLE_STAND_ALONE
	.create		= dir40_create,
	.layout		= dir40_layout,
	.metadata	= dir40_metadata,
	.link		= dir40_link,
	.unlink		= dir40_unlink,
	.links		= dir40_links,
	.truncate	= dir40_truncate,
	.add_entry	= dir40_add_entry,
	.rem_entry	= dir40_rem_entry,
	.attach		= dir40_attach,
	.detach		= dir40_detach,
	.clobber	= dir40_clobber,
	.origin         = dir40_origin,
	.realize	= dir40_realize,
	
	.seek		= NULL,
	.write		= NULL,
	
	.check_struct	= NULL,
	.check_attach	= dir40_check_attach,
#endif
	.follow		= NULL,
	.read		= NULL,
	.offset		= NULL,
	
	.open		= dir40_open,
	.close		= dir40_close,
	.reset		= dir40_reset,
	.lookup		= dir40_lookup,
	.size		= dir40_size,
	.seekdir	= dir40_seekdir,
	.readdir	= dir40_readdir,

#ifndef ENABLE_STAND_ALONE
	.telldir	= dir40_telldir,
#else
	.telldir	= NULL
#endif
};

reiser4_plug_t dir40_plug = {
	.cl    = CLASS_INIT,
	.id    = {OBJECT_DIRTORY40_ID, DIRTORY_OBJECT, OBJECT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "dir40",
	.desc  = "Compound directory for reiser4, ver. " VERSION,
	.data  = NULL,
#endif
	.o = {
		.object_ops = &dir40_ops
	}
};

static reiser4_plug_t *dir40_start(reiser4_core_t *c) {
	core = c;
	return &dir40_plug;
}

plug_register(dir40, dir40_start, NULL);
