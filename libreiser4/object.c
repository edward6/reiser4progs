/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   object.c -- common code for all reiser4 objects (regular files, directories,
   symlinks, etc). */

#include <aux/aux.h>
#include <reiser4/reiser4.h>

/* Helper funtion, which initializes @object->entity by @object->info. */
static errno_t reiser4_object_init(reiser4_object_t *object,
				   reiser4_object_t *parent)
{
	place_t *place;
	reiser4_plug_t *plug;

	aal_assert("umka-2380", object != NULL);

	/* Getting object plugin by first item coord. */
	place = object_start(object);
	
	if (!(plug = reiser4_semantic_plug(object->tree, place)))
		return -EINVAL;
	    
	/* Requesting object plugin to open the object on passed @tree and
	   @place. If it fails, we will continue lookup. */
	if (!(object->entity = plug_call(plug->o.object_ops,
					 open, object->info)))
	{
		return -EINVAL;
	}

	return 0;
}

/* Closes object entity. */
static void reiser4_object_fini(reiser4_object_t *object) {
	plug_call(object->entity->plug->o.object_ops,
		  close, object->entity);
	
	object->entity = NULL;
}

/* Returns object size. That is stat data field st_size. Actually it might be
   got by means of using object_stat() function, but, we implemented this
   function as helper, because using object_stat() is rather complicated due to
   somplex initializing stat data extensions to be loaded by it. */
uint64_t reiser4_object_size(reiser4_object_t *object) {
	statdata_hint_t hint;
	sdext_lw_hint_t lw_hint;
	
	aal_assert("umka-1961", object != NULL);

	/* Initializing stat data hint. And namely extension mask of extension
	   slot we are interested in. Size lies in light weight extension. */

	/* FIXME-UMKA: Why object (on API abstraction level) knows, that size
	   lies in LW extension? What if someone will move it to another one? */
	hint.extmask = 1 << SDEXT_LW_ID;
	hint.ext[SDEXT_LW_ID] = &lw_hint;

	/* Calling objects stat() method. */
	if (plug_call(object->entity->plug->o.object_ops,
		      stat, object->entity, &hint))
	{
		return 0;
	}

	return lw_hint.size;
}

/* Updates object stat data coord by means of using tree_lookup(). */
errno_t reiser4_object_refresh(reiser4_object_t *object) {
	object_info_t *info = object->info;

	switch (reiser4_tree_lookup(object->tree, &info->object,
				    LEAF_LEVEL, FIND_EXACT,
				    object_start(object)))
	{
	case PRESENT:
		return 0;
	default:
		return -EINVAL;
	}
}

/* Resolve passed @path. */
errno_t reiser4_object_resolve(reiser4_object_t *object,
			       char *path, bool_t follow)
{
	reiser4_key_t *root_key;
	
	aal_assert("umka-2247", path != NULL);
	aal_assert("umka-2246", object != NULL);

	/* Resolving object by @path starting from root key. */
	root_key = &object->tree->key;

	/* Calling semantic resolve. */
	if (!(object->entity = reiser4_semantic_resolve(object->tree,
							path, root_key,
							follow)))
	{
		return -EINVAL;
	}

	/* Assigning info reference to entity info instance */
	object->info = &object->entity->info;
	
	return 0;
}

/* This function opens object by its name */
reiser4_object_t *reiser4_object_open(
	reiser4_tree_t *tree,		/* tree object will be opened on */
	char *path,                     /* name of object to be opened */
	bool_t follow)                  /* follow symlinks */
{
#ifndef ENABLE_STAND_ALONE
	char *name;
#endif
	reiser4_object_t *object;
    
	aal_assert("umka-678", tree != NULL);
	aal_assert("umka-789", path != NULL);

	if (!(object = aal_calloc(sizeof(*object), 0)))
		return NULL;
    
	object->tree = tree;

	/* Semantic resolve of @path. */
	if (reiser4_object_resolve(object, path, follow))
		goto error_free_object;
	

	/* Initializing object name. It is stat data key as string. */
#ifndef ENABLE_STAND_ALONE
	name = reiser4_print_key(&object->info->object, PO_INODE);
	aal_strncpy(object->name, name, sizeof(object->name));
#endif

	return object;
    
 error_free_object:
	aal_free(object);
	return NULL;
}

/* Tries to open object at @place. Uses @init_func for initializing object
   entity. It is needed, because libreiser4 itself uses one style of object
   entity initializing and librepair another one, but both they use some amount
   of common code, which was moved to this function and used by both in such a
   manner. */
reiser4_object_t *reiser4_object_guess(reiser4_tree_t *tree,
				       reiser4_object_t *parent,
				       reiser4_key_t *okey,
				       place_t *place,
				       object_init_t init_func)

{
	errno_t res;
	object_info_t info;
	reiser4_object_t *object;
	
	aal_assert("umka-1508", tree != NULL);
	aal_assert("umka-1509", place != NULL);
	aal_assert("vpf-1221",  init_func != NULL);

	aal_memset(&info, 0, sizeof(info));
	
	if (!(object = aal_calloc(sizeof(*object), 0)))
		return INVAL_PTR;

	object->tree = tree;
	
	/* Initializing object info. */
	object->info = &info;
	object->info->tree = tree;

	/* Parent is not passed. Using object's key as parent's one. */
	if (parent) {
		reiser4_key_assign(&object->info->parent, 
				   &parent->info->object);
	}

	/* Putting object key to info struct. */
	if (okey) {
		/* We may want to open and fix the object even if 
		   @place->key does not match @okey. */
		reiser4_key_assign(&object->info->object, okey);
	}

	/* Copying item coord. */
	aal_memcpy(&object->info->start, place, sizeof(*place));

	/* Calling @init_func. It returns zero for success. */
	if ((res = init_func(object, parent)))
		goto error_free_object;

	object->info = &object->entity->info;

	/* Initializing object name. */
#ifndef ENABLE_STAND_ALONE
	{
		char *name = reiser4_print_key(&object->info->object,
					       PO_INODE);
		
		aal_strncpy(object->name, name, sizeof(object->name));
	}
#endif
	
	return object;
	
 error_free_object:
	aal_free(object);
	return NULL;
}

/* This function opens object by its @place. */
reiser4_object_t *reiser4_object_realize(reiser4_tree_t *tree, 
					 reiser4_object_t *parent,
					 place_t *place)
{
	reiser4_object_t *object;
	
	aal_assert("vpf-1223", place != NULL);
	
	object = reiser4_object_guess(tree, parent, &place->key,
				      place, reiser4_object_init);

	return (object == INVAL_PTR ? NULL : object);
}

#ifndef ENABLE_STAND_ALONE
/* Try to open the object on the base of the given key. */
reiser4_object_t *reiser4_object_launch(reiser4_tree_t *tree,
					reiser4_object_t *parent,
					reiser4_key_t *key) 
{
	place_t place;

	aal_assert("vpf-1136", tree != NULL);
	aal_assert("vpf-1185", key != NULL);
	
	switch (reiser4_tree_lookup(tree, key, LEAF_LEVEL,
				    FIND_EXACT, &place))
	{
	case PRESENT:
		/* The key must point to the start of the object. */
		if (reiser4_key_compfull(&place.key, key))
			return NULL;
	
		/* If the pointed item was found, object must be
		   openable. @parent probably should be passed here. */
		return reiser4_object_realize(tree, parent, &place);
	default:
		return NULL;
	}
}

errno_t reiser4_object_truncate(
	reiser4_object_t *object,           /* object for truncating */
	uint64_t n)			    /* the number of entries */
{
	aal_assert("umka-1154", object != NULL);
	aal_assert("umka-1155", object->entity != NULL);
    
	return plug_call(object->entity->plug->o.object_ops, 
			 truncate, object->entity, n);
}

/* Adds @entry to @object */
errno_t reiser4_object_add_entry(
	reiser4_object_t *object,           /* object for adding entry */
	entry_hint_t *entry)                /* entry hint to be added */
{
	aal_assert("umka-1975", object != NULL);
	aal_assert("umka-1976", object->entity != NULL);

	if (!object->entity->plug->o.object_ops->add_entry)
		return -EINVAL;
	
	return plug_call(object->entity->plug->o.object_ops, 
			 add_entry, object->entity, entry);
}

/* Removes @entry to @object */
errno_t reiser4_object_rem_entry(
	reiser4_object_t *object,           /* object for removing */
	entry_hint_t *entry)                /* entry hint to be added */
{
	aal_assert("umka-1977", object != NULL);
	aal_assert("umka-1978", object->entity != NULL);
    
	if (!object->entity->plug->o.object_ops->rem_entry)
		return -EINVAL;
	
	return plug_call(object->entity->plug->o.object_ops, 
			 rem_entry, object->entity, entry);
}

/* Adds speficied entry into passed opened dir */
int64_t reiser4_object_write(
	reiser4_object_t *object,           /* object for writing */
	void *buff,			    /* new entries buffer */
	uint64_t n)			    /* the number of entries to be created */
{
	aal_assert("umka-862", object != NULL);
	aal_assert("umka-863", object->entity != NULL);
    
	if (!object->entity->plug->o.object_ops->write)
		return -EINVAL;
	
	return plug_call(object->entity->plug->o.object_ops, 
			 write, object->entity, buff, n);
}

/* Loads object stat data to @hint. */
errno_t reiser4_object_stat(reiser4_object_t *object,
			    statdata_hint_t *hint)
{
	aal_assert("umka-2570", object != NULL);
	aal_assert("umka-2571", object->entity != NULL);

	return plug_call(object->entity->plug->o.object_ops,
			 stat, object->entity, hint);
}

/* Saves stat data described by @hint to @object stat data item in tree. */
errno_t reiser4_object_update(reiser4_object_t *object,
			      statdata_hint_t *hint)
{
	aal_assert("umka-2572", object != NULL);
	aal_assert("umka-2573", object->entity != NULL);

	return plug_call(object->entity->plug->o.object_ops,
			 update, object->entity, hint);
}

/* Helper function for prepare object key to be used for creating new object. */
static void reiser4_object_base(reiser4_tree_t *tree,
				entry_hint_t *entry,
				object_hint_t *hint,
				object_info_t *info) 
{
	oid_t locality;
	oid_t ordering;
	oid_t objectid;
	
	/* Initializing fields and preparing the keys */
	info->tree = tree;

	if (hint->parent) {
		/* Parent if defined, getting locality from it. */
		reiser4_key_assign(&info->parent, hint->parent);
		objectid = reiser4_oid_allocate(tree->fs->oid);
		locality = reiser4_key_get_objectid(&info->parent);
	} else {
		/* Parent is not defined, root key is used. */
		reiser4_key_assign(&info->parent, &tree->key);
		locality = reiser4_key_get_locality(&tree->key);
		objectid = reiser4_key_get_objectid(&tree->key);
	}
	
	/* New object is identified by its locality and objectid. Set them to
	   the @object->info.object key and plugin create method will build the
	   whole key there. */
	info->object.plug = tree->key.plug;

	/* Ordering component of key to be used for object. */
	ordering = reiser4_key_get_ordering(&entry->offset);

	/* Building object stat data key. */
	reiser4_key_build_generic(&info->object, KEY_STATDATA_TYPE,
				  locality, ordering, objectid, 0);
}

/* Creates new object on specified filesystem */
reiser4_object_t *reiser4_object_create(
	reiser4_tree_t *tree,                /* tree object to be created on */
	entry_hint_t *entry,                 /* entry hint object to be used */
	object_hint_t *hint)                 /* object hint */
{
	char *name;
	object_info_t info;
	reiser4_object_t *object;
	
	aal_assert("umka-790", tree != NULL);
	aal_assert("umka-1128", hint != NULL);
	aal_assert("umka-1917", hint->plug != NULL);

	/* Allocating the memory for object instance */
	if (!(object = aal_calloc(sizeof(*object), 0)))
		return NULL;

	/* Preparing object info. */
	reiser4_object_base(tree, entry, hint, &info);

	/* Calling object plugin to create its body in the tree */
	if (!(object->entity = plug_call(hint->plug->o.object_ops,
					 create, &info, hint)))
	{
		goto error_free_object;
	}

	/* Assigning into to entity's info. */
	object->info = &object->entity->info;
	
	/* @hint->object key is built by plugin create method. */
	name = reiser4_print_key(&object->info->object, PO_INODE);
	aal_strncpy(object->name, name, sizeof(object->name));
	
	return object;
	
 error_free_object:
	aal_free(object);
	return NULL;
}

/* Removes object body and stat data */
errno_t reiser4_object_clobber(reiser4_object_t *object) {
	aal_assert("umka-2297", object != NULL);

	return plug_call(object->entity->plug->o.object_ops,
			 clobber, object->entity);
}

/* Returns @nlink value from the stat data if any */
uint32_t reiser4_object_links(reiser4_object_t *object) {
	aal_assert("umka-2293", object != NULL);

	return plug_call(object->entity->plug->o.object_ops,
			 links, object->entity);
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
		reiser4_key_assign(&entry->object, &child->info->object);

		if ((res = reiser4_object_add_entry(object, entry))) {
			aal_exception_error("Can't add entry %s to %s.",
					    entry->name, object->name);
			return res;
		}
	}

	/* Add one hard link to @child. */
	if (child->entity->plug->o.object_ops->link) {
		if ((res = plug_call(child->entity->plug->o.object_ops,
				     link, child->entity)))
		{
			return res;
		}
	}

	/* Attach @child to @parent. */
	if (child->entity->plug->o.object_ops->attach) {
		object_entity_t *parent = object ?
			object->entity : NULL;

		return plug_call(child->entity->plug->o.object_ops,
				 attach, child->entity, parent);
	}

	return 0;
}

/* Removes entry from the @object if it is a directory */
errno_t reiser4_object_unlink(reiser4_object_t *object,
			      entry_hint_t *entry)
{
	place_t place;
	errno_t res = 0;
	reiser4_object_t *child;
	
	aal_assert("umka-1910", object != NULL);

	/* Getting child statdata key */
	if (reiser4_object_lookup(object, entry->name,
				  entry) != PRESENT)
	{
		aal_exception_error("Can't find entry %s in %s.",
				    entry->name, object->name);
		return -EINVAL;
	}

	/* Removing entry from @object. */
	if ((res = reiser4_object_rem_entry(object, entry))) {
		aal_exception_error("Can't remove entry %s in %s.",
				    entry->name, object->name);
		return res;
	}
	
	/* Looking up for the victim's statdata place */
	if (reiser4_tree_lookup(object->info->tree, &entry->object,
				LEAF_LEVEL, FIND_EXACT, &place) != PRESENT)
	{
		char *key = reiser4_print_key(&entry->object, PO_DEFAULT);
		aal_exception_error("Can't find an item pointed by %s. "
				    "Entry %s/%s points to nowere.",
				    key, object->name, entry->name);
		return -EINVAL;
	}

	/* Opening victim object by found place */
	if (!(child = reiser4_object_realize(object->tree, object, &place))) {
		aal_exception_error("Can't open %s/%s. Object is corrupted?",
				    object->name, entry->name);
		return -EINVAL;
	}

	/* Remove one hard link from child. */
	if (child->entity->plug->o.object_ops->unlink) {
		if ((res = plug_call(child->entity->plug->o.object_ops,
				     unlink, child->entity)))
		{
			return res;
		}
	}

	/* Detach @child from parent. */
	if (child->entity->plug->o.object_ops->detach) {
		if ((res = plug_call(child->entity->plug->o.object_ops,
				     detach, child->entity, object->entity)))
		{
			return res;
		}
	}

	reiser4_object_close(child);
	return res;
}

/* Helper function for printing passed @place into @stream. */
static errno_t callback_print_place(void *entity, place_t *place,
				    void *data)
{
	errno_t res;
	
	aal_stream_t *stream = (aal_stream_t *)data;
	
	if ((res = reiser4_item_print(place, stream))) {
		aal_exception_error("Can't print item %u in "
				    "node %llu.", place->pos.item,
				    node_blocknr(place->node));
		return res;
	}
		
	aal_stream_write(stream, "\n", 1);
	return 0;
}

/* Prints object items into passed stream */
errno_t reiser4_object_print(reiser4_object_t *object,
			     aal_stream_t *stream)
{
	place_func_t place_func = callback_print_place;
	return reiser4_object_metadata(object, place_func, stream);
}

/* Enumerates all blocks passed @object occupies */
errno_t reiser4_object_layout(
	reiser4_object_t *object,   /* object we working with */
	region_func_t region_func,  /* layout callback function */
	void *data)                 /* user-specified data */
{
	reiser4_plug_t *plug;
	
	aal_assert("umka-1469", object != NULL);
	aal_assert("umka-1470", region_func != NULL);

	plug = object->entity->plug;
	
	if (!plug->o.object_ops->layout)
		return 0;
	
	return plug_call(plug->o.object_ops, layout,
			 object->entity, region_func, data);
}

/* Enumerates all items object consists of */
errno_t reiser4_object_metadata(
	reiser4_object_t *object,   /* object we working with */
	place_func_t place_func,    /* metadata layout callback */
	void *data)                 /* user-spaecified data */
{
	reiser4_plug_t *plug;
	
	aal_assert("umka-1714", object != NULL);
	aal_assert("umka-1715", place_func != NULL);

	plug = object->entity->plug;
	
	if (!plug->o.object_ops->metadata)
		return 0;
	
	return plug_call(plug->o.object_ops, metadata,
			 object->entity, place_func, data);
}

/* Makes lookup inside the @object */
lookup_t reiser4_object_lookup(reiser4_object_t *object,
			       const char *name,
			       entry_hint_t *entry)
{
	aal_assert("umka-1919", object != NULL);
	aal_assert("umka-1920", name != NULL);

	if (!object->entity->plug->o.object_ops->lookup)
		return -EINVAL;
	
	return plug_call(object->entity->plug->o.object_ops,
			 lookup, object->entity, (char *)name,
			 (void *)entry);
}
#endif

/* Sets directory current position to passed pos */
errno_t reiser4_object_seek(
	reiser4_object_t *object,    /* object position to be changed in */
	uint32_t offset)	     /* offset for seeking */
{
	aal_assert("umka-1129", object != NULL);
	aal_assert("umka-1153", object->entity != NULL);
    
	if (!object->entity->plug->o.object_ops->seek)
		return -EINVAL;
	
	return plug_call(object->entity->plug->o.object_ops, 
			 seek, object->entity, offset);
}

/* Closes specified object */
void reiser4_object_close(
	reiser4_object_t *object)    /* object to be closed */
{
	aal_assert("umka-680", object != NULL);
	aal_assert("umka-1149", object->entity != NULL);

	reiser4_object_fini(object);
	aal_free(object);
}

/* Resets directory position */
errno_t reiser4_object_reset(
	reiser4_object_t *object)    /* dir to be reset */
{
	aal_assert("umka-842", object != NULL);
	aal_assert("umka-843", object->entity != NULL);

	return plug_call(object->entity->plug->o.object_ops, 
			 reset, object->entity);
}

/* Reads @n bytes of data at the current offset of @object to passed
   @buff. Returns numbers bytes read. */
int64_t reiser4_object_read(
	reiser4_object_t *object,   /* object entry will be read from */
	void *buff,		    /* buffer result will be stored in */
	uint64_t n)                 /* buffer size */
{
	aal_assert("umka-860", object != NULL);
	aal_assert("umka-861", object->entity != NULL);

	if (!object->entity->plug->o.object_ops->read)
		return -EINVAL;
	
	return plug_call(object->entity->plug->o.object_ops, 
			 read, object->entity, buff, n);
}

/* Returns current position in directory */
uint32_t reiser4_object_offset(
	reiser4_object_t *object)    /* dir position will be obtained from */
{
	aal_assert("umka-875", object != NULL);
	aal_assert("umka-876", object->entity != NULL);

	return plug_call(object->entity->plug->o.object_ops, 
			 offset, object->entity);
}

/* Reads entry at current @object offset to passed @entry hint */
errno_t reiser4_object_readdir(reiser4_object_t *object,
			       entry_hint_t *entry)
{
	aal_assert("umka-1973", object != NULL);
	aal_assert("umka-1974", entry != NULL);

	if (!object->entity->plug->o.object_ops->readdir)
		return -EINVAL;
	
	return plug_call(object->entity->plug->o.object_ops, 
			 readdir, object->entity, entry);
}

#ifndef ENABLE_STAND_ALONE
/* Change current position in passed @object if it is a directory */
errno_t reiser4_object_seekdir(reiser4_object_t *object,
			       reiser4_key_t *offset)
{
	aal_assert("umka-1979", object != NULL);
	aal_assert("umka-1980", offset != NULL);

	if (!object->entity->plug->o.object_ops->seekdir)
		return -EINVAL;

	return plug_call(object->entity->plug->o.object_ops,
			 seekdir, object->entity, offset);
}

/* Return current position in passed @object if it is a directory */
errno_t reiser4_object_telldir(reiser4_object_t *object,
			       reiser4_key_t *offset)
{
	aal_assert("umka-1981", object != NULL);
	aal_assert("umka-1982", offset != NULL);

	if (!object->entity->plug->o.object_ops->telldir)
		return -EINVAL;

	return plug_call(object->entity->plug->o.object_ops,
			 telldir, object->entity, offset);
}

/* Completes object creating. */
static reiser4_object_t *reiser4_object_comp(reiser4_tree_t *tree,
					     reiser4_object_t *parent,
					     entry_hint_t *entry,
					     object_hint_t *hint)
{
	reiser4_object_t *object;
	
	/* Preparing @entry to be used for object creating and linking to parent
	   object. This is name and offset key. */
	if (parent) {
		if (!parent->entity->plug->o.object_ops->build_entry) {
			aal_exception_error("Object %s has not build_entry() "
					    "method implemented. Is it dir "
					    "object at all?", parent->name);
			return NULL;
		}
		
		plug_call(parent->entity->plug->o.object_ops,
			  build_entry, parent->entity, entry);
	} else {
		reiser4_key_assign(&entry->offset, &tree->key);
	}
	
	/* Creating object by passed parameters */
	if (!(object = reiser4_object_create(tree, entry, hint)))
		return NULL;

	if (parent) {
		if (reiser4_object_link(parent, object, entry)) {
			reiser4_object_clobber(object);
			reiser4_object_close(object);
			return NULL;
		}
	}

	return object;
}

/* Enumerates all enries in @object. Calls @open_func for each of them. Used in
   semantic path in librepair. */
errno_t reiser4_object_traverse(reiser4_object_t *object,
				object_open_func_t open_func,
				void *data)
{
	errno_t res;
	entry_hint_t entry;
	
	aal_assert("vpf-1090", object != NULL);
	aal_assert("vpf-1103", open_func != NULL);

	/* Check if object has readdir() method implemented. */
	if (!object->entity->plug->o.object_ops->readdir)
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
	rid_t pid;
	entry_hint_t entry;
	object_hint_t hint;
	
	aal_assert("vpf-1053", fs != NULL);
	
	pid = reiser4_param_value("directory");
	
	/* Preparing object hint */
	hint.plug = reiser4_factory_ifind(OBJECT_PLUG_TYPE, pid);

	if (!hint.plug) {
		aal_exception_error("Can't find directory plugin "
				    "by its id 0x%x.", pid);
		return NULL;
	}

	/* Preparing directory label. */
	hint.label.mode = 0;
	hint.label.statdata = reiser4_param_value("statdata");
	
	/* Preparing directory body. */
	hint.body.dir.hash = reiser4_param_value("hash");
	hint.body.dir.direntry = reiser4_param_value("direntry");
	hint.parent = (parent ? &parent->info->object : NULL);

	if (name) {
		aal_strncpy(entry.name, name,
			    sizeof(entry.name));
	} else {
		entry.name[0] = '\0';
	}
	
	return reiser4_object_comp(fs->tree, parent, &entry, &hint);
}

/* Creates regular file, using all plugins it need from profile. Links new
   created file to @parent with @name. */
reiser4_object_t *reiser4_reg_create(reiser4_fs_t *fs,
				     reiser4_object_t *parent,
				     const char *name)
{
	rid_t regular;
	entry_hint_t entry;
	object_hint_t hint;
	
	aal_assert("vpf-1054", fs != NULL);
	
	regular = reiser4_param_value("regular");
	
	/* Preparing object hint */
	hint.plug = reiser4_factory_ifind(OBJECT_PLUG_TYPE, regular);

	if (!hint.plug) {
		aal_exception_error("Can't find regual file plugin "
				    "by its id 0x%x.", regular);
		return NULL;
	}

	/* Preparing label fields. */
	hint.label.mode = 0;
	hint.label.statdata = reiser4_param_value("statdata");

	/* Preparing body fields. */
	hint.body.reg.tail = reiser4_param_value("tail");
	hint.body.reg.extent = reiser4_param_value("extent");
	hint.body.reg.policy = reiser4_param_value("policy");
	hint.parent = (parent ? &parent->info->object : NULL);
	
	if (name) {
		aal_strncpy(entry.name, name,
			    sizeof(entry.name));
	} else {
		entry.name[0] = '\0';
	}

	return reiser4_object_comp(fs->tree, parent, &entry, &hint);
}

/* Creates symlink. Uses params preset for all plugin. */
reiser4_object_t *reiser4_sym_create(reiser4_fs_t *fs,
				     reiser4_object_t *parent,
		                     const char *name,
		                     const char *target)
{
	rid_t symlink;
	entry_hint_t entry;
	object_hint_t hint;
	
	aal_assert("vpf-1186", fs != NULL);
	aal_assert("vpf-1057", target != NULL);
	
	symlink = reiser4_param_value("symlink");
	
	/* Preparing object hint */
	hint.plug = reiser4_factory_ifind(OBJECT_PLUG_TYPE, symlink);

	if (!hint.plug) {
		aal_exception_error("Can't find symlink plugin by "
				    "its id 0x%x.", symlink);
		return NULL;
	}

	/* Preparing label fields. */
	hint.label.mode = 0;
	hint.label.statdata = reiser4_param_value("statdata");

	/* Preparing body fields. */
	hint.body.sym = (char *)target;
	hint.parent = (parent ? &parent->info->object : NULL);
	
	if (name) {
		aal_strncpy(entry.name, name,
			    sizeof(entry.name));
	} else {
		entry.name[0] = '\0';
	}

	return reiser4_object_comp(fs->tree, parent, &entry, &hint);
}

/* Creates special file. Uses params preset for all plugin. */
reiser4_object_t *reiser4_spl_create(reiser4_fs_t *fs,
				     reiser4_object_t *parent,
		                     const char *name,
				     uint32_t mode,
		                     uint64_t rdev)
{
	rid_t special;
	entry_hint_t entry;
	object_hint_t hint;
	
	aal_assert("umka-2534", fs != NULL);
	aal_assert("umka-2535", rdev != 0);
	
	special = reiser4_param_value("special");
	
	/* Preparing object hint. */
	hint.plug = reiser4_factory_ifind(OBJECT_PLUG_TYPE, special);

	if (!hint.plug) {
		aal_exception_error("Can't find special file plugin "
				    "by its id 0x%x.", special);
		return NULL;
	}

	/* Preparing label fields. */
	hint.label.mode = mode;
	hint.label.statdata = reiser4_param_value("statdata");

	/* Preparing body fields. */
	hint.body.spl.rdev = rdev;
	hint.parent = (parent ? &parent->info->object : NULL);
	
	if (name) {
		aal_strncpy(entry.name, name,
			    sizeof(entry.name));
	} else {
		entry.name[0] = '\0';
	}

	return reiser4_object_comp(fs->tree, parent, &entry, &hint);
}
#endif
