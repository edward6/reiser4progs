/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   object.c -- common code for all reiser4 objects (regular files, directories,
   symlinks, etc). */

#include <aux/aux.h>
#include <reiser4/libreiser4.h>

/* Init object pset by its SD. */
static errno_t reiser4_object_init(reiser4_object_t *object) {
	reiser4_place_t *start;
	sdhint_plug_t plugh;
	trans_hint_t trans;
	stat_hint_t stat;
	errno_t res;
	
	aal_assert("umka-2380", object != NULL);

	aal_memset(&stat, 0, sizeof(stat));
	aal_memset(&plugh, 0, sizeof(plugh));
	
	/* Preparing hint and mask */
	trans.specific = &stat;
	trans.place_func = NULL;
	trans.region_func = NULL;
	trans.shift_flags = SF_DEFAULT;
	stat.ext[SDEXT_PLUG_ID] = &plugh;
	
	start = &object->info.start;
	
	if (reiser4_place_valid(start)) {
		if ((res = reiser4_place_fetch(start)))
			return res;

		/* Init is allowed on a SD item only. */
		if (start->plug->p.id.group != STAT_ITEM)
			return -EINVAL;
	} else {
		/* Init is allowed on a SD item only. */
		return -EINVAL;
	}
	
	/* Getting object plugin by first item coord. */
	if ((res = objcall(start, object->fetch_units, &trans)) != 1)
		return res;
	
	aal_memcpy(&object->info.opset, &plugh, sizeof(plugh));
	
	reiser4_opset_complete((reiser4_tree_t *)object->info.tree, 
			       &object->info.opset);
	
	/* Object plugin must be detected. */
	return reiser4_psobj(object) ? 0 : -EINVAL;
}

/* Tries to open object at @place. Uses @init_func for initializing object
   entity. It is needed, because libreiser4 itself uses one style of object
   entity initializing and librepair another one, but both they use some amount
   of common code, which was moved to this function and used by both in such a
   manner. */
reiser4_object_t *reiser4_object_prep(reiser4_tree_t *tree,
				      reiser4_object_t *parent,
				      reiser4_key_t *okey,
				      reiser4_place_t *place)
{
	reiser4_object_t *object;
	
	aal_assert("umka-1508", tree != NULL);
	aal_assert("umka-1509", place != NULL);
	aal_assert("vpf-1409",  okey != NULL);

	if (!(object = aal_calloc(sizeof(*object), 0)))
		return NULL;

	/* Initializing object info. */
	object->info.tree = (tree_entity_t *)tree;

	/* Putting object key to info struct. We may want to open and fix 
	   the object even if @place->key does not match @okey. */
	aal_memcpy(&object->info.object, okey, sizeof(*okey));

	/* Copying item coord. */
	aal_memcpy(&object->info.start, place, sizeof(*place));

	/* Parent is not passed. Using object's key as parent's one. */
	if (parent) {
		aal_memcpy(&object->info.parent, 
			   &parent->info.object, 
			   sizeof(object->info.parent));
	}

	/* Try to init on the StatData. */
	if (reiser4_object_init(object)) {
		aal_free(object);
		return NULL;
	}
	
	return object;
}

/* This function opens object by its @place. */
reiser4_object_t *reiser4_object_open(reiser4_tree_t *tree, 
				      reiser4_object_t *parent,
				      reiser4_place_t *place)
{
	reiser4_object_t *object;
	
	aal_assert("vpf-1223", place != NULL);

	if (!(object = reiser4_object_prep(tree, parent, 
					   &place->key, place)))
	{
		return NULL;
	}
	
	if (plugcall(reiser4_psobj(object), open, object)) {
		aal_free(object);
		return NULL;
	}

	return object;
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
#ifndef ENABLE_MINIMAL
	hint.collision = NULL;
#endif

	if (reiser4_tree_lookup(tree, &hint, FIND_EXACT, &place) != PRESENT)
		return NULL;

	/* The key must point to the start of the object. */
	if (place.pos.unit != 0 && place.pos.unit != MAX_UINT32)
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
	aal_memset(&hint, 0, sizeof(hint));

	/* FIXME-UMKA: Why object (on API abstraction level) knows, that size
	   lies in LW extension? What if someone will move it to another one? */
	hint.extmask = 1 << SDEXT_LW_ID;
	hint.ext[SDEXT_LW_ID] = &lwh;

	/* Calling objects stat() method. */
	if (plugcall(reiser4_psobj(object), stat, object, &hint))
		return 0;
	
	return lwh.size;
}

/* Closes specified object */
void reiser4_object_close(reiser4_object_t *object) {
	aal_assert("umka-680", object != NULL);

	if (reiser4_psobj(object)->close)
		reiser4_psobj(object)->close(object);
	
	aal_free(object);
}

#ifndef ENABLE_MINIMAL
/* Adds @entry to @object */
errno_t reiser4_object_add_entry(
	reiser4_object_t *object,           /* object for adding entry */
	entry_hint_t *entry)                /* entry hint to be added */
{
	aal_assert("umka-1975", object != NULL);

	if (!reiser4_psobj(object)->add_entry)
		return -EINVAL;
	
	return plugcall(reiser4_psobj(object), add_entry, object, entry);
}

/* Removes @entry to @object */
errno_t reiser4_object_rem_entry(
	reiser4_object_t *object,           /* object for removing */
	entry_hint_t *entry)                /* entry hint to be added */
{
	aal_assert("umka-1977", object != NULL);
    
	if (!reiser4_psobj(object)->rem_entry)
		return -EINVAL;
	
	return plugcall(reiser4_psobj(object), rem_entry, object, entry);
}

errno_t reiser4_object_truncate(
	reiser4_object_t *object,           /* object for truncating */
	uint64_t n)			    /* the number of entries */
{
	aal_assert("umka-1154", object != NULL);
    
	return plugcall(reiser4_psobj(object), truncate, object, n);
}

/* Adds speficied entry into passed opened dir */
int64_t reiser4_object_write(
	reiser4_object_t *object,           /* object for writing */
	void *buff,			    /* new entries buffer */
	uint64_t n)			    /* the number of entries to 
					       be created */
{
	aal_assert("umka-862", object != NULL);
    
	if (!reiser4_psobj(object)->write)
		return -EINVAL;
	
	return plugcall(reiser4_psobj(object), write, object, buff, n);
}

/* Updates object stat data coord by means of using tree_lookup(). */
errno_t reiser4_object_refresh(reiser4_object_t *object) {
	reiser4_tree_t *tree;
	lookup_hint_t hint;

	hint.level = LEAF_LEVEL;
	hint.key = &object->info.object;

	hint.collision = NULL;

	tree = (reiser4_tree_t *)object->info.tree;
	
	switch (reiser4_tree_lookup(tree, &hint, FIND_EXACT, 
				    object_start(object)))
	{
	case PRESENT:
		return 0;
	default:
		return -EINVAL;
	}
}

errno_t reiser4_object_entry_prep(reiser4_tree_t *tree,
				  reiser4_object_t *parent,
				  entry_hint_t *entry,
				  const char *name)
{
	aal_assert("vpf-1717", tree != NULL);
	aal_assert("vpf-1718", entry != NULL);

	aal_memset(entry, 0, sizeof(*entry));
	
	if (name) {
		aal_strncpy(entry->name, name, sizeof(entry->name));
	}

	/* Preparing @entry to be used for object creating and linking to parent
	   object. This is name and offset key. */
	if (parent) {
		if (!reiser4_psobj(parent)->build_entry) {
			aal_error("Object %s has not build_entry() method "
				  "implemented. Is it dir object at all?", 
				  reiser4_print_inode(&parent->info.object));
			return -EINVAL;
		}
		
		plugcall(reiser4_psobj(parent), build_entry, parent, entry);
	} else {
		aal_memcpy(&entry->offset, &tree->key, sizeof(tree->key));
	}
	
	return 0;
}

/* Helper function for prepare object key to be used for creating new object. */
static void reiser4_object_maintain(reiser4_object_t *object, 
				    entry_hint_t *entry) 
{
	reiser4_tree_t *tree;
	oid_t locality;
	oid_t ordering;
	oid_t objectid;
	
	aal_assert("vpf-1715", entry != NULL);
	aal_assert("vpf-1714", object != NULL);
	aal_assert("vpf-1822", object->info.tree != NULL);

	/* Initializing fields and preparing the keys */
	tree = (reiser4_tree_t *)object->info.tree;

	if (object->info.parent.plug) {
		/* Parent if defined, getting locality from it. */
		objectid = reiser4_oid_allocate(tree->fs->oid);
		locality = reiser4_key_get_objectid(&object->info.parent);
	} else {
		/* Parent is not defined, root key is used. */
		aal_memcpy(&object->info.parent, &tree->key, sizeof(tree->key));
		locality = reiser4_key_get_locality(&tree->key);
		objectid = reiser4_key_get_objectid(&tree->key);
		reiser4_oid_set_used(tree->fs->oid, 
			reiser4_oid_get_used(tree->fs->oid) + 1);
	}
	
	/* New object is identified by its locality and objectid. Set them to
	   the @object->info.object key and plugin create method will build the
	   whole key there. */
	object->info.object.plug = tree->key.plug;

	/* Ordering component of key to be used for object. */
	ordering = reiser4_key_get_ordering(&entry->offset);

	/* Building object stat data key. */
	reiser4_key_build_generic(&object->info.object, KEY_STATDATA_TYPE,
				  locality, ordering, objectid, 0);
}

errno_t reiser4_object_attach(reiser4_object_t *object, 
			      reiser4_object_t *parent) 
{
	errno_t res;

	aal_assert("vpf-1720", object != NULL);
	
	if (!reiser4_psobj(object)->attach) 
		return 0;
	
	if ((res = plugcall(reiser4_psobj(object), attach, 
			    object, parent ? parent : NULL)))
	{
		aal_error("Can't attach %s to %s.",
			  reiser4_print_inode(&object->info.object),
			  parent == NULL ? "itself" :
			  reiser4_print_inode(&parent->info.object));
	}

	return res;
}

/* Detach the object from the parent. If parent is NULL, there is no checks 
   it detaching is allowed or not. */
errno_t reiser4_object_detach(reiser4_object_t *object, 
			      reiser4_object_t *parent) 
{
	errno_t res;

	aal_assert("vpf-1721", object != NULL);

	if (!reiser4_psobj(object)->detach) 
		return 0;
	
	if ((res = plugcall(reiser4_psobj(object), detach,
			    object, parent ? parent : NULL))) 
	{
		aal_error("Can't detach %s from %s.",
			  reiser4_print_inode(&object->info.object),
			  parent == NULL ? "itself" :
			  reiser4_print_inode(&parent->info.object));
	}

	return res;
}

/* Creates new object on specified filesystem */
reiser4_object_t *reiser4_object_create(
	entry_hint_t *entry,			/* entry hint object to be used */
	object_info_t *info,			/* object init info */
	object_hint_t *hint)			/* object hint */
{
	reiser4_object_t *object;
	
	aal_assert("umka-1917", info != NULL);

	/* Allocating the memory for object instance */
	if (!(object = aal_calloc(sizeof(*object), 0)))
		return NULL;

	aal_memcpy(&object->info, info, sizeof (*info));
	
	/* Preparing object info. */
	reiser4_object_maintain(object, entry);
	
	aal_assert("vpf-1823", reiser4_psobj(object) != NULL);
	
	/* Calling object plugin to create its body in the tree */
	if (plugcall(reiser4_psobj(object), create, object, hint)) {
		aal_free(object);
		return NULL;
	}

	return object;
}

/* Removes object body and stat data */
errno_t reiser4_object_clobber(reiser4_object_t *object) {
	aal_assert("umka-2297", object != NULL);

	return plugcall(reiser4_psobj(object), clobber, object);
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
		aal_memcpy(&entry->object, 
			   &child->info.object, 
			   sizeof(entry->offset));

		if ((res = reiser4_object_add_entry(object, entry))) {
			aal_error("Can't add entry %s to %s.", entry->name, 
				  reiser4_print_inode(&object->info.object));
			return res;
		}
	}

	/* Add one hard link to @child. */
	if (reiser4_psobj(child)->link) {
		res = plugcall(reiser4_psobj(child), link, child);
		
		if (res) {
			aal_error("Can't link the object %s. ",
				  reiser4_print_inode(&child->info.object));
			
			goto error_rem_entry;
		}
	}

	/* Attach @child to @parent. */
	if ((res = reiser4_object_attach(child, object)))
		goto error_unlink_child;
	    
	return 0;

 error_unlink_child:
	if (reiser4_psobj(child)->unlink) {
		if (plugcall(reiser4_psobj(child), unlink, child)) {
			aal_error("Can't unlink the object %s.",
				  reiser4_print_inode(&child->info.object));
		}
	}
	
 error_rem_entry:
	if (reiser4_object_rem_entry(object, entry)) {
		aal_error("Can't remove entry %s in %s.", entry->name,
			  reiser4_print_inode(&object->info.object));
	}
	
	return res;
}

/* Removes entry from the @object if it is a directory */
errno_t reiser4_object_unlink(reiser4_object_t *object, char *name) {
	reiser4_object_t *child;
	reiser4_place_t place;
	reiser4_tree_t *tree;
	lookup_hint_t hint;
	entry_hint_t entry;
	errno_t res = 0;
	
	aal_assert("umka-1910", object != NULL);

	/* Getting entry poining to the child. */
	if (reiser4_object_lookup(object, name, &entry) != PRESENT) {
		aal_error("Can't find entry %s in %s.", name, 
			  reiser4_print_inode(&object->info.object));
		return -EINVAL;
	}

	if (entry.type == ET_SPCL) {
		aal_error("Can't unlink the special link '%s'.", name);
		return -EINVAL;
	}
	
	hint.level = LEAF_LEVEL;
	hint.key = &entry.object;
	hint.collision = NULL;
	
	tree = (reiser4_tree_t *)object->info.tree;
	
	/* Looking up for the victim's statdata place */
	if (reiser4_tree_lookup(tree, &hint, FIND_EXACT, &place) != PRESENT) {
		char *key = reiser4_print_key(&entry.object);
		aal_error("Can't find an item pointed by %s. "
			  "Entry %s/%s points to nowere.", key, 
			  reiser4_print_inode(&object->info.object), 
			  name);
		return -EINVAL;
	}

	/* Opening victim object by found place */
	if (!(child = reiser4_object_open(tree, object, &place))) {
		aal_error("Can't open %s/%s. Object is corrupted?",
			  reiser4_print_inode(&object->info.object), 
			  name);
		return -EINVAL;
	}

	/* Detach @child from parent. */
	if ((res = reiser4_object_detach(child, object)))
		return res;

	/* Remove one hard link from child. */
	if (reiser4_psobj(child)->unlink) {
		if ((res = plugcall(reiser4_psobj(child), unlink, child)))
			goto error_attach_child;
	}

	/* Removing entry from @object. */
	if ((res = reiser4_object_rem_entry(object, &entry))) {
		aal_error("Can't remove entry %s in %s.", name, 
			  reiser4_print_inode(&object->info.object));
		
		goto error_link_child;
	}

	reiser4_object_close(child);
	return 0;

 error_link_child:
	if (reiser4_psobj(child)->link) {
		if (plugcall(reiser4_psobj(child), link, child)) {
			aal_error("Can't link the object %s.",
				  reiser4_print_inode(&child->info.object));
		}
	}
 error_attach_child:
	reiser4_object_attach(child, object);
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

	if (!reiser4_psobj(object)->layout)
		return 0;
	
	return plugcall(reiser4_psobj(object), layout, 
			object, region_func, data);
}

/* Enumerates all items object consists of */
errno_t reiser4_object_metadata(
	reiser4_object_t *object,   /* object we working with */
	place_func_t place_func,    /* metadata layout callback */
	void *data)                 /* user-spaecified data */
{
	aal_assert("umka-1714", object != NULL);
	aal_assert("umka-1715", place_func != NULL);

	if (!reiser4_psobj(object)->metadata)
		return 0;
	
	return plugcall(reiser4_psobj(object), metadata, 
			object, place_func, data);
}

/* Makes lookup inside the @object */
lookup_t reiser4_object_lookup(reiser4_object_t *object,
			       const char *name, entry_hint_t *entry)
{
	aal_assert("umka-1919", object != NULL);
	aal_assert("umka-1920", name != NULL);

	if (!reiser4_psobj(object)->lookup)
		return -EINVAL;
	
	return plugcall(reiser4_psobj(object), lookup, 
			object, (char *)name, (void *)entry);
}

/* Loads object stat data to @hint. */
errno_t reiser4_object_stat(reiser4_object_t *object, stat_hint_t *hint) {
	aal_assert("umka-2570", object != NULL);

	return plugcall(reiser4_psobj(object), stat, object, hint);
}

/* Resets directory position */
errno_t reiser4_object_reset(
	reiser4_object_t *object)    /* dir to be reset */
{
	aal_assert("umka-842", object != NULL);
	aal_assert("umka-843", object != NULL);

	return plugcall(reiser4_psobj(object), reset, object);
}

/* Sets directory current position to passed pos */
errno_t reiser4_object_seek(
	reiser4_object_t *object,    /* object position to be changed in */
	uint32_t offset)	     /* offset for seeking */
{
	aal_assert("umka-1129", object != NULL);
    
	if (!reiser4_psobj(object)->seek)
		return -EINVAL;
	
	return plugcall(reiser4_psobj(object), seek, object, offset);
}

/* Change current position in passed @object if it is a directory */
errno_t reiser4_object_seekdir(reiser4_object_t *object,
			       reiser4_key_t *offset)
{
	aal_assert("umka-1979", object != NULL);
	aal_assert("umka-1980", offset != NULL);

	if (!reiser4_psobj(object)->seekdir)
		return -EINVAL;

	return plugcall(reiser4_psobj(object), seekdir, object, offset);
}

/* Returns current position in directory */
uint32_t reiser4_object_offset(
	reiser4_object_t *object)    /* dir position will be obtained from */
{
	aal_assert("umka-875", object != NULL);

	return plugcall(reiser4_psobj(object), offset, object);
}

/* Return current position in passed @object if it is a directory */
errno_t reiser4_object_telldir(reiser4_object_t *object,
			       reiser4_key_t *offset)
{
	aal_assert("umka-1981", object != NULL);
	aal_assert("umka-1982", offset != NULL);

	if (!reiser4_psobj(object)->telldir)
		return -EINVAL;

	return plugcall(reiser4_psobj(object), telldir, object, offset);
}

/* Reads @n bytes of data at the current offset of @object to passed
   @buff. Returns numbers bytes read. */
int64_t reiser4_object_read(
	reiser4_object_t *object,   /* object entry will be read from */
	void *buff,		    /* buffer result will be stored in */
	uint64_t n)                 /* buffer size */
{
	aal_assert("umka-860", object != NULL);

	if (!reiser4_psobj(object)->read)
		return -EINVAL;
	
	return plugcall(reiser4_psobj(object), read, object, buff, n);
}

/* Reads entry at current @object offset to passed @entry hint */
errno_t reiser4_object_readdir(reiser4_object_t *object,
			       entry_hint_t *entry)
{
	aal_assert("umka-1973", object != NULL);
	aal_assert("umka-1974", entry != NULL);

	if (!reiser4_psobj(object)->readdir)
		return -EINVAL;
	
	return plugcall(reiser4_psobj(object), readdir, object, entry);
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
	if (!reiser4_psobj(object)->readdir)
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

/* Completes object creating. */
static reiser4_object_t *reiser4_obj_create(reiser4_object_t *parent,
					    object_info_t *info,
					    object_hint_t *hint,
					    const char *name)
{
	reiser4_object_t *object;
	entry_hint_t entry;
	
	aal_assert("vpf-1849", parent != NULL);
	aal_assert("vpf-1849", info != NULL);
	aal_assert("vpf-1849", info->opset.plug[OPSET_OBJ] != NULL);
	
	/* Preparing object hint */
	/* Inherit from the parent. */
	if (plugcall((reiser4_object_plug_t *)info->opset.plug[OPSET_OBJ], 
		     inherit, info, &parent->info))
	{
		return NULL;
	}
		
	aal_memcpy(&info->parent, &parent->info.object, 
		   sizeof(info->parent));
	
	info->tree = parent->info.tree;
	
	if (reiser4_object_entry_prep((reiser4_tree_t *)info->tree, 
				      parent, &entry, name))
	{
		return NULL;
	}
	
	entry.place_func = NULL;
	
	/* Creating object by passed parameters. */
	if (!(object = reiser4_object_create(&entry, info, hint)))
		return NULL;

	if (parent) {
		if (reiser4_object_link(parent, object, &entry)) {
			reiser4_object_clobber(object);
			reiser4_object_close(object);
			return NULL;
		}
	}

	return object;
}

/* Creates directory. Uses params preset for all plugin. */
reiser4_object_t *reiser4_dir_create(reiser4_object_t *parent, 
				     const char *name)
{
	object_info_t info;
	
	aal_assert("vpf-1053", parent != NULL);
	
	aal_memset(&info, 0, sizeof(info));
	info.opset.plug[OPSET_OBJ] = parent->info.tree->tpset[TPSET_DIRFILE];
	info.opset.plug_mask |= (1 << OPSET_OBJ);

	return reiser4_obj_create(parent, &info, NULL, name);
}

/* Creates regular file, using all plugins it need from profile. Links new
   created file to @parent with @name. */
reiser4_object_t *reiser4_reg_create(reiser4_object_t *parent,
				     const char *name)
{
	object_info_t info;
	
	aal_assert("vpf-1054", parent != NULL);
	
	aal_memset(&info, 0, sizeof(info));
	info.opset.plug_mask |= (1 << OPSET_OBJ);
	info.opset.plug[OPSET_OBJ] = 
		reiser4_factory_ifind(OBJECT_PLUG_TYPE, 
				      reiser4_pscreate(parent)->objid);
	
	return reiser4_obj_create(parent, &info, NULL, name);
}

/* Creates symlink. Uses params preset for all plugin. */
reiser4_object_t *reiser4_sym_create(reiser4_object_t *parent,
		                     const char *name,
		                     const char *target)
{
	object_hint_t hint;
	object_info_t info;
	
	aal_assert("vpf-1186", parent != NULL);
	aal_assert("vpf-1057", target != NULL);
	
	aal_memset(&hint, 0, sizeof(hint));
	hint.str = (char *)target;
	
	aal_memset(&info, 0, sizeof(info));
	info.opset.plug[OPSET_OBJ] = parent->info.tree->tpset[TPSET_SYMFILE];
	info.opset.plug_mask |= (1 << OPSET_OBJ);

	return reiser4_obj_create(parent, &info, &hint, name);
}

/* Creates special file. Uses params preset for all plugin. */
reiser4_object_t *reiser4_spl_create(reiser4_object_t *parent,
		                     const char *name,
				     uint32_t mode,
		                     uint64_t rdev)
{
	object_hint_t hint;
	object_info_t info;
	
	aal_assert("umka-2534", parent != NULL);
	aal_assert("umka-2535", rdev != 0);
	
	aal_memset(&hint, 0, sizeof(hint));
	hint.mode = mode;
	hint.rdev = rdev;
	
	aal_memset(&info, 0, sizeof(info));
	info.opset.plug[OPSET_OBJ] = parent->info.tree->tpset[TPSET_SPLFILE];
	info.opset.plug_mask |= (1 << OPSET_OBJ);
	
	return reiser4_obj_create(parent, &info, &hint, name);
}

/* Creates special file. Uses params preset for all plugin. */
reiser4_object_t *reiser4_ccreg_create(reiser4_object_t *parent,
				       const char *name,
				       const char *key)
{
	object_hint_t hint;
	object_info_t info;
	
	aal_assert("umka-2534", parent != NULL);
	
	aal_memset(&hint, 0, sizeof(hint));
	hint.str = (char *)key;
	
	aal_memset(&info, 0, sizeof(info));
	info.opset.plug_mask |= (1 << OPSET_OBJ);
	
	if (!(info.opset.plug[OPSET_OBJ] = 
	      reiser4_factory_ifind(OBJECT_PLUG_TYPE, OBJECT_CCREG40_ID))) 
	{
		aal_error("Can't find the CRC object plugin\n.");
		return NULL;
	}

	return reiser4_obj_create(parent, &info, &hint, name);
}
#endif
