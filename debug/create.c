/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   create.c -- a demo program that creates a reg file on reiser4. */

#include "busy.h"

errno_t create_cmd(busy_ctx_t *ctx) {
	reiser4_object_t *parent, *object;
	char *sep;

	aal_assert("vpf-1710", ctx != NULL);

	if (!ctx->in.fs) {
		aal_error("Create failed: fs is not openned. "
			  "Wrong PAth is specified: %s.",
			  ctx->in.path);
		return -EINVAL;
	}
	
	if (ctx->in.path[0] == 0) {
		aal_error("Create failed: NULL path is given.");
		return -EINVAL;
	}
	
	if (!(sep = aal_strrchr(ctx->in.path, '/'))) {
		aal_error("Wrong PATH format is detected: %s.", ctx->in.path);
		return -EINVAL;
	}

	if (sep == ctx->in.path) {
		/* Create file in the root. */
		parent = reiser4_semantic_open(ctx->in.fs->tree, 
					       "/", NULL, 1);
	} else {
		sep[0] = 0;
		parent = reiser4_semantic_open(ctx->in.fs->tree, 
					       ctx->in.path, NULL, 1);
		sep[0] = '/';
	}
	
	if (!parent) {
		aal_error("Can't open file %s.", ctx->in.path);
		sep[0] = '/';
		return -EINVAL;
	}
	
	sep[0] = '/';
	sep++;

	if (ctx->objtype == REG_OBJECT)
		object = reiser4_reg_create(ctx->in.fs, parent, sep);
	else if (ctx->objtype == DIR_OBJECT)
		object = reiser4_dir_create(ctx->in.fs, parent, sep);
	else if (ctx->objtype == SPL_OBJECT)
		object = reiser4_spl_create(ctx->in.fs, parent, sep, 
					    ctx->mode, ctx->rdev);
	else if (ctx->objtype == SYM_OBJECT) {
		if (ctx->out.path[0] == 0)
			goto error_free_parent;
		
		object = reiser4_sym_create(ctx->in.fs, parent, 
					    sep, ctx->out.path);
	} else {
		aal_error("Illegal object type is given (%d).", ctx->objtype);
		goto error_free_parent;
	}
	
	if (!object) {
		aal_error("Failed to create the file %s.", ctx->in.path);
		goto error_free_parent;
	}

	reiser4_object_close(object);
	reiser4_object_close(parent);
	
	return 0;
	
 error_free_parent:
	reiser4_object_close(parent);
	return -EIO;
}

