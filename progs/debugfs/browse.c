/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   browse.c -- filesystem browse code. */

#include <unistd.h>
#include <stdio.h>
#include "debugfs.h"

/* If file is a regular one we show its contant here */
static errno_t debugfs_reg_cat(reiser4_object_t *object) {
	errno_t res;
	int32_t read;
	char buff[4096];
	
	if ((res = reiser4_object_reset(object))) {
		aal_error("Can't reset object %s.",
			  object->name);
		return res;
	}

	/* The loop until object_read returns zero bytes read */
	while (1) {
		aal_memset(buff, 0, sizeof(buff));
		
		read = reiser4_object_read(object, buff,
				           sizeof(buff));
		if (read <= 0)
			break;

		debugfs_print_buff(buff, read);
	}

	return 0;
}

/* If object is the directory, we show its contant here */
static errno_t debugfs_dir_cat(reiser4_object_t *object) {
	errno_t res;
	char buff[4096];
	entry_hint_t entry;
	
	if ((res = reiser4_object_reset(object))) {
		aal_error("Can't reset object %s.",
			  object->name);
		return res;
	}

	/* The loop until all entry read */
	while (reiser4_object_readdir(object, &entry) > 0) {
		char *key = reiser4_print_key(&entry.object,
					      PO_DEFAULT);
		
		aal_memset(buff, 0, sizeof(buff));
		
		aal_snprintf(buff, sizeof(buff), "[%s] %s\n",
			     key, entry.name);

		debugfs_print_buff(buff, aal_strlen(buff));
	}

	printf("\n");
	
	return 0;
}

/* Show special device info. */
static errno_t debugfs_spl_cat(reiser4_object_t *object) {
	errno_t res;
	char buff[256];

	sdhint_lw_t lwh;
	stat_hint_t stath;
	sdhint_unix_t unixh;

	aal_memset(&stath, 0, sizeof(stath));

	/* Preparing stat data hint. */
	stath.extmask = (1 << SDEXT_UNIX_ID |
			 1 << SDEXT_LW_ID);
	
	stath.ext[SDEXT_LW_ID] = &lwh;
	stath.ext[SDEXT_UNIX_ID] = &unixh;

	if ((res = reiser4_object_stat(object, &stath))) {
		aal_error("Can't stat object %s.",
			  object->name);
		return res;
	}

	/* Printing @rdev and @mode. */
	aal_memset(buff, 0, sizeof(buff));
		
	aal_snprintf(buff, sizeof(buff), "rdev:\t0x%llx\n"
		     "mode:\t0x%u\n", unixh.rdev, lwh.mode);

	debugfs_print_buff(buff, aal_strlen(buff));

	return 0;
}

/* Common entry point for --ls and --cat options handling code */
errno_t debugfs_browse(reiser4_fs_t *fs, char *filename) {
	errno_t res = -EINVAL;
	reiser4_object_t *object;
	
	if (!(object = reiser4_semantic_open(fs->tree, filename, 1))) {
		aal_error("Can't open %s.", filename);
		return -EINVAL;
	}

	switch (object->ent->opset.plug[OPSET_OBJ]->id.group) {
	case REG_OBJECT:
		res = debugfs_reg_cat(object);
		break;
	case DIR_OBJECT:
		res = debugfs_dir_cat(object);
		break;
	case SYM_OBJECT:
		res = debugfs_reg_cat(object);
		break;
	case SPL_OBJECT:
		res = debugfs_spl_cat(object);
		break;
	default:
		aal_info("Sorry, browsing of the special "
			 "files is not implemented yet.");
	}
	
	reiser4_object_close(object);
	return res;
}
