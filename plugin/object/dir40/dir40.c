/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40.c -- reiser4 directory object plugin. */

#ifndef ENABLE_STAND_ALONE
#  include <unistd.h>
#endif

#include "dir40.h"
#include "dir40_repair.h"

reiser4_core_t *dir40_core = NULL;

#ifndef ENABLE_STAND_ALONE
/* Return current position in directory into passed @offset. */
static errno_t dir40_telldir(object_entity_t *entity,
			     reiser4_key_t *position)
{
	dir40_t *dir = (dir40_t *)entity;
	
	aal_assert("umka-1985", entity != NULL);
	aal_assert("umka-1986", position != NULL);

	/* Getting current dir key and adjust. */
	plug_call(dir->position.plug->o.key_ops,
		  assign, position, &dir->position);

	/* Adjust is offset inside collided keys arrays and needed for
	   positioning right in such a case. In normal case it is zero. */
	position->adjust = dir->position.adjust;
	
	return 0;
}
#endif

/* This fucntion checks if passed @place points to item related to @entity, that
   is belong to this directory. */
int32_t dir40_belong(dir40_t *dir, reiser4_place_t *place) {
	/* Checking if item component in @place->pos is valid one. This is
	   needed because tree_lookup() does not fetch item data at place if it
	   was not found. So, it may point to unexistent item and we should
	   check this here. */
	if (!obj40_valid_item(place))
		return 0;

	/* Fetching item info at @place. This is needed to make sue, that all
	   @place fields are initialized rigth. Normally it is doing by
	   tree_lookup(), if it is sure, that place points to valid postion in
	   node. This happen if lookup found a key. Otherwise it leaves place
	   not initialized and caller is supoposed to take care about. */
	if (obj40_fetch_item(place))
		return 0;
	
	/* Must be the same plugin. */
	if (!plug_equal(dir->body.plug, place->plug))
		return 0;
	
	/* Is the place of the same object? */
	return plug_call(dir->position.plug->o.key_ops, compshort,
			 &dir->position, &place->key) ? 0 : 1;
}

/* Close directiry instance. */
static void dir40_close(object_entity_t *entity) {
	aal_assert("umka-750", entity != NULL);
	aal_free(entity);
}

/* Positioning inside directory by passed @position key. Normally, user should use
   key got from telldir() function. But, this is possible to generate directory
   key by himself and pass here. */
static errno_t dir40_seekdir(object_entity_t *entity,
			     reiser4_key_t *position)
{
	dir40_t *dir;
	
	aal_assert("umka-1983", entity != NULL);
	aal_assert("umka-1984", position != NULL);

	dir = (dir40_t *)entity;

	/* Getting adjust from the passed key and puting it to
	   @dir->adjust. Seekdir is accepting key, which might be got from
	   telldir() function. So, adjust will be set too. */
#ifndef ENABLE_STAND_ALONE
	dir->position.adjust = position->adjust;
#endif

	/* Saving passed key to @dir->position. */
	plug_call(dir->position.plug->o.key_ops,
		  assign, &dir->position, position);

	return 0;
}

/* Resets current direntry position to zero. */
errno_t dir40_reset(object_entity_t *entity) {
	dir40_t *dir = (dir40_t *)entity;
    
	aal_assert("umka-864", entity != NULL);
	
	/* Preparing key of the first entry in directory and set directory
	   adjust to zero. */
#ifndef ENABLE_STAND_ALONE
	dir->position.adjust = 0;
#endif

	/* Building key itself. */
	plug_call(STAT_KEY(&dir->obj)->plug->o.key_ops, 
		  build_hashed, &dir->position, dir->hash, 
		  dir->fibre, obj40_locality(&dir->obj), 
		  obj40_objectid(&dir->obj), ".");

	return 0;
}

/* Fetches current unit to passed @entry */
errno_t dir40_fetch(dir40_t *dir, entry_hint_t *entry) {
	trans_hint_t hint;

	hint.count = 1;
	hint.specific = entry;
	hint.place_func = NULL;
	hint.region_func = NULL;
	hint.shift_flags = SF_DEFAULT;

	/* Reading entry to passed @entry */
	if (plug_call(dir->body.plug->o.item_ops->object,
		      fetch_units, &dir->body, &hint) != 1)
	{
		return -EIO;
	}

	/* Copying entry place. */
	aal_memcpy(&entry->place, &dir->body,
		   sizeof(reiser4_place_t));

	return 0;
}

/* Switches current dir body item onto next one. Returns PRESENT on success,
   ABSENT in the case of directory is over and values < 0 on error. */
lookup_t dir40_next(dir40_t *dir) {
	lookup_t res;
	reiser4_place_t place;

	aal_assert("umka-2063", dir != NULL);

	/* Getting next directory item coord. */
	if ((res = dir40_core->tree_ops.next_item(dir->obj.info.tree,
						  &dir->body, &place)))
	{
		return res;
	}

	/* Check if this item owned by this directory. */
	if (!place.node || !dir40_belong(dir, &place)) {
		uint64_t offset;
		
		/* Set offset to non-existent value as the end is reached. */
		offset = plug_call(dir->position.plug->o.key_ops,
				   get_offset, &dir->position);
		
		plug_call(dir->position.plug->o.key_ops, set_offset,
			  &dir->position, offset + 1);
		
		return ABSENT;
	}
	
	aal_memcpy(&dir->body, &place, sizeof(place));
	
	/* Correcting unit pos for next body item. */
	if (dir->body.pos.unit == MAX_UINT32)
		dir->body.pos.unit = 0;

	return PRESENT;
}

/* Updates current body place by place found by @dir->position and
   @dir->adjust. */
lookup_t dir40_update_body(object_entity_t *entity, int check_group) {
	dir40_t *dir = (dir40_t *)entity;
	lookup_t res;
	uint32_t units;
	
#ifndef ENABLE_STAND_ALONE
	uint32_t adjust = dir->position.adjust;
#endif
	
	/* Making lookup by current dir key. */
	if ((res = obj40_find_item(&dir->obj, &dir->position, 
				   FIND_EXACT, NULL, NULL,
				   &dir->body)) < 0)
		return res;

	if (res == ABSENT) {
		/* Directory is over. */
		if (!dir40_belong(dir, &dir->body))
			return ABSENT;
		
		/* If ABSENT means there is no any dir item, check this again
		   for the case key matches. */
		if (check_group && dir->body.plug->id.group != DIRENTRY_ITEM)
			return ABSENT;
		
#ifndef ENABLE_STAND_ALONE
		/* No adjusting for the ABSENT result. */
		adjust = 0;
#endif
	}
	
	/* Checking if directory is over. */
	units = plug_call(dir->body.plug->o.item_ops->balance,
			  units, &dir->body);
	
	/* Correcting unit pos for next body item. */
	if (dir->body.pos.unit == MAX_UINT32)
		dir->body.pos.unit = 0;

#ifndef ENABLE_STAND_ALONE
	/* Adjusting current position by key's adjust. This is needed
	   for working fine when key collisions take place. */
	while (adjust || dir->body.pos.unit >= units) {
		entry_hint_t temp;

		if (dir->body.pos.unit >= units) {
			/* Getting next directory item */
			if ((res = dir40_next(dir)) < 0)
				return res;
			
			/* No more items in the tree. */
			if (res == ABSENT)
				return ABSENT;
			
			/* Some item of the dir was found. */
			if (adjust == 0) 
				return PRESENT;

			units = plug_call(dir->body.plug->o.item_ops->balance,
					  units, &dir->body);
		}
		
		if (dir40_fetch(dir, &temp))
			return -EIO;

		/* If greater key is reached, return PRESENT. */
		if (plug_call(temp.offset.plug->o.key_ops, compfull, 
			      &temp.offset, &dir->position))
			return PRESENT;
		
		adjust--;
		dir->body.pos.unit++;
	}
#endif
	if (dir->body.pos.unit >= units)
		return ABSENT;

	return PRESENT;
}

/* Reads one current directory entry to passed @entity hint. Returns count of
   read entries, zero for the case directory is over and nagtive values fopr
   errors. */
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
	if ((res = dir40_update_body(entity, 1)) < 0)
		return res;

	/* Directory is over? */
	if (res == ABSENT)
		return 0;

	/* Reading next entry */
	if ((res = dir40_fetch(dir, entry)))
		return res;

	/* Setting up the entry type. It is essential for fsck to know what is
	   the NAME -- that needs to be traversed semantically to be recovered
	   completely -- and what is not -- that needs some other special
	   actions, e.g. check_attach for ".." (and even "..." if it is needed
	   one day), etc. */
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

	units = plug_call(dir->body.plug->o.item_ops->balance,
			  units, &dir->body);

	/* Getting next entry in odrer to set up @dir->position correctly. */
	if (++dir->body.pos.unit >= units) {
		/* Switching to the next directory item */
		if ((res = dir40_next(dir)) < 0)
			return res;
	} else {
		/* There is no needs to switch */
		res = 1;
	}

	if (res == 1) {
		entry_hint_t temp;
		
		if ((res = dir40_fetch(dir, &temp)))
			return res;

#ifndef ENABLE_STAND_ALONE
		/* Taking care about adjust */
		if (!plug_call(temp.offset.plug->o.key_ops,
			       compfull, &temp.offset, &dir->position))
		{
			temp.offset.adjust = dir->position.adjust + 1;
		} else {
			temp.offset.adjust = 0;
		}
#endif
		dir40_seekdir(entity, &temp.offset);
	}
	
	return 1;
}

/* Makes lookup inside directory. This is needed to be used in add_entry() for
   two reasons: for make sure, that passed entry does not exists and to use
   lookup result for consequent insert. */
static lookup_t dir40_search(object_entity_t *entity, char *name,
			     lookup_bias_t bias, entry_hint_t *entry)
{
	dir40_t *dir;
	lookup_t res;
	coll_hint_t hint;
	coll_func_t func;

	aal_assert("umka-1118", name != NULL);
	aal_assert("umka-1117", entity != NULL);

	dir = (dir40_t *)entity;

	/* Preparing key to be used for lookup. It is generating from the
	   directory oid, locality and name by menas of using hash plugin. */
	plug_call(STAT_KEY(&dir->obj)->plug->o.key_ops, 
		  build_hashed, &dir->body.key, dir->hash, 
		  dir->fibre, obj40_locality(&dir->obj), 
		  obj40_objectid(&dir->obj), name);

#ifndef ENABLE_STAND_ALONE
	hint.specific = name;
	hint.type = DIRENTRY_ITEM;
	func = dir40_core->tree_ops.collision;
#endif
	
	if ((res = obj40_find_item(&dir->obj, &dir->body.key, bias, 
				   func, &hint, &dir->body)) < 0)
	{
		return res;
	}

	if (entry) {
		aal_memset(entry, 0, sizeof(*entry));
		
		aal_memcpy(&entry->place, &dir->body,
			   sizeof(reiser4_place_t));

		aal_memcpy(&entry->offset, &dir->body.key,
			   sizeof(reiser4_key_t));

		if (res == PRESENT) {
			if (dir40_fetch(dir, entry))
				return -EIO;
		}
	}

	return res;
}

/* Makes lookup inside @entity by passed @name. Saves found entry in passed
   @entry hint. */
lookup_t dir40_lookup(object_entity_t *entity,
		      char *name, entry_hint_t *entry) 
{
	return dir40_search(entity, name, FIND_EXACT, entry);
}

/* Initializing dir40 instance, resetring directory be means of using reset()
   function and return instance to caller. */
static object_entity_t *dir40_open(object_info_t *info) {
	dir40_t *dir;

	aal_assert("umka-836", info != NULL);
	aal_assert("umka-837", info->tree != NULL);
	
	if (info->start.plug->id.group != STATDATA_ITEM)
		return NULL;
	
	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return NULL;

	/* Initializing obj handle for the directory */
	obj40_init(&dir->obj, &dir40_plug, dir40_core, info);
	
	if (obj40_pid(&dir->obj, OBJECT_PLUG_TYPE,
		      "directory") !=  dir40_plug.id.id)
	{
		goto error_free_dir;
	}
	
	/* Getting hash plugin basing on stat data and/or param set. */
	if (!(dir->hash = obj40_plug(&dir->obj, HASH_PLUG_TYPE, "hash")))
                goto error_free_dir;
	
	/* Getting fibre plugin basing on stat data and/or param set. */
	if (!(dir->fibre = obj40_plug(&dir->obj, FIBRE_PLUG_TYPE, "fibre")))
                goto error_free_dir;

	/* Positioning to the first directory unit. */
	dir40_reset((object_entity_t *)dir);
	
	return (object_entity_t *)dir;

 error_free_dir:
	aal_free(dir);
	return NULL;
}

/* Loads stat data to passed @hint. */
static errno_t dir40_stat(object_entity_t *entity,
			  statdata_hint_t *hint)
{
	dir40_t *dir;
	
	aal_assert("umka-2563", entity != NULL);
	aal_assert("umka-2564", hint != NULL);

	dir = (dir40_t *)entity;
	return obj40_load_stat(&dir->obj, hint);
}

#ifndef ENABLE_STAND_ALONE
/* Gets size from the object stat data */
static uint64_t dir40_size(object_entity_t *entity) {
	dir40_t *dir;

	aal_assert("umka-2277", entity != NULL);
	
	dir = (dir40_t *)entity;
	
	/* Updating stat data place. */
	if (obj40_update(&dir->obj))
		return 0;

	return obj40_get_size(&dir->obj);
}

/* Creates dir40 instance. Creates its stat data item, and body item with one
   "." unit. Yet another unit ".." will be inserted latter, then directiry will
   be attached to a parent object. */
static object_entity_t *dir40_create(object_info_t *info,
				     object_hint_t *hint)
{
	rid_t pid;
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
	
	/* Initializing obj handle. */
	obj40_init(&dir->obj, &dir40_plug, dir40_core, info);

	/* Getting hash plugin */
	if (!(dir->hash = dir40_core->factory_ops.ifind(HASH_PLUG_TYPE, 
							hint->body.dir.hash))) 
	{
		aal_error("Can't find hash plugin by its "
			  "id 0x%x.", hint->body.dir.hash);
		goto error_free_dir;
	}

	/* Getting fibration plugin */
	if (!(dir->fibre = dir40_core->factory_ops.ifind(FIBRE_PLUG_TYPE, 
							 hint->body.dir.fibre))) 
	{
		aal_error("Can't find fibration plugin by its "
			  "id 0x%x.", hint->body.dir.fibre);
		goto error_free_dir;
	}

	/* Initializing body plugin. */
	pid = hint->body.dir.direntry;
	
	if (!(body_plug = dir40_core->factory_ops.ifind(ITEM_PLUG_TYPE, pid))) {
		aal_error("Can't find direntry item plugin by "
			  "its id 0x%x.", pid);
		goto error_free_dir;
	}
    
	aal_memset(&body_hint, 0, sizeof(body_hint));
	
	/* Initializing direntry item hint. This should be done before the stat
	   data item hint, because we will need size of direntry item during
	   stat data initialization. */
   	body_hint.count = 1;
	body_hint.plug = body_plug;
	
	plug_call(info->object.plug->o.key_ops, 
		  build_hashed, &body_hint.offset, dir->hash, 
		  dir->fibre, obj40_locality(&dir->obj), 
		  obj40_objectid(&dir->obj), ".");

	/* Preparing hint for the empty directory. It consists only "." for
	   unlinked directories. */
	aal_strncpy(entry.name, ".", 1);

	/* Initializing entry stat data key. */
	plug_call(info->object.plug->o.key_ops, assign,
		  &entry.object, &info->object);

	/* Initializing entry hash key. */
	plug_call(info->object.plug->o.key_ops, assign,
		  &entry.offset, &body_hint.offset);

	body_hint.specific = &entry;
	body_hint.place_func = NULL;
	body_hint.region_func = NULL;
	body_hint.shift_flags = SF_DEFAULT;
	
	dir40_reset((object_entity_t *)dir);
	
        /* Looking for place to insert directory body */
	switch (obj40_find_item(&dir->obj, &body_hint.offset,
				FIND_CONV, NULL, NULL, &dir->body))
	{
	case ABSENT:
		/* Inserting the direntry body item into the tree. */
		if (obj40_insert(&dir->obj, &dir->body,
				 &body_hint, LEAF_LEVEL) < 0)
		{
			goto error_free_dir;
		}
		
		break;
	default:
		goto error_free_dir;
	}

	/* Create stat data item. */
	mask = (1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID);
	
	if (obj40_create_stat(&dir->obj, hint->label.statdata, mask,
			      1, body_hint.len, 0, 1, S_IFDIR, NULL))
	{
	
		/* Removing body item. */	
		if (dir40_update_body((object_entity_t *)dir, 1) == 0) {
			body_hint.count = 1;
			body_hint.place_func = NULL;
			body_hint.region_func = NULL;
			
			obj40_remove(&dir->obj, &dir->body, &body_hint);
		}
		
		goto error_free_dir;
	}
	
	return (object_entity_t *)dir;
	
 error_free_dir:
	aal_free(dir);
	return NULL;
}

/* Removes all directory body items. */
static errno_t dir40_truncate(object_entity_t *entity,
			      uint64_t n)
{
	errno_t res;
	dir40_t *dir;
	reiser4_key_t key;

	aal_assert("umka-1925", entity != NULL);

	dir = (dir40_t *)entity;

	/* Making sure, that dir->body points to correct item */
	if ((res = dir40_update_body(entity, 1)) < 0)
		return res;

	/* There is no body in directory */
	if (res == ABSENT)
		return 0;

	/* Creating maximal possible key in order to find last directory item
	   and remove it from the tree. Thanks to Nikita for this idea. */
	plug_call(dir->body.plug->o.key_ops,
		  set_offset, &key, MAX_UINT64);

	while (1) {
		trans_hint_t hint;
		reiser4_place_t place;

		/* Looking for the last directory item */
		if ((res = obj40_find_item(&dir->obj, &key, FIND_EXACT,
					   NULL, NULL, &place)) < 0)
		{
			return res;
		}

		/* Checking if found item belongs this directory */
		if (!dir40_belong(dir, &place))
			return 0;

		hint.count = 1;
		hint.place_func = NULL;
		hint.region_func = NULL;
		hint.shift_flags = SF_DEFAULT;
		
		/* Removing item from the tree */
		if ((res = obj40_remove(&dir->obj, &place, &hint)))
			return res;
	}
	
	return 0;
}

/* Removes directory body and stat data from the tree. */
static errno_t dir40_clobber(object_entity_t *entity) {
	errno_t res;
		
	aal_assert("umka-2298", entity != NULL);

	/* Truncates directory body. */
	if ((res = dir40_truncate(entity, 0)))
		return res;

	/* Cloberring stat data. */
	return obj40_clobber(&((dir40_t *)entity)->obj);
}

/* Attaches passed directory denoted by @entity to @parent object. */
static errno_t dir40_attach(object_entity_t *entity,
			    object_entity_t *parent)
{
	errno_t res;
	dir40_t *dir;
	entry_hint_t entry;
	
	aal_assert("umka-2289", entity != NULL);
	aal_assert("umka-2359", parent != NULL);

	dir = (dir40_t *)entity;

	aal_memset(&entry, 0, sizeof(entry));
	
	aal_strncpy(entry.name, "..", sizeof(entry.name));

	/* Adding ".." pointing to parent to @entity object. */
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

/* Detaches @entity from @parent. */
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

/* Return number of hard links. */
static uint32_t dir40_links(object_entity_t *entity) {
	dir40_t *dir;
	
	aal_assert("umka-2294", entity != NULL);

	dir = (dir40_t *)entity;
	return obj40_links(&dir->obj);
}

/* Addes one had link. */
static errno_t dir40_link(object_entity_t *entity) {
	dir40_t *dir;
	
	aal_assert("umka-1908", entity != NULL);

	dir = (dir40_t *)entity;
	return obj40_link(&dir->obj);
}

/* Removes one hard link. */
static errno_t dir40_unlink(object_entity_t *entity) {
	dir40_t *dir;
	
	aal_assert("umka-1907", entity != NULL);

	dir = (dir40_t *)entity;
	return obj40_unlink(&dir->obj);
}

/* Helper function. Builds entry key by entry name. */
static errno_t dir40_build_entry(object_entity_t *entity, 
				 entry_hint_t *entry)
{
	dir40_t *dir;
	uint64_t locality;
	uint64_t objectid;
	
	aal_assert("umka-2528", entry != NULL);
	aal_assert("umka-2527", entity != NULL);

	dir = (dir40_t *)entity;
	
	locality = obj40_locality(&dir->obj);
	objectid = obj40_objectid(&dir->obj);
	
	return plug_call(STAT_KEY(&dir->obj)->plug->o.key_ops, 
			 build_hashed, &entry->offset, dir->hash, 
			 dir->fibre, locality, objectid, entry->name);
}

/* Add new entry to directory. */
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
	
	/* Getting place new entry will be inserted at. */
	switch (dir40_search(entity, entry->name,
			     FIND_EXACT, &temp))
	{
	case ABSENT:
		if ((res = obj40_fetch_item(&temp.place)))
			return res;
		
		break;
	case PRESENT:
		aal_error("Entry \"%s\" already exists.",
			  entry->name);
	default:
		return -EINVAL;
	}

	/* Prepare trans hint. */
	hint.count = 1;
	hint.region_func = NULL;
	hint.place_func = entry->place_func;
	hint.data = entry->data;
	
	hint.plug = temp.place.plug;
	hint.specific = (void *)entry;
	hint.shift_flags = SF_DEFAULT;

	/* Building key of the new entry and hint's one */
	dir40_build_entry(entity, entry);

	/* Copying key to @hint */
	plug_call(entry->offset.plug->o.key_ops, assign,
		  &hint.offset, &entry->offset);

	/* Inserting entry described by @hint to tree at @temp.place */
	if ((res = obj40_insert(&dir->obj, &temp.place,
				&hint, LEAF_LEVEL)) < 0)
	{
		return res;
	}

	/* Updating stat data fields. */
	if ((res = obj40_update(&dir->obj)))
		return res;
	
	entry->len = hint.len;
	size = dir40_size(entity) + 1;
	bytes = obj40_get_bytes(&dir->obj) + hint.bytes;
	
	return obj40_touch(&dir->obj, size, bytes);
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
	switch (dir40_search(entity, entry->name, FIND_EXACT, &temp)) {
	case PRESENT:
		hint.count = 1;
		hint.place_func = NULL;
		hint.region_func = NULL;
		hint.shift_flags = SF_DEFAULT;
		
		/* Removing one unit from directory */
		if ((res = obj40_remove(&dir->obj, &temp.place, &hint)))
			return res;

		if (!plug_call(dir->position.plug->o.key_ops,
			       compfull, &dir->position, &temp.offset))
		{
			if (entry->offset.adjust < dir->position.adjust)
				dir->position.adjust--;
		}
		
		break;
	default:
		return -EINVAL;
	}

	/* Updating stat data fields */
	if ((res = obj40_update(&dir->obj)))
		return res;

	entry->len = hint.len;
	size = dir40_size(entity) - 1;
	bytes = obj40_get_bytes(&dir->obj) - hint.bytes;
	
	return obj40_touch(&dir->obj, size, bytes);
}

/* Directory enumerating related stuff.*/
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

/* This fucntion implements hashed directory enumerator function. It is used for
   calculating fargmentation, prining. */
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

	/* Update current body item coord. */
	if ((res = dir40_update_body(entity, 1)) < 0)
		return res;

	/* There is no body in directory */
	if (res == ABSENT)
		return 0;

	/* Prepare layout hint. */
	hint.data = data;
	hint.entity = entity;
	hint.region_func = region_func;

	/* Loop until all items are enumerated. */
	while (1) {
		reiser4_place_t *place = &dir->body;
		
		if (dir->body.plug->o.item_ops->object->layout) {
			/* Calling item's layout method */
			if ((res = plug_call(place->plug->o.item_ops->object,
					     layout, place, callback_item_layout,
					     &hint)))
			{
				return res;
			}
		} else {
			/* Layout method is not implemented. Counting item
			   itself. */
			blk_t blk = place->node->block->nr;
			
			if ((res = callback_item_layout(place, blk,
							1, &hint)))
			{
				return res;
			}
		}

		/* Getting next directory item. */
		if ((res = dir40_next(dir)) < 0)
			return res;

		/* Directory is over? */
		if (res == ABSENT)
			return 0;
	}
    
	return 0;
}

/* This fucntion implements hashed directory metadata enumerator function. This
   is needed for getting directory metadata for pack them, etc. */
static errno_t dir40_metadata(object_entity_t *entity,
			      place_func_t place_func,
			      void *data)
{
	errno_t res;
	dir40_t *dir;
	
	aal_assert("umka-1712", entity != NULL);
	aal_assert("umka-1713", place_func != NULL);
	
	dir = (dir40_t *)entity;

	/* Calculating stat data item. */
	if ((res = obj40_metadata(&dir->obj, place_func, data)))
		return res;

	/* Update current body item coord. */
	if ((res = dir40_update_body(entity, 1)) < 0)
		return res;

	if (res == ABSENT)
		return 0;

	/* Loop until all items are enumerated. */
	while (1) {
		/* Calling callback function. */
		if ((res = place_func(&dir->body, data)))
			return res;

		/* Getting next item. */
		if ((res = dir40_next(dir)) < 0)
			return res;
		
		if (res == ABSENT)
			return 0;
	}
	
	return 0;
}

/* Updates stat data from passed @hint */
static errno_t dir40_update(object_entity_t *entity,
			    statdata_hint_t *hint)
{
	dir40_t *dir;
	
	aal_assert("umka-2565", entity != NULL);
	aal_assert("umka-2566", hint != NULL);

	dir = (dir40_t *)entity;
	return obj40_save_stat(&dir->obj, hint);
}
#endif

/* Directory object operations. */
static reiser4_object_ops_t dir40_ops = {
#ifndef ENABLE_STAND_ALONE
	.create		= dir40_create,
	.layout		= dir40_layout,
	.metadata	= dir40_metadata,
	.link		= dir40_link,
	.unlink		= dir40_unlink,
	.links		= dir40_links,
	.update         = dir40_update,
	.truncate	= dir40_truncate,
	.add_entry	= dir40_add_entry,
	.rem_entry	= dir40_rem_entry,
	.build_entry    = dir40_build_entry,
	.attach		= dir40_attach,
	.detach		= dir40_detach,
	.clobber	= dir40_clobber,
	.recognize	= dir40_recognize,
	.form		= dir40_form,
	.fake		= dir40_fake,
	.check_struct	= dir40_check_struct,
	.check_attach	= dir40_check_attach,

	.seek		= NULL,
	.write		= NULL,
	.convert        = NULL,
#endif
	.follow		= NULL,
	.read		= NULL,
	.offset		= NULL,
	
	.stat           = dir40_stat,
	.open		= dir40_open,
	.close		= dir40_close,
	.reset		= dir40_reset,
	.lookup		= dir40_lookup,
	.seekdir	= dir40_seekdir,
	.readdir	= dir40_readdir,

#ifndef ENABLE_STAND_ALONE
	.telldir	= dir40_telldir,
#else
	.telldir	= NULL
#endif
};

reiser4_plug_t dir40_plug = {
	.cl    = class_init,
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
	dir40_core = c;
	return &dir40_plug;
}

plug_register(dir40, dir40_start, NULL);
