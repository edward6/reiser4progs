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
static errno_t debugfs_file_cat(reiser4_file_t *file) {
	int32_t read;
	char buff[4096];
	
	if (reiser4_file_reset(file)) {
		aal_exception_error("Can't reset file %s.", file->name);
		return -1;
	}
	
	while (1) {
		aal_memset(buff, 0, sizeof(buff));

		if (!(read = reiser4_file_read(file, buff, sizeof(buff))))
			break;

		debugfs_print_buff(buff, read);
	}

	return 0;
}

/* If file is the directory, we show its contant here */
static errno_t debugfs_file_ls(reiser4_file_t *file) {
	reiser4_entry_hint_t entry;
	
	if (reiser4_file_reset(file)) {
		aal_exception_error("Can't reset file %s.", file->name);
		return -1;
	}
	
	while (reiser4_file_read(file, &entry, 1)) {
		aal_stream_t stream;
		aal_stream_init(&stream);
		
		reiser4_key_print(&entry.object, &stream);
		aal_stream_format(&stream, " %s\n", entry.name);
		debugfs_print_stream(&stream);
		
		aal_stream_fini(&stream);
	}

	printf("\n");
	
	return 0;
}

/* Common entry point for --ls and --cat options handling code */
errno_t debugfs_browse(reiser4_fs_t *fs, char *filename) {
	errno_t res = 0;
	reiser4_file_t *file;
	
	if (!(file = reiser4_file_open(fs, filename)))
		return -1;

	/* Determining what the type file is */
	if (file->entity->plugin->h.group == REGULAR_FILE)
		res = debugfs_file_cat(file);
	else if (file->entity->plugin->h.group == DIRTORY_FILE)
		res = debugfs_file_ls(file);
	else {
		aal_exception_info("Sorry, browsing of the special files "
				   "is not implemented yet.");
	}
	
	reiser4_file_close(file);
	return res;
}
