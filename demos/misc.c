/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   misc.c -- miscellaneous useful stuff. */

#include "busy.h"

reiser4_object_t *busy_misc_open_parent(reiser4_tree_t *tree, 
					char **path)
{
	reiser4_object_t *object;
	char *sep;
	
	if (!(sep = aal_strrchr(*path, '/'))) {
		aal_error("Wrong PATH format is detected: %s.", *path);
		return INVAL_PTR;
	}

	if (sep == *path) {
		/* Create file in the root. */
		object = reiser4_semantic_open(tree, "/", NULL, 1, 1);
	} else {
		sep[0] = 0;
		object = reiser4_semantic_open(tree, *path, NULL, 1, 1);
		sep[0] = '/';
	}
	
	if (!object) {
		aal_error("Can't open file %s.", *path);
		sep[0] = '/';
		return NULL;
	}
	
	sep[0] = '/';
	*path = sep + 1;

	return object;
}
