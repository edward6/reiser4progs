/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   object.c -- common code for all reiser4 objects (regular files, directories,
   symlinks, etc). */

#include <aux/aux.h>
#include <reiser4/libreiser4.h>

/* Init object pset by its SD. */
errno_t reiser4_object_init(object_info_t *info) {
	sdhint_plug_t plugh;
	trans_hint_t trans;
	stat_hint_t stat;
	errno_t res;
	
	aal_assert("umka-2380", info != NULL);

	aal_memset(&stat, 0, sizeof(stat));
	aal_memset(&plugh, 0, sizeof(plugh));
	
	/* Preparing hint and mask */
	trans.specific = &stat;
	trans.place_func = NULL;
	trans.region_func = NULL;
	trans.shift_flags = SF_DEFAULT;
	stat.ext[SDEXT_PLUG_ID] = &plugh;
	
	if (reiser4_place_valid(&info->start)) {
		if ((res = reiser4_place_fetch(&info->start)))
			return res;

		/* Init is allowed on a SD item only. */
		if (info->start.plug->id.group != STAT_ITEM)
			return -EINVAL;
	} else {
		/* Init is allowed on a SD item only. */
		return -EINVAL;
	}
	
	/* Getting object plugin by first item coord. */
	if ((res = plug_call(info->start.plug->o.item_ops->object,
			     fetch_units, &info->start, &trans)) != 1)
		return res;
	
	aal_memcpy(&info->opset.plug, &plugh, sizeof(plugh));
	
	/* Object plugin must be detected. */
	return info->opset.plug[OPSET_OBJ] ? 0 : -EINVAL;
}

/* Helper funtion, which initializes @object->ent by @object->info. */
object_entity_t *reiser4_object_recognize(object_info_t *info) {
	if (reiser4_object_init(info))
		return INVAL_PTR;

	/* Requesting object plugin to open the object on passed @tree and
	   @place. If it fails, we will continue lookup. */
	return plug_call(info->opset.plug[OPSET_OBJ]->o.object_ops, open, info);
}

/* Tries to open object at @place. Uses @init_func for initializing object
   entity. It is needed, because libreiser4 itself uses one style of object
   entity initializing and librepair another one, but both they use some amount
   of common code, which was moved to this function and used by both in such a
   manner. */
reiser4_object_t *reiser4_object_form(reiser4_tree_t *tree,
				      reiser4_object_t *parent,
				      reiser4_key_t *okey,
				      reiser4_place_t *place,
				      object_init_t init_func)
{
	reiser4_object_t *object;
	object_info_t info;
	
	aal_assert("umka-1508", tree != NULL);
	aal_assert("umka-1509", place != NULL);
	aal_assert("vpf-1409",  okey != NULL);
	aal_assert("vpf-1221",  init_func != NULL);

	aal_memset(&info, 0, sizeof(info));
	
	if (!(object = aal_calloc(sizeof(*object), 0)))
		return INVAL_PTR;

	/* Initializing object info. */
	info.tree = (tree_entity_t *)tree;

	/* Putting object key to info struct. We may want to open and fix 
	   the object even if @place->key does not match @okey. */
	reiser4_key_assign(&info.object, okey);

	/* Copying item coord. */
	aal_memcpy(&info.start, place, sizeof(*place));

	/* Parent is not passed. Using object's key as parent's one. */
	if (parent)
		reiser4_key_assign(&info.parent, &parent->ent->object);

	/* Calling @init_func. It returns zero for success. */
	if (!(object->ent = init_func(&info)) || object->ent == INVAL_PTR)
		goto error_free_object;
	
#ifndef ENABLE_STAND_ALONE
	{
		/* Initializing object name. */
		char *name = reiser4_print_key(&object->ent->object, PO_INODE);

		aal_strncpy(object->name, name, sizeof(object->name));
	}
#endif
	
	return object;

 error_free_object:
	aal_free(object);
	return NULL;
}

/* This function opens object by its @place. */
reiser4_object_t *reiser4_object_open(reiser4_tree_t *tree, 
				      reiser4_object_t *parent,
				      reiser4_place_t *place)
{
	reiser4_object_t *object;
	
	aal_assert("vpf-1223", place != NULL);

	object = reiser4_object_form(tree, parent, &place->key, place, 
				     reiser4_object_recognize);

	return (object == INVAL_PTR ? NULL : object);
}

/* Try to open the object on the base of the given key. 
   Lookup by @key + object_form. */
reiser4_object_t *reiser4_object_obtain(reiser4_tree_t *tree,
					reiser4_object_t *parent,
					reiser4_key_t *key) 
{
	lookup_hint_t hint;
	reiser4_place_t place;

	aal_assert("vpf-1136", tree != NULL);
	aal_assert("vpf-1185", key != NULL);

	hint.key = key;
	hint.level = LEAF_LEVEL;
#ifndef ENABLE_STAND_ALONE
	hint.collision = NULL;
#endif

	if (reiser4_tree_lookup(tree, &hint, FIND_EXACT, &place) != PRESENT)
		return NULL;

	/* The key must point to the start of the object. */
	if (reiser4_key_compfull(&place.key, key))
		return NULL;

	/* If the pointed item was found, object must be
	   openable. @parent probably should be passed here. */
	return reiser4_object_open(tree, parent, &place);
}

/* Returns object size. That is stat data field st_size. Actually it might be
   got by means of using object_stat() function, but, we implemented this
   function as helper, because using object_stat() is rather complicated due to
   somplex initializing stat data extensions to be loaded by it. */
uint64_t reiser4_object_size(reiser4_object_t *object) {
	stat_hint_t hint;
	sdhint_lw_t lwh;

	aal_assert("umka-1961", object != NULL);

	/* Initializing stat data hint. And namely extension mask of extension
	   slot we are interested in. Size lies in light weight extension. */

	/* FIXME-UMKA: Why object (on API abstraction level) knows, that size
	   lies in LW extension? What if someone will move it to another one? */
	hint.extmask = 1 << SDEXT_LW_ID;
	hint.ext[SDEXT_LW_ID] = &lwh;

	/* Calling objects stat() method. */
	if (plug_call(objplug(object)->o.object_ops,
		      stat, object->ent, &hint))
		return 0;

	return lwh.size;
}

/* Closes specified object */
void reiser4_object_close(
	reiser4_object_t *object)    /* object to be closed */
{
	aal_assert("umka-680", object != NULL);
	aal_assert("umka-1149", object->ent != NULL);

	plug_call(objplug(object)->o.object_ops, close, object->ent);
	aal_free(object);
}

#ifndef ENABLE_STAND_ALONE
/* Adds @entry to @object */
errno_t reiser4_object_add_entry(
	reiser4_object_t *object,           /* object for adding entry */
	entry_hint_t *entry)                /* entry hint to be added */
{
	aal_assert("umka-1975", object != NULL);
	aal_assert("umka-1976", object->ent != NULL);

	if (!objplug(object)->o.object_ops->add_entry)
		return -EINVAL;
	
	return plug_call(objplug(object)->o.object_ops,
			 add_entry, object->ent, entry);
}

/* Removes @entry to @object */
errno_t reiser4_object_rem_entry(
	reiser4_object_t *object,           /* object for removing */
	entry_hint_t *entry)                /* entry hint to be added */
{
	aal_assert("umka-1977", object != NULL);
	aal_assert("umka-1978", object->ent != NULL);
    
	if (!objplug(object)->o.object_ops->rem_entry)
		return -EINVAL;
	
	return plug_call(objplug(object)->o.object_ops,
			 rem_entry, object->ent, entry);
}

errno_t reiser4_object_truncate(
	reiser4_object_t *object,           /* object for truncating */
	uint64_t n)			    /* the number of entries */
{
	aal_assert("umka-1154", object != NULL);
	aal_assert("umka-1155", object->ent != NULL);
    
	return plug_call(objplug(object)->o.object_ops,
			 truncate, object->ent, n);
}

/* Adds speficied entry into passed opened dir */
int64_t reiser4_object_write(
	reiser4_object_t *object,           /* object for writing */
	void *buff,			    /* new entries buffer */
	uint64_t n)			    /* the number of entries to 
					       be created */
{
	aal_assert("umka-862", object != NULL);
	aal_assert("umka-863", object->ent != NULL);
    
	if (!objplug(object)->o.object_ops->write)
		return -EINVAL;
	
	return plug_call(objplug(object)->o.object_ops,
			 write, object->ent, buff, n);
}

/* Updates object stat data coord by means of using tree_lookup(). */
errno_t reiser4_object_refresh(reiser4_object_t *object) {
	reiser4_tree_t *tree;
	lookup_hint_t hint;

	hint.level = LEAF_LEVEL;
	hint.key = &object->ent->object;

	hint.collision = NULL;

	tree = (reiser4_tree_t *)object->ent->tree;
	
	switch (reiser4_tree_lookup(tree, &hint, FIND_EXACT, 
				    object_start(object)))
	{
	case PRESENT:
		return 0;
	default:
		return -EINVAL;
	}
}

/* Saves stat data described by @hint to @object stat data item in tree. */
errno_t reiser4_object_update(reiser4_object_t *object, stat_hint_t *hint) {
	aal_assert("umka-2572", object != NULL);
	aal_assert("umka-2573", object->ent != NULL);

	return plug_call(objplug(object)->o.object_ops,
			 update, object->ent, hint);
}

/* Helper function for prepare object key to be used for creating new object. */
static void reiser4_object_maintain(reiser4_tree_t *tree,
				    entry_hint_t *entry,
				    object_hint_t *hint) 
{
	oid_t locality;
	oid_t ordering;
	oid_t objectid;
	
	/* Initializing fields and preparing the keys */
	hint->info.tree = (tree_entity_t *)tree;

	if (hint->info.parent.plug) {
		/* Parent if defined, getting locality from it. */
		objectid = reiser4_oid_allocate(tree->fs->oid);
		locality = reiser4_key_get_objectid(&hint->info.parent);
	} else {
		/* Parent is not defined, root key is used. */
		reiser4_key_assign(&hint->info.parent, &tree->key);
		locality = reiser4_key_get_locality(&tree->key);
		objectid = reiser4_key_get_objectid(&tree->key);
	}
	
	/* New object is identified by its locality and objectid. Set them to
	   the @object->info.object key and plugin create method will build the
	   whole key there. */
	hint->info.object.plug = tree->key.plug;

	/* Ordering component of key to be used for object. */
	ordering = reiser4_key_get_ordering(&entry->offset);

	/* Building object stat data key. */
	reiser4_key_build_generic(&hint->info.object, KEY_STATDATA_TYPE,
				  locality, ordering, objectid, 0);
}

/* Creates new object on specified filesystem */
reiser4_object_t *reiser4_object_create(
	reiser4_tree_t *tree,                /* tree object to be created on */
	entry_hint_t *entry,                 /* entry hint object to be used */
	object_hint_t *hint)                 /* object hint */
{
	reiser4_object_t *object;
	reiser4_plug_t *plug;
	char *name;
	
	aal_assert("umka-790", tree != NULL);
	aal_assert("umka-1128", hint != NULL);

	plug = hint->info.opset.plug[OPSET_OBJ];
	
	aal_assert("umka-1917", plug != NULL);

	/* Allocating the memory for object instance */
	if (!(object = aal_calloc(sizeof(*object), 0)))
		return NULL;

	/* Preparing object info. */
	reiser4_object_maintain(tree, entry, hint);

	/* Calling object plugin to create its body in the tree */
	if (!(object->ent = plug_call(plug->o.object_ops, create, hint)))
		goto error_free_object;

	/* @hint->object key is built by plugin create method. */
	name = reiser4_print_key(&object->ent->object, PO_INODE);
	aal_strncpy(object->name, name, sizeof(object->name));
	
	return object;
	
 error_free_object:
	aal_free(object);
	return NULL;
}

/* Removes object body and stat data */
errno_t reiser4_object_clobber(reiser4_object_t *object) {
	aal_assert("umka-2297", object != NULL);

	return plug_call(objplug(object)->o.object_ops,
			 clobber, object->ent);
}

/* Links @child to @object if it is a directory */
errno_t reiser4_object_link(reiser4_object_t *object,
			    reiser4_object_t *child,
			    entry_hint_t *entry)
{
	errno_t res;
	
	aal_assert("umka-1945", child != NULL);

	/* Check if we need to add entry in parent @object */
	if (entry && object) {
		reiser4_key_assign(&entry->object, 
				   &child->ent->object);

		if ((res = reiser4_object_add_entry(object, entry))) {
			aal_error("Can't add entry %s to %s.",
				  entry->name, object->name);
			return res;
		}
	}

	/* Add one hard link to @child. */
	if (objplug(child)->o.object_ops->link) {
		if ((res = plug_call(objplug(child)->o.object_ops, 
				     link, child->ent)))
		     return res;
	}

	/* Attach @child to @parent. */
	if (objplug(child)->o.object_ops->attach) {
		object_entity_t *parent = object ?
			object->ent : NULL;

		return plug_call(objplug(child)->o.object_ops,
				 attach, child->ent, parent);
	}

	return 0;
}

/* Removes entry from the @object if it is a directory */
errno_t reiser4_object_unlink(reiser4_object_t *object,
			      entry_hint_t *entry)
{
	reiser4_object_t *child;
	reiser4_place_t place;
	reiser4_tree_t *tree;
	lookup_hint_t hint;
	errno_t res = 0;
	
	aal_assert("umka-1910", object != NULL);

	/* Getting child statdata key */
	if (reiser4_object_lookup(object, entry->name,
				  entry) != PRESENT)
	{
		aal_error("Can't find entry %s in %s.",
			  entry->name, object->name);
		return -EINVAL;
	}

	/* Removing entry from @object. */
	if ((res = reiser4_object_rem_entry(object, entry))) {
		aal_error("Can't remove entry %s in %s.",
			  entry->name, object->name);
		return res;
	}

	hint.level = LEAF_LEVEL;
	hint.key = &entry->object;
	hint.collision = NULL;
	
	tree = (reiser4_tree_t *)object->ent->tree;
	
	/* Looking up for the victim's statdata place */
	if (reiser4_tree_lookup(tree, &hint, FIND_EXACT, &place) != PRESENT) {
		char *key = reiser4_print_key(&entry->object, PO_DEFAULT);
		aal_error("Can't find an item pointed by %s. "
			  "Entry %s/%s points to nowere.",
			  key, object->name, entry->name);
		return -EINVAL;
	}

	/* Opening victim object by found place */
	if (!(child = reiser4_object_open(tree, object, &place))) {
		aal_error("Can't open %s/%s. Object is corrupted?",
			  object->name, entry->name);
		return -EINVAL;
	}

	/* Remove one hard link from child. */
	if (objplug(child)->o.object_ops->unlink) {
		if ((res = plug_call(objplug(child)->o.object_ops,
				     unlink, child->ent)))
			return res;
	}

	/* Detach @child from parent. */
	if (objplug(child)->o.object_ops->detach) {
		if ((res = plug_call(objplug(child)->o.object_ops, detach,
				     child->ent, object->ent)))
			return res;
	}

	reiser4_object_close(child);
	return res;
}

/* Enumerates all blocks passed @object occupies */
errno_t reiser4_object_layout(
	reiser4_object_t *object,   /* object we working with */
	region_func_t region_func,  /* layout callback function */
	void *data)                 /* user-specified data */
{
	aal_assert("umka-1469", object != NULL);
	aal_assert("umka-1470", region_func != NULL);

	if (!objplug(object)->o.object_ops->layout)
		return 0;
	
	return plug_call(objplug(object)->o.object_ops, layout, 
			 object->ent, region_func, data);
}

/* Enumerates all items object consists of */
errno_t reiser4_object_metadata(
	reiser4_object_t *object,   /* object we working with */
	place_func_t place_func,    /* metadata layout callback */
	void *data)                 /* user-spaecified data */
{
	aal_assert("umka-1714", object != NULL);
	aal_assert("umka-1715", place_func != NULL);

	if (!objplug(object)->o.object_ops->metadata)
		return 0;
	
	return plug_call(objplug(object)->o.object_ops, metadata, 
			 object->ent, place_func, data);
}

/* Makes lookup inside the @object */
lookup_t reiser4_object_lookup(reiser4_object_t *object,
			       const char *name,
			       entry_hint_t *entry)
{
	aal_assert("umka-1919", object != NULL);
	aal_assert("umka-1920", name != NULL);

	if (!objplug(object)->o.object_ops->lookup)
		return -EINVAL;
	
	return plug_call(objplug(object)->o.object_ops, lookup, 
			 object->ent, (char *)name, (void *)entry);
}

/* Loads object stat data to @hint. */
errno_t reiser4_object_stat(reiser4_object_t *object, stat_hint_t *hint) {
	aal_assert("umka-2570", object != NULL);
	aal_assert("umka-2571", object->ent != NULL);

	return plug_call(objplug(object)->o.object_ops,
			 stat, object->ent, hint);
}

/* Resets directory position */
errno_t reiser4_object_reset(
	reiser4_object_t *object)    /* dir to be reset */
{
	aal_assert("umka-842", object != NULL);
	aal_assert("umka-843", object->ent != NULL);

	return plug_call(objplug(object)->o.object_ops,
			 reset, object->ent);
}

/* Sets directory current position to passed pos */
errno_t reiser4_object_seek(
	reiser4_object_t *object,    /* object position to be changed in */
	uint32_t offset)	     /* offset for seeking */
{
	aal_assert("umka-1129", object != NULL);
	aal_assert("umka-1153", object->ent != NULL);
    
	if (!objplug(object)->o.object_ops->seek)
		return -EINVAL;
	
	return plug_call(objplug(object)->o.object_ops,
			 seek, object->ent, offset);
}

/* Change current position in passed @object if it is a directory */
errno_t reiser4_object_seekdir(reiser4_object_t *object,
			       reiser4_key_t *offset)
{
	aal_assert("umka-1979", object != NULL);
	aal_assert("umka-1980", offset != NULL);

	if (!objplug(object)->o.object_ops->seekdir)
		return -EINVAL;

	return plug_call(objplug(object)->o.object_ops,
			 seekdir, object->ent, offset);
}

/* Returns current position in directory */
uint32_t reiser4_object_offset(
	reiser4_object_t *object)    /* dir position will be obtained from */
{
	aal_assert("umka-875", object != NULL);
	aal_assert("umka-876", object->ent != NULL);

	return plug_call(objplug(object)->o.object_ops, 
			 offset, object->ent);
}

/* Return current position in passed @object if it is a directory */
errno_t reiser4_object_telldir(reiser4_object_t *object,
			       reiser4_key_t *offset)
{
	aal_assert("umka-1981", object != NULL);
	aal_assert("umka-1982", offset != NULL);

	if (!objplug(object)->o.object_ops->telldir)
		return -EINVAL;

	return plug_call(objplug(object)->o.object_ops,
			 telldir, object->ent, offset);
}

/* Reads @n bytes of data at the current offset of @object to passed
   @buff. Returns numbers bytes read. */
int64_t reiser4_object_read(
	reiser4_object_t *object,   /* object entry will be read from */
	void *buff,		    /* buffer result will be stored in */
	uint64_t n)                 /* buffer size */
{
	aal_assert("umka-860", object != NULL);
	aal_assert("umka-861", object->ent != NULL);

	if (!objplug(object)->o.object_ops->read)
		return -EINVAL;
	
	return plug_call(objplug(object)->o.object_ops, 
			 read, object->ent, buff, n);
}

/* Reads entry at current @object offset to passed @entry hint */
errno_t reiser4_object_readdir(reiser4_object_t *object,
			       entry_hint_t *entry)
{
	aal_assert("umka-1973", object != NULL);
	aal_assert("umka-1974", entry != NULL);

	if (!objplug(object)->o.object_ops->readdir)
		return -EINVAL;
	
	return plug_call(objplug(object)->o.object_ops, 
			 readdir, object->ent, entry);
}

/* Completes object creating. */
static reiser4_object_t *reiser4_obj_create(reiser4_tree_t *tree,
					    reiser4_object_t *parent,
					    entry_hint_t *entry,
					    object_hint_t *hint)
{
	reiser4_object_t *object;
	
	/* Preparing @entry to be used for object creating and linking to parent
	   object. This is name and offset key. */
	if (parent) {
		if (!objplug(parent)->o.object_ops->build_entry) {
			aal_error("Object %s has not build_entry() "
				  "method implemented. Is it dir "
				  "object at all?", parent->name);
			return NULL;
		}
		
		plug_call(objplug(parent)->o.object_ops, 
			  build_entry, parent->ent, entry);
	} else {
		reiser4_key_assign(&entry->offset, &tree->key);
	}
	
	entry->place_func = NULL;
	
	/* Creating object by passed parameters. */
	if (!(object = reiser4_object_create(tree, entry, hint)))
		return NULL;

	if (parent) {
		if (reiser4_object_link(parent, object, entry)) {
			aal_warn("Removing not attached object %s "
				 "from tree.", object->name);
			reiser4_object_clobber(object);
			reiser4_object_close(object);
			return NULL;
		}
	}

	return object;
}

/* Enumerates all enries in @object. Calls @open_func for each of them. Used in
   semanthic path in librepair. */
errno_t reiser4_object_traverse(reiser4_object_t *object,
				object_open_func_t open_func,
				void *data)
{
	errno_t res;
	entry_hint_t entry;
	
	aal_assert("vpf-1090", object != NULL);
	aal_assert("vpf-1103", open_func != NULL);

	/* Check if object has readdir() method implemented. */
	if (!objplug(object)->o.object_ops->readdir)
		return 0;

	/* Main loop until all entries enumerated. */
	while ((res = reiser4_object_readdir(object, &entry)) > 0) {
		reiser4_object_t *child = NULL;

		/* Opening child object by @entry. */
		if ((child = open_func(object, &entry, data)) == INVAL_PTR)
			return -EINVAL;
		
		if (child == NULL)
			continue;

		/* Making recursive call to object_traverse() in order to
		   traverse new opened child object. */
		res = reiser4_object_traverse(child, open_func, data);
		
		reiser4_object_close(child);
		
		if (res != 0)
			return res;
	}
	
	return res;
}

/* Creates directory. Uses params preset for all plugin. */
reiser4_object_t *reiser4_dir_create(reiser4_fs_t *fs,
				     reiser4_object_t *parent,
				     const char *name)
{
	entry_hint_t entry;
	object_hint_t hint;
	tree_entity_t *tent;
	
	aal_assert("vpf-1053", fs != NULL);
	aal_assert("vpf-1611", fs->tree != NULL);
	
	aal_memset(&hint, 0, sizeof(hint));

	tent = &fs->tree->ent;
	
	/* Preparing object hint */
	hint.mode = 0;
	hint.info.opset.plug[OPSET_OBJ] = tent->opset[OPSET_MKDIR];
	hint.info.opset.plug[OPSET_STAT] = tent->opset[OPSET_STAT];
	hint.info.opset.plug[OPSET_HASH] = tent->opset[OPSET_HASH];
	hint.info.opset.plug[OPSET_FIBRE] = tent->opset[OPSET_FIBRE];
	hint.info.opset.plug[OPSET_DIRITEM] = tent->opset[OPSET_DIRITEM];
	
	if (parent) {
		reiser4_key_assign(&hint.info.parent, 
				   &parent->ent->object);
	}

	if (name) {
		aal_strncpy(entry.name, name, sizeof(entry.name));
	} else {
		entry.name[0] = '\0';
	}

	return reiser4_obj_create(fs->tree, parent, &entry, &hint);
}

/* Creates regular file, using all plugins it need from profile. Links new
   created file to @parent with @name. */
reiser4_object_t *reiser4_reg_create(reiser4_fs_t *fs,
				     reiser4_object_t *parent,
				     const char *name)
{
	entry_hint_t entry;
	object_hint_t hint;
	tree_entity_t *tent;
	
	aal_assert("vpf-1054", fs != NULL);
	
	aal_memset(&hint, 0, sizeof(hint));

	tent = &fs->tree->ent;
	
	/* Preparing object hint */
	hint.info.opset.plug[OPSET_OBJ] = tent->opset[OPSET_CREATE];

	/* Preparing label fields. */
	hint.mode = 0;
	hint.info.opset.plug[OPSET_STAT] = tent->opset[OPSET_STAT];

	/* Preparing body fields. */
	hint.info.opset.plug[OPSET_TAIL] = tent->opset[OPSET_TAIL];
	hint.info.opset.plug[OPSET_EXTENT] = tent->opset[OPSET_EXTENT];
	hint.info.opset.plug[OPSET_POLICY] = tent->opset[OPSET_POLICY];
	
	if (parent) {
		reiser4_key_assign(&hint.info.parent, 
				   &parent->ent->object);
	}

	if (name) {
		aal_strncpy(entry.name, name, sizeof(entry.name));
	} else {
		entry.name[0] = '\0';
	}

	return reiser4_obj_create(fs->tree, parent, &entry, &hint);
}

/* Creates symlink. Uses params preset for all plugin. */
reiser4_object_t *reiser4_sym_create(reiser4_fs_t *fs,
				     reiser4_object_t *parent,
		                     const char *name,
		                     const char *target)
{
	entry_hint_t entry;
	object_hint_t hint;
	tree_entity_t *tent;
	
	aal_assert("vpf-1186", fs != NULL);
	aal_assert("vpf-1057", target != NULL);
	
	tent = &fs->tree->ent;
	
	/* Preparing object hint */
	hint.info.opset.plug[OPSET_OBJ] = tent->opset[OPSET_SYMLINK];

	/* Preparing label fields. */
	hint.mode = 0;
	hint.info.opset.plug[OPSET_STAT] = tent->opset[OPSET_STAT];

	/* Preparing body fields. */
	hint.name = (char *)target;
	
	if (parent) {
		reiser4_key_assign(&hint.info.parent, 
				   &parent->ent->object);
	}

	if (name) {
		aal_strncpy(entry.name, name, sizeof(entry.name));
	} else {
		entry.name[0] = '\0';
	}

	return reiser4_obj_create(fs->tree, parent, &entry, &hint);
}

/* Creates special file. Uses params preset for all plugin. */
reiser4_object_t *reiser4_spl_create(reiser4_fs_t *fs,
				     reiser4_object_t *parent,
		                     const char *name,
				     uint32_t mode,
		                     uint64_t rdev)
{
	entry_hint_t entry;
	object_hint_t hint;
	tree_entity_t *tent;
	
	aal_assert("umka-2534", fs != NULL);
	aal_assert("umka-2535", rdev != 0);
	
	tent = &fs->tree->ent;
	
	/* Preparing object hint. */
	hint.info.opset.plug[OPSET_OBJ] = tent->opset[OPSET_MKNODE];

	/* Preparing label fields. */
	hint.mode = mode;
	hint.info.opset.plug[OPSET_STAT] = tent->opset[OPSET_STAT];

	/* Preparing body fields. */
	hint.rdev = rdev;
	
	if (parent) {
		reiser4_key_assign(&hint.info.parent, 
				   &parent->ent->object);
	}
	
	if (name) {
		aal_strncpy(entry.name, name, sizeof(entry.name));
	} else {
		entry.name[0] = '\0';
	}

	return reiser4_obj_create(fs->tree, parent, &entry, &hint);
}

#endif
