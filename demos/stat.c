/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   stat.c -- a demo program that works similar to the standard stat utility. */

#include "busy.h"

errno_t stat_cmd(busy_ctx_t *ctx) {
	reiser4_object_t *object;
	aal_stream_t stream;

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
					     ctx->in.path, NULL, 1)))
	{
		aal_error("Can't open file %s.", ctx->in.path);
		return -EIO;
	}

	aal_stream_init(&stream, NULL, &memory_stream);

	plug_call(object->info.start.plug->pl.item->debug, print, 
		  &object->info.start, &stream, 0);
		
	printf("%s\n", (char *)stream.entity);
	
	aal_stream_fini(&stream);
	reiser4_object_close(object);
	
	return 0;
}

