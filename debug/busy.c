/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   busy.c -- reiser4 busybox -- set of progs are used in debugging. */

#include "busy.h"
#include <sys/stat.h>

busy_cmd_t tests[] = {
	[0] = {
		.name = "create",
		.options = "PATH",
		.handler = create_cmd,
		.ops_num = 1,
	},
	[1] = {
		.name = "mkdir",
		.options = "PATH",
		.handler = create_cmd,
		.ops_num = 1,
	},
	[2] = {
		.name = "mknod",
		.options = "PATH [char/fifo/block/sock] MAJOR MINOR",
		.handler = create_cmd,
		.ops_num = 4,
	},
	[3] = {
		.name = "ln-s",
		.options = "PATH link_name",
		.handler = create_cmd,
		.ops_num = 2,
	},
	[4] = {
		.name = "ln",
		.options = "PATH link_name",
		.handler = ln_cmd,
		.ops_num = 2,
	},
	[5] = {
		.name = "ls",
		.options = "PATH",
		.handler = ls_cmd,
		.ops_num = 1,
	},
	[6] = {
		.name = "rm",
		.options = "PATH",
		.handler = rm_cmd,
		.ops_num = 1,
	},
	[7] = {
		.name = "cp",
		.options = "PATH PATH in_offset out_offset count blk_size",
		.handler = NULL,
		.ops_num = 6,
	},
	[8] = {
		.name = "stat",
		.options = "PATH",
		.handler = stat_cmd,
		.ops_num = 1,
	},
	[9] = {
		.name = "reg",
		.options = "PATH",
		.handler = reg_test,
		.ops_num = 1,
	},
	[10] = {
		.name = "sym",
		.options = "PATH",
		.handler = sym_test,
		.ops_num = 1,
	},
	[11] = {
		.name = "read",
		.options = "PATH output",
		.handler = read_test,
		.ops_num = 2,
	}
};

static reiser4_fs_t *busy_fs_open(char *name) {
	aal_device_t *device;
	reiser4_fs_t *fs;

	if (!(device = aal_device_open(&file_ops, name, 512, O_RDWR))) {
		aal_error("Can't open device %s.", name);
		return NULL;
	}
    
	if (!(fs = reiser4_fs_open(device, 1))) {
		aal_error("Can't open filesystem on %s.", name);
		goto error_free_device;
	}

	fs->tree->mpc_func = misc_mpressure_detect;
	
	return fs;
	
 error_free_device:
	aal_device_close(device);
	return NULL;
}

static void busy_fs_close(reiser4_fs_t *fs) {
	aal_device_t *device;
	
	if (!fs) return;
	
	device = fs->device;
	reiser4_fs_close(fs);
	aal_device_close(device);
}

static void busy_print_usage(void) {
	unsigned int i;

	fprintf(stderr, "\nUsage: busy COMMAND args\n");
	fprintf(stderr, "Commands: \n");
	
	for (i = 0; i < TESTS_COUNT; i++)
		fprintf(stderr, "\t%s %s\n", tests[i].name, tests[i].options);
		
	fprintf(stderr, "\nPATH = {device:path || !path}\n");
	fprintf(stderr, "when working with reiser4 fs on 'device' and others "
		"respectively.\n\n");
}

static int busy_get_testno(char *name) {
	unsigned int i;

	for (i = 0; i < TESTS_COUNT; i++) {
		if (aal_strlen(name) != aal_strlen(tests[i].name))
			continue;
		
		if (!aal_memcmp(name , tests[i].name, 
				aal_strlen(tests[i].name)))
		{
			return i;
		}
	}

	return -1;
}

static errno_t busy_init(void) {
	int i;
	
	for (i = 0; i < EXCEPTION_TYPE_LAST; i++)
		misc_exception_set_stream(i, stderr);

	if (libreiser4_init()) {
		aal_error("Can't initialize libreiser4.");
		return 0x80;
	}

	return 0;
}

errno_t sym_test(busy_ctx_t *ctx) {
	reiser4_object_t *dir0, *dir1, *dir2, *sym0, *sym1, *reg0;
	reiser4_fs_t *fs = ctx->in.fs;
	char *path = ctx->in.path;
	char name[256];
	
	if (!(dir0 = reiser4_semantic_open(fs->tree, path, NULL, 1))) {
                aal_error("Can't open dir %s.", path);
                return -EIO;
        }

	if (!(dir1 = reiser4_dir_create(fs, dir0, "test1 dir1"))) {
		aal_error("Failed to create a dir 'test1 dir1'.");
		return -EIO;
	}

	if (!(dir2 = reiser4_dir_create(fs, dir0, "test1 dir2"))) {
		aal_error("Failed to create a dir 'test1 dir2'.");
		return -EIO;
	}
	
	if (!(reg0 = reiser4_reg_create(fs, dir1, "test1 file0"))) {
		aal_error("Failed to create a reg 'test1 file0'.");
		return -EIO;
	}
	
	if (!(sym0 = reiser4_sym_create(fs, dir2, "test1 sym0", 
					"../test1 dir1/test1 file0")))
	{
		aal_error("Failed to create a sym 'test1 sym0'.");
		return -EIO;
	}
	
	aal_snprintf(name, 256, "%s/test1 dir1/test1 file0", path);
	
	if (!(sym1 = reiser4_sym_create(fs, dir2, "test1 sym1", name))) {
		aal_error("Failed to create a sym 'test1 sym1'.");
		return -EIO;
	}
	
	reiser4_object_close(dir0);
	reiser4_object_close(dir1);
	reiser4_object_close(dir2);
	reiser4_object_close(reg0);
	reiser4_object_close(sym0);
	reiser4_object_close(sym1);
	
	aal_snprintf(name, 256, "%s/test1 dir2/test1 sym0", path);

	if (!(sym0 = reiser4_semantic_open(fs->tree, name, NULL, 1))) {
		aal_error("Failed to open a sym '%s'.", name);
		return -EIO;
	}

	aal_snprintf(name, 256, "%s/test1 dir2/test1 sym1", path);
	
	if (!(sym1 = reiser4_semantic_open(fs->tree, name, NULL, 1))) {
		aal_error("Failed to create a sym '%s'.", name);
		return -EIO;
	}
	
	reiser4_object_close(sym0);
	reiser4_object_close(sym1);

	reiser4_tree_compress(fs->tree);
	return 0;
}

errno_t read_test(busy_ctx_t *ctx) {
	reiser4_fs_t *fs = ctx->in.fs;
	char *path = ctx->in.path;
	char *out = ctx->out.path;
	reiser4_object_t *obj;
	char buf[40960];
	int read, wrote;
	FILE *file;
	
#define ENABLE_MINIMAL	
	if (!path) {
		aal_error("No file on the filesystem is specified.");
		return -EINVAL;
	}
		
	if (!out) {
		aal_error("No output file is specified.");
		return -EINVAL;
	}
	
	if (!(obj = reiser4_semantic_open(fs->tree, path, NULL, 1))) {
                aal_error("Can't open dir %s.", path);
                return -EINVAL;
        }

	if (obj->ent->opset.plug[OPSET_OBJ]->id.group != REG_OBJECT) {
		aal_error("Not regular file is specified %s.", path);
		goto error;
	}

	
	if (!(file = fopen(out, "w"))) {
		aal_error("Failed to open the output file %s.", out);
		goto error;
	}
	
	while ((read = reiser4_object_read(obj, buf, 40960)) > 0) {
		wrote = fwrite(buf, 1, read, file);
		
		if (read != wrote)
			aal_error("Write failed: read %d bytes, wrote %d.", read, wrote);
	}

	if (read < 0)
		aal_error("Read failed.");
	
	fclose(file);
	reiser4_object_close(obj);
#undef ENABLE_MINIMAL	
	
	return 0;
	
 error:
	reiser4_object_close(obj);
	return -EINVAL;
}

errno_t reg_test(busy_ctx_t *ctx) {
	reiser4_fs_t *fs = ctx->in.fs;
	reiser4_object_t *dir;
	char *path = ctx->in.path;
	
	int i, k;
	char name[6][256];

	//FILE *file;
	reiser4_object_t *object;
	entry_hint_t entry;

//	misc_param_override("hash=deg_hash");
//	misc_param_override("policy=tails");

	if (!(dir = reiser4_semantic_open(fs->tree, path, NULL, 1))) {
                aal_error("Can't open dir %s.", path);
                return -EINVAL;
        }
	
	srandom(time(0));

	//		file = fopen("/home/umka/tmp/out", "r");

	//		while (!feof(file)) {
	for (i = 0; i < 1000; i++) {
		//                        int j, count;
		//			char part[256];
		uint64_t size[3] = {51200, 0, 1024};

		aal_snprintf(name[0], 256, "test0 file name%d", i/*random()*/);
		aal_snprintf(name[1], 256, "test0 file name%d.c", i/*random()*/);
		aal_snprintf(name[2], 256, "test0 file name%d.o", i/*random()*/);
		/*
		aal_snprintf(name[3], 256, "test0 very very long file name%d", i);
		aal_snprintf(name[4], 256, "test0 very very long file name%d.c", i);
		aal_snprintf(name[5], 256, "test0 very very long file name%d.o", i);

		//			fscanf(file, "%s %s\n", name, part);
		//			strcat(name, " ");
		//			strcat(name, part);
		//			printf("%s\n", name);
		*/
		for (k = 0; k < 3; k++) {
			if (!(object = reiser4_reg_create(fs, dir, name[k])))
				continue;


			if (reiser4_object_write(object, NULL, size[k]) < 0) {
				aal_error("Can't write data to file %s.", 
					  name[k]);
				break;
			}
#if 0
			count = 1000;

			for (j = 0; j < count; j++) {
				/* reiser4_object_seek(object, 
				   reiser4_object_offset(object) + 8193);*/

				if (reiser4_object_write(object, name[k],
							 aal_strlen(name[k])) < 0)
				{
					aal_error("Can't write data "
						  "to file %s.", name[k]);
					break;
				}
			}
#endif
			if (object) {
				reiser4_object_close(object);
			} else {
				aal_error("Can't create an object '%s'.", 
					  name[k]);
			}

			/* Get the second file and write there some more. */
			if (reiser4_object_lookup(dir, name[k], &entry) != 
			    PRESENT)  
			{
				aal_error("Can't find just created object %s.",
					  name[1]);
				break;
			}

			if (!(object = reiser4_object_obtain(fs->tree, dir, 
							     &entry.object))) 
			{
				aal_error("Can't open the object %s.", name[1]);
				break;
			}

			if (reiser4_object_write(object, NULL, 1024) < 0) {
				aal_error("Can't write data to file %s.", 
					  name[k]);
				break;
			}

			reiser4_object_close(object);
		}
	}
	
	reiser4_object_close(dir);
	reiser4_tree_compress(fs->tree);
	return 0;
}

static errno_t busy_path_parse(busy_target_t *target, char *path) {
	int i, len;
	
	aal_assert("vpf-1708", target != NULL);
	aal_assert("vpf-1709", path != NULL);

	i = 0;
	len = aal_strlen(path);
	
	if (path[0] != '!') {
		for (i = 0; i < 256 && i < len; i++) {
			if (path[i] == ':')
				break;

			target->path[i] = path[i];
		}

		if (i == len || path[i] != ':') {
			aal_error("Wrong PATH format detected.");
			return -EINVAL;
		}
			
		target->path[i] = 0;

		if (!(target->fs = busy_fs_open(target->path)))
			return -EINVAL;

		path += i + 1;
	}
	
	target->path[0] = 0;

	aal_memset(target->path, 0, PATH_MAXLEN);
	aal_strncpy(target->path, path, PATH_MAXLEN - 1);
	
	return 0;
}

static void busy_ctx_fini(busy_ctx_t *ctx) {
	if (!ctx) return;

	if (ctx->in.fs)
		busy_fs_close(ctx->in.fs);
	if (ctx->out.fs)
		busy_fs_close(ctx->out.fs);
}

static int busy_set_objtype(int testno) {
	if (testno == 0)
		return REG_OBJECT;
	if (testno == 1)
		return DIR_OBJECT;
	if (testno == 2)
		return SPL_OBJECT;
	if (testno == 3)
		return SYM_OBJECT;
	return -1;
}

static errno_t busy_ctx_init(busy_ctx_t *ctx, int count, char *params[]) {
	aal_assert("vpf-1707", ctx != NULL);

	aal_memset(ctx, 0, sizeof(*ctx));
	
	if (count < 3) {
		busy_print_usage();
		return 0x10;
	}
	
	/* Get the test number. */
	if ((ctx->testno = busy_get_testno(params[1])) == -1) {
		aal_error("Cannot find test '%s'.", params[1]);
		return 0x10;
	}

	if (!tests[ctx->testno].handler) {
		aal_error("The test '%s' is not implemented yet.", params[1]);
		return 0x10;
	}

	if (tests[ctx->testno].ops_num + 2 != count) {
		aal_error("Wrong parameters are given to the test '%s'.", 
			  params[1]);
		return 0x10;
	}
	
	/* First parameter should be a PATH on reiser4. */
	if (busy_path_parse(&ctx->in, params[2]))
		return 0x8;
	
	if (ctx->testno == 7) {
		/* The second parameter is also PATH. */
		if (busy_path_parse(&ctx->out, params[3])) {
			busy_ctx_fini(ctx);
			return 0x8;
		}
	}

	if (ctx->testno == 3 || 
	    ctx->testno == 4 || 
	    ctx->testno == 11) 
	{
		/* The second parameter is NAME. */
		aal_strncpy(ctx->out.path, params[3], PATH_MAXLEN - 1);
	}

	ctx->objtype = busy_set_objtype(ctx->testno);
	
	if (ctx->testno == 2) {
		long long num;
		
		/* Parse Special device type & rdev. */
		if (!aal_strcmp(params[3], "char"))
			ctx->mode |= S_IFCHR;
		else if (!aal_strcmp(params[3], "fifo"))
			ctx->mode |= S_IFIFO;
		else if (!aal_strcmp(params[3], "block"))
			ctx->mode |= S_IFBLK;
		else if (!aal_strcmp(params[3], "sock"))
			ctx->mode |= S_IFSOCK;
		else {
			aal_error("The special file type '%s' is not correct.",
				  params[3]);
			return 0x10;
		}

		if ((num = misc_str2long(params[4], 0)) == INVAL_DIG) {
			aal_error("Invalid major is spesified: '%s'.", 
				  params[4]);
			return 0x10;
		}
		
		ctx->rdev = num << 8;
		
		if ((num = misc_str2long(params[5], 0)) == INVAL_DIG) {
			aal_error("Invalid MINOR is spesified: '%s'.", 
				  params[5]);
			return 0x10;
		}

		ctx->rdev |= num;
	}
	
	return 0;
}


int main(int argc, char *argv[]) {
	busy_ctx_t ctx;
	errno_t res;
	
	if (busy_init())
	    return 0x80;

	aal_memset(&ctx, 0, sizeof(ctx));
	if ((res = busy_ctx_init(&ctx, argc, argv))) {
		libreiser4_fini();
		return res;
	}

	tests[ctx.testno].handler(&ctx);
	
	busy_ctx_fini(&ctx);
	libreiser4_fini();
    
	return 0;
}
