/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   semantic.c -- reiser4 semantic tree related code. */

#include <aux/aux.h>
#include <reiser4/libreiser4.h>

typedef struct resolve {
	bool_t follow;
	bool_t present;
	reiser4_object_t *parent;
	reiser4_object_t *object;
	reiser4_tree_t *tree;
	reiser4_key_t key;
} resolve_t;

/* Callback function for finding statdata of the current directory */
static errno_t cb_find_statdata(char *path, char *entry, void *data) {
#ifdef ENABLE_SYMLINKS
	reiser4_object_plug_t *plug;
#endif
	resolve_t *resol;
	
	resol = (resolve_t *)data;


	if (!(resol->object = reiser4_object_obtain(resol->tree, 
						    resol->parent,
						    &resol->key)))
	{
		aal_error("Can't open object %s given in %s.", entry, path);
		return -EINVAL;
	}
	
#ifdef ENABLE_SYMLINKS
	plug = reiser4_oplug(resol->object);
	
	/* Symlinks handling. Method follow() should be implemented if object
	   wants to be resolved (symlink). */
	if (resol->follow && plug->follow) {
		errno_t res;

		/* Calling object's follow() in order to get stat data key of
		   the object that current object points to. */
		res = plugcall(plug, follow, resol->object,
			       (resol->parent ? &resol->parent->info.object :
				&resol->tree->key), &resol->key);

	        /* Close current object. */
		reiser4_object_close(resol->object);

		/* Symlink cannot be followed. */
		if (res != 0) {
			aal_error("Can't follow %s in %s.", entry, path);
			return res;
		}
		
		if (!(resol->object = reiser4_object_obtain(resol->tree,
							    resol->parent,
							    &resol->key)))
		{
			aal_error("Can't open object %s in %s.", entry, path);
			return -EINVAL;
		}
	}
#endif

	return 0;
}

/* Callback function to find @name inside the current object. */
static errno_t cb_find_entry(char *path, char *name, void *data) {
	entry_hint_t entry;
	resolve_t *resol;
	lookup_t res;

	resol = (resolve_t *)data;

	if (name == NULL) {
		/* Start from the root. */
		aal_memcpy(&resol->key, &resol->tree->key, sizeof(resol->key));

		return 0;
	}
	
	/* Looking up for @entry in current directory */
	if ((res = plugcall(reiser4_oplug(resol->object), lookup, 
			     resol->object, name, &entry)) < 0)
	{
		return res;
	}
	
	if (res != PRESENT) {
		if (resol->present) {
			/* The object by @path must present. */
			aal_error("Can't find %s in %s.", name, path);
		}
		return -EINVAL;
	}

	/* Close the parent object. */
	if (resol->parent) {
		reiser4_object_close(resol->parent);
		resol->parent = NULL;
	}
	
	/* Updating parent key. */
	resol->parent = resol->object;
	
	/* Save found key. */
	aal_memcpy(&resol->key, &entry.object, sizeof(resol->key));
	
	return 0;
}

/* This function opens object by its name */
static reiser4_object_t *reiser4_semantic_open_object(
	reiser4_tree_t *tree,		/* tree object will be opened on */
	char *path,                     /* name of object to be opened */
	reiser4_key_t *from,		/* key to start  resolving from */
	bool_t follow,                  /* follow symlinks */
	bool_t present)			/* if object must present or not */
{
	resolve_t resol;
    
	aal_assert("umka-678", tree != NULL);
	aal_assert("umka-789", path != NULL);

	resol.present = present;
	resol.follow = follow;
	resol.tree = tree;
	resol.parent = resol.object = NULL;

	/* Initializing the key. */
	aal_memcpy(&resol.key, from ? from : &tree->key, sizeof(resol.key));
	
	/* Parsing path and looking for object's stat data. We assume, that name
	   is absolute one. So, user, who calls this method should convert name
	   previously into absolute one by means of using getcwd() function. */
	if (aux_parse_path(path, cb_find_statdata,
			   cb_find_entry, &resol))
		goto error_free_resol;

	if (resol.parent)
		reiser4_object_close(resol.parent);
	
	return resol.object;
	
 error_free_resol:
	if (resol.parent)
		reiser4_object_close(resol.parent);

	if (resol.object)
		reiser4_object_close(resol.object);

	return NULL;
}

/* This function opens object by its name */
reiser4_object_t *reiser4_semantic_try_open(
	reiser4_tree_t *tree,		/* tree object will be opened on */
	char *path,                     /* name of object to be opened */
	reiser4_key_t *from,		/* key to start  resolving from */
	bool_t follow)                  /* follow symlinks */
{
	return reiser4_semantic_open_object(tree, path, from, follow, 0);
}

/* This function opens object by its name */
reiser4_object_t *reiser4_semantic_open(
	reiser4_tree_t *tree,		/* tree object will be opened on */
	char *path,                     /* name of object to be opened */
	reiser4_key_t *from,		/* key to start  resolving from */
	bool_t follow)                  /* follow symlinks */
{
	return reiser4_semantic_open_object(tree, path, from, follow, 1);
}
