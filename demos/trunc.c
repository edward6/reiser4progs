/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   trunc.c -- a demo program that truncates the file to the given size. */

#include "busy.h"

errno_t trunc_cmd(busy_ctx_t *ctx) {
	reiser4_object_t *object;

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
	
	if (!(object = reiser4_semantic_open(ctx->in.fs->tree, 
					     ctx->in.path, NULL, 1))) 
	{
		aal_error("Can't open file %s.", ctx->in.path);
		return -EIO;
	}
	
	if (objplug(object)->id.group != REG_OBJECT) {
		aal_error("The object %s is not a regular file.", 
			  ctx->in.path);
		goto error_close_object;
	}
	
	/* Unlink the object. */
	if (reiser4_object_truncate(object, ctx->count)) {
		aal_error("Can't truncate object %s to %lld bytes.", 
			  ctx->in.path, ctx->count);
		goto error_close_object;
	}
	
	reiser4_object_close(object);
	return 0;
	
 error_close_object:
	reiser4_object_close(object);
	return -EIO;
}

