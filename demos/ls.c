/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   ls.c -- a demo program that works similar like to the standard ls utility. */

#include "busy.h"

errno_t ls_cmd(busy_ctx_t *ctx) {
	reiser4_object_t *object;
	entry_hint_t entry;

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
	
	if (!(object = reiser4_semantic_open(ctx->in.fs->tree, 
					     ctx->in.path, NULL, 1))) {
		aal_error("Can't open file %s.", ctx->in.path);
		return -EIO;
	}

	if (object->ent->opset.plug[OPSET_OBJ]->id.group == DIR_OBJECT) {
		while (reiser4_object_readdir(object, &entry) > 0) {
			printf("[%s] %s\n", reiser4_print_key(&entry.object),
			       entry.name);
		}
	} else {
		printf("[%s] %s\n", 
		       reiser4_print_key(&object->ent->object), 
		       ctx->in.path);
	}
	
	reiser4_object_close(object);
	return 0;
}

