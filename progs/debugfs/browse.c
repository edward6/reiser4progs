/*
  browse.c -- filesystem browse code.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include "debugfs.h"

/* If file is a regular one we show its contant here */
static errno_t debugfs_object_cat(reiser4_object_t *object) {
	int32_t read;
	char buff[4096];
	
	if (reiser4_object_reset(object)) {
		aal_exception_error("Can't reset object %s.",
				    object->name);
		return -1;
	}

	/* The loop until object_read returns zero bytes read */
	while (1) {
		aal_memset(buff, 0, sizeof(buff));

		if ((read = reiser4_object_read(object, buff,
						sizeof(buff))) <= 0)
			break;

		debugfs_print_buff(buff, read);
	}

	return 0;
}

/* If object is the directory, we show its contant here */
static errno_t debugfs_object_ls(reiser4_object_t *object) {
	char buff[4096];
	reiser4_entry_hint_t entry;
	
	if (reiser4_object_reset(object)) {
		aal_exception_error("Can't reset object %s.",
				    object->name);
		return -1;
	}

	/* The loop until all entry read */
	while (1) {
		aal_memset(buff, 0, sizeof(buff));

		if (reiser4_object_read_entry(object, &entry) != 0)
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
	errno_t res = -1;
	reiser4_object_t *object;
	
	if (!(object = reiser4_object_open(fs, filename)))
		return -1;

	switch (object->entity->plugin->h.group) {
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
