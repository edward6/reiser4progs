/*
  ls.c -- a demo program which works like standard ls utility.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_COMPACT
#  include <stdio.h>
#  include <fcntl.h>
#endif

#include <aal/aal.h>
#include <reiser4/reiser4.h>

#include <misc/misc.h>

static void ls_print_usage(void) {
	fprintf(stderr, "Usage: ls FILE DIR\n");
}

static void ls_init(void) {
	int i;
	for (i = 0; i < 5; i++)
		progs_exception_set_stream(i, stderr);
}

int main(int argc, char *argv[]) {
	reiser4_fs_t *fs;
	aal_device_t *device;

	reiser4_file_t *dir;
	reiser4_entry_hint_t entry;

#ifndef ENABLE_COMPACT    
    
	if (argc < 3) {
		ls_print_usage();
		return 0xfe;
	}
    
	ls_init();

	if (libreiser4_init()) {
		aal_exception_error("Can't initialize libreiser4.");
		return 0xff;
	}
    
	if (!(device = aal_file_open(argv[1], DEFAULT_BLOCKSIZE, O_RDWR))) {
		aal_exception_error("Can't open device %s.", argv[1]);
		goto error_free_libreiser4;
	}
    
	if (!(fs = reiser4_fs_open(device, device, 0))) {
		aal_exception_error("Can't open filesystem on %s.", 
				    aal_device_name(device));
		goto error_free_device;
	}
    
	if (!(fs->root = reiser4_file_open(fs, "/"))) {
		aal_exception_error("Can't open root dir.");
		goto error_free_fs;
	}
    
	if (!(dir = reiser4_file_open(fs, argv[2]))) {
		aal_exception_error("Can't open dir %s.", argv[2]);
		goto error_free_root;
	}
    
	{
		reiser4_plugin_t *dir_plugin;
		reiser4_file_hint_t dir_hint;
	
		dir_hint.plugin = fs->root->entity->plugin;
		dir_hint.statdata_pid = ITEM_STATDATA40_ID;
	
		dir_hint.body.dir.direntry_pid = ITEM_CDE40_ID;
		dir_hint.body.dir.hash_pid = HASH_R5_ID;
	
		{
			int i;
			char name[256];
			reiser4_file_t *file;
	    
			for (i = 0; i < 89; i++) {
				aal_memset(name, 0, sizeof(name));
				aal_snprintf(name, 256, "testdir%d", i);

				if ((file = reiser4_file_create(fs, &dir_hint, dir, name)))
					reiser4_file_close(file);
			}
		}
	}
    
	if (reiser4_file_reset(dir)) {
		aal_exception_error("Can't rewind dir %s.", argv[2]);
		goto error_free_dir;
	}
    
	while (reiser4_file_read(dir, (char *)&entry, 1)) {
		fprintf(stdout, "[%llx:%llx] %s\n", (entry.objid.locality >> 4), 
			entry.objid.objectid, entry.name);
	}
    
	reiser4_file_close(dir);
//        reiser4_fs_sync(fs);

	reiser4_file_close(fs->root);
	reiser4_fs_close(fs);
    
	libreiser4_done();
	aal_file_close(device);
    
	return 0;

 error_free_dir:
	reiser4_file_close(dir);
 error_free_root:
	reiser4_file_close(fs->root);
 error_free_fs:
	reiser4_fs_close(fs);
 error_free_device:
	aal_file_close(device);
 error_free_libreiser4:
	libreiser4_done();
    
#endif
	return 0xff;
}

