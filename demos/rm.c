/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   rm.c -- a demo program that works similar to the standard rm utility. */

#include "busy.h"

errno_t rm_cmd(busy_ctx_t *ctx) {
	reiser4_object_t *parent, *object;
	char *name;

	aal_assert("vpf-1712", ctx != NULL);

	if (!ctx->in.fs) {
		aal_error("Fs is not openned. Wrong PAth is specified: %s.",
			  ctx->in.path);
		return -EINVAL;
	}
	
	if (ctx->in.path[0] == 0) {
		aal_error("NULL path is given.");
		return -EINVAL;
	}
	
	name = ctx->in.path;
	if (!(parent = busy_misc_open_parent(ctx->in.fs->tree, &name)))
		return -EINVAL;

	if (!(object = reiser4_semantic_open(ctx->in.fs->tree, 
					     ctx->in.path, NULL, 1))) 
	{
		aal_error("Can't open file %s.", ctx->in.path);
		goto error_close_parent;
	}
	
	/* Unlink the object. */
	if (reiser4_object_unlink(parent, name)) {
		aal_error("Can't unlink %s.", ctx->in.path);
		goto error_close_object;
	}
	
	if (!plug_call(objplug(object)->o.object_ops, linked, object->ent)) {
		/* There are no other link to this object, destroy it. */
		if (reiser4_object_clobber(object)) {
			aal_error("Can't to erase the object %s.", 
				  ctx->in.path);
			goto error_close_object;
		}
	}
	
	reiser4_object_close(object);
	reiser4_object_close(parent);

	return 0;
	
 error_close_object:
	reiser4_object_close(object);
 error_close_parent:
	reiser4_object_close(parent);
	return -EIO;
}

