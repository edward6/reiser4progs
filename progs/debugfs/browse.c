/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   browse.c -- filesystem browse code. */

#include <unistd.h>
#include <stdio.h>
#include "debugfs.h"

/* If file is a regular one we show its contant here */
static errno_t debugfs_object_cat(reiser4_object_t *object) {
	errno_t res;
	
	if ((res = reiser4_object_reset(object))) {
		aal_exception_error("Can't reset object %s.",
				    object->name);
		return res;
	}

	/* The loop until object_read returns zero bytes read */
	while (1) {
		int32_t read;
		unsigned char buff[4096];
		
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
static errno_t debugfs_object_ls(reiser4_object_t *object) {
	errno_t res;
	char buff[4096];
	entry_hint_t entry;
	
	if ((res = reiser4_object_reset(object))) {
		aal_exception_error("Can't reset object %s.",
				    object->name);
		return res;
	}

	/* The loop until all entry read */
	while (1) {
		aal_memset(buff, 0, sizeof(buff));

		if (reiser4_object_readdir(object, &entry) != 0)
			break;

		reiser4_key_string(&entry.object, buff);

		aal_snprintf(buff + aal_strlen(buff), sizeof(buff),
			     " %s\n", entry.name);

		debugfs_print_buff(buff, aal_strlen(buff));
	}

	printf("\n");
	
	return 0;
}

/* Common entry point for --ls and --cat options handling code */
errno_t debugfs_browse(reiser4_fs_t *fs, char *filename) {
	errno_t res = -EINVAL;
	reiser4_object_t *object;
	
	if (!(object = reiser4_object_open(fs->tree, filename, TRUE)))
		return -EINVAL;

	switch (object->entity->plug->id.group) {
	case FILE_OBJECT:
		res = debugfs_object_cat(object);
		break;
	case DIRTORY_OBJECT:
		res = debugfs_object_ls(object);
		break;
	default:
		aal_exception_info("Sorry, browsing of the special "
				   "files is not implemented yet.");
	}
	
	reiser4_object_close(object);
	return res;
}
