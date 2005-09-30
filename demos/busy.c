/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
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
		.info = "Creates a regular file on reiser4.",
	},
	[1] = {
		.name = "mkdir",
		.options = "PATH",
		.handler = create_cmd,
		.ops_num = 1,
		.info = "Creates a directory on reiser4.",
	},
	[2] = {
		.name = "mknod",
		.options = "PATH [char/fifo/block/sock] MAJOR MINOR",
		.handler = create_cmd,
		.ops_num = 4,
		.info = "Creates a device file on reiser4.",
	},
	[3] = {
		.name = "ln-s",
		.options = "PATH link_name",
		.handler = create_cmd,
		.ops_num = 2,
		.info = "Creates a symlink on reiser4.",
	},
	[4] = {
		.name = "ln",
		.options = "PATH link_name",
		.handler = ln_cmd,
		.ops_num = 2,
		.info = "Creates a hard link on reiser4.",
	},
	[5] = {
		.name = "ls",
		.options = "PATH",
		.handler = ls_cmd,
		.ops_num = 1,
		.info = "Lists a directory.",
	},
	[6] = {
		.name = "rm",
		.options = "PATH",
		.handler = rm_cmd,
		.ops_num = 1,
		.info = "Removes a file.",
	},
	[7] = {
		.name = "cp",
		.options = "PATH1 PATH2 in_offset out_offset count blk_size",
		.handler = cp_cmd,
		.ops_num = 6,
		.info = "Copies <count> blocks of <blk_size> bytes from "
			"<PATH1> to <PATH2>\n\tskipping <in_offset> bytes, "
			"seeking on <out_offset> bytes.\n\t<in_offset>, "
			"<out_offset> and <count> could be given -1, "
			"that is,\n\tstart, end, unlimited correspondingly.",
	},
	[8] = {
		.name = "stat",
		.options = "PATH",
		.handler = stat_cmd,
		.ops_num = 1,
		.info = "Stats a file.",
	},
	[9] = {
		.name = "trunc",
		.options = "PATH size",
		.handler = trunc_cmd,
		.ops_num = 2,
		.info = "Truncates a file.",
	},
	[10] = {
		.name = "reg",
		.options = "PATH",
		.handler = reg_test,
		.ops_num = 1,
		.info = "Just a test. Creates some amount of files.",
	},
	[11] = {
		.name = "sym",
		.options = "PATH",
		.handler = sym_test,
		.ops_num = 1,
		.info = "Just a test. Creates some amount of symlinks.",
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
		goto error_close_device;
	}

	if (!(fs->journal = reiser4_journal_open(fs, device))) {
		aal_error("Failed to open the journal on %s.", name);
		goto error_close_fs;
	}
	
	fs->tree->mpc_func = misc_mpressure_detect;
	
	if (reiser4_journal_replay(fs->journal)) {
		aal_error("Failed to replay the journal on %s.", name);
		goto error_close_journal;
	}
	
	reiser4_journal_close(fs->journal);
	fs->journal = NULL;

	reiser4_fs_sync(fs);
	return fs;

 error_close_journal:
	reiser4_journal_close(fs->journal);
 error_close_fs:
	reiser4_fs_close(fs);
 error_close_device:
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

	fprintf(stderr, "\nUsage: busy COMMAND ARGS\n");
	fprintf(stderr, "Commands with args: \n");
	
	for (i = 0; i < TESTS_COUNT; i++) {
		fprintf(stderr, "\t%s %s\n", tests[i].name, tests[i].options);
		fprintf(stderr, "\t%s\n\n", tests[i].info);
	}
		
	fprintf(stderr, "\nPATH = {device:path || ^path}\n");
	fprintf(stderr, "when working with reiser4 fs on 'device' and others "
		"respectively.\nFor others '^' is mandatory.\n\n");
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

	if (!(dir1 = reiser4_dir_create(dir0, "test1 dir1"))) {
		aal_error("Failed to create a dir 'test1 dir1'.");
		return -EIO;
	}

	if (!(dir2 = reiser4_dir_create(dir0, "test1 dir2"))) {
		aal_error("Failed to create a dir 'test1 dir2'.");
		return -EIO;
	}
	
	if (!(reg0 = reiser4_reg_create(dir1, "test1 file0"))) {
		aal_error("Failed to create a reg 'test1 file0'.");
		return -EIO;
	}
	
	if (!(sym0 = reiser4_sym_create(dir2, "test1 sym0", 
					"../test1 dir1/test1 file0")))
	{
		aal_error("Failed to create a sym 'test1 sym0'.");
		return -EIO;
	}
	
	aal_snprintf(name, 256, "%s/test1 dir1/test1 file0", path);
	
	if (!(sym1 = reiser4_sym_create(dir2, "test1 sym1", name))) {
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
			if (!(object = reiser4_reg_create(dir, name[k])))
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
	
	if (path[0] != '^') {
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
	} else {
		path++;
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
	    ctx->testno == 12) 
	{
		/* The second parameter is NAME. */
		if (busy_path_parse(&ctx->out, params[3])) {
			busy_ctx_fini(ctx);
			return 0x8;
		}
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
			aal_error("Invalid major is specified: '%s'.", 
				  params[4]);
			return 0x10;
		}
		
		ctx->rdev = num << 8;
		
		if ((num = misc_str2long(params[5], 0)) == INVAL_DIG) {
			aal_error("Invalid MINOR is specified: '%s'.", 
				  params[5]);
			return 0x10;
		}

		ctx->rdev |= num;
	}
	
	if (ctx->testno == 7) {
		/* Parse numbers given to the test "cp". */
		ctx->in.offset = misc_str2long(params[4], 0);
		ctx->out.offset = misc_str2long(params[5], 0);
		ctx->count = misc_str2long(params[6], 0);
		ctx->blksize = misc_str2long(params[7], 0);
		
		if (ctx->in.offset == INVAL_DIG || ctx->in.offset < 0) {
			aal_error("Invalid input offset is specified: '%s'.", 
				  params[4]);
			return 0x10;
		}

		if (ctx->out.offset == INVAL_DIG || ctx->out.offset < 0) {
			aal_error("Invalid output offset is specified: '%s'.", 
				  params[5]);
			return 0x10;
		}

		if (ctx->count == INVAL_DIG) {
			aal_error("Invalid block count is specified: '%s'.", 
				  params[6]);
			return 0x10;
		}

		if (ctx->blksize == INVAL_DIG || ctx->blksize <= 0) {
			aal_error("Invalid block size is specified: '%s'.", 
				  params[7]);
			return 0x10;
		}
	}

	if (ctx->testno == 9) {
		ctx->count = misc_str2long(params[3], 0);
		if (ctx->count == INVAL_DIG || ctx->count < 0) {
			aal_error("Invalid size is specified: '%s'.", 
				  params[4]);
			return 0x10;
		}
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

	res = tests[ctx.testno].handler(&ctx);
	
	busy_ctx_fini(&ctx);
	libreiser4_fini();
    
	return res;
}
