/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   cp.c -- a demo program that works similar to the standard dd utility. */

#include "busy.h"

errno_t cp_cmd(busy_ctx_t *ctx) {
	reiser4_object_t *src_obj, *dst_obj;
	FILE *src_file, *dst_file;
	int64_t rbytes, wbytes;
	uint64_t count;
	errno_t res;
	char *buf;
    
	if (ctx->in.path[0] == 0 || ctx->out.path[0] == 0) {
		aal_error("NULL path is given.");
		return -EINVAL;
	}

	/* Open source file. */
	if (ctx->in.fs) {
		if (!(src_obj = reiser4_semantic_open(ctx->in.fs->tree, 
						      ctx->in.path, NULL, 1)))
		{
			aal_error("Can't open file %s.", ctx->in.path);
			return -EIO;
		}

		src_file = NULL;
	} else {
		if (!(src_file = fopen(ctx->in.path, "r"))) {
			aal_error("Can't open file %s.", ctx->in.path);
			return -EIO;
		}

		src_obj = NULL;
	}
	
	/* Open destination file. */
	if (ctx->out.fs) {
		if (!(dst_obj = reiser4_semantic_try_open(ctx->out.fs->tree, 
							  ctx->out.path, 
							  NULL, 1)))
		{
			char *name = ctx->out.path;
			reiser4_object_t *parent;
			
			parent = busy_misc_open_parent(ctx->out.fs->tree, &name);
			if (!parent || parent == INVAL_PTR)
				goto error_src_close;
			
			dst_obj = reiser4_reg_create(ctx->out.fs, parent, name);
			if (!dst_obj) {
				aal_error("Failed to create file %s.", 
					  ctx->out.path);
				goto error_src_close;
			}
		}

		dst_file = NULL;
	} else {
		if (!(dst_file = fopen(ctx->out.path, "w+"))) {
			aal_error("Can't open file %s.", ctx->out.path);
			goto error_src_close;
		}

		dst_obj = NULL;
	}

	if (src_obj && objplug(src_obj)->id.group != REG_OBJECT) {
		aal_error("File %s is not a regular file.", ctx->in.path);
		goto error_dst_close;
	}

	if (dst_obj && objplug(dst_obj)->id.group != REG_OBJECT) {
		aal_error("File %s is not a regular file.", ctx->out.path);
		goto error_dst_close;
	}

	if (src_file) {
		struct stat st;

		if (fstat(fileno(src_file), &st)) {
			aal_error("Can't stat the file %s.", ctx->in.path);
			goto error_dst_close;
		}

		if (!S_ISREG(st.st_mode)) {
			aal_error("File %s is not a regular file.", 
				  ctx->in.path);
			goto error_dst_close;
		}
	}
	
	if (dst_file) {
		struct stat st;

		if (fstat(fileno(dst_file), &st)) {
			aal_error("Can't stat the file %s.", ctx->out.path);
			goto error_dst_close;
		}

		if (!S_ISREG(st.st_mode)) {
			aal_error("File %s is directory.", ctx->out.path);
			goto error_dst_close;
		}
	}

	/* Seek the source file. */
	if (ctx->in.offset) {
		res = src_obj ?
			reiser4_object_seek(src_obj, ctx->in.offset):
			fseek(src_file, ctx->in.offset, SEEK_SET);
		
		if (res) {
			aal_error("Can't seek to %lld bytes in file %s.",
				  ctx->in.offset, ctx->in.path);
			goto error_dst_close;
		}
	}

	/* Seek the destination file. */
	if (ctx->out.offset) {
		res = dst_obj ?
			reiser4_object_seek(dst_obj, ctx->out.offset):
			fseek(dst_file, ctx->out.offset, SEEK_SET);
		
		if (res) {
			aal_error("Can't seek to %lld bytes in file %s.",
				  ctx->out.offset, ctx->out.path);
			goto error_dst_close;
		}
	}

	/* Alloc the data buffer. */
	if (!(buf = aal_malloc(ctx->blksize))) {
		aal_error("Can't allocate %d bytes for the buffer.", 
			  ctx->blksize);
		goto error_dst_close;
	}

	/* Do copying. */
	for (count = ctx->count; count > 0; count--) {
		/* Read. */
		
		rbytes = src_obj ?
			reiser4_object_read(src_obj, buf, ctx->blksize) :
			fread(buf, 1, ctx->blksize, src_file);
		
		if (rbytes < 0) {
			aal_error("Read of %llu-th by %d bytes failed.",
				  ctx->count - count, ctx->blksize);
			goto error_free_buf;
		}
		
		if (rbytes == 0)
			break;

		/* Write. */
		wbytes = dst_obj ?
			reiser4_object_write(dst_obj, buf, rbytes):
			fwrite(buf, 1, rbytes, dst_file);

		if (wbytes < rbytes) {
			aal_error("Write of %llu-th by %d bytes failed.",
				  ctx->count - count, ctx->blksize);
			goto error_free_buf;
		}
	}
	
	aal_free(buf);

	if (dst_obj)
		reiser4_object_close(dst_obj);
	if (dst_file)
		fclose(dst_file);
	if (src_obj)
		reiser4_object_close(src_obj);
	if (src_file)
		fclose(src_file);

	return 0;
	
 error_free_buf:
	aal_free(buf);
 error_dst_close:
	if (dst_obj)
		reiser4_object_close(dst_obj);
	if (dst_file)
		fclose(dst_file);

 error_src_close:
	if (src_obj)
		reiser4_object_close(src_obj);
	if (src_file)
		fclose(src_file);

	return -EIO;
}
