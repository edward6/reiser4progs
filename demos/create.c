/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   create.c -- a demo program that creates a reg file on reiser4. */

#include "busy.h"

errno_t create_cmd(busy_ctx_t *ctx) {
	reiser4_object_t *parent, *object;
	char *name;

	aal_assert("vpf-1710", ctx != NULL);

	if (!ctx->in.fs) {
		aal_error("Fs is not openned. Wrong PATH is specified: %s.",
			  ctx->in.path);
		return -EINVAL;
	}
	
	if (ctx->in.path[0] == 0) {
		aal_error("NULL path is given.");
		return -EINVAL;
	}
	
	name = ctx->in.path;
	
	parent = busy_misc_open_parent(ctx->in.fs->tree, &name);
	if (!parent || parent == INVAL_PTR)
		return -EINVAL;
	
	if (ctx->objtype == REG_OBJECT)
		object = reiser4_reg_create(parent, name);
	else if (ctx->objtype == DIR_OBJECT)
		object = reiser4_dir_create(parent, name);
	else if (ctx->objtype == SPL_OBJECT)
		object = reiser4_spl_create(parent, name, ctx->mode, ctx->rdev);
	else if (ctx->objtype == SYM_OBJECT) {
		if (ctx->out.path[0] == 0)
			goto error_free_parent;
		
		object = reiser4_sym_create(parent, name, ctx->out.path);
	} else if (ctx->objtype == (REG_OBJECT | OBJFLAG_CRYCOM)) {
		object = reiser4_ccreg_create(parent, name, NULL);
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

