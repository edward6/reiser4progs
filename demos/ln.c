/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   ln.c -- a demo program that works similar to the standard ln utility. */

#include "busy.h"

errno_t ln_cmd(busy_ctx_t *ctx) {
	reiser4_object_t *parent, *object;
	reiser4_plug_t *plug;
	entry_hint_t entry;
	lookup_t res;
	char *name;

	aal_assert("vpf-1710", ctx != NULL);

	if (!ctx->in.fs) {
		aal_error("Fs is not openned. Wrong PAth is specified: %s.",
			  ctx->in.path);
		return -EINVAL;
	}
	
	if (ctx->in.path[0] == 0) {
		aal_error("NULL path is given.");
		return -EINVAL;
	}
	
	if (ctx->out.path[0] == 0) {
		aal_error("NULL link name is given.");
		return -EINVAL;
	}

	if (!(object = reiser4_semantic_open(ctx->in.fs->tree, 
					     ctx->in.path, NULL, 1)))
	{
		aal_error("Failed to open the file %s.", ctx->in.path);
		return -EINVAL;
	}
	
	name = ctx->out.path;
	
	parent = busy_misc_open_parent(ctx->in.fs->tree, &name);
	if (!parent || parent == INVAL_PTR)
		goto error_object_close;
	
	/* Looking up for @entry in current directory */
	plug = parent->ent->opset.plug[OPSET_OBJ];
	if ((res = plug_call(plug->o.object_ops, lookup, 
			     parent->ent, name, &entry)) < 0)
		goto error_close_parent;

	if (res == PRESENT) {
		aal_error("The file %s already exists.", ctx->out.path);
		goto error_close_parent;
	}

	if (reiser4_object_entry_prep(ctx->in.fs->tree, parent, 
				      &entry, name))
		goto error_close_parent;

	if (reiser4_object_link(parent, object, &entry)) {
		aal_error("Failed to link object.");
		goto error_close_parent;
	}
		
	reiser4_object_close(object);
	reiser4_object_close(parent);
	
	return 0;
	
 error_close_parent:
	reiser4_object_close(parent);
 error_object_close:
	reiser4_object_close(object);
	return -EIO;
}

