/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   object.c -- common code for all reiser4 objects (regular files, directories,
   symlinks, etc). */

#include <aux/aux.h>
#include <reiser4/reiser4.h>

static errno_t reiser4_object_init(reiser4_object_t *object,
				   reiser4_object_t *parent)
{
	rid_t pid;
	reiser4_plug_t *plug;
	reiser4_place_t *place;

	aal_assert("umka-2380", object != NULL);
	
	place = object_start(object);
	
	if (!place->plug->o.item_ops->plugid) {
		/* FIXME-UMKA: Here we should try to understand what object
		   plugin is by means of asking @parent. */
		pid = INVAL_PID;
	} else {
		pid = plug_call(place->plug->o.item_ops, plugid, 
				(place_t *)place, OBJECT_PLUG_TYPE);
		
		if (pid == INVAL_PID) {
			aal_exception_error("Can't guess object plugin.");
			return -EINVAL;
		}
	}

	if (!(plug = reiser4_factory_ifind(OBJECT_PLUG_TYPE, pid))) {
		aal_exception_error("Can't find object plugin by its "
				    "id 0x%x.", pid);
		return -EINVAL;
	}
	
	/* Requesting object plugin to open the object on passed @tree and
	   @place. If it fails, we will continue lookup. */
	if (!(object->entity = plug_call(plug->o.object_ops,
					 open, object->info)))
	{
		return -EINVAL;
	}

	return 0;
}

static void reiser4_object_fini(reiser4_object_t *object) {
	plug_call(object->entity->plug->o.object_ops,
		  close, object->entity);
	object->entity = NULL;
}

uint64_t reiser4_object_size(reiser4_object_t *object) {
	aal_assert("umka-1961", object != NULL);

	return plug_call(object->entity->plug->o.object_ops,
			 size, object->entity);
}

/* Looks up for the object stat data place in tree */
errno_t reiser4_object_stat(reiser4_object_t *object) {
	object_info_t *info = object->info;

	switch (reiser4_tree_lookup(object->tree, &info->object,
				    LEAF_LEVEL, object_start(object)))
	{
	case PRESENT:
		reiser4_key_assign(&info->object,
				   &info->start.key);
		return 0;
	default:
		return -EINVAL;
	}
}

/* Callback function for finding statdata of the current directory */
static errno_t callback_find_statdata(char *track, char *entry,
				      void *data)
{
	errno_t res;
	reiser4_plug_t *plug;
	reiser4_object_t *object;

	object = (reiser4_object_t *)data;

	if ((res = reiser4_object_stat(object))) {
		aal_exception_error("Can't find stat data of %s.",
				    track);
		return res;
	}

	/* FIXME_UMKA: Here also should be parent passed. */
	if ((res = reiser4_object_init(object, NULL))) {
		aal_exception_error("Can't init object %s.", track);
		return res;
	}
	
#ifdef ENABLE_SYMLINKS
	plug = object->entity->plug;

	/* Symlinks handling. Method follow() should be implemented */
	if (object->follow && plug->o.object_ops->follow) {

		/* Calling object's follow() in order to get stat data key of
		   the real stat data item. */
		res = plug_call(plug->o.object_ops, follow, object->entity,
				&object->info->parent, &object->info->object);

	        /* Finalizing entity on old place */
		reiser4_object_fini(object);

		/* Symlink cannot be followed */
		if (res != 0) {
			aal_exception_error("Can't follow %s.", track);
			return res;
		}
		
		/* Getting stat data place by key returned by follow() */
		if ((res = reiser4_object_stat(object)))
			return res;

		/* Initializing entity on new place */
		if ((res = reiser4_object_init(object, NULL)))
			return res;
	}

	reiser4_key_assign(&object->info->parent,
			   &object->info->object);
#endif

	return 0;
}

/* Callback function for finding passed @entry inside the current directory */
static errno_t callback_find_entry(char *track, char *name,
				   void *data)
{
	errno_t res;
	entry_hint_t entry;
	reiser4_object_t *object;

	object = (reiser4_object_t *)data;

	/* Looking up for @entry in current directory */
	switch (plug_call(object->entity->plug->o.object_ops,
			  lookup, object->entity, name, &entry))
	{
	case PRESENT:
		res = reiser4_key_assign(&object->info->object,
					 &entry.object);
	default:
		aal_exception_error("Can't find %s.", track);
		res = -EINVAL;
	}

	reiser4_object_fini(object);
	return res;
}

errno_t reiser4_object_resolve(reiser4_object_t *object,
			       char *filename, bool_t follow)
{
	object_info_t info;
	
	aal_assert("umka-2246", object != NULL);
	aal_assert("umka-2247", filename != NULL);
	aal_assert("umka-2378", object->entity == NULL);
	
	object->info = &info;
	object->follow = follow;

	/* Initializing parent key to root key */
#ifdef ENABLE_SYMLINKS
	reiser4_key_assign(&object->info->parent,
			   &object->tree->key);
#endif

	/* Initializing info tree reference */
	object->info->tree = object->tree;
	
	/* Resolving path, starting from the root */
	reiser4_key_assign(&object->info->object,
			   &object->tree->key);

	/* Parsing path and looking for object's stat data. We assume, that name
	   is absolute one. So, user, who calls this method should convert name
	   previously into absolute one by means of using getcwd function. */
	if (aux_parse_path(filename, callback_find_statdata,
			   callback_find_entry, object))
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
	char *filename,                 /* name of object to be opened */
	bool_t follow)                  /* follow symlinks */
{
	reiser4_object_t *object;
    
	aal_assert("umka-678", tree != NULL);
	aal_assert("umka-789", filename != NULL);

	if (!(object = aal_calloc(sizeof(*object), 0)))
		return NULL;
    
	object->tree = tree;

#ifndef ENABLE_STAND_ALONE
	aal_strncpy(object->name, filename,
		    sizeof(object->name));
#endif

	if (reiser4_object_resolve(object, filename, follow))
		goto error_free_object;

	return object;
    
 error_free_object:
	aal_free(object);
	return NULL;
}

reiser4_object_t *reiser4_object_guess(
	reiser4_tree_t *tree,		/* tree object will be opened on */
	reiser4_object_t *parent,	/* parent of object to be opened */
	reiser4_key_t *okey,		/* key of the object to be openned */
	reiser4_place_t *place,		/* start place of the object */
	object_init_t init_func)

{
	object_info_t info;
	reiser4_object_t *object;
	
	aal_assert("umka-1508", tree != NULL);
	aal_assert("umka-1509", place != NULL);
	aal_assert("vpf-1222",  okey != NULL);
	aal_assert("vpf-1221",  init_func != NULL);

	aal_memset(&info, 0, sizeof(info));
	
	if (!(object = aal_calloc(sizeof(*object), 0)))
		return INVAL_PTR;

	/* Initializing info */
	object->info = &info;
	object->info->tree = tree;
	
	if (parent) {
		reiser4_key_assign(&object->info->parent, 
				   &parent->info->object);
	}
	
	/* We may want to open and fix the object even if @place->key 
	   does not match @okey. */
	reiser4_key_assign(&object->info->object, okey);
	aal_memcpy(&object->info->start, place, sizeof(*place));

	if (init_func(object, parent))
		goto error_free_object;

	object->info = &object->entity->info;
	
#ifndef ENABLE_STAND_ALONE
	{
		char *name = reiser4_print_key(&object->info->object, PO_INO);
		aal_strncpy(object->name, name, sizeof(object->name));
	}
#endif
	
	return object;
	
 error_free_object:
	aal_free(object);
	return NULL;
}

/* This function opens object by its @place. */
reiser4_object_t *reiser4_object_realize(
	reiser4_tree_t *tree, 
	reiser4_object_t *parent,
	reiser4_place_t *place)
{
	reiser4_object_t *object;
	
	aal_assert("vpf-1223", place != NULL);
	
	object = reiser4_object_guess(tree, parent, &place->key,
				      place, reiser4_object_init);

	return object == INVAL_PTR ? NULL : object;
}

#ifndef ENABLE_STAND_ALONE
/* Try to open the object on the base of the given key. */
reiser4_object_t *reiser4_object_launch(reiser4_tree_t *tree,
					reiser4_object_t *parent,
					reiser4_key_t *key) 
{
	reiser4_place_t place;

	aal_assert("vpf-1136", tree != NULL);
	aal_assert("vpf-1185", key != NULL);
	
	switch (reiser4_tree_lookup(tree, key, LEAF_LEVEL, &place)) {
	case PRESENT:
		/* The key must point to the start of the object. */
		if (reiser4_key_compare(&place.key, key))
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
int32_t reiser4_object_write(
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

/* Prepases @info to be used for creating new object */
static void reiser4_object_base(reiser4_tree_t *tree,
				reiser4_object_t *parent,
				object_info_t *info) 
{
	oid_t objectid;
	oid_t locality;
	
	/* Initializing fields and preparing the keys */
	info->tree = tree;

	if (parent) {
		reiser4_key_assign(&info->parent, &parent->info->object);
		
		objectid = reiser4_oid_allocate(tree->fs->oid);
		locality = reiser4_key_get_objectid(&info->parent);
	} else {
		/* Parent if not defined, root key is used */
		reiser4_key_assign(&info->parent, &tree->key);
		locality = reiser4_key_get_locality(&tree->key);
		objectid = reiser4_key_get_objectid(&tree->key);
	}
	
	/* New object is identified by its locality and objectid. Set them to
	   the @object->info.object key and plugin create method will build the
	   whole key there. */
	info->object.plug = tree->key.plug;

	/* FIXME-UMKA: Here should be used correct ordering component. It may be
	 * taken from entry in parent object. */
	reiser4_key_build_gener(&info->object, KEY_STATDATA_TYPE,
				locality, 0, objectid, 0);
}

/* Creates new object on specified filesystem */
reiser4_object_t *reiser4_object_create(
	reiser4_tree_t *tree,                /* tree object to be created on */
	reiser4_object_t *parent,            /* parent object */
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

	/* Preparing info */
	reiser4_object_base(tree, parent, &info);

	/* Calling object plugin to create its body in the tree */
	if (!(object->entity = plug_call(hint->plug->o.object_ops,
					 create, &info, hint)))
	{
		goto error_free_object;
	}

	/* Assigning into to entity's info */
	object->info = &object->entity->info;
	
	/* @hint->object key is built by plugin create method. */
	name = reiser4_print_key(&object->info->object, PO_INO);
	
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
			    const char *name)
{
	errno_t res;
	entry_hint_t entry;
	
	aal_assert("umka-1945", child != NULL);

	/* Check if we need to add entry in parent @object */
	if (name && object) {
		aal_memset(&entry, 0, sizeof(entry));	

		aal_strncpy(entry.name, name, sizeof(entry.name));
		reiser4_key_assign(&entry.object, &child->info->object);

		if ((res = reiser4_object_add_entry(object, &entry))) {
			aal_exception_error("Can't add entry %s to %s.",
					    name, object->name);
			return res;
		}
	}

	if ((res = plug_call(child->entity->plug->o.object_ops,
			     link, child->entity)))
	{
		return res;
	}
	
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
			      const char *name)
{
	errno_t res = 0;
	entry_hint_t entry;
	reiser4_place_t place;
	reiser4_object_t *child;
	
	aal_assert("umka-1910", object != NULL);
	aal_assert("umka-1911", name != NULL);

	/* Getting child statdata key */
	if (reiser4_object_lookup(object, name, &entry) != PRESENT) {
		aal_exception_error("Can't find entry %s in %s.",
				    name, object->name);
		return -EINVAL;
	}

	/* Removing entry */
	if ((res = reiser4_object_rem_entry(object, &entry))) {
		aal_exception_error("Can't remove entry %s in %s.",
				    name, object->name);
		return res;
	}

	/* Looking up for the victim statdata place */
	if (reiser4_tree_lookup(object->info->tree, &entry.object,
				LEAF_LEVEL, &place) != PRESENT)
	{
		aal_exception_error("Can't find an item pointed by %s. "
				    "Entry %s/%s points to nowere.",
				    reiser4_print_key(&entry.object, PO_DEF),
				    object->name, name);
		return -EINVAL;
	}

	/* Opening victim object by found place */
	if (!(child = reiser4_object_realize(object->tree, object, &place))) {
		aal_exception_error("Can't open %s/%s.", object->name, name);
		return -EINVAL;
	}

	if ((res = plug_call(child->entity->plug->o.object_ops,
			     unlink, child->entity)))
	{
		return res;
	}
	
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

/* Helper function for printing passed @place into @stream */
static errno_t callback_print_place(
	void *entity,              /* object to be printed */
	place_t *place,            /* next object block */
	void *data)                /* user-specified data */
{
	errno_t res;
	
	aal_stream_t *stream = (aal_stream_t *)data;
	reiser4_place_t *p = (reiser4_place_t *)place;
	
	if ((res = reiser4_item_print(p, stream))) {
		aal_exception_error("Can't print item %u in node %llu.",
				    p->pos.item, node_blocknr(p->node));
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
		return FAILED;
	
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
int32_t reiser4_object_read(
	reiser4_object_t *object,   /* object entry will be read from */
	void *buff,		    /* buffer result will be stored in */
	uint32_t n)                 /* buffer size */
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

/* Creates directory */
reiser4_object_t *reiser4_dir_create(reiser4_fs_t *fs,
				     reiser4_object_t *parent,
				     const char *name)
{
	rid_t pid;
	object_hint_t hint;
	reiser4_object_t *object;
	
	aal_assert("vpf-1053", fs != NULL);
	
	pid = reiser4_profile_value("directory");
	
	/* Preparing object hint */
	hint.plug = reiser4_factory_ifind(OBJECT_PLUG_TYPE, pid);

	if (!hint.plug) {
		aal_exception_error("Can't find dir plugin by its id "
				    "0x%x.", pid);
		return NULL;
	}
    
	hint.statdata = reiser4_profile_value("statdata");
	hint.body.dir.hash = reiser4_profile_value("hash");
	hint.body.dir.direntry = reiser4_profile_value("cde");

	/* Creating object by passed parameters */
	if (!(object = reiser4_object_create(fs->tree, parent, &hint)))
		return NULL;

	if (parent) {
		if (reiser4_object_link(parent, object, name))
			return NULL;
	}

	return object;
}

/* Creates file */
reiser4_object_t *reiser4_reg_create(reiser4_fs_t *fs,
				     reiser4_object_t *parent,
				     const char *name)
{
	rid_t regular;
	object_hint_t hint;
	reiser4_object_t *object;
	
	aal_assert("vpf-1054", fs != NULL);
	
	regular = reiser4_profile_value("regular");
	
	/* Preparing object hint */
	hint.plug = reiser4_factory_ifind(OBJECT_PLUG_TYPE, regular);

	if (!hint.plug) {
		aal_exception_error("Can't find dir plugin by its id "
				    "0x%x.", regular);
		return NULL;
	}
	
	hint.statdata = reiser4_profile_value("statdata");
	hint.body.reg.tail = reiser4_profile_value("tail");
	hint.body.reg.extent = reiser4_profile_value("extent");
	hint.body.reg.policy = reiser4_profile_value("policy");
	
	/* Creating object by passed parameters */
	if (!(object = reiser4_object_create(fs->tree, parent, &hint)))
		return NULL;

	if (parent) {
		if (reiser4_object_link(parent, object, name))
			return NULL;
	}

	return object;
}

/* Creates symlink */
reiser4_object_t *reiser4_sym_create(reiser4_fs_t *fs,
				     reiser4_object_t *parent,
		                     const char *name,
		                     const char *target)
{
	rid_t symlink;
	object_hint_t hint;
	reiser4_object_t *object;
	
	aal_assert("vpf-1186", fs != NULL);
	aal_assert("vpf-1057", target != NULL);
	
	symlink = reiser4_profile_value("symlink");
	
	/* Preparing object hint */
	hint.plug = reiser4_factory_ifind(OBJECT_PLUG_TYPE, symlink);

	if (!hint.plug) {
		aal_exception_error("Can't find dir plugin by its id "
				    "0x%x.", symlink);
		return NULL;
	}
	
	hint.body.sym = (char *)target;
	hint.statdata = reiser4_profile_value("statdata");
	
	/* Creating object by passed parameters */	
	if (!(object = reiser4_object_create(fs->tree, parent, &hint)))
		return NULL;

	if (parent) {
		if (reiser4_object_link(parent, object, name))
			return NULL;
	}
	
	return object;
}

errno_t reiser4_object_traverse(reiser4_object_t *object,
				object_open_func_t open_func,
				void *data)
{
	entry_hint_t entry;
	errno_t res = 0;
	
	aal_assert("vpf-1090", object != NULL);
	aal_assert("vpf-1103", open_func != NULL);
	
	while (!reiser4_object_readdir(object, &entry)) {
		reiser4_object_t *child = NULL;
		
		if ((child = open_func(object, &entry, data)) == INVAL_PTR)
			return -EINVAL;
		
		if (child == NULL)
			continue;

		res = reiser4_object_traverse(child, open_func, data);
		
		reiser4_object_close(child);
		
		if (res)
			return res;
	}
	
	return 0;
}
#endif
