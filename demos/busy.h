/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   busy.h -- reiser4 busybox declarations. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <misc/misc.h>
#include <reiser4/libreiser4.h>

#define TESTS_COUNT (sizeof(tests) / sizeof(busy_cmd_t))
#define PATH_MAXLEN 4096

typedef struct busy_target {
	reiser4_fs_t *fs;
	char path[PATH_MAXLEN];
	int64_t offset;
} busy_target_t;

typedef struct busy_ctx {
	int testno;
	
	busy_target_t in;
	busy_target_t out;
	
	int objtype;
	uint32_t mode;
	uint64_t rdev;
	int64_t count;
	uint32_t blksize;
} busy_ctx_t;

typedef errno_t (*cmd_handler_t) (busy_ctx_t *ctx);

typedef struct busy_cmd {
	char *name;
	char *options;
	cmd_handler_t handler;
	int ops_num;
} busy_cmd_t;


extern errno_t reg_test(busy_ctx_t *ctx);
extern errno_t read_test(busy_ctx_t *ctx);
extern errno_t sym_test(busy_ctx_t *ctx);

extern errno_t ls_cmd(busy_ctx_t *ctx);
extern errno_t stat_cmd(busy_ctx_t *ctx);
extern errno_t create_cmd(busy_ctx_t *ctx);
extern errno_t ln_cmd(busy_ctx_t *ctx);
extern errno_t rm_cmd(busy_ctx_t *ctx);
extern errno_t cp_cmd(busy_ctx_t *ctx);
extern errno_t trunc_cmd(busy_ctx_t *ctx);


extern reiser4_object_t *busy_misc_open_parent(reiser4_tree_t *tree, 
					       char **path);
