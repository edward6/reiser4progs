/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40.c -- reiser4 default directory object plugin. */

#ifndef ENABLE_STAND_ALONE
#  include <time.h>
#  include <unistd.h>
#endif

#include "dir40.h"

reiser4_core_t *dcore = NULL;
extern reiser4_plug_t dir40_plug;

/* Gets size from the object stat data */
static uint64_t dir40_size(object_entity_t *entity) {
	dir40_t *dir;

	aal_assert("umka-2277", entity != NULL);
	
	dir = (dir40_t *)entity;
	
	/* Updating stat data place */
	if (obj40_update(&dir->obj))
		return -EINVAL;

	return obj40_get_size(&dir->obj);
}

static void dir40_close(object_entity_t *entity) {
	aal_assert("umka-750", entity != NULL);
	aal_free(entity);
}

#ifndef ENABLE_STAND_ALONE
static errno_t dir40_telldir(object_entity_t *entity,
			     key_entity_t *offset)
{
	dir40_t *dir = (dir40_t *)entity;
	
	aal_assert("umka-1985", entity != NULL);
	aal_assert("umka-1986", offset != NULL);

	/* Getting current dir key and adjust */
	plug_call(dir->offset.plug->o.key_ops,
		  assign, offset, &dir->offset);

#ifdef ENABLE_COLLISIONS
	offset->adjust = dir->adjust;
#endif
	
	return 0;
}
#endif

/* This fucntion checks if passed @place point to item related to @entity, that
   is belong to directory. */
static int32_t dir40_belong(object_entity_t *entity,
			    place_t *place)
{
	dir40_t *dir = (dir40_t *)entity;

	/* Checking if item component in @place->pos is valid one. This is
	   needed because tree_lookup() does not fetch item data at place if it
	   was not found. So, it may point to unexistent item and we should
	   check this here. */
	if (!dcore->tree_ops.valid(dir->obj.info.tree, place))
		return 0;

	/* Fetching item info at @place */
	if (obj40_fetch(&dir->obj, place))
		return 0;
	
	/* Checking if item plugins are the same. This is needed, because item
	   method mergeable() expects to have items of the same type. */
	if (!plug_equal(dir->body.plug, place->plug))
		return 0;

	/* Calling item mergeable() method in order to determine if items are
	   mergeable. Directory items will check only locality component of
	   keys. If it the same for both arguments, items are mergeable, as
	   belong to the same directory. */
	return plug_call(dir->body.plug->o.item_ops,
			 mergeable, &dir->body, place);
}

static errno_t dir40_seekdir(object_entity_t *entity,
			     key_entity_t *offset)
{
	dir40_t *dir;
	
	aal_assert("umka-1983", entity != NULL);
	aal_assert("umka-1984", offset != NULL);

	dir = (dir40_t *)entity;

#ifdef ENABLE_COLLISIONS
	dir->adjust = offset->adjust;
#endif
	
	plug_call(dir->offset.plug->o.key_ops,
		  assign, &dir->offset, offset);

	return 0;
}

/* Resets internal direntry position at zero */
errno_t dir40_reset(object_entity_t *entity) {
	dir40_t *dir = (dir40_t *)entity;
    
	aal_assert("umka-864", entity != NULL);
	
	/* Preparing key of the first entry in directory and set directory
	   adjust to zero. */
#ifdef ENABLE_COLLISIONS
	dir->adjust = 0;
#endif
	
	plug_call(STAT_KEY(&dir->obj)->plug->o.key_ops, build_entry,
		  &dir->offset, dir->hash, obj40_locality(&dir->obj),
		  obj40_objectid(&dir->obj), ".");

	return 0;
}

/* Fetches current unit to passed @entry */
errno_t dir40_fetch(object_entity_t *entity, entry_hint_t *entry) {
	dir40_t *dir;
	trans_hint_t hint;

	hint.count = 1;
	hint.specific = entry;
	dir = (dir40_t *)entity;

	/* Reading entry to passed @entry */
	if (plug_call(dir->body.plug->o.item_ops,
		      fetch, &dir->body, &hint) != 1)
	{
		return -EIO;
	}

	/* Copying entry place */
	aal_memcpy(&entry->place, &dir->body,
		   sizeof(place_t));

	return 0;
}

/* Switches current dir body item onto next one. Returns 1 on success, 0 on the
   case of directory is over and values < 0 on error. */
static int32_t dir40_next(object_entity_t *entity) {
	errno_t res;
	dir40_t *dir;
	place_t place;

	aal_assert("umka-2063", entity != NULL);

	dir = (dir40_t *)entity;
	
	/* Getting next directory item */
	if ((res = dcore->tree_ops.next(dir->obj.info.tree,
					&dir->body, &place)))
	{
		return res;
	}

	if (!place.node || !dir40_belong(entity, &place)) {
		uint64_t offset;
		
		/* Making offset pointed to nowhere in order to let know that
		   directory is over. */
		offset = plug_call(dir->offset.plug->o.key_ops,
				   get_offset, &dir->offset);
		
		plug_call(dir->offset.plug->o.key_ops, set_offset,
			  &dir->offset, offset + 1);
		
		return 0;
	}
	
	aal_memcpy(&dir->body, &place, sizeof(place));
	return 1;
}

/* Updates current body place by place found by @dir->offset and @dir->adjust */
static int32_t dir40_update(object_entity_t *entity) {
	dir40_t *dir;
	lookup_res_t res;
	
	dir = (dir40_t *)entity;
	
	switch ((res = obj40_lookup(&dir->obj, &dir->offset,
				    LEAF_LEVEL, EXACT, &dir->body)))
	{
	case FAILED:
		return -EINVAL;
	default: {
#ifdef ENABLE_COLLISIONS
		uint32_t units;
		uint32_t adjust;
#endif
		/* Correcting unit pos for next body item */
		if (dir->body.pos.unit == MAX_UINT32)
			dir->body.pos.unit = 0;

		if (res == ABSENT) {

			/* Directory is over */
			if (!dir40_belong(entity, &dir->body))
				return 0;

			/* Checking if directory is over */
			units = plug_call(dir->body.plug->o.item_ops,
					  units, &dir->body);
			
			if (dir->body.pos.unit >= units)
				return 0;
		}

#ifdef ENABLE_COLLISIONS
		/* Adjusting current position by key's adjust. This is needed
		   for working fine when key collitions take place. */
		for (adjust = dir->adjust; adjust;) {
			uint32_t off = adjust;

			units = plug_call(dir->body.plug->o.item_ops,
					  units, &dir->body);
			
			if (off > units - 1 - dir->body.pos.unit)
				off = units - dir->body.pos.unit;

			dir->body.pos.unit += off - 1;

			if ((adjust -= off) > 0) {
				switch (dir40_next(entity)) {
				case 1:
					break;
				case 0:
					return 0;
				default:
					return -EINVAL;
				}
			}
		}
#endif
		return 1;
	}
	}
}

/* Reads one entry from passed @entity */
static int32_t dir40_readdir(object_entity_t *entity, 
			     entry_hint_t *entry)
{
	errno_t res;
	dir40_t *dir;
	uint32_t units;

	aal_assert("umka-845", entry != NULL);
	aal_assert("umka-844", entity != NULL);

	dir = (dir40_t *)entity;

	/* Getting place of current unit */
	if ((res = dir40_update(entity)) != 1)
		return res;

	/* Reading next entry */
	if ((res = dir40_fetch(entity, entry)))
		return res;

	/* Setting up the entry type. It is essential for fsck to know
	   what is the NAME -- that needs to be traversed semantically
	   to be recovered completely -- and what is not -- that needs
	   some other special actions, e.g. check_attach for ".." (and
	   even "..." if it is needed one day), etc. */
#ifndef ENABLE_STAND_ALONE
	entry->type = ET_NAME;
		
	if (aal_strlen(entry->name) == 1 &&
	    !aal_strncmp(entry->name, ".", 1))
	{
		entry->type = ET_SPCL;
	}

	if (aal_strlen(entry->name) == 2 &&
	    !aal_strncmp(entry->name, "..",2))
	{
		entry->type = ET_SPCL;
	}
#endif

	units = plug_call(dir->body.plug->o.item_ops,
			  units, &dir->body);

	/* Getting next entry in odrer to set up @dir->offset correctly */
	if (++dir->body.pos.unit >= units) {
		/* Switching to the next directory item */
		if ((res = dir40_next(entity)) < 0)
			return res;
	} else {
		/* There is no needs to switch */
		res = 1;
	}

	if (res == 1) {
		entry_hint_t temp;
		
		if ((res = dir40_fetch(entity, &temp)))
			return res;

#ifdef ENABLE_COLLISIONS
		/* Taking care about adjust */
		if (!plug_call(temp.offset.plug->o.key_ops,
			       compfull, &temp.offset, &dir->offset))
		{
			temp.offset.adjust = dir->adjust + 1;
		} else {
			temp.offset.adjust = 0;
		}
#endif
		dir40_seekdir(entity, &temp.offset);
	}
	
	return 1;
}

static int32_t dir40_compent(char *entry1, char *entry2) {
	uint32_t len1 = aal_strlen(entry1);
	uint32_t len2 = aal_strlen(entry2);
                                                                                    
        if (len1 > len2)
                return 1;

	if (len1 < len2)
                return -1;

	return aal_strncmp(entry1, entry2, len1);
}

/* Makes lookup iside directory with specified position correcting mode. This is
   needed to be used in add_entry() for two reasons: for make sure, that passed
   entry does not exists and to use lookup result for consequent insert. As it
   is used for insert, we should specify pos correctring mode INST. */
static lookup_res_t dir40_search(object_entity_t *entity,
				 char *name, lookup_mod_t mode,
				 entry_hint_t *entry)
{
	dir40_t *dir;
	lookup_res_t res;

	aal_assert("umka-1118", name != NULL);
	aal_assert("umka-1117", entity != NULL);

	dir = (dir40_t *)entity;

	/* Preparing key to be used for lookup. It is generating from the
	   directory oid, locality and name by menas of using hash plugin. */
	plug_call(STAT_KEY(&dir->obj)->plug->o.key_ops, build_entry,
		  &dir->body.key, dir->hash, obj40_locality(&dir->obj),
		  obj40_objectid(&dir->obj), name);

	/* Making tree_lookup() to find entry by key */
	switch ((res = obj40_lookup(&dir->obj, &dir->body.key,
				    LEAF_LEVEL, mode, &dir->body)))
	{
	case PRESENT:
		/* Correcting unit pos */
		if (dir->body.pos.unit == MAX_UINT32)
			dir->body.pos.unit = 0;

		break;
	default:
		if (entry) {
			aal_memcpy(&entry->place, &dir->body,
				   sizeof(place_t));
		}
		
		return res;
	}

#ifdef ENABLE_COLLISIONS
	/* Key collisions handling. Sequentional search of the needed entry by
	   its name. */
	entry->offset.adjust = 0;
		
	while (1) {
		int32_t comp;
		uint32_t units;
		entry_hint_t temp;
		
		units = plug_call(dir->body.plug->o.item_ops,
				  units, &dir->body);

		if (dir->body.pos.unit >= units) {
			switch (dir40_next(entity)) {
			case 1:
				break;
			case 0:
				return ABSENT;
			default:
				return FAILED;
			}
		}
		
		if (dir40_fetch(entity, &temp))
			return FAILED;

		if (entry) {
			aal_memcpy(&entry->place, &temp.place,
				   sizeof(temp.place));
		}

		comp = dir40_compent(temp.name, name);
			
		if (comp < 0) {
			dir->body.pos.unit++;

			if (entry) {
				entry->offset.adjust++;
				entry->place.pos.unit++;
			}
				
		} else if (comp > 0) {
			return ABSENT;
		} else {
			if (entry) {
				aal_memcpy(entry, &temp,
					   sizeof(temp));
			}
				
			return PRESENT;
		}
	}
#else
	/* Fetching found entry to passed @entry */
	if (entry && dir40_fetch(entity, entry))
		return FAILED;

	return PRESENT;
#endif
}

/* Makes lookup inside @entity by passed @name */
lookup_res_t dir40_lookup(object_entity_t *entity,
			  char *name, entry_hint_t *entry) 
{
	return dir40_search(entity, name, EXACT, entry);
}

/* Initializing dir40 instance by stat data place, resetring directory be means
   of using dir40_reset function and return instance to caller. */
static object_entity_t *dir40_open(object_info_t *info) {
	dir40_t *dir;

	aal_assert("umka-836", info != NULL);
	aal_assert("umka-837", info->tree != NULL);
	
	if (info->start.plug->id.group != STATDATA_ITEM)
		return NULL;
	
	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return NULL;

	/* Initializing obj handle for the directory */
	obj40_init(&dir->obj, &dir40_plug, dcore, info);
	
	if (obj40_pid(&dir->obj, OBJECT_PLUG_TYPE,
		      "directory") !=  dir40_plug.id.id)
	{
		goto error_free_dir;
	}
	
	/* Guessing hash plugin basing on stat data */
	if (!(dir->hash = obj40_plug(&dir->obj, HASH_PLUG_TYPE, "hash")))
                goto error_free_dir;

	/* Positioning to the first directory unit */
	dir40_reset((object_entity_t *)dir);
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
	uint64_t mask;
	entry_hint_t entry;
	trans_hint_t body_hint;
	reiser4_plug_t *body_plug;
    
	aal_assert("umka-835", info != NULL);
	aal_assert("umka-1739", hint != NULL);
	aal_assert("vpf-1095", info->tree != NULL);

	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return NULL;
	
	/* Initializing obj handle */
	obj40_init(&dir->obj, &dir40_plug, dcore, info);

	/* Getting hash plugin */
	if (!(dir->hash = dcore->factory_ops.ifind(HASH_PLUG_TYPE, 
						   hint->body.dir.hash))) 
	{
		aal_exception_error("Can't find hash plugin by its "
				    "id 0x%x.", hint->body.dir.hash);
		goto error_free_dir;
	}
   
	if (!(body_plug = dcore->factory_ops.ifind(ITEM_PLUG_TYPE, 
						   hint->body.dir.direntry)))
	{
		aal_exception_error("Can't find direntry item plugin by "
				    "its id 0x%x.", hint->body.dir.direntry);
		goto error_free_dir;
	}
    
	aal_memset(&body_hint, 0, sizeof(body_hint));
	
	/* Initializing direntry item hint. This should be done before the stat
	   data item hint, because we will need size of direntry item durring
	   stat data initialization. */
   	body_hint.count = 1;
	body_hint.plug = body_plug;
	
	plug_call(info->object.plug->o.key_ops, build_entry,
		  &body_hint.key, dir->hash, obj40_locality(&dir->obj),
		  obj40_objectid(&dir->obj), ".");

	/* Preparing hint for the empty directory. It consists only "." for
	   unlinked directories. */
	aal_strncpy(entry.name, ".", 1);

	/* Initializing entry stat data key. */
	plug_call(info->object.plug->o.key_ops, assign,
		  &entry.object, &info->object);

	/* Initializing entry hash key. */
	plug_call(info->object.plug->o.key_ops, assign,
		  &entry.offset, &body_hint.key);

	body_hint.specific = &entry;
	
        /* Looking for place to insert directory body */
	switch (obj40_lookup(&dir->obj, &body_hint.key,
			     LEAF_LEVEL, CONV, &dir->body))
	{
	case ABSENT:
		/* Inserting the direntry item into the tree */
		if (obj40_insert(&dir->obj, &dir->body,
				 &body_hint, LEAF_LEVEL))
		{
			goto error_free_dir;
		}
		
		break;
	default:
		goto error_free_dir;
	}

	/* New directory will have two links on it, because of dot 
	   entry which points onto directory itself and entry in 
	   parent directory, which points to this new directory. */
	mask = (1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID);
	
	if (obj40_create_stat(&dir->obj, hint->statdata, mask,
			      1, body_hint.len, 1, S_IFDIR, NULL))
	{
		goto error_free_dir;
	}
	
	dir40_reset((object_entity_t *)dir);
	return (object_entity_t *)dir;
	
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

	/* Making sure, that dir->body points to correct item */
	if ((res = dir40_update(entity)) != 1)
		return res;

	/* Creating maximal possible key in order to find last directory item
	 * and remove it from the tree. Thanks to Nikita for this idea. */
	plug_call(dir->body.plug->o.key_ops,
		  set_offset, &key, MAX_UINT64);

	while (1) {
		place_t place;
		trans_hint_t hint;

		/* Looking for the last directory item */
		switch ((obj40_lookup(&dir->obj, &key, LEAF_LEVEL,
				      EXACT, &place)))
		{
		case FAILED:
			return -EINVAL;
		default:
			/* Checking if found item belongs this directory */
			if (!dir40_belong(entity, &place))
				return 0;

			hint.count = 1;
			
			/* Removing item from the tree */
			if ((res = obj40_remove(&dir->obj, &place, &hint)))
				return res;
		}
	}
	
	return 0;
}

/* Removes directory body and stat data from the tree */
static errno_t dir40_clobber(object_entity_t *entity) {
	errno_t res;
	dir40_t *dir;
	trans_hint_t hint;
		
	aal_assert("umka-2298", entity != NULL);

	if ((res = dir40_truncate(entity, 0)))
		return res;

	dir = (dir40_t *)entity;

	if (obj40_update(&dir->obj))
		return -EINVAL;

	hint.count = 1;
	
	return obj40_remove(&dir->obj,
			    STAT_PLACE(&dir->obj), &hint);
}

static errno_t dir40_attach(object_entity_t *entity,
			    object_entity_t *parent)
{
	errno_t res;
	dir40_t *dir;
	entry_hint_t entry;
	
	aal_assert("umka-2289", entity != NULL);
	aal_assert("umka-2359", parent != NULL);

	dir = (dir40_t *)entity;
	aal_strncpy(entry.name, "..", sizeof(entry.name));

	/* Adding ".." to child */
	plug_call(STAT_KEY(&dir->obj)->plug->o.key_ops,
		  assign, &entry.object, &parent->info.object);

	if ((res = plug_call(entity->plug->o.object_ops,
			     add_entry, entity, &entry)))
	{
		return res;
	}

	/* Increasing parent's @nlink by one */
	return plug_call(parent->plug->o.object_ops, link, parent);
}

static errno_t dir40_detach(object_entity_t *entity,
			    object_entity_t *parent)
{
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
		if (parent) {
			/* Decreasing parent's @nlink by one */
			return plug_call(parent->plug->o.object_ops,
					 unlink, parent);
		}

		return 0;
	}
}

static uint32_t dir40_links(object_entity_t *entity) {
	dir40_t *dir;
	
	aal_assert("umka-2294", entity != NULL);

	dir = (dir40_t *)entity;
	
	if (obj40_update(&dir->obj))
		return -EINVAL;
	
	return obj40_get_nlink(&((dir40_t *)entity)->obj);
}

static errno_t dir40_link(object_entity_t *entity) {
	dir40_t *dir;
	
	aal_assert("umka-1908", entity != NULL);

	dir = (dir40_t *)entity;
	
	if (obj40_update(&dir->obj))
		return -EINVAL;
	
	return obj40_link(&dir->obj, 1);
}

static errno_t dir40_unlink(object_entity_t *entity) {
	dir40_t *dir;
	
	aal_assert("umka-1907", entity != NULL);

	dir = (dir40_t *)entity;
	
	if (obj40_update(&dir->obj))
		return -EINVAL;
	
	return obj40_link(&dir->obj, -1);
}

/* Adding new entry */
static errno_t dir40_add_entry(object_entity_t *entity, 
			       entry_hint_t *entry)
{
	errno_t res;
	dir40_t *dir;
	uint64_t size;
	uint64_t bytes;
	
	entry_hint_t temp;
	trans_hint_t hint;

	aal_assert("umka-844", entity != NULL);
	aal_assert("umka-845", entry != NULL);

	dir = (dir40_t *)entity;
	aal_memset(&hint, 0, sizeof(hint));
	
	/* Getting place new entry will be inserted at */
	switch (dir40_search(entity, entry->name, EXACT, &temp)) {
	case ABSENT:
		if ((res = obj40_fetch(&dir->obj, &temp.place)))
			return res;
		
		break;
	default:
		return -EINVAL;
	}

	hint.count = 1;
	hint.plug = temp.place.plug;
	hint.specific = (void *)entry;

	/* Building key of the new entry and hint's one */
	plug_call(STAT_KEY(&dir->obj)->plug->o.key_ops, build_entry,
		  &entry->offset, dir->hash, obj40_locality(&dir->obj),
		  obj40_objectid(&dir->obj), entry->name);

	/* Copying key to @hint */
	plug_call(entry->offset.plug->o.key_ops, assign, &hint.key,
		  &entry->offset);

	/* Inserting entry described by @hint to tree at @temp.place */
	if ((res = obj40_insert(&dir->obj, &temp.place,
				&hint, LEAF_LEVEL)))
	{
		return res;
	}

	/* Updating stat data fields */
	entry->len = hint.len;
	size = dir40_size(entity) + 1;
	bytes = obj40_get_bytes(&dir->obj) + hint.bytes;
	return obj40_touch(&dir->obj, size, bytes, time(NULL));
}

/* Removing entry from the directory */
static errno_t dir40_rem_entry(object_entity_t *entity,
			       entry_hint_t *entry)
{
	errno_t res;
	dir40_t *dir;
	uint64_t size;
	uint64_t bytes;
	
	entry_hint_t temp;
	trans_hint_t hint;
	
	aal_assert("umka-1923", entry != NULL);
	aal_assert("umka-1922", entity != NULL);
	aal_assert("umka-2390", entry->name != NULL);

	dir = (dir40_t *)entity;

	/* Looking for place to insert directory entry */
	switch (dir40_search(entity, entry->name, EXACT, &temp)) {
	case PRESENT:
		hint.count = 1;
		
		/* Removing one unit from directory */
		if ((res = obj40_remove(&dir->obj, &temp.place, &hint)))
			return res;

		if (!plug_call(dir->offset.plug->o.key_ops,
			       compfull, &dir->offset, &temp.offset))
		{
			if (entry->offset.adjust < dir->adjust)
				dir->adjust--;
		}
		
		break;
	default:
		return -EINVAL;
	}

	/* Updating stat data fields */
	entry->len = hint.len;
	size = dir40_size(entity) - 1;
	bytes = obj40_get_bytes(&dir->obj) - hint.bytes;
	return obj40_touch(&dir->obj, size, bytes, time(NULL));
}

struct layout_hint {
	void *data;
	object_entity_t *entity;
	region_func_t region_func;
};

typedef struct layout_hint layout_hint_t;

static errno_t callback_item_layout(void *place, blk_t start,
				    count_t width, void *data)
{
	layout_hint_t *hint = (layout_hint_t *)data;

	return hint->region_func(hint->entity, start,
				 width, hint->data);
}

/* Layout function implementation. It is needed for printing, fragmentation
   calculating, etc. */
static errno_t dir40_layout(object_entity_t *entity,
			    region_func_t region_func,
			    void *data)
{
	errno_t res;
	dir40_t *dir;
	layout_hint_t hint;

	aal_assert("umka-1473", entity != NULL);
	aal_assert("umka-1474", region_func != NULL);

	dir = (dir40_t *)entity;
	
	if ((res = dir40_update(entity)) != 1)
		return res;

	hint.data = data;
	hint.entity = entity;
	hint.region_func = region_func;
	
	while (1) {
		place_t *place = &dir->body;
		
		if (dir->body.plug->o.item_ops->layout) {
			
			/* Calling item's layout method */
			if ((res = plug_call(place->plug->o.item_ops, layout,
					     place, callback_item_layout,
					     &hint)))
			{
				return res;
			}
		} else {
			blk_t blk = place->block->nr;
			
			if ((res = callback_item_layout(place, blk,
							1, &hint)))
			{
				return res;
			}
		}
		
		switch ((res = dir40_next(entity))) {
		case 1:
			break;
		case 0:
			return 0;
		default:
			return res;
		}
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
	errno_t res;
	dir40_t *dir;
	
	aal_assert("umka-1712", entity != NULL);
	aal_assert("umka-1713", place_func != NULL);
	
	dir = (dir40_t *)entity;
	
	if ((res = dir40_update(entity)) != 1)
		return res;
	
	if ((res = place_func(entity, STAT_PLACE(&dir->obj), data)))
		return res;

	while (1) {
		if ((res = place_func(entity, &dir->body, data)))
			return res;
		
		switch ((res = dir40_next(entity))) {
		case 1:
			break;
		case 0:
			return 0;
		default:
			return res;
		}
	}
	
	return 0;
}

extern object_entity_t *dir40_fake(object_info_t *info);
extern errno_t dir40_update_info(object_entity_t *object);
extern object_entity_t *dir40_recognize(object_info_t *info);

extern errno_t dir40_check_attach(object_entity_t *object, 
				  object_entity_t *parent, 
				  uint8_t mode);

extern errno_t dir40_check_struct(object_entity_t *object, 
				  place_func_t place_func,
				  region_func_t region_func,
				  void *data, uint8_t mode);
#endif

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
	.recognize	= dir40_recognize,
	.update		= dir40_update_info,
	
	.seek		= NULL,
	.write		= NULL,
	.convert        = NULL,
	
	.fake		= dir40_fake,
	.check_struct	= dir40_check_struct,
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
	.id    = {OBJECT_DIR40_ID, DIR_OBJECT, OBJECT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "dir40",
	.desc  = "Compound directory for reiser4, ver. " VERSION,
#endif
	.o = {
		.object_ops = &dir40_ops
	}
};

static reiser4_plug_t *dir40_start(reiser4_core_t *c) {
	dcore = c;
	return &dir40_plug;
}

plug_register(dir40, dir40_start, NULL);
