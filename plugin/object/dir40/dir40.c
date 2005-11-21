/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40.c -- reiser4 directory object plugin. */

#ifndef ENABLE_MINIMAL
#  include <unistd.h>
#endif

#include "dir40.h"
#include "dir40_repair.h"

/* Return current position in directory into passed @offset. */
static errno_t dir40_telldir(reiser4_object_t *dir,
			     reiser4_key_t *position)
{
	aal_assert("umka-1985", dir != NULL);
	aal_assert("umka-1986", position != NULL);

	/* Getting current dir key and adjust. */
	aal_memcpy(position, &dir->position, sizeof(*position));

#ifndef ENABLE_MINIMAL
	/* Adjust is offset inside collided keys arrays and needed for
	   positioning right in such a case. In normal case it is zero. */
	position->adjust = dir->position.adjust;
#endif
	
	return 0;
}

/* Positioning inside directory by passed @position key. Normally, user should use
   key got from telldir() function. But, this is possible to generate directory
   key by himself and pass here. */
static errno_t dir40_seekdir(reiser4_object_t *dir,
			     reiser4_key_t *position)
{
	aal_assert("umka-1983", dir != NULL);
	aal_assert("umka-1984", position != NULL);

	/* Set directory position to the given one. */
	aal_memcpy(&dir->position, position, sizeof(*position));
	return 0;
}

/* Resets current direntry position to zero. */
errno_t dir40_reset(reiser4_object_t *dir) {
	aal_assert("umka-864", dir != NULL);
	
	/* Preparing key of the first entry in directory and set directory
	   adjust to zero. */
#ifndef ENABLE_MINIMAL
	dir->position.adjust = 0;
#endif

	/* Building key itself. */
	plugcall(dir->info.object.plug, build_hashed, &dir->position,
		  (reiser4_hash_plug_t *)dir->info.opset.plug[OPSET_HASH],
		  (reiser4_fibre_plug_t *)dir->info.opset.plug[OPSET_FIBRE], 
		  objcall(&dir->info.object, get_locality),
		  objcall(&dir->info.object, get_objectid), ".");

	return 0;
}

/* Fetches current unit to passed @entry */
errno_t dir40_fetch(reiser4_object_t *dir, entry_hint_t *entry) {
	trans_hint_t hint;

	aal_memset(&hint, 0, sizeof(hint));
	hint.count = 1;
	hint.specific = entry;
	hint.shift_flags = SF_DEFAULT;

	/* Reading entry to passed @entry */
	if (objcall(&dir->body, object->fetch_units, &hint) != 1)
		return -EIO;
	
	/* Copying entry place. */
	aal_memcpy(&entry->place, &dir->body,
		   sizeof(reiser4_place_t));

	return 0;
}

#ifndef ENABLE_MINIMAL
static void dir40_entry_type(entry_hint_t *entry) {
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
}
#else
#define dir40_entry_type(entry) do{;} while(0)
#endif

errno_t dir40_entry_comp(reiser4_object_t *dir, void *data) {
	entry_hint_t entry;
	reiser4_key_t *key;

	aal_assert("vpf-1834", dir != NULL);

	if (!dir->body.plug)
		return -EINVAL;
#ifndef EINVAL	
	if (dir->body.plug->id.group != DIR_ITEM)
		return -ESTRUCT;
#endif
	if (dir40_fetch(dir, &entry))
		return -EIO;

	if (!data) {
		key = &dir->position;
	} else {
		key = (reiser4_key_t *)data;
	}
	
	/* If greater key is reached, return PRESENT. */
	return objcall(&entry.offset, compfull, key) ? 1 : 0;
}

/* Reads one current directory entry to passed @entry hint. Returns count of
   read entries, zero for the case directory is over and nagtive values fopr
   errors. */
static int32_t dir40_readdir(reiser4_object_t *dir, 
			     entry_hint_t *entry)
{
	uint32_t units;
	errno_t res;

	aal_assert("umka-845", entry != NULL);
	aal_assert("umka-844", dir != NULL);

	/* Getting place of current unit */
	if ((res = obj40_update_body(dir, dir40_entry_comp)) != PRESENT)
		return res == ABSENT ? 0 : res;

	/* Reading next entry */
	if ((res = dir40_fetch(dir, entry)))
		return res;

	/* Setting up the entry type. It is essential for fsck to know what is
	   the NAME -- that needs to be traversed semantically to be recovered
	   completely -- and what is not -- that needs some other special
	   actions, e.g. check_attach for ".." (and even "..." if it is needed
	   one day), etc. */
	dir40_entry_type(entry);

	units = objcall(&dir->body, balance->units);

	/* Getting next entry in odrer to set up @dir->position correctly. */
	if (++dir->body.pos.unit >= units) {
		/* Switching to the next directory item */
		if ((res = obj40_next_item(dir)) < 0)
			return res;

		if (res == ABSENT) {
			uint64_t offset;
			
			/* Set offset to non-existent value. */
			offset = objcall(&dir->position, get_offset);
			objcall(&dir->position, set_offset, offset + 1);
		}
	} else {
		/* There is no need to switch */
		res = 1;
	}

	if (res == 1) {
		entry_hint_t temp;
		
		if ((res = dir40_fetch(dir, &temp)))
			return res;

#ifndef ENABLE_MINIMAL
		/* Taking care about adjust */
		if (!objcall(&temp.offset, compfull, &dir->position))
			temp.offset.adjust = dir->position.adjust + 1;
		else
			temp.offset.adjust = 0;
#endif
		dir40_seekdir(dir, &temp.offset);
	}
	
	return 1;
}

/* Makes lookup inside directory. This is needed to be used in add_entry() for
   two reasons: for make sure, that passed entry does not exists and to use
   lookup result for consequent insert. */
static lookup_t dir40_search(reiser4_object_t *dir, char *name,
			     lookup_bias_t bias, entry_hint_t *entry)
{
	lookup_t res;
#ifndef ENABLE_MINIMAL
	coll_hint_t hint;
	coll_func_t func;
#endif

	aal_assert("umka-1118", name != NULL);
	aal_assert("umka-1117", dir != NULL);

	/* Preparing key to be used for lookup. It is generating from the
	   directory oid, locality and name by menas of using hash plugin. */
	plugcall(dir->info.object.plug, build_hashed, &dir->body.key,
		 (reiser4_hash_plug_t *)dir->info.opset.plug[OPSET_HASH],
		 (reiser4_fibre_plug_t *)dir->info.opset.plug[OPSET_FIBRE],
		 objcall(&dir->info.object, get_locality),
		 objcall(&dir->info.object, get_objectid), name);

#ifndef ENABLE_MINIMAL
	hint.specific = name;
	hint.type = DIR_ITEM;
	func = obj40_core->tree_ops.collision;
#endif
	
	if ((res = obj40_find_item(dir, &dir->body.key, bias, 
#ifndef ENABLE_MINIMAL
				   func, &hint,
#else
				   NULL, NULL,
#endif
				   &dir->body)) < 0)
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

			dir40_entry_type(entry);
		}
	}

	return res;
}

/* Makes lookup inside @dir by passed @name. Saves found entry in passed
   @entry hint. */
lookup_t dir40_lookup(reiser4_object_t *dir,
		      char *name, entry_hint_t *entry) 
{
	return dir40_search(dir, name, FIND_EXACT, entry);
}

#ifndef ENABLE_MINIMAL
/* Creates dir40 instance. Creates its stat data item, and body item with one
   "." unit. Yet another unit ".." will be inserted latter, then directiry will
   be attached to a parent object. */
static errno_t dir40_create(reiser4_object_t *dir, object_hint_t *hint) {
	trans_hint_t body_hint;
	entry_hint_t entry;
	reiser4_key_t *key;
	uint32_t mode;
	errno_t res;
    
	aal_assert("vpf-1816",  dir != NULL);
	aal_assert("vpf-1095",  dir->info.tree != NULL);

	aal_memset(&body_hint, 0, sizeof(body_hint));
	
	/* Initializing direntry item hint. This should be done before the stat
	   data item hint, because we will need size of direntry item during
	   stat data initialization. */
   	body_hint.count = 1;
	body_hint.plug = 
		(reiser4_item_plug_t *)dir->info.opset.plug[OPSET_DIRITEM];
	
	key = &dir->info.object;
	plugcall(key->plug, build_hashed, &body_hint.offset, 
		 (reiser4_hash_plug_t *)dir->info.opset.plug[OPSET_HASH], 
		 (reiser4_fibre_plug_t *)dir->info.opset.plug[OPSET_FIBRE],
		 objcall(&dir->info.object, get_locality),
		 objcall(&dir->info.object, get_objectid), ".");

	/* Preparing hint for the empty directory. It consists only "." for
	   unlinked directories. */
	aal_strncpy(entry.name, ".", 1);

	/* Initializing entry stat data key. */
	aal_memcpy(&entry.object, key, sizeof(*key));

	/* Initializing entry hash key. */
	aal_memcpy(&entry.offset, &body_hint.offset, sizeof(entry.offset));

	body_hint.specific = &entry;
	body_hint.shift_flags = SF_DEFAULT;
	
	dir40_reset(dir);
	
        /* Looking for place to insert directory body */
	switch (obj40_find_item(dir, &body_hint.offset,
				FIND_CONV, NULL, NULL, &dir->body))
	{
	case ABSENT:
		/* Inserting the direntry body item into the tree. */
		if ((res = obj40_insert(dir, &dir->body,
					&body_hint, LEAF_LEVEL)) < 0)
		{
			return res;
		}
		
		break;
	default:
		return -EIO;
	}
	
	mode = (hint ? hint->mode : 0) | S_IFDIR | 0755;
	
	/* Create stat data item. */
	if ((res = obj40_create_stat(dir, 1, body_hint.len, 
				     0, 1, mode, NULL))) 
	{
	
		/* Removing body item. */	
		if (obj40_update_body(dir, dir40_entry_comp) == PRESENT) {
			body_hint.count = 1;
			body_hint.place_func = NULL;
			body_hint.region_func = NULL;
			dir->body.pos.unit = MAX_UINT32;
			
			obj40_remove(dir, &dir->body, &body_hint);
		}
		
		return res;
	}
	
	return 0;
}

/* Removes all directory body items. */
static errno_t dir40_mkempty(reiser4_object_t *dir, uint64_t n) {
	errno_t res;
	reiser4_key_t key;

	aal_assert("umka-1925", dir != NULL);

	/* Making sure, that dir->body points to correct item */
	if ((res = obj40_update_body(dir, dir40_entry_comp)) != PRESENT)
		return res  == ABSENT ? 0 : res;

	/* Creating maximal possible key in order to find last directory item
	   and remove it from the tree. Thanks to Nikita for this idea. */
	aal_memcpy(&key, &dir->body.key, sizeof(key));
	objcall(&key, set_offset, MAX_UINT64);

	while (1) {
		trans_hint_t hint;

		/* Looking for the last directory item. 
		   FIXME: the possible speedup here is to get rid of 
		   lookup */
		if ((res = obj40_find_item(dir, &key, FIND_EXACT, NULL, 
					   NULL, &dir->body)) < 0)
		{
			return res;
		}

		/* Checking if found item belongs this directory */
		if (obj40_belong(&dir->body, &key) == ABSENT)
			return 0;

		aal_memset(&hint, 0, sizeof(hint));
		
		hint.count = 1;
		hint.shift_flags = SF_DEFAULT;
		dir->body.pos.unit = MAX_UINT32;
		
		/* Removing item from the tree */
		if ((res = obj40_remove(dir, &dir->body, &hint)))
			return res;
	}
	
	return 0;
}

/* Removes directory body and stat data from the tree. */
static errno_t dir40_clobber(reiser4_object_t *dir) {
	uint32_t nlink;
	errno_t res;
		
	aal_assert("umka-2298", dir != NULL);
	
	/* Check that truncate is allowed -- i.e. nlink == 2. */
	if ((nlink = obj40_get_nlink(dir, 1)) != 2) {
		aal_error("Can't detach the object "
			  "with nlink (%d).", nlink);
		return -EINVAL;
	}

	dir40_reset(dir);
	
	/* Truncates directory body. */
	if ((res = dir40_mkempty(dir, 0)))
		return res;

	/* Cloberring stat data. */
	return obj40_clobber(dir);
}

/* Return number of hard links. */
static bool_t dir40_linked(reiser4_object_t *dir) {
	return obj40_get_nlink(dir, 1) != 1;
}

/* Helper function. Builds @entry->offset key by @entry->name. */
static errno_t dir40_build_entry(reiser4_object_t *dir, 
				 entry_hint_t *entry)
{
	aal_assert("umka-2528", entry != NULL);
	aal_assert("umka-2527", dir != NULL);
	
	plugcall(dir->info.object.plug, build_hashed, &entry->offset, 
		 (reiser4_hash_plug_t *)dir->info.opset.plug[OPSET_HASH],
		 (reiser4_fibre_plug_t *)dir->info.opset.plug[OPSET_FIBRE],
		 objcall(&dir->info.object, get_locality),
		 objcall(&dir->info.object, get_objectid), entry->name);

	return 0;
}

/* Add new entry to directory. */
static errno_t dir40_add_entry(reiser4_object_t *dir, 
			       entry_hint_t *entry)
{
	errno_t res;
	
	entry_hint_t temp;
	trans_hint_t hint;

	aal_assert("umka-844", dir != NULL);
	aal_assert("umka-845", entry != NULL);

	aal_memset(&hint, 0, sizeof(hint));
	
	/* Getting place new entry will be inserted at. */
	switch (dir40_search(dir, entry->name,
			     FIND_EXACT, &temp))
	{
	case ABSENT:
		if (!obj40_valid_item(&temp.place))
			return -EINVAL;
		
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
	dir40_build_entry(dir, entry);

	/* Copying key to @hint */
	aal_memcpy(&hint.offset, &entry->offset, sizeof(hint.offset));

	/* Inserting entry described by @hint to tree at @temp.place */
	if ((res = obj40_insert(dir, &temp.place,
				&hint, LEAF_LEVEL)) < 0)
	{
		return res;
	}

	entry->len = hint.len;
	
	/* Updating stat data fields. */
	return obj40_touch(dir, 1, hint.bytes);
}

/* Removing entry from the directory */
static errno_t dir40_rem_entry(reiser4_object_t *dir,
			       entry_hint_t *entry)
{
	errno_t res;
	entry_hint_t temp;
	trans_hint_t hint;
	
	aal_assert("umka-1923", entry != NULL);
	aal_assert("umka-1922", dir != NULL);
	aal_assert("umka-2390", entry->name != NULL);

	/* Looking for place to insert directory entry */
	if (dir40_search(dir, entry->name, FIND_EXACT, &temp) != PRESENT)
		return -EINVAL;
	
	aal_memset(&hint, 0, sizeof(hint));
	
	hint.count = 1;
	hint.shift_flags = SF_DEFAULT;
	
	/* Removing one unit from directory */
	if ((res = obj40_remove(dir, &dir->body, &hint)))
		return res;
	
	if (!objcall(&dir->position, compfull, &temp.offset)) {
		if (entry->offset.adjust < dir->position.adjust)
			dir->position.adjust--;
	}

	entry->len = hint.len;
	
	/* Updating stat data fields */
	return obj40_touch(dir, -1, -hint.bytes);
}

/* Attaches the given directory @dir to @parent object. */
static errno_t dir40_attach(reiser4_object_t *dir,
			    reiser4_object_t *parent)
{
	errno_t res;
	entry_hint_t entry;
	
	aal_assert("umka-2289", dir != NULL);
	aal_assert("umka-2359", parent != NULL);

	aal_memset(&entry, 0, sizeof(entry));
	
	aal_strncpy(entry.name, "..", sizeof(entry.name));

	/* Adding ".." pointing to parent to @dir object. */
	aal_memcpy(&entry.object, &parent->info.object, sizeof(entry.object));

	if ((res = dir40_add_entry(dir, &entry)))
		return res;

	/* Increasing parent's @nlink by one */
	return plugcall(reiser4_oplug(parent), link, parent);
}

/* Detaches @dir from @parent. */
static errno_t dir40_detach(reiser4_object_t *dir,
			    reiser4_object_t *parent)
{
	entry_hint_t entry;
	errno_t res;

	aal_assert("umka-2291", dir != NULL);

	/* Removing ".." from child if it is found */
	if (dir40_lookup(dir, "..", &entry) == PRESENT) {
		if ((res = dir40_rem_entry(dir, &entry)))
			return res;
	}
	
	if (!parent) 
		return 0;
	
	/* Decreasing parent's @nlink by one */
	return plugcall(reiser4_oplug(parent), unlink, parent);
}

/* This fucntion implements hashed directory enumerator function. It is used for
   calculating fargmentation, prining. */
static errno_t dir40_layout(reiser4_object_t *dir,
			    region_func_t func,
			    void *data)
{
	dir40_reset(dir);
	return obj40_layout(dir, func, dir40_entry_comp, data);
}

/* This fucntion implements hashed directory metadata enumerator function. This
   is needed for getting directory metadata for pack them, etc. */
static errno_t dir40_metadata(reiser4_object_t *dir,
			      place_func_t place_func,
			      void *data)
{
	dir40_reset(dir);
	return obj40_traverse(dir, place_func, dir40_entry_comp, data);
}
#endif

/* Directory object operations. */
reiser4_object_plug_t dir40_plug = {
	.p = {
		.id    = {OBJECT_DIR40_ID, DIR_OBJECT, OBJECT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "dir40",
		.desc  = "Directory file plugin.",
#endif
	},

#ifndef ENABLE_MINIMAL
	.inherit	= obj40_inherit,
	.create		= dir40_create,
	.layout		= dir40_layout,
	.metadata	= dir40_metadata,
	.link		= obj40_link,
	.unlink		= obj40_unlink,
	.linked		= dir40_linked,
	.update         = obj40_save_stat,
	.truncate	= NULL,
	.add_entry	= dir40_add_entry,
	.rem_entry	= dir40_rem_entry,
	.build_entry    = dir40_build_entry,
	.attach		= dir40_attach,
	.detach		= dir40_detach,
	.clobber	= dir40_clobber,
	.recognize	= obj40_recognize,
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
	
	.stat           = obj40_load_stat,
	.open		= obj40_open,
	.close		= NULL,
	.reset		= dir40_reset,
	.lookup		= dir40_lookup,
	.seekdir	= dir40_seekdir,
	.readdir	= dir40_readdir,
	.telldir	= dir40_telldir,

#ifndef ENABLE_MINIMAL
	.sdext_mandatory = (1 << SDEXT_LW_ID),
	.sdext_unknown   = (1 << SDEXT_SYMLINK_ID  |
			    1 << SDEXT_CLUSTER_ID)
#endif
};
