/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   semantic.c -- reiser4 semantic tree related code. */

#include <aux/aux.h>
#include <reiser4/libreiser4.h>

struct resolve {
	bool_t follow;
	object_info_t info;
	object_entity_t *ent;
};

typedef struct resolve resolve_t;

/* Callback function for finding statdata of the current directory */
static errno_t callback_find_statdata(char *track, char *entry,
				      void *data)
{
	errno_t res;
	resolve_t *resol;

	lookup_hint_t hint;
	reiser4_tree_t *tree;
	reiser4_plug_t *plug;
	reiser4_place_t *place;

	resol = (resolve_t *)data;
	place = &resol->info.start;

#ifndef ENABLE_STAND_ALONE
	hint.collision = NULL;
#endif
	
	hint.level = LEAF_LEVEL;
	hint.key = &resol->info.object;
	tree = (reiser4_tree_t *)resol->info.tree;

	/* Looking for current @resolve->object in order to the coord of stat
	   data of find object pointed by @resolve->object. It is needed for
	   consequent handling. */
	if ((res = reiser4_tree_lookup(tree, &hint, FIND_EXACT, place)) < 0) {
		return res;
	} else {
		/* Key is not found. */
		if (res != PRESENT)
			return -EINVAL;
	}

	/* Trying to recognize the object at @place. */
	if (!(resol->ent = reiser4_object_recognize(&resol->info)) || 
	    (resol->ent == INVAL_PTR))
	{
		aal_error("Can't open object %s.", track);
		return -EINVAL;
	}

#ifdef ENABLE_SYMLINKS
	/* Symlinks handling. Method follow() should be implemented if object
	   wants to be resolved (symlink). */
	if (resol->follow && plug->o.object_ops->follow) {

		/* Updating parent key. */
		reiser4_key_assign(&resol->info.parent, &resol->info.object);
	
		/* Calling object's follow() in order to get stat data key of
		   the object that current object points to. */
		res = plug_call(plug->o.object_ops, follow, resol->ent,
				&resol->info.parent, &resol->info.object);

	        /* Close current object. */
		plug_call(plug->o.object_ops, close, resol->ent);

		/* Symlink cannot be followed. */
		if (res != 0) {
			aal_error("Can't follow %s.", track);
			return res;
		}
		
		/* Getting stat data place by key returned from follow() */
		if ((res = reiser4_tree_lookup(tree, &hint, FIND_EXACT,
					       place)) < 0)
		{
			return res;
		} else {
			/* Key is not found. Symlink points to unexistent
			   object. */
			if (res != PRESENT)
				return -EINVAL;
		}

		/* Initializing the object entity @resol->info.start points
		   to. */
		if (!(resol->ent = plug_call(plug->o.object_ops,
					     open, &resol->info)))
		{
			aal_error("Can't open object %s.", track);
			return -EINVAL;
		}
	}
#endif

	return 0;
}

/* Callback function to find @name inside the current object. */
static errno_t callback_find_entry(char *track, char *name,
				   void *data)
{
	lookup_t res;
	resolve_t *resol;
	entry_hint_t entry;

	resol = (resolve_t *)data;

	/* Looking up for @entry in current directory */
	if ((res = plug_call(resol->ent->opset[OPSET_OBJ]->o.object_ops,
			     lookup, resol->ent, name, &entry)) < 0)
	{
		return res;
	} else {
		if (res != PRESENT) {
			aal_error("Can't find %s.", track);
			
			plug_call(resol->ent->opset[OPSET_OBJ]->o.object_ops,
				  close, resol->ent);
			return -EINVAL;
		}
	}

	/* Updating parent key. */
	reiser4_key_assign(&resol->info.parent,
			   &resol->info.object);
	
	/* Save found key. */
	reiser4_key_assign(&resol->info.object,
			   &entry.object);

	/* Close current object. */
	plug_call(resol->ent->opset[OPSET_OBJ]->o.object_ops,
		  close, resol->ent);
	
	return 0;
}

/* Resolves @path and stores key of stat data into @sdkey */
object_entity_t *reiser4_semantic_resolve(reiser4_tree_t *tree, char *path,
					  reiser4_key_t *from, bool_t follow)
{
	errno_t res;
	resolve_t resol;
	
	aal_assert("umka-2574", path != NULL);
	aal_assert("umka-2578", from != NULL);

	resol.follow = follow;
	resol.info.tree = (tree_entity_t *)tree;

#ifdef ENABLE_SYMLINKS
	/* Initializing parent key to root key */
	reiser4_key_assign(&resol.info.parent, &tree->key);
#endif

	/* Resolving path will be performted starting from key @from. */
	reiser4_key_assign(&resol.info.object, from);

	/* Parsing path and looking for object's stat data. We assume, that name
	   is absolute one. So, user, who calls this method should convert name
	   previously into absolute one by means of using getcwd() function. */
	if ((res = aux_parse_path(path, callback_find_statdata,
				  callback_find_entry, &resol)))
	{
		return NULL;
	}

	return resol.ent;
}

/* This function opens object by its name */
reiser4_object_t *reiser4_semantic_open(
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
    
	/* Semantic resolve of @path. */
	if (!(object->ent = reiser4_semantic_resolve(tree, path,
						     &tree->key,
						     follow)))
	{
		goto error_free_object;
	}
	
	/* Initializing object name. It is stat data key as string. */
#ifndef ENABLE_STAND_ALONE
	name = reiser4_print_key(&object->ent->object, PO_INODE);
	aal_strncpy(object->name, name, sizeof(object->name));
#endif

	return object;
    
 error_free_object:
	aal_free(object);
	return NULL;
}
